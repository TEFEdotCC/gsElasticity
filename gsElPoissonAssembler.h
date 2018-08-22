/** @file gsElPoissonAssembler.h

   @brief Provides assembler for the Poisson equation with correctly working penalization,
   as well as some extra functionality used in Thermo-Elasticity simulations

   This file is part of the G+Smo library.

   This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/.

   Author(s): A. Shamanskiy
*/

#pragma once

#include <gsAssembler/gsPoissonAssembler.h>
#include <fstream>



namespace gismo
{

/** @brief
   Thermo-elasticity module utilizes the Poisson Assembler to solve
   the heat equation. Since the incoming heat data from Navier-Stokes solver
   are to be set repeatedly, the penalization Dirichlet strategy was chosen.
   However, the standard Poisson solver has a minor bug in the penalizeDirichletDofs()
   method. This version corrects the bug and add extra possibilities of setting Dirichlet
   DOFs coming from the outside.

*/
template <class T>
class gsElPoissonAssembler : public gsPoissonAssembler<T>
{
public:

    gsElPoissonAssembler(const gsMultiPatch<T> & patches,
                         const gsMultiBasis<T> & bases,
                         const gsBoundaryConditions<T> & bcInfo,
                         const gsFunction<T> & force,
                         T conductivity = 1.,
                         dirichlet::strategy dirStrategy = dirichlet::elimination,
                         iFace::strategy intStrategy = iFace::glue);

   void assemble();

   void penalizeDirichletDofs();

   /// Add already computed Dirichlet boundary data to the specified sides.
   /// Orientation check only for 2D.
   void addDirichletData(const gsMultiPatch<> & sourceGeometry,
                         const gsMultiPatch<> & sourceSolution,
                         int sourcePatch, const boxSide & sourceSide,
                         int targetPatch, const boxSide & targetSide);

   void addDirichletData(const gsField<> & sourceField,
                         int sourcePatch, const boxSide & sourceSide,
                         int targetPatch, const boxSide & targetSide);

   /// Add already computed neumann boundary data to the rhs vector.
   /// Resulting rhs is saved in rhsExtra member and can be accessed
   /// or cleared by the corresponding class methods.
   /// Orientation check for 2D only.

   void addNeummannData(const gsMultiPatch<> & sourceGeometry,
                        const gsMultiPatch<> & sourceSolution,
                        int sourcePatch, const boxSide & sourceSide,
                        int targetPatch, const boxSide & targetSide);

   void addNeummannData(const gsField<> & sourceField,
                        int sourcePatch, const boxSide & sourceSide,
                        int targetPatch, const boxSide & targetSide);

   void setUnitingConstraint(const boxSide & side, bool verbosity = false);

   void extractSolutionVector(const gsMultiPatch<> & solution, gsVector<> & vector);

protected:

   void setDirichletDoFs(const gsMatrix<> & ddofs, int targetPatch, const boxSide & targetSide);

   /// Check if two boundaries have at least the same starting and ending points
   /// Returns 1 if they match, return -1 if they match,
   /// but parameterization directions are opposite,
   /// return 0 if ends don't match
   int checkMatchingBoundaries(const gsGeometry<> & sourceBoundary,
                               const gsGeometry<> & targetBoundary);

protected:

   // Members from gsAssembler
   using gsPoissonAssembler<T>::m_pde_ptr;
   using gsPoissonAssembler<T>::m_bases;
   using gsPoissonAssembler<T>::m_ddof;
   using gsPoissonAssembler<T>::m_options;
   using gsPoissonAssembler<T>::m_system;

   const T PP = 1e9; // magic number
};







} // namespace gismo


#ifndef GISMO_BUILD_LIB
#include GISMO_HPP_HEADER(gsPoissonAssembler.hpp)
#endif
