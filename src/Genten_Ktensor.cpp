//@HEADER
// ************************************************************************
//     Genten: Software for Generalized Tensor Decompositions
//     by Sandia National Laboratories
//
// Sandia National Laboratories is a multimission laboratory managed
// and operated by National Technology and Engineering Solutions of Sandia,
// LLC, a wholly owned subsidiary of Honeywell International, Inc., for the
// U.S. Department of Energy's National Nuclear Security Administration under
// contract DE-NA0003525.
//
// Copyright 2017 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// ************************************************************************
//@HEADER


#include <assert.h>
#include <algorithm>
#include <iostream>

#include "Genten_Ktensor.hpp"
#include "Genten_RandomMT.hpp"
#include "Genten_IndxArray.hpp"

#ifdef HAVE_CALIPER
#include <caliper/cali.h>
#endif

template <typename ExecSpace>
Genten::KtensorT<ExecSpace>::
KtensorT(ttb_indx nc, ttb_indx nd):
  lambda(nc), data(nd)
{
  setWeights(1.0);
}

template <typename ExecSpace>
Genten::KtensorT<ExecSpace>::
KtensorT(ttb_indx nc, ttb_indx nd, const Genten::IndxArrayT<ExecSpace> & sz):
  lambda(nc), data(nd,sz,nc)
{
  setWeights(1.0);
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
setWeightsRand() const
{
  lambda.rand();
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
setWeights(ttb_real val) const
{
  lambda = val;
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
setWeights(const Genten::ArrayT<ExecSpace> &  newWeights) const
{
  assert(newWeights.size() == lambda.size());
  deep_copy(lambda, newWeights);
  return;
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
setMatricesRand() const
{
  ttb_indx nd = data.size();
  for (ttb_indx n = 0; n < nd; n ++)
  {
    data[n].rand();
  }
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
setMatricesScatter(const bool bUseMatlabRNG,
                   const bool bUseParallelRNG,
                   Genten::RandomMT &   cRMT) const
{
  ttb_indx nd = data.size();
  for (ttb_indx n = 0; n < nd; n ++)
  {
    data[n].scatter (bUseMatlabRNG, bUseParallelRNG, cRMT);
  }
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
setRandomUniform (const bool bUseMatlabRNG,
                  Genten::RandomMT & cRMT) const
{
  // Set factor matrices to random values, then normalize each component
  // vector so that it sums to one.
  ttb_indx nComps = lambda.size();
  ttb_indx nd = data.size();
  ArrayT<ExecSpace>  cTotals(nComps);
  setWeights (1.0);
  for(ttb_indx  n = 0; n < nd; n++)
  {
    cTotals = 0.0;
    for (ttb_indx  c = 0; c < nComps; c++)
    {
      ttb_indx nRows = data[n].nRows();
      for (ttb_indx  i = 0; i < nRows; i++)
      {
        ttb_real  dNextRan;
        if (bUseMatlabRNG)
          dNextRan = cRMT.genMatlabMT();
        else
          dNextRan = cRMT.genrnd_double();

        data[n].entry(i,c) = dNextRan;
        cTotals[c] += dNextRan;
      }
    }
    data[n].colScale (cTotals, true);
    for (ttb_indx  c = 0; c < nComps; c++)
      weights(c) *= cTotals[c];
  }

  // Adjust weights by a random factor.
  // Random values for weights are generated after all factor elements
  // to match Matlab Genten function create_problem.
  for (ttb_indx  c = 0; c < nComps; c++)
  {
    ttb_real  dNextRan;
    if (bUseMatlabRNG)
      dNextRan = cRMT.genMatlabMT();
    else
      dNextRan = cRMT.genrnd_double();

    weights(c) *= dNextRan;
  }

  // Normalize the weights so they sum to one.
  ttb_real  dTotal = 0.0;
  for (ttb_indx  c = 0; c < nComps; c++)
    dTotal += weights(c);
  for (ttb_indx  c = 0; c < nComps; c++)
    weights(c) *= (1.0 / dTotal);

  return;
}


// Only called by Ben Allan's parallel test code.
#if !defined(_WIN32)
template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
scaleRandomElements(ttb_real fraction, ttb_real scale, bool columnwise) const
{
  for (ttb_indx i =0; i< data.size(); i++) {
    data[i].scaleRandomElements(fraction, scale, columnwise);
  }
}
#endif

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
setMatrices(ttb_real val) const
{
  data = val;
}

template <typename ExecSpace>
bool Genten::KtensorT<ExecSpace>::
isConsistent() const
{
  ttb_indx nc = lambda.size();
  for (ttb_indx n = 0; n < data.size(); n ++)
  {
    if (data[n].nCols() != nc)
    {
      return false;
    }
  }
  return true;
}

template <typename ExecSpace>
bool Genten::KtensorT<ExecSpace>::
isConsistent(const Genten::IndxArrayT<ExecSpace> & sz) const
{
  if (data.size() != sz.size())
  {
    return false;
  }

  ttb_indx nc = lambda.size();
  for (ttb_indx n = 0; n < data.size(); n ++)
  {
    if ((data[n].nCols() != nc) || (data[n].nRows() != sz[n]))
    {
      return false;
    }
  }
  return true;
}

template <typename ExecSpace>
bool Genten::KtensorT<ExecSpace>::
hasNonFinite(ttb_indx &bad) const
{
  bad = 0;
  if (lambda.hasNonFinite(bad)) {
    std::cout << "Genten::Ktensor::hasNonFinite lambda.data["<<bad<<"] nonfinite " << std::endl;
    return true;
  }
  for (ttb_indx i = 0; i < data.size(); i++) {
    if (data[i].hasNonFinite(bad)) {
      std::cout << "Genten::Ktensor::hasNonFinite data["<<i<<"] nonfinite element " << bad << std::endl;
      return true;
    }
  }

  return false;
}

template <typename ExecSpace>
bool Genten::KtensorT<ExecSpace>::
isNonnegative(bool bDisplayErrors) const
{
  for (ttb_indx  n = 0; n < ndims(); n++)
  {
    for (ttb_indx  i = 0; i < factors()[n].nRows(); i++)
    {
      for (ttb_indx  j = 0; j < ncomponents(); j++)
      {
        if (factors()[n].entry(i,j) < 0.0)
        {
          if (bDisplayErrors)
          {
            std::cout << "Ktensor::isNonnegative()"
                      << " - element (" << i << "," << j << ")"
                      << " of mode " << n << " is negative"
                      << std::endl;
          }
          return( false );
        }
      }
    }
  }
  for (ttb_indx  r = 0; r < ncomponents(); r++)
  {
    if (weights(r) < 0.0)
    {
      if (bDisplayErrors)
      {
        std::cout << "Ktensor::isNonnegative()"
                  << " - weight " << r << " is negative" << std::endl;
      }
      return( false );
    }
  }

  return( true );
}

template <typename ExecSpace>
bool Genten::KtensorT<ExecSpace>::
isEqual(const Genten::KtensorT<ExecSpace> & b, ttb_real tol) const
{
  // Check for equal sizes.
  if ((this->ndims() != b.ndims()) || (this->ncomponents() != b.ncomponents()))
  {
    return( false );
  }

  // Check for equal weights (within tolerance).
  if (this->weights().isEqual (b.weights(), tol) == false)
  {
    return( false );
  }

  // Check for equal factor matrices (within tolerance).
  for (ttb_indx  i = 0; i < ndims(); i++)
  {
    if (this->data[i].isEqual (b[i], tol) == false)
    {
      return( false );
    }
  }
  return( true );
}

template <typename ExecSpace>
ttb_real Genten::KtensorT<ExecSpace>::
entry(const Genten::IndxArrayT<ExecSpace> & subs) const
{
  ttb_indx nd = this->ndims();
  assert(subs.size() == nd);

  // This vector product is fundamental to many big computations; hence,
  // stride should be one.  Since FacMatrix stores by row, the factor vectors
  // are columns so that rowTimes() is across a row.

  // Copy lambda array to temp array.
  Genten::ArrayT<ExecSpace> x(lambda.size());
  deep_copy(x,lambda);

  // Compute a vector of elementwise products of corresponding rows
  // of factor matrices.
  for (ttb_indx i = 0; i < nd; i ++)
  {
    // Update temp array with elementwise product.
    // If a subscript is out of bounds, it will be caught by rowTimes().
    data[i].rowTimes(x, subs[i]);
  }

  // Return sum of elementwise products stored in temp array.
  return(x.sum());
}

template <typename ExecSpace>
ttb_real Genten::KtensorT<ExecSpace>::
entry(const Genten::IndxArrayT<ExecSpace> & subs,
      const Genten::ArrayT<ExecSpace> & altLambda) const
{
  ttb_indx nd = this->ndims();
  assert(subs.size() == nd);
  assert(altLambda.size() == lambda.size());

  // This vector product is fundamental to many big computations; hence,
  // stride across lambda should be one.  Since FacMatrix stores by row,
  // the factor vectors are columns so that rowTimes() is across a row.

  // Copy lambda array to temp array.
  Genten::ArrayT<ExecSpace> lambdaForEntry(lambda.size());
  for (ttb_indx  i = 0; i < lambdaForEntry.size(); i++)
    lambdaForEntry[i] = altLambda[i];

  // Compute a vector of elementwise products of corresponding rows
  // of factor matrices.
  for (ttb_indx i = 0; i < nd; i ++)
  {
    // Update temp array with elementwise product.
    // If a subscript is out of bounds, it will be caught by rowTimes().
    data[i].rowTimes (lambdaForEntry, subs[i]);
  }

  // Return sum of elementwise products stored in temp array.
  return( lambdaForEntry.sum() );
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
distribute() const
{
  // Take nd^th root of each component of lambda
  const ttb_indx nd = this->ndims();
  lambda.power(1.0/nd);

  // Scale factor matrix columns by rooted lambda
  for (ttb_indx i=0; i<nd; ++i)
    data[i].colScale(lambda, false);

  // Reset weights to 1
  lambda = 1.0;
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
distribute(ttb_indx i) const
{
  data[i].colScale(lambda,false);
  lambda = 1.0;
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
normalize(Genten::NormType norm_type, ttb_indx i) const
{
#ifdef HAVE_CALIPER
  cali::Function cali_func("Genten::Ktensor::normalize");
#endif

#ifndef _GENTEN_CK_FINITE
#define CKFINITE 0 // set to 1 to turn on inf/nan checking.
#else
#define CKFINITE 1
#endif
  const ttb_indx n = lambda.size();
  Genten::ArrayT<ExecSpace> norms(n);

#if CKFINITE
  ttb_indx bad= 0;
  if (norms.hasNonFinite(bad)) {
    std::cout << " Genten::Ktensor::normalize bad norms element "<< bad << " at line " << __LINE__ << std::endl;
  }
  if (data[i].hasNonFinite(bad)) {
    std::cout << " Genten::Ktensor::normalize bad data["<<i<<"] element "<< bad << " at line " << __LINE__ << std::endl;
  }
#endif

  data[i].colNorms(norm_type, norms, 0.0);

  // for (ttb_indx k = 0; k < n; k++)
  // {
  //   if (norms[k] == 0)
  //   {
  //     norms[k] = 1;
  //   }
  // }
  Kokkos::parallel_for(Kokkos::RangePolicy<ExecSpace>(0,n),
                       KOKKOS_LAMBDA(const ttb_indx k)
  {
    if (norms[k] == ttb_real(0))
      norms[k] = ttb_real(1);
  }, "Genten::Ktensor::normalize_init_kernel");

#if CKFINITE
  if (data[i].hasNonFinite(bad)) {
    std::cout << " Genten::Ktensor::normalize bad data["<<i<<"] element "<< bad << " at line " << __LINE__ << std::endl;
  }
  if (norms.hasNonFinite(bad)) {
    std::cout << " Genten::Ktensor::normalize bad norms element "<< bad << " at line " << __LINE__ << std::endl;
  }
#endif

  data[i].colScale(norms, true);

#if CKFINITE
  if (data[i].hasNonFinite(bad)) {
    std::cout << " Genten::Ktensor::normalize bad data["<<i<<"] element "<< bad << " at line " << __LINE__ << std::endl;
  }
  if (norms.hasNonFinite(bad)) {
    std::cout << " Genten::Ktensor::normalize bad norms element "<< bad << " at line " << __LINE__ << std::endl;
  }
#endif

  lambda.times(norms);

#if CKFINITE
  if (lambda.hasNonFinite(bad)) {
    std::cout << " Genten::Ktensor::normalize bad lambda element "<< bad << " at line " << __LINE__ << std::endl;
  }
#endif
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
normalize(Genten::NormType norm_type) const
{
// could be much better vectorized instead of walking memory data.size times.
  for (ttb_indx n = 0; n < data.size(); n ++)
  {
    this->normalize(norm_type, n);
  }
}

struct greater_than
{
  template<class T>
  bool operator()(T const &a, T const &b) const { return a.first > b.first; }
};

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
arrange() const
{
  // sort lambda by value and keep track of sort index
  auto lambda_host = create_mirror_view(lambda);
  deep_copy(lambda_host, lambda);
  std::vector<std::pair<ttb_real,ttb_indx> > lambda_pair;
  for (ttb_indx i = 0 ; i != lambda_host.size() ; i++) {
    lambda_pair.push_back(std::make_pair(lambda_host[i], i));
  }
  sort(lambda_pair.begin(),lambda_pair.end(),greater_than());

  // create permuted indices
  Genten::IndxArray p(lambda_host.size());
  for (size_t i = 0 ; i != lambda_host.size() ; i++)
    p[i] = lambda_pair[i].second;

  // arrange the columns of the factor matrices using the permutation
  this->arrange(p);
}

template <typename ExecSpace>
void Genten::KtensorT<ExecSpace>::
arrange(const Genten::IndxArray& permutation_indices) const
{
  // permute factor matrices
  for (ttb_indx n = 0; n < data.size(); n ++)
    data[n].permute(permutation_indices);

  // permute lambda values
  auto lambda_host = create_mirror_view(lambda);
  deep_copy(lambda_host, lambda);
  Genten::Array new_lambda(lambda_host.size());
  for (ttb_indx i = 0; i < lambda_host.size(); i ++)
    new_lambda[i] = lambda_host[permutation_indices[i]];
  deep_copy(lambda, new_lambda);

}

template <typename ExecSpace>
ttb_real Genten::KtensorT<ExecSpace>::
normFsq() const
{
#ifdef HAVE_CALIPER
  cali::Function cali_func("Genten::Ktensor::normFsq");
#endif

  ttb_real  dResult = 0.0;

  // This technique computes an RxR matrix of dot products between all factor
  // column vectors of each mode, then forms the Hadamard product of these
  // matrices.  The last step is the scalar \lambda' H \lambda.
  const ttb_indx n = ncomponents();
  Genten::FacMatrixT<ExecSpace>  cH(n,n);
  cH = 1;
  Genten::FacMatrixT<ExecSpace>  cG(n,n);
  for (ttb_indx  n = 0; n < ndims(); n++)
  {
    cG.gramian(data[n]);
    cH.times(cG);
  }

  dResult = 0.0;
  // for (ttb_indx  r = 0; r < n; r++)
  // {
  //   dResult += lambda[r] * lambda[r] * cH.entry(r,r);
  //   for (ttb_indx  q = r+1; q < n; q++)
  //   {
  //     dResult += 2.0 * lambda[r] * lambda[q] * cH.entry(r,q);
  //   }
  // }
  Genten::ArrayT<ExecSpace> l = lambda;
  Kokkos::parallel_reduce("Genten::Ktensor::normFsq_kernel",
                          Kokkos::RangePolicy<ExecSpace>(0,n),
                          KOKKOS_LAMBDA(const ttb_indx r, ttb_real& d)
  {
    const ttb_real lr = l[r];
    d += lr * lr * cH.entry(r,r);
    for (ttb_indx q=r+1; q<n; ++q)
      d += ttb_real(2) * lr * l[q] * cH.entry(r,q);
  }, dResult);
  Kokkos::fence();

  return dResult;
}

template <typename ExecSpace>
ttb_real Genten::KtensorT<ExecSpace>::
normFsq(const Genten::ArrayT<ExecSpace>& l) const
{
#ifdef HAVE_CALIPER
  cali::Function cali_func("Genten::Ktensor::normFsq");
#endif

  ttb_real  dResult = 0.0;

  // This technique computes an RxR matrix of dot products between all factor
  // column vectors of each mode, then forms the Hadamard product of these
  // matrices.  The last step is the scalar \lambda' H \lambda.
  const ttb_indx n = ncomponents();
  Genten::FacMatrixT<ExecSpace>  cH(n,n);
  cH = 1;
  Genten::FacMatrixT<ExecSpace>  cG(n,n);
  for (ttb_indx  n = 0; n < ndims(); n++)
  {
    cG.gramian(data[n]);
    cH.times(cG);
  }

  dResult = 0.0;
  // for (ttb_indx  r = 0; r < n; r++)
  // {
  //   dResult += lambda[r] * lambda[r] * cH.entry(r,r);
  //   for (ttb_indx  q = r+1; q < n; q++)
  //   {
  //     dResult += 2.0 * lambda[r] * lambda[q] * cH.entry(r,q);
  //   }
  // }
  Kokkos::parallel_reduce("Genten::Ktensor::normFsq_kernel",
                          Kokkos::RangePolicy<ExecSpace>(0,n),
                          KOKKOS_LAMBDA(const ttb_indx r, ttb_real& d)
  {
    const ttb_real lr = l[r];
    d += lr * lr * cH.entry(r,r);
    for (ttb_indx q=r+1; q<n; ++q)
      d += ttb_real(2) * lr * l[q] * cH.entry(r,q);
  }, dResult);
  Kokkos::fence();

  return dResult;
}

#define INST_MACRO(SPACE) template class Genten::KtensorT<SPACE>;
GENTEN_INST(INST_MACRO)
