/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2019 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2019 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2019 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All right reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file MsrsbUtils.cpp
 */

#include "MsrsbUtils.hpp"

#include "common/GEOS_RAJA_Interface.hpp"
#include "linearAlgebra/multiscale/MeshObjectManager.hpp"
#include "linearAlgebra/multiscale/MeshUtils.hpp"
#include "LvArray/src/sortedArrayManipulation.hpp"

namespace geosx
{
namespace multiscale
{
namespace msrsb
{

ArrayOfSets< localIndex >
buildNodalConnectivity( MeshObjectManager const & nodeManager,
                        MeshObjectManager const & cellManager )
{
  MeshObjectManager::MapViewConst const nodeToCell = nodeManager.toDualRelation().toViewConst();
  MeshObjectManager::MapViewConst const cellToNode = cellManager.toDualRelation().toViewConst();
  arrayView1d< integer const > const cellGhostRank = cellManager.ghostRank();

  // Collect row sizes
  array1d< localIndex > rowLength( nodeManager.size() );
  forAll< parallelHostPolicy >( nodeManager.size(), [=, rowLength = rowLength.toView()]( localIndex const k )
  {
    meshUtils::forUniqueNeighbors< 256 >( k, nodeToCell, cellToNode, cellGhostRank, [&]( localIndex const )
    {
      ++rowLength[k];
    } );
  } );

  // Resize
  ArrayOfSets< localIndex > conn( nodeManager.size() );
  conn.resizeFromCapacities< parallelHostPolicy >( rowLength.size(), rowLength.data() );

  // Fill the map
  forAll< parallelHostPolicy >( nodeManager.size(), [=, conn = conn.toView()]( localIndex const k )
  {
    meshUtils::forUniqueNeighbors< 256 >( k, nodeToCell, cellToNode, cellGhostRank, [&]( localIndex const n )
    {
      conn.insertIntoSet( k, n );
    } );
  } );

  return conn;
}

array1d< localIndex >
makeSeededPartition( ArrayOfSetsView< localIndex const > const & connectivity,
                     arrayView1d< localIndex const > const & seeds,
                     ArrayOfSetsView< localIndex const > const & supports )
{
  localIndex const numParts = seeds.size();
  localIndex const numNodes = connectivity.size();

  array1d< localIndex > part( numNodes );
  part.setValues< parallelHostPolicy >( -1 );

  // Initialize the partitions and expansion front
  array1d< localIndex > front;
  front.reserve( numNodes );
  forAll< serialPolicy >( numParts, [&]( localIndex const ip )
  {
    part[seeds[ip]] = ip;
    for( localIndex const k : connectivity[seeds[ip]] )
    {
      if( part[k] < 0 && supports.contains( k, ip ) )
      {
        front.emplace_back( k );
      }
    }
  } );

  // Use AoA with 1 array for its atomic emplace capability
  // TODO: this might not be efficient due to thread contention;
  //       may need to use a serial policy instead (benchmark me)
  ArrayOfArrays< localIndex > unassigned( 1, 12 * numNodes );
  array1d< localIndex > newPart;

  integer numIter = 0;
  while( !front.empty() )
  {
    // Make the list unique
    localIndex const numFrontNodes = LvArray::sortedArrayManipulation::makeSortedUnique( front.begin(), front.end() );
    front.resize( numFrontNodes );
    newPart.resize( numFrontNodes );
    newPart.setValues< parallelHostPolicy >( -1 );

    // Pass 1: assign partitions to the front nodes based on majority among neighbors
    RAJA::ReduceSum< parallelHostReduce, localIndex > numAssn = 0;
    forAll< parallelHostPolicy >( front.size(), [connectivity, supports, numAssn,
                                                 front = front.toViewConst(),
                                                 part = part.toViewConst(),
                                                 newPart = newPart.toView()]( localIndex const i )
    {
      localIndex const k = front[i];
      localIndex maxCount = 0;
      meshUtils::forUniqueNeighborValues< 256 >( k, connectivity, part,
                                                 []( localIndex const p ){ return p >= 0; }, // only assigned nodes
                                                 [&]( localIndex const p, localIndex const count )
      {
        if( p >= 0 && count > maxCount && supports.contains( k, p ) )
        {
          newPart[i] = p;
          maxCount = count;
        }
      } );

      if( maxCount > 0 )
      {
        numAssn += 1;
      }
    } );

    // Terminate the loop as soon as no new assignments are made
    if( numAssn.get() == 0 )
    {
      break;
    }

    // Pass 2: copy new front partition assignments back into full partition array
    forAll< parallelHostPolicy >( front.size(), [front = front.toViewConst(),
                                                 newPart = newPart.toViewConst(),
                                                 part = part.toView()] ( localIndex const i )
    {
      part[front[i]] = newPart[i];
    } );

    // Pass 3: build a list of unassigned neighbor indices to become the new front
    forAll< parallelHostPolicy >( front.size(), [connectivity,
                                                 front = front.toViewConst(),
                                                 part = part.toViewConst(),
                                                 unassigned = unassigned.toView()]( localIndex const i )
    {
      localIndex const k = front[i];
      meshUtils::forUniqueNeighborValues< 256 >( k, connectivity,
                                                 []( localIndex const _ ){ return _; }, // just unique neighbor indices
                                                 [&]( localIndex const n ){ return part[n] < 0; }, // only unassigned nodes
                                                 [&]( localIndex const n )
      {
        unassigned.emplaceBackAtomic< parallelHostAtomic >( 0, n );
      } );
    } );

    // Make the new front expansion list
    front.clear();
    front.insert( 0, unassigned[0].begin(), unassigned[0].end() );
    unassigned.clearArray( 0 );

    ++numIter;
  }

  GEOSX_WARNING_IF( !front.empty(), "[MsRSB]: nodes not assigned to initial partition: " << front );

  // Attempt to fix unassigned nodes, if any
  forAll< parallelHostPolicy >( front.size(), [=, front = front.toViewConst(),
                                               part = part.toView() ]
                                  ( localIndex const i )
  {
    localIndex const k = front[i];
    part[k] = supports( k, 0 ); // assign to the first support the node belongs to
  } );

  return part;
}

SparsityPattern< globalIndex >
buildProlongationSparsity( MeshObjectManager const & fineManager,
                           MeshObjectManager const & coarseManager,
                           ArrayOfSetsView< localIndex const > const & supports,
                           integer const numComp )
{
  GEOSX_MARK_FUNCTION;

  // This assumes an SDC-type pattern (i.e. no coupling between dof components on the same node)
  array1d< localIndex > rowLengths( numComp * fineManager.numOwnedObjects() );
  forAll< parallelHostPolicy >( fineManager.numOwnedObjects(), [=, rowLengths = rowLengths.toView()]( localIndex const k )
  {
    for( integer ic = 0; ic < numComp; ++ic )
    {
      rowLengths[k * numComp + ic] = supports.sizeOfSet( k );
    }
  } );

  SparsityPattern< globalIndex > pattern;
  pattern.resizeFromRowCapacities< parallelHostPolicy >( rowLengths.size(),
                                                         numComp * ( coarseManager.maxGlobalIndex() + 1 ),
                                                         rowLengths.data() );

  arrayView1d< globalIndex const > const coarseLocalToGlobal = coarseManager.localToGlobalMap();
  forAll< parallelHostPolicy >( fineManager.numOwnedObjects(), [=, pattern = pattern.toView()]( localIndex const k )
  {
    localIndex const fineOffset = k * numComp;
    for( localIndex const inc : supports[k] )
    {
      globalIndex const coarseOffset = coarseLocalToGlobal[inc] * numComp;
      for( integer ic = 0; ic < numComp; ++ic )
      {
        pattern.insertNonZero( fineOffset + ic, coarseOffset + ic );
      }
    }
  } );

  return pattern;
}

void makeGlobalDofLists( arrayView1d< integer const > const & indicator,
                         integer const numComp,
                         localIndex const numLocalNodes,
                         globalIndex const firstLocalDof,
                         array1d< globalIndex > & boundaryDof,
                         array1d< globalIndex > & interiorDof )
{
  boundaryDof.reserve( numLocalNodes * numComp );
  interiorDof.reserve( numLocalNodes * numComp );
  forAll< serialPolicy >( numLocalNodes, [&]( localIndex const inf )
  {
    globalIndex const globalDof = firstLocalDof + inf * numComp;
    if( indicator[inf] )
    {
      for( integer c = 0; c < numComp; ++c )
      {
        boundaryDof.emplace_back( globalDof + c );
      }
    }
    else
    {
      for( integer c = 0; c < numComp; ++c )
      {
        interiorDof.emplace_back( globalDof + c );
      }
    }
  } );
}

} // namespace msrsb
} // namespace multiscale
} // namespace geosx
