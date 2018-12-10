/** @file gsVisitorNonLinElasticityMixedTH.h

    @brief Taylor-Hood element visitor for a 2-field mixed method for 
	nonlinear (near) incompressible linear elasticity for 2D plain strain 
	and 3D continua.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): O. Weeger
*/

#pragma once

#include <gsElasticity/gsVisitorLinearElasticityMixedTH.h>

namespace gismo
{


template <class T>
class gsVisitorNonLinElasticityMixedTH : public gsVisitorLinearElasticityMixedTH<T>
{
public:

	typedef gsVisitorLinearElasticityMixedTH<T> Base;

    /// Constructor
    gsVisitorNonLinElasticityMixedTH(T lambda, T mu, T rho, 
		                             const gsFunction<T> & body_force, 
									 const gsGeometry<T> & deformation,
									 const gsGeometry<T> & pressure,
									 T tfac = 1.0) : 
    Base(lambda, mu, rho, body_force, tfac),
	m_deformation(getEvaluator(NEED_JACOBIAN, deformation)),
	m_pressure(getEvaluator(NEED_NORMAL, pressure))
    { 
		m_dim = body_force.targetDim();

		defDer_k.resize(m_dim,m_dim);
		defDer_kv.resize(m_dim*m_dim);

		displGrad.resize(m_dim,m_dim);
		defGrad.resize(m_dim,m_dim);
		defGrad_inv.resize(m_dim,m_dim);

		gradU.resize(m_dim,m_dim);
		gradV.resize(m_dim,m_dim);
		defGrad_inv_gradU.resize(m_dim,m_dim);
		defGrad_inv_gradV.resize(m_dim,m_dim);
		
		locResMat.resize(m_dim,m_dim);
		locResVec.resize(m_dim);
	}

    /// Sets the gsGeometryEvaluator \em m_deformation using \em deformed and \em m_pressure using \em pressure
    void setDeformed(const gsGeometry<T> & deformation,
		             const gsGeometry<T> & pressure)
    {
        m_deformation = memory::make_unique(getEvaluator(NEED_JACOBIAN, deformation)); // (NEED_MEASURE | NEED_JACOBIAN | NEED_GRAD_TRANSFORM));
		m_pressure = memory::make_unique(getEvaluator(NEED_VALUE, pressure));
    }

    /// Evaluate on element
    inline void evaluate(gsBasisRefs<T> const   & basisRefs,
                         gsGeometryEvaluator<T> & geoEval,
                         gsMatrix<T> const      & quNodes)
    {
        Base::evaluate(basisRefs,geoEval,quNodes);

        // Evaluate deformation and pressure fields
        m_deformation->evaluateAt(quNodes);
		m_pressure->evaluateAt(quNodes);
    }

	/// Assemble
    inline void assemble(gsDomainIterator<T>    & element, 
                         gsGeometryEvaluator<T> & geoEval,
                         gsVector<T> const      & quWeights)
    {
        gsMatrix<T> & bVals  = basisData[0];
        gsMatrix<T> & bGrads = basisData[1];

		//const typename gsMatrix<T>::Block bVals_p  = basisVals_p.topRows(numActive_p);

        for (index_t k = 0; k < quWeights.rows(); ++k) // loop over quadrature nodes
        {           
            
			// Multiply weight by the geometry measure
            weight = quWeights[k] * geoEval.measure(k);

			// compute physical gradients at k as a Dim x NumActive matrix
            geoEval.transformGradients(k, bGrads, physGrad);

			//geoEval.transformGradients(k, 
			//gsAsConstMatrix<T,3,3> geoJac(geoEval->jacobian(k).data(),3,3 );
			//gsAsConstMatrix<T,3,3> defDer(m_deformation->jacobian(k).data(),3,3 );
			defDer_k = m_deformation->jacobian(k).transpose();
			//defDer_kv = defDer_k.asVector();
			//geoEval.transformGradients(k, defDer_kv, displGrad);

			// Evaluate pressure
			prex_k = m_mu * (m_pressure->value(k))(0,0);
			muprex = m_mu - prex_k;

			// Displacement gradient, H = du = (dx/dxi)'^-1 * (du/dxi)'
			displGrad = geoEval.jacobian(k).transpose().inverse() * defDer_k;

			// Deformation gradient, F = I + H
			defGrad = displGrad.transpose();
			for (size_t di = 0; di < m_dim; di++)
				defGrad(di,di) += 1.;

			// Determinant of deformation gradient, J = det(F)
			detF = defGrad.determinant();
            logdetF = weight * m_mu * math::log(detF);

			// Inverse of Fi = F^-1
			defGrad_inv = defGrad.inverse();

			// Local internal force vector contribution, mu*(F-Fi') + p*Fi' 
			locResMat = (weight*m_mu) * (defGrad - defGrad_inv.transpose()) + (weight*prex_k) * defGrad_inv.transpose();

			// 1st basis function (U/i)
			for (index_t i = 0; i < numActive; i++)
			{
				locResVec = locResMat * physGrad.col(i);

				// Spatial dimensions of 1st basis function
				for (size_t di = 0; di < m_dim; di++)
				{				
					// Write to Rhs
					localRhs_u(di*numActive+i) -= locResVec(di);
					
					// Write gradient as matrix
					gradU.setZero();
					gradU.row(di) = physGrad.col(i);

					defGrad_inv_gradU = defGrad_inv * gradU;				// Fi*dU
					defGrad_inv_gradU_trace = defGrad_inv_gradU.trace();	// tr(Fi*dU) = Fi':dU
						
					// 2nd basis function (V/j)
					//for (index_t j = 0; j < numActive; j++)
					// Exploit symmetry of C
					for (index_t j = i; j < numActive; j++)
					{						
						// Spatial dimensions of 2nd basis function
						for (size_t dj = 0; dj < m_dim; dj++)
						{				
							// Write gradient as matrix
							gradV.setZero();
							gradV.row(dj) = physGrad.col(j);

							defGrad_inv_gradV = defGrad_inv * gradV;		// Fi*dV

							// Local tangent stiffnees matrix contribution
							locKtgVal = m_mu * ( gradU.transpose()*gradV ).trace()
							          + muprex * ( defGrad_inv_gradU * defGrad_inv_gradV ).trace();
							          //+ prex_k * defGrad_inv_gradU_trace * defGrad_inv_gradV.trace();

							// Write to Mat
							localMatK(di*numActive+i, dj*numActive+j) += weight * locKtgVal;
						}
					}

					// 2nd basis function for pressure (Q/j)
					for (index_t j = 0; j < numActive_p; j++)
					{
						localMatB(j, di*numActive+i) += weight * m_mu * defGrad_inv_gradU_trace * basisVals_p(j,k);
					}
				}				
			}

			if (m_lambda < std::numeric_limits<T>::infinity())
				nearmup = m_mu*m_mu/m_lambda * weight;

			for (index_t i = 0; i < numActive_p; i++)
			{
				localRhs_p(i) -= logdetF * basisVals_p(i,k);
				
				// near incompressible
				if (m_lambda < std::numeric_limits<T>::infinity())
				{
					localRhs_p(i) += nearmup/m_mu * prex_k * basisVals_p(i,k);
					
					//for (index_t j = 0; j < numActive_p; j++)
					// Exploit symmetry of C
					for (index_t j = i; j < numActive_p; j++)
					{
						localMatC(i, j) -= nearmup * basisVals_p(i,k) * basisVals_p(j,k);
					}
				}
			}
			
			// Local rhs vector contribution
            for (size_t j = 0; j < m_dim; ++j)
                localRhs_u.middleRows(j*numActive,numActive).noalias() += 
                    weight * m_rho * forceVals(j,k) * m_tfac * bVals.col(k) ;
        }
        //gsDebug<< "local Mat: \n"<< localMat << "\n";
    }


	// We need to over-write localToGlobal from linear visitor, 
	//   because RHS should not be modified anymore for fixed DoFs!
	inline void localToGlobal(const gsStdVectorRef<gsDofMapper> & mappers,
                              const gsMatrix<T>     & eliminatedDofs,
                              const int patchIndex,
                              gsSparseMatrix<T>     & sysMatrix,
                              gsMatrix<T>           & rhsMatrix )
    {
       	
		// Local DoFs to global DoFs
		std::vector< gsMatrix<unsigned> > ci_actives(m_dim+1,actives);

        for (size_t ci = 0; ci != m_dim; ++ci)
			mappers[ci].localToGlobal(actives, patchIndex, ci_actives[ci]);
		
		mappers[m_dim].localToGlobal(actives_p, patchIndex, ci_actives[m_dim]);

        for (size_t ci = 0; ci!= m_dim; ++ci)
		{          
			for (index_t ai=0; ai < numActive; ++ai)
            {
                const index_t gi = ci * numActive +  ai; // row index
                const index_t ii = ci_actives[ci](ai);

                if ( mappers[ci].is_free_index(ii) )
                {
                    rhsMatrix.row(ii) += localRhs_u.row(gi);
                    
					// Exploit symmetry of A
                    for (index_t aj=ai; aj < numActive; ++aj)
					{
                        for (size_t cj = 0; cj!= m_dim; ++cj)
                        {
                            const index_t gj = cj * numActive +  aj; // column index
                            const index_t jj = ci_actives[cj](aj);
                            
                            if ( mappers[cj].is_free_index(jj) )
                            {
                                sysMatrix.coeffRef(ii, jj) += localMatK(gi, gj);
								if (aj > ai)
									sysMatrix.coeffRef(jj, ii) += localMatK(gi, gj);
							}                            
                        }
					}

					// matrix B and B'
					size_t cj = m_dim;
                    for (index_t aj=0; aj < numActive_p; ++aj)
                    {
                        const index_t gj = aj;					// column index
                        const index_t jj = ci_actives[cj](aj);
                            
                        if ( mappers[cj].is_free_index(jj) )
                        {
                            sysMatrix.coeffRef(ii, jj) += localMatB(gj, gi);
							sysMatrix.coeffRef(jj, ii) += localMatB(gj, gi);
                        }
                    }
					
                }
				
            }

		}

		// matrix C
		size_t ci = m_dim;
		for (index_t ai=0; ai < numActive_p; ++ai)
        {
            const index_t gi = ai;					// row index
            const index_t ii = ci_actives[ci](ai);

			if ( mappers[ci].is_free_index(ii) )
            {
                rhsMatrix.row(ii) += localRhs_p.row(gi);

				size_t cj = m_dim;
                //for (index_t aj=0; aj < numActive_p; ++aj)
				// Exploit symmetry of C
				for (index_t aj=ai; aj < numActive_p; ++aj)
                {
                    const index_t gj = aj;					// column index
                    const index_t jj = ci_actives[cj](aj);
                            
                    if ( mappers[cj].is_free_index(jj) )
                    {
                        sysMatrix.coeffRef(ii, jj) += localMatC(gi, gj);
						if (aj > ai)
							sysMatrix.coeffRef(jj, ii) += localMatC(gi, gj);
                    }                    
                }
			}
		}

    }
    

    // see http://eigen.tuxfamily.org/dox-devel/group__TopicStructHavingEigenMembers.html
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

protected:

	/// Contains the geometry evaluations for the deformed configuration
	typename gsGeometryEvaluator<T>::uPtr m_deformation;
	typename gsGeometryEvaluator<T>::uPtr m_pressure;
	
	// Kinematics
	T weight;
	gsMatrix<T> defDer_k;
	gsVector<T> defDer_kv;

	T prex_k;

	gsMatrix<T> displGrad;
	gsMatrix<T> defGrad;
	gsMatrix<T> defGrad_inv;
	T detF, logdetF, muprex;

	gsMatrix<T> gradU;
	gsMatrix<T> gradV;
	gsMatrix<T> defGrad_inv_gradU;
	gsMatrix<T> defGrad_inv_gradV;
	T defGrad_inv_gradU_trace;

	gsMatrix<T> locResMat;
	gsVector<T> locResVec;
	T locKtgVal;

	gsVector<T> locResVec_p;

protected:

    // Basis values
    using Base::basisData;
    using Base::actives;
	using Base::physGrad;	
	using Base::physGrad_symm;	
	using Base::numActive;

	// Pressure basis values
    using Base::basisVals_p;
    using Base::actives_p;
	using Base::numActive_p;

	//using Base::normal;
	using Base::m_C;

protected:

	// Dimension
	using Base::m_dim;
	using Base::m_dimStrain;

    // Lambda, mu, rho
    using Base::m_lambda;
	using Base::m_mu;
	using Base::m_rho;

protected:

    // Body forces
    using Base::m_bodyForce_ptr;
	using Base::m_tfac;

    // Local values of the surface forces
    using Base::forceVals;

	using Base::nearmup;
    
protected:
    // Local matrices
    using Base::localMatK;
	using Base::localMatB;
	using Base::localMatC;
	using Base::localRhs_u;
	using Base::localRhs_p;

};


} // namespace gismo
