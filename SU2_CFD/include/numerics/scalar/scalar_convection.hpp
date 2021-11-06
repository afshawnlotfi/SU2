﻿/*!
 * \file scalar_convection.hpp
 * \brief Delarations of numerics classes for discretization of
 *        convective fluxes in scalar problems.
 * \author F. Palacios, T. Economon
 * \version 7.2.1 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2021, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "../CNumerics.hpp"

/*!
 * \class CUpwScalar
 * \brief Template class for scalar upwind fluxes between nodes i and j.
 * \details This class serves as a template for the scalar upwinding residual
 *   classes.  The general structure of a scalar upwinding calculation is the
 *   same for many different  models, which leads to a lot of repeated code.
 *   By using the template design pattern, these sections of repeated code are
 *   moved to this shared base class, and the specifics of each model
 *   are implemented by derived classes.  In order to add a new residual
 *   calculation for a convection residual, extend this class and implement
 *   the pure virtual functions with model-specific behavior.
 * \ingroup ConvDiscr
 * \author C. Pederson, A. Bueno., and A. Campos.
 */
template <class FlowIndices>
class CUpwScalar : public CNumerics {
 protected:
  su2double a0 = 0.0;               /*!< \brief The maximum of the face-normal velocity and 0 */
  su2double a1 = 0.0;               /*!< \brief The minimum of the face-normal velocity and 0 */
  su2double* Flux = nullptr;        /*!< \brief Final result, diffusive flux/residual. */
  su2double** Jacobian_i = nullptr; /*!< \brief Flux Jacobian w.r.t. node i. */
  su2double** Jacobian_j = nullptr; /*!< \brief Flux Jacobian w.r.t. node j. */

  const bool implicit = false, incompressible = false, dynamic_grid = false;

  /*!
   * \brief A pure virtual function; Adds any extra variables to AD
   */
  virtual void ExtraADPreaccIn() = 0;

  /*!
   * \brief Model-specific steps in the ComputeResidual method, derived classes
   *        compute the Flux and its Jacobians via this method.
   * \param[in] config - Definition of the particular problem.
   */
  virtual void FinishResidualCalc(const CConfig* config) = 0;

 public:
  /*!
   * \brief Constructor of the class.
   * \param[in] ndim - Number of dimensions of the problem.
   * \param[in] nvar - Number of variables of the problem.
   * \param[in] config - Definition of the particular problem.
   */
  CUpwScalar(unsigned short ndim, unsigned short nvar, const CConfig* config)
    : CNumerics(ndim, nvar, config),
      implicit(config->GetKind_TimeIntScheme_Turb() == EULER_IMPLICIT),
      incompressible(config->GetKind_Regime() == ENUM_REGIME::INCOMPRESSIBLE),
      dynamic_grid(config->GetDynamic_Grid()) {
    Flux = new su2double[nVar];
    Jacobian_i = new su2double*[nVar];
    Jacobian_j = new su2double*[nVar];
    for (unsigned short iVar = 0; iVar < nVar; iVar++) {
      Jacobian_i[iVar] = new su2double[nVar];
      Jacobian_j[iVar] = new su2double[nVar];
    }
  }

  /*!
   * \brief Destructor of the class.
   */
  ~CUpwScalar() override {
    delete[] Flux;
    if (Jacobian_i != nullptr) {
      for (unsigned short iVar = 0; iVar < nVar; iVar++) {
        delete[] Jacobian_i[iVar];
        delete[] Jacobian_j[iVar];
      }
      delete[] Jacobian_i;
      delete[] Jacobian_j;
    }
  }

  /*!
   * \brief Compute the scalar upwind flux between two nodes i and j.
   * \param[in] config - Definition of the particular problem.
   * \return A lightweight const-view (read-only) of the residual/flux and Jacobians.
   */
  CNumerics::ResidualType<> ComputeResidual(const CConfig* config) final {
    AD::StartPreacc();
    AD::SetPreaccIn(Normal, nDim);
    AD::SetPreaccIn(ScalarVar_i, nVar);
    AD::SetPreaccIn(ScalarVar_j, nVar);
    if (dynamic_grid) {
      AD::SetPreaccIn(GridVel_i, nDim);
      AD::SetPreaccIn(GridVel_j, nDim);
    }

    ExtraADPreaccIn();

    Density_i = V_i[nDim + 2];
    Density_j = V_j[nDim + 2];

    su2double q_ij = 0.0;
    if (dynamic_grid) {
      for (unsigned short iDim = 0; iDim < nDim; iDim++) {
        su2double Velocity_i = V_i[iDim + 1] - GridVel_i[iDim];
        su2double Velocity_j = V_j[iDim + 1] - GridVel_j[iDim];
        q_ij += 0.5 * (Velocity_i + Velocity_j) * Normal[iDim];
      }
    } else {
      for (unsigned short iDim = 0; iDim < nDim; iDim++) {
        q_ij += 0.5 * (V_i[iDim + 1] + V_j[iDim + 1]) * Normal[iDim];
      }
    }

    a0 = 0.5 * (q_ij + fabs(q_ij));
    a1 = 0.5 * (q_ij - fabs(q_ij));

    FinishResidualCalc(config);

    AD::SetPreaccOut(Flux, nVar);
    AD::EndPreacc();

    return ResidualType<>(Flux, Jacobian_i, Jacobian_j);
  }
};
