/** @file gsElasticityMixedTHNewton.h

    @brief Performs Newton iterations to solve a nonlinear equation system.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): A. Goyal, A. Mantzaflaris, O. Weeger
*/


#pragma once

#include <cmath>


namespace gismo
{

/** 
    @brief Performs Newton iterations to solve a nonlinear equation system.
    
    \tparam T coefficient type
    
    \ingroup ThinShell
*/
template <class T> 
class gsElasticityMixedTHNewton
{
public:

    /// Constructor giving access to the gsElasticityAssembler object to create a linear system per iteration
    gsElasticityMixedTHNewton(  gsElasticityMixedTHAssembler<T> & assembler, 
                                const gsMultiPatch<T> & initialSolution, 
                                const gsMultiPatch<T> & initialPressure)
    : m_assembler(assembler),
      m_curSolution(initialSolution),
	  m_curPressure(initialPressure),
      m_numIterations(0),
      m_maxIterations(100),
      m_tolerance(1e-12),
      m_converged(false)
    { 

    }


public:

    /// Applies Newton method and Performs Newton iterations until convergence or maximum iterations.
    void solve();

    /// Solves linear system obtained using linear elasticity in first step and computes residual
    void firstIteration();

    /// Solves linear system in each iteration based on last solution and computes residual
    void nextIteration();

public:

    /// Returns the latest configuration
    const gsMultiPatch<T> & solution(index_t i) const 
	{
		if (i == 0)
			return m_curSolution;
		else
			return m_curPressure;
	}

    /// Tells if the Newton method converged
    bool converged() const {return m_converged;}

    /// Returns the number of Newton iterations performed
    index_t numIterations() const { return m_numIterations;}

    /// Returns the tolerance value used
    T tolerance() const {return m_tolerance;}

    /// Returns the error after solving the nonlinear system
    T residue()   const {return m_residue;}

    /// Set the maximum number of Newton iterations allowed
    void setMaxIterations(index_t nIter) {m_maxIterations = nIter;}

    /// Set the tolerance for convergence
    void setTolerance(T tol) {m_tolerance = tol;}

protected:

    /// gsElasticityAssembler object to generate the linear system for each iteration when solving shells
    gsElasticityMixedTHAssembler<T> & m_assembler;

    /// The latest/current solution
    gsMultiPatch<T>     m_curSolution;
	gsMultiPatch<T>     m_curPressure;

    /// Solution of the linear system in each iteration
    gsMatrix<T>         m_updateVector;

    /// Linear solver employed
    Eigen::SparseLU<gsSparseMatrix<>, Eigen::COLAMDOrdering<index_t> >  m_solver;
    //Eigen::BiCGSTAB< gsSparseMatrix<>, Eigen::DiagonalPreconditioner<real_t> > solver;
    //Eigen::SparseQR<gsSparseMatrix<>, Eigen::COLAMDOrdering<index_t> >  solver;

protected:

    /// Number of Newton iterations performed
    index_t m_numIterations;

    /// Maximum number of Newton iterations allowed
    index_t m_maxIterations;

    /// Tolerance value to decide convergence
    T       m_tolerance;

protected:

    /// Convergence result
    bool m_converged;

    /// Final error
    T m_residue;
	T m_updnorm;

};


} // namespace gismo

//////////////////////////////////////////////////
//////////////////////////////////////////////////

namespace gismo
{


template <class T> 
void gsElasticityMixedTHNewton<T>::solve()
{
    firstIteration();

    const T initResidue = m_residue;
	const T initUpdate = m_updnorm;
    
    // ----- Iterations start -----
    for (m_numIterations = 1; m_numIterations < m_maxIterations; ++m_numIterations)
    {
        nextIteration();
        
        // termination criteria
        if ( std::abs(m_updnorm / initUpdate)  < m_tolerance || 
             std::abs(m_residue / initResidue) < m_tolerance )
        {
            m_converged = true;
            break;
        }
    }
}


template <class T> 
void gsElasticityMixedTHNewton<T>::firstIteration()
{
    // ----- First iteration -----
    m_converged = false;

    // Construct the linear system
    m_assembler.assemble();

    // Compute the newton update
    m_solver.compute( m_assembler.matrix() );
    m_updateVector = m_solver.solve( m_assembler.rhs() );

    // Update the deformed solution
    m_assembler.constructSolution(m_updateVector, m_curSolution, 0);
	m_assembler.constructSolution(m_updateVector, m_curPressure, 1);

    // Compute initial residue
    m_residue = m_assembler.rhs().norm();
	m_updnorm = m_updateVector.norm();

	gsDebug<<"Iteration: "<< 0
               <<", residue: "<< m_residue
               <<", update norm: "<< m_updnorm
               <<"\n";
}

template <class T> 
void gsElasticityMixedTHNewton<T>::nextIteration()
{
        // Construct linear system for next iteration
        m_assembler.assemble( m_curSolution, m_curPressure );
	
        // Compute the newton update
        //m_solver.compute( m_assembler.matrix() );
		m_solver.factorize( m_assembler.matrix() );
        m_updateVector = m_solver.solve( m_assembler.rhs() );
        
        // Update the deformed solution
        m_assembler.updateSolution(m_updateVector, m_curSolution, m_curPressure);
        
        // Compute residue
        m_residue = m_assembler.rhs().norm();
        m_updnorm = m_updateVector.norm();

        gsDebug<<"Iteration: "<< m_numIterations
               <<", residue: "<< m_residue
               <<", update norm: "<< m_updnorm
               <<"\n";
}



} // namespace gismo

