// Copyright (c) 2013-2014 Anton Kozhevnikov, Thomas Schulthess
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that 
// the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the 
//    following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
//    and the following disclaimer in the documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED 
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/** \file matching_coefficients.h
 *   
 *  \brief Contains definition and partial implementation of sirius::Matching_coefficients class.
 */

#ifndef __MATCHING_COEFFICIENTS_H__
#define __MATCHING_COEFFICIENTS_H__

#include "global.h"

namespace sirius {

/** The following matching conditions must be fulfilled:
 *  \f[
 *      \frac{\partial^j}{\partial r^j} \sum_{L \nu} A_{L \nu}^{\bf k}({\bf G})u_{\ell \nu}(r) 
 *       Y_{L}(\hat {\bf r}) \bigg|_{R^{MT}}  = \frac{\partial^j}{\partial r^j} \frac{4 \pi}{\sqrt \Omega} 
 *       e^{i{\bf (G+k)\tau}} \sum_{L}i^{\ell} j_{\ell}(|{\bf G+k}|r) Y_{L}^{*}(\widehat {\bf G+k}) Y_{L}(\hat {\bf r}) \bigg|_{R^{MT}} 
 *  \f]
 *  where \f$ L = \{ \ell, m \} \f$. Dropping sum over L we arrive to the following system of linear equations:
 *  \f[
 *      \sum_{\nu} \frac{\partial^j u_{\ell \nu}(r)}{\partial r^j} \bigg|_{R^{MT}} A_{L \nu}^{\bf k}({\bf G}) =
 *      \frac{4 \pi}{\sqrt \Omega} e^{i{\bf (G+k)\tau}} i^{\ell} \frac{\partial^j j_{\ell}(|{\bf G+k}|r)}{\partial r^j}
 *      \bigg|_{R^{MT}} Y_{L}^{*}(\widehat {\bf G+k}) 
 *  \f]
 *  The matching coefficients are then equal to:
 *  \f[
 *      A_{L \nu}^{\bf k}({\bf G}) = \sum_{j} \bigg[ \frac{\partial^j u_{\ell \nu}(r)}{\partial r^j} \bigg|_{R^{MT}} \bigg]_{\nu j}^{-1}
 *      \frac{\partial^j j_{\ell}(|{\bf G+k}|r)}{\partial r^j} \bigg|_{R^{MT}} \frac{4 \pi}{\sqrt \Omega} i^{\ell} 
 *      e^{i{\bf (G+k)\tau}} Y_{L}^{*}(\widehat {\bf G+k})  
 *  \f]
 */
class Matching_coefficients
{
    private:

        Global& parameters_;

        int num_gkvec_;

        mdarray<double_complex, 2> gkvec_ylm_;

        std::vector<double> gkvec_len_;
        
        /// Phase factors \f$ e^{i \bf{(G+k)\tau_{\alpha}}} \f$.
        mdarray<double_complex, 2> gkvec_phase_factors_;

        /// Precomputed values for the linear equations for matching coefficients.
        mdarray<double_complex, 4> alm_b_;
        
        /// Generate matching coefficients for a specific \f$ \ell \f$ and order. 
        /** \param [in] ngk Number of G+k vectors.
         *  \param [in] offset Offset in the G+k array.
         *  \param [in] ia Index of atom.
         *  \param [in] iat Index of atom type.
         *  \param [in] l Orbital quantum nuber.
         *  \param [in] lm Composite l,m index.
         *  \param [in] nu Order of radial function \f$ u_{\ell \nu}(r) \f$ for which coefficients are generated.
         *  \param [inout] A Matrix of radial derivatives.
         *  \param [out] alm Pointer to alm coefficients.
         */
        template <int N>
        inline void generate(int ngk,
                             int offset,
                             int ia, 
                             int iat, 
                             int l, 
                             int lm, 
                             int nu, 
                             matrix3d<double>& A, 
                             double_complex* alm)
        {
            /* invert matrix of radial derivatives */
            switch (N)
            {
                case 1:
                {
                    if (debug_level >= 1 && fabs(A(0, 0)) < 1.0 / sqrt(parameters_.unit_cell()->omega()))
                    {   
                        std::stringstream s;
                        s << "Ill defined plane wave matching problem for atom " << ia << ", l = " << l << std::endl
                            << "  radial function value at the MT boundary : " << A(0, 0); 

                        warning_local(__FILE__, __LINE__, s);
                    }
                                    
                    A(0, 0) = 1.0 / A(0, 0);
                    break;
                }
                case 2:
                {
                    double det = A(0, 0) * A(1, 1) - A(0, 1) * A(1, 0);

                    if (debug_level >= 1 && fabs(det) < 1.0 / sqrt(parameters_.unit_cell()->omega()))
                    {   
                        std::stringstream s;
                        s << "Ill defined plane wave matching problem for atom " << ia << ", l = " << l << std::endl
                            << "  radial function value at the MT boundary : " << A(0 ,0); 

                        warning_local(__FILE__, __LINE__, s);
                    }

                    std::swap(A(0, 0), A(1, 1));
                    A(0, 0) /= det;
                    A(1, 1) /= det;
                    A(0, 1) = -A(0, 1) / det;
                    A(1, 0) = -A(1, 0) / det;
                    break;
                }
                case 3:
                {
                    A = inverse(A);
                    break;
                }
            }
            
            double_complex zt;

            for (int igk = 0; igk < ngk; igk++)
            {
                switch (N)
                {
                    case 1:
                    {
                        zt = alm_b_(0, offset + igk, l, iat) * A(0, 0);
                        break;
                    }
                    case 2:
                    {
                        zt = alm_b_(0, offset + igk, l, iat) * A(nu, 0) + 
                             alm_b_(1, offset + igk, l, iat) * A(nu, 1);
                        break;
                    }
                    case 3:
                    {
                        zt = alm_b_(0, offset + igk, l, iat) * A(nu, 0) + 
                             alm_b_(1, offset + igk, l, iat) * A(nu, 1) + 
                             alm_b_(2, offset + igk, l, iat) * A(nu, 2);
                        break;
                    }
                }
                alm[igk] = gkvec_phase_factors_(offset + igk, ia) * conj(gkvec_ylm_(offset + igk, lm)) * zt;
            }
        }

    public:

        Matching_coefficients(Global& parameters__, 
                              int num_gkvec__, 
                              std::vector<gklo_basis_descriptor>& gklo_basis_descriptors)
            : parameters_(parameters__),
              num_gkvec_(num_gkvec__)
        {
            int lmax = parameters_.lmax_apw();
            int lmmax = Utils::lmmax(lmax);

            gkvec_phase_factors_.set_dimensions(num_gkvec_, parameters_.unit_cell()->num_atoms());
            gkvec_phase_factors_.allocate();

            gkvec_ylm_.set_dimensions(num_gkvec_, lmmax);
            gkvec_ylm_.allocate();
            
            gkvec_len_.resize(num_gkvec_);

            /* get length and Ylm harmonics of G+k vectors */
            #pragma omp parallel
            {
                std::vector<double_complex> ylm(lmmax);

                #pragma omp for
                for (int igk = 0; igk < num_gkvec_; igk++)
                {
                    /* get r, theta, phi */
                    auto vs = SHT::spherical_coordinates(gklo_basis_descriptors[igk].gkvec_cart);

                    /* get spherical harmonics */
                    SHT::spherical_harmonics(lmax, vs[1], vs[2], &ylm[0]);
                    gkvec_len_[igk] = vs[0];

                    for (int lm = 0; lm < lmmax; lm++) gkvec_ylm_(igk, lm) = ylm[lm];

                    /* get phase factors of G+k vectors */
                    for (int ia = 0; ia < parameters_.unit_cell()->num_atoms(); ia++)
                    {
                        double phase = twopi * Utils::scalar_product(gklo_basis_descriptors[igk].gkvec, 
                                                                     parameters_.unit_cell()->atom(ia)->position());

                        gkvec_phase_factors_(igk, ia) = exp(complex_i * phase);
                    }
                }
            }
            
            alm_b_.set_dimensions(3, num_gkvec_, lmax + 1, parameters_.unit_cell()->num_atom_types());
            alm_b_.allocate();
            alm_b_.zero();
            
            /* value and first two derivatives of spherical Bessel functions */
            mdarray<double, 2> sbessel_mt(lmax + 2, 3);

            for (int igk = 0; igk < num_gkvec_; igk++)
            {
                for (int iat = 0; iat < parameters_.unit_cell()->num_atom_types(); iat++)
                {
                    double R = parameters_.unit_cell()->atom_type(iat)->mt_radius();

                    double RGk = R * gkvec_len_[igk];

                    /* compute values and first and second derivatives of the spherical Bessel functions at the MT boundary */
                    gsl_sf_bessel_jl_array(lmax + 1, RGk, &sbessel_mt(0, 0));
                    
                    /* Bessel function derivative: f_{{n}}^{{\prime}}(z)=-f_{{n+1}}(z)+(n/z)f_{{n}}(z)
                     *
                     * In[]:= FullSimplify[D[SphericalBesselJ[n,a*x],{x,1}]]
                     * Out[]= (n SphericalBesselJ[n,a x])/x-a SphericalBesselJ[1+n,a x]
                     *
                     * In[]:= FullSimplify[D[SphericalBesselJ[n,a*x],{x,2}]]
                     * Out[]= (((-1+n) n-a^2 x^2) SphericalBesselJ[n,a x]+2 a x SphericalBesselJ[1+n,a x])/x^2
                     */
                    for (int l = 0; l <= lmax; l++)
                    {
                        sbessel_mt(l, 1) = -sbessel_mt(l + 1, 0) * gkvec_len_[igk] + (l / R) * sbessel_mt(l, 0);
                        sbessel_mt(l, 2) = 2 * gkvec_len_[igk] * sbessel_mt(l + 1, 0) / R + 
                                           ((l - 1) * l - pow(RGk, 2)) * sbessel_mt(l, 0) / pow(R, 2);
                    }
                    
                    for (int l = 0; l <= lmax; l++)
                    {
                        double_complex z = std::pow(complex_i, l);
                        double f = fourpi / sqrt(parameters_.unit_cell()->omega());
                        alm_b_(0, igk, l, iat) = z * f * sbessel_mt(l, 0); 
                        alm_b_(1, igk, l, iat) = z * f * sbessel_mt(l, 1);
                        alm_b_(2, igk, l, iat) = z * f * sbessel_mt(l, 2);
                    }
                }
            }
        }

        /// Generate plane-wave matching coefficents for the radial solutions of a given atom.
        /** \param [in] ia Index of atom.
         *  \param [out] alm Array of matching coefficients with dimension indices \f$ ({\bf G+k}, \xi) \f$.
         */
        void generate(int ia, mdarray<double_complex, 2>& alm)
        {
            Timer t("sirius::Matching_coefficients::generate:atom");

            Atom* atom = parameters_.unit_cell()->atom(ia);
            Atom_type* type = atom->type();

            assert(type->max_aw_order() <= 3);

            int iat = type->id();
                
            #pragma omp parallel
            {
                matrix3d<double> A;

                #pragma omp for
                for (int xi = 0; xi < type->mt_aw_basis_size(); xi++)
                {
                    int l = type->indexb(xi).l;
                    int lm = type->indexb(xi).lm;
                    int nu = type->indexb(xi).order; 

                    /* order of augmentation for a given orbital quantum number */
                    int num_aw = (int)type->aw_descriptor(l).size();
                    
                    /* create matrix of radial derivatives */
                    for (int order = 0; order < num_aw; order++)
                    {
                        for (int dm = 0; dm < num_aw; dm++) A(dm, order) = atom->symmetry_class()->aw_surface_dm(l, order, dm);
                    }

                    switch (num_aw)
                    {
                        /* APW */
                        case 1:
                        {
                            generate<1>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm(0, xi));
                            break;
                        }
                        /* LAPW */
                        case 2:
                        {
                            generate<2>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm(0, xi));
                            break;
                        }
                        /* Super LAPW */
                        case 3:
                        {
                            generate<3>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm(0, xi));
                            break;
                        }
                        default:
                        {
                            error_local(__FILE__, __LINE__, "wrong order of augmented wave");
                        }
                    }
                }
            }
        }

        /// Generate plane-wave matching coefficents for the radial solutions of all atoms. 
        /** Normal layout of matching coefficients: G+k vectors are along rows, AW basis functions are 
         *  along columns. This layout is used to generate first-variational states. 
         *  Transposed layout: AW basis functions are along rows, G+k vectors are along columns. This layout
         *  is used to setup Hamiltonian and overlap matrices.
         */
        template <bool normal_layout>
        void generate(dmatrix<double_complex>& alm)
        {
            Timer t("sirius::Matching_coefficients::generate:panel");

            int num_mt_aw_loc = normal_layout ? alm.num_cols_local() : alm.num_rows_local();
            int num_gkvec_loc = normal_layout ? alm.num_rows_local() : alm.num_cols_local();

            assert(num_gkvec_loc == num_gkvec_);

            #pragma omp parallel
            {
                matrix3d<double> A;
                std::vector<double_complex> alm_tmp;
                if (!normal_layout) alm_tmp.resize(num_gkvec_loc);

                #pragma omp for
                for (int i = 0; i < num_mt_aw_loc; i++)
                {
                    /* global composite index of atom and xi */
                    int j = normal_layout ? alm.icol(i) : alm.irow(i);
                    int ia = parameters_.unit_cell()->mt_aw_basis_descriptor(j).ia;
                    int xi = parameters_.unit_cell()->mt_aw_basis_descriptor(j).xi;
                    Atom* atom = parameters_.unit_cell()->atom(ia);
                    Atom_type* type = atom->type();
                    int iat = type->id();
                    int l = type->indexb(xi).l;
                    int lm = type->indexb(xi).lm;
                    int nu = type->indexb(xi).order; 

                    int num_aw = (int)type->aw_descriptor(l).size();

                    for (int order = 0; order < num_aw; order++)
                    {
                        for (int dm = 0; dm < num_aw; dm++) A(dm, order) = atom->symmetry_class()->aw_surface_dm(l, order, dm);
                    }
                    
                    switch (num_aw)
                    {
                        case 1:
                        {
                            if (normal_layout)
                            {
                                generate<1>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm(0, i));
                            }
                            else
                            {
                                generate<1>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm_tmp[0]);
                            }
                            break;
                        }
                        case 2:
                        {
                            if (normal_layout)
                            {
                                generate<2>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm(0, i));
                            }
                            else
                            {
                                generate<2>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm_tmp[0]);
                            }
                            break;
                        }
                        case 3:
                        {
                            if (normal_layout)
                            {
                                generate<3>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm(0, i));
                            }
                            else
                            {
                                generate<3>(num_gkvec_, 0, ia, iat, l, lm, nu, A, &alm_tmp[0]);
                            }
                            break;
                        }
                        default:
                        {
                            error_local(__FILE__, __LINE__, "wrong order of augmented wave");
                        }
                    }
                    if (!normal_layout)
                    {
                        for (int igk = 0; igk < num_gkvec_loc; igk++) alm(i, igk) = alm_tmp[igk];
                    }
                }
            }
        }

        void generate(splindex<block>& sspl_gkvec_col, 
                      mdarray<double_complex, 2>& alm_v)
        {
            Timer t("sirius::Matching_coefficients::generate:slice");

            int naw = parameters_.unit_cell()->mt_aw_basis_size();

            #pragma omp parallel
            {
                matrix3d<double> A;
                std::vector<double_complex> alm_tmp(sspl_gkvec_col.local_size());

                #pragma omp for
                for (int j = 0; j < naw; j++)
                {
                    int ia = parameters_.unit_cell()->mt_aw_basis_descriptor(j).ia;
                    int xi = parameters_.unit_cell()->mt_aw_basis_descriptor(j).xi;
                    Atom* atom = parameters_.unit_cell()->atom(ia);
                    Atom_type* type = atom->type();
                    int iat = type->id();
                    int l = type->indexb(xi).l;
                    int lm = type->indexb(xi).lm;
                    int nu = type->indexb(xi).order; 

                    int num_aw = (int)type->aw_descriptor(l).size();

                    for (int order = 0; order < num_aw; order++)
                    {
                        for (int dm = 0; dm < num_aw; dm++) A(dm, order) = atom->symmetry_class()->aw_surface_dm(l, order, dm);
                    }
                    
                    switch (num_aw)
                    {
                        case 1:
                        {
                            generate<1>((int)sspl_gkvec_col.local_size(), (int)sspl_gkvec_col.global_offset(), ia, iat, l, lm, nu, A, &alm_tmp[0]);
                            break;
                        }
                        case 2:
                        {
                            generate<2>((int)sspl_gkvec_col.local_size(), (int)sspl_gkvec_col.global_offset(), ia, iat, l, lm, nu, A, &alm_tmp[0]);
                            break;
                        }
                        case 3:
                        {
                            generate<3>((int)sspl_gkvec_col.local_size(), (int)sspl_gkvec_col.global_offset(), ia, iat, l, lm, nu, A, &alm_tmp[0]);
                            break;
                        }
                        default:
                        {
                            error_local(__FILE__, __LINE__, "wrong order of augmented wave");
                        }
                    }
                    for (int i = 0; i < (int)sspl_gkvec_col.local_size(); i++) alm_v(j, i) = alm_tmp[i];
                }
            }
        }
};

}

#endif // __MATCHING_COEFFICIENTS_H__
