/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */


/**
 * @file Damage.hpp
 * @brief This class overrides the SSLE constitutive updates to account for a Damage field
 *
 * In a phase-field for fracture model, the damage variable affects the Elasticity equation
 * with the degradation of the stresses. Instead of sigma = C : epsilon, we have sigma = g(d)*C:epsilon,
 * where g(d) is the degradation function. This degradation function can either be a quadratic one
 * (set LORENTZ 0) or a quasi-quadratic one (set LORENTZ 1). In general, the quadratic one will give you
 * brittle fracture behaviour. The quasi-quadratic one, combined with linear dissipation, will give you
 * cohesive fracture behaviour, with a user-defined critical stress. If you use quadratic dissipation in
 * your damage solver, set QUADRATIC_DISSIPATION to 1.
 *
 * References:
 *
 * Miehe, Christian; Hofacker, Martina; Welschinger, Fabian. A phase field model for rate-independent crack
 * propagation: Robust algorithmic implementation based on operator splits.
 * Computer Methods in Applied Mechianics and Engineering, v. 199, n. 45-48, p. 2765-2778, 2010
 *
 * Borden, Micheal J., et al. A phase-field description of dynamic brittle fracture.
 * Computer Methods in Applied Mechanics and Engineering, v. 217, p. 77-95, 2012
 *
 * Bourdin, Blaise; Francfort, Gille A.; Marigo, Jean-Jacques. The variational approach to fracture.
 * Journal of Elasticity, v. 91, n. 1-3, p. 5-148, 2008.
 *
 */

#ifndef GEOSX_CONSTITUTIVE_SOLID_DAMAGE_HPP_
#define GEOSX_CONSTITUTIVE_SOLID_DAMAGE_HPP_

// TODO: switch is placed here for different dissipation functions, etc.
//       if different damage models will use different functions, perhaps
//       should explicitly name them rather than use macros.

#define QUADRATIC_DISSIPATION 0
#define LORENTZ 0
#include "constitutive/solid/SolidBase.hpp"

namespace geosx
{
namespace constitutive
{

// DAMAGE MODEL UPDATES
//
// NOTE: This model uses the m_newStress array to represent the stress in an
//       elastic, "undamaged" configuration.  We then scale the results
//       by the damage factor whenever the true stress is requested through a getter.
//
// TODO: This approach is probably error prone---e.g. when stress data is
//       accessed directly through an array view.  We should refactor so
//       the true state is saved, similar to the plasticity models

template< typename UPDATE_BASE >
class DamageUpdates : public UPDATE_BASE
{
public:
  template< typename ... PARAMS >
  DamageUpdates( arrayView2d< real64 > const & inputDamage,
                 arrayView2d< real64 > const & inputStrainEnergyDensity,
                 real64 const & inputLengthScale,
                 real64 const & inputCriticalFractureEnergy,
                 real64 const & inputcriticalStrainEnergy,
                 PARAMS && ... baseParams ):
    UPDATE_BASE( std::forward< PARAMS >( baseParams )... ),
    m_damage( inputDamage ),
    m_strainEnergyDensity( inputStrainEnergyDensity ),
    m_lengthScale( inputLengthScale ),
    m_criticalFractureEnergy( inputCriticalFractureEnergy ),
    m_criticalStrainEnergy( inputcriticalStrainEnergy )
  {}

  using DiscretizationOps = typename UPDATE_BASE::DiscretizationOps;

  using UPDATE_BASE::smallStrainNoStateUpdate;
  using UPDATE_BASE::smallStrainUpdate;
  using UPDATE_BASE::smallStrainNoStateUpdate_StressOnly;
  using UPDATE_BASE::smallStrainUpdate_StressOnly;
  using UPDATE_BASE::hypoUpdate;
  using UPDATE_BASE::hypoUpdate_StressOnly;
  using UPDATE_BASE::hyperUpdate;
  using UPDATE_BASE::hyperUpdate_StressOnly;

  //Quasi-Quadratic Lorentz Degradation Function
  #if LORENTZ

  //Lorentz type Degradation Function

  GEOSX_FORCE_INLINE
  GEOSX_HOST_DEVICE
  virtual real64 getDegradationValue( localIndex const k,
                                      localIndex const q ) const override
  {
    #if QUADRATIC_DISSIPATION
    real64 m = m_criticalFractureEnergy/(2*m_lengthScale*m_criticalStrainEnergy);
    #else
    real64 m = 3*m_criticalFractureEnergy/(8*m_lengthScale*m_criticalStrainEnergy);
    #endif
    real64 p = 1;
    return pow( 1 - m_damage( k, q ), 2 ) /( pow( 1 - m_damage( k, q ), 2 ) + m * m_damage( k, q ) * (1 + p*m_damage( k, q )) );
  }

  GEOSX_FORCE_INLINE
  GEOSX_HOST_DEVICE
  virtual real64 getDegradationDerivative( real64 const d ) const override
  {
    #if QUADRATIC_DISSIPATION
    real64 m = m_criticalFractureEnergy/(2*m_lengthScale*m_criticalStrainEnergy);
    #else
    real64 m = 3*m_criticalFractureEnergy/(8*m_lengthScale*m_criticalStrainEnergy);
    #endif
    real64 p = 1;
    return -m*(1 - d)*(1 + (2*p + 1)*d) / pow( pow( 1-d, 2 ) + m*d*(1+p*d), 2 );
  }

  GEOSX_FORCE_INLINE
  GEOSX_HOST_DEVICE
  virtual real64 getDegradationSecondDerivative( real64 const d ) const override
  {
    #if QUADRATIC_DISSIPATION
    real64 m = m_criticalFractureEnergy/(2*m_lengthScale*m_criticalStrainEnergy);
    #else
    real64 m = 3*m_criticalFractureEnergy/(8*m_lengthScale*m_criticalStrainEnergy);
    #endif
    real64 p = 1;
    return -2*m*( pow( d, 3 )*(2*m*p*p + m*p + 2*p + 1) + pow( d, 2 )*(-3*m*p*p -3*p) + d*(-3*m*p - 3) + (-m+p+2) )/pow( pow( 1-d, 2 ) + m*d*(1+p*d), 3 );
  }

  #else
  //Standard Quadratic Degradation Function

  GEOSX_FORCE_INLINE
  GEOSX_HOST_DEVICE
  virtual real64 getDegradationValue( localIndex const k,
                                      localIndex const q ) const
  {
    return (1 - m_damage( k, q ))*(1 - m_damage( k, q ));
  }

  GEOSX_FORCE_INLINE
  GEOSX_HOST_DEVICE
  virtual real64 getDegradationDerivative( real64 const d ) const
  {
    return -2*(1 - d);
  }

  GEOSX_FORCE_INLINE
  GEOSX_HOST_DEVICE
  virtual real64 getDegradationSecondDerivative( real64 const d ) const
  {
    GEOSX_UNUSED_VAR( d )
    return 2.0;
  }
  #endif


  GEOSX_HOST_DEVICE
  virtual void smallStrainUpdate( localIndex const k,
                                  localIndex const q,
                                  real64 const ( &strainIncrement )[6],
                                  real64 ( & stress )[6],
                                  DiscretizationOps & stiffness ) const override
  {
    UPDATE_BASE::smallStrainUpdate( k, q, strainIncrement, stress, stiffness );
    real64 factor = getDegradationValue( k, q );
    LvArray::tensorOps::scale< 6 >( stress, factor );
    stiffness.scaleParams( factor );
  }


  // TODO: The code below assumes the strain energy density will never be
  //       evaluated in a non-converged / garbage configuration.

  GEOSX_HOST_DEVICE
  virtual real64 getStrainEnergyDensity( localIndex const k,
                                         localIndex const q ) const override
  {
    real64 const sed = SolidBaseUpdates::getStrainEnergyDensity( k, q );

    if( sed > m_strainEnergyDensity( k, q ) )
    {
      m_strainEnergyDensity( k, q ) = sed;
    }

    return m_strainEnergyDensity( k, q );
  }


  GEOSX_HOST_DEVICE
  real64 getRegularizationLength() const
  {
    return m_lengthScale;
  }

  GEOSX_HOST_DEVICE
  real64 getCriticalFractureEnergy() const
  {
    return m_criticalFractureEnergy;
  }

  GEOSX_HOST_DEVICE
  virtual real64 getEnergyThreshold() const
  {
    #if LORENTZ
    return m_criticalStrainEnergy;
    #else
    return 3*m_criticalFractureEnergy/(16 * m_lengthScale);
    #endif
  }

  arrayView2d< real64 > const m_damage;
  arrayView2d< real64 > const m_strainEnergyDensity;
  real64 const m_lengthScale;
  real64 const m_criticalFractureEnergy;
  real64 const m_criticalStrainEnergy;
};



class DamageBase : public SolidBase
{};

template< typename BASE >
class Damage : public BASE
{
public:

  /// @typedef Alias for LinearElasticIsotropicUpdates
  using KernelWrapper = DamageUpdates< typename BASE::KernelWrapper >;

  Damage( string const & name, dataRepository::Group * const parent );
  virtual ~Damage() override;

  static std::string CatalogName() { return string( "Damage" ) + BASE::m_catalogNameString; }
  virtual string getCatalogName() const override { return CatalogName(); }

  virtual void PostProcessInput() override;

  virtual void allocateConstitutiveData( dataRepository::Group * const parent,
                                         localIndex const numConstitutivePointsPerParentIndex ) override;


  KernelWrapper createKernelUpdates()
  {
    return BASE::template createDerivedKernelUpdates< KernelWrapper >( m_damage.toView(),
                                                                       m_strainEnergyDensity.toView(),
                                                                       m_lengthScale,
                                                                       m_criticalFractureEnergy,
                                                                       m_criticalStrainEnergy );
  }

  struct viewKeyStruct : public BASE::viewKeyStruct
  {
    static constexpr auto damageString =  "damage";
    static constexpr auto strainEnergyDensityString =  "strainEnergyDensity";
    /// string/key for regularization length
    static constexpr auto lengthScaleString  = "lengthScale";
    /// string/key for Gc
    static constexpr auto criticalFractureEnergyString = "criticalFractureEnergy";
    /// string/key for sigma_c
    static constexpr auto criticalStrainEnergyString = "criticalStrainEnergy";

  };


protected:
  array2d< real64 > m_damage;
  array2d< real64 > m_strainEnergyDensity;
  real64 m_lengthScale;
  real64 m_criticalFractureEnergy;
  real64 m_criticalStrainEnergy;
};

}
} /* namespace geosx */

#endif /* GEOSX_CONSTITUTIVE_SOLID_DAMAGE_HPP_ */
