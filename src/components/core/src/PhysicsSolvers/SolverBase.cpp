/*
 * SolverBase.cpp
 *
 *  Created on: Dec 2, 2014
 *      Author: rrsettgast
 */

#include "SolverBase.hpp"

namespace geosx
{

using namespace dataRepository;

SolverBase::SolverBase( std::string const & name,
                        ManagedGroup * const parent ) :
  ManagedGroup( name, parent )
{}

SolverBase::~SolverBase()
{}

SolverBase::CatalogInterface::CatalogType& SolverBase::GetCatalog()
{
  static SolverBase::CatalogInterface::CatalogType catalog;
  return catalog;
}

void SolverBase::ReadXML( pugi::xml_node const & solverNode, cxx_utilities::InputDocumentation & docNode  )
{
  *(this->getData<real64>(keys::courant)) = solverNode.attribute("courant").as_double(0.5);
}

void SolverBase::Registration( dataRepository::ManagedGroup * const /*domain*/ )
{
  this->RegisterViewWrapper<real64>(keys::maxDt);
  this->RegisterViewWrapper<real64>(keys::courant);
}

void SolverBase::Initialize( dataRepository::ManagedGroup& /*domain*/ )
{
  *(this->getData<real64>(keys::courant)) = std::numeric_limits<real64>::max();
}

} /* namespace ANST */
