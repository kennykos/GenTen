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

#pragma once

#include "Genten_GCP_Sampler.hpp"
#include "Genten_AlgParams.hpp"
#include "Genten_SystemTimer.hpp"
#include "Genten_GCP_SamplingKernels.hpp"

namespace Genten {

  template <typename ExecSpace, typename LossFunction>
  class UniformSampler : public Sampler<ExecSpace,LossFunction> {
  public:

    typedef Sampler<ExecSpace,LossFunction> base_type;
    typedef typename base_type::pool_type pool_type;
    typedef typename base_type::map_type map_type;

    UniformSampler(const SptensorT<ExecSpace>& X_,
                      const AlgParams& algParams_) :
      X(X_), algParams(algParams_)
    {
      num_samples_nonzeros_value = algParams.num_samples_nonzeros_value;
      num_samples_zeros_value = algParams.num_samples_zeros_value;
      num_samples_grad = algParams.num_samples_nonzeros_grad;
      weight_nonzeros_value = algParams.w_f_nz;
      weight_zeros_value = algParams.w_f_z;
      weight_grad = algParams.w_g_nz;

      // Compute number of samples if necessary
      const ttb_indx nnz = X.nnz();
      const ttb_real tsz = X.numel_float();
      const ttb_real nz = tsz - nnz;
      const ttb_indx maxEpochs = algParams.maxiters;
      const ttb_indx ftmp = std::max((nnz+99)/100,ttb_indx(100000));
      if (num_samples_nonzeros_value == 0)
        num_samples_nonzeros_value = std::min(ftmp, nnz);
      if (num_samples_zeros_value == 0)
        num_samples_zeros_value =
          ttb_indx(std::min(ttb_real(num_samples_nonzeros_value), nz));
      if (num_samples_grad == 0)
        num_samples_grad =
          ttb_indx(std::min(std::max(ttb_real(10.0)*tsz/maxEpochs, ttb_real(1e3)), tsz));

      // Compute weights if necessary
      if (weight_nonzeros_value < 0.0)
        weight_nonzeros_value =
          ttb_real(nnz) / ttb_real(num_samples_nonzeros_value);
      if (weight_zeros_value < 0.0)
        weight_zeros_value =
          ttb_real(tsz-nnz) / ttb_real(num_samples_zeros_value);
      if (weight_grad < 0.0)
        weight_grad =
          tsz / ttb_real(num_samples_grad);
    }

    virtual ~UniformSampler() {}

    virtual void initialize(const pool_type& rand_pool_,
                            std::ostream& out) override
    {
      rand_pool = rand_pool_;

      // Sort/hash tensor if necessary for faster sampling
      if (algParams.printitn > 0) {
        if (algParams.hash)
          out << "Hashing tensor for faster sampling...";
        else
          out << "Sorting tensor for faster sampling...";
      }
      SystemTimer timer(1, algParams.timings);
      timer.start(0);
      if (algParams.hash)
        hash_map = this->buildHashMap(X,out);
      else if (!X.isSorted())
        X.sort();
      timer.stop(0);
      if (algParams.printitn > 0)
        out << timer.getTotalTime(0) << " seconds" << std::endl;
    }

    virtual void print(std::ostream& out) override
    {
      out << "Function sampler:  stratified with " << num_samples_nonzeros_value
          << " nonzero and " << num_samples_zeros_value << " zero samples\n"
          << "Gradient sampler:  uniform with " << num_samples_grad
          << " samples"
          << std::endl;
    }

    virtual void sampleTensor(const bool gradient,
                              const KtensorT<ExecSpace>& u,
                              const LossFunction& loss_func,
                              SptensorT<ExecSpace>& Xs,
                              ArrayT<ExecSpace>& w) override
    {
      if (gradient) {
        if (algParams.hash)
          Impl::uniform_sample_tensor_hash(
            X, hash_map, num_samples_grad, weight_grad, u, loss_func, false,
            Xs, w, this->rand_pool, this->algParams);
        else
          Impl::uniform_sample_tensor(
            X, num_samples_grad, weight_grad, u, loss_func, true,
            Xs, w, rand_pool, algParams);
      }
      else {
        if (algParams.hash)
          Impl::stratified_sample_tensor_hash(
            X, hash_map,
            num_samples_nonzeros_value, num_samples_zeros_value,
            weight_nonzeros_value, weight_zeros_value,
            u, loss_func, false,
            Xs, w, rand_pool, algParams);
        else
          Impl::stratified_sample_tensor(
            X, num_samples_nonzeros_value, num_samples_zeros_value,
            weight_nonzeros_value, weight_zeros_value,
            u, loss_func, false,
            Xs, w, rand_pool, algParams);
      }
    }

    virtual void fusedGradient(const KtensorT<ExecSpace>& u,
                               const LossFunction& loss_func,
                               const KtensorT<ExecSpace>& g,
                               SystemTimer& timer,
                               const int timer_nzs,
                               const int timer_zs) override
    {
      Genten::error("Fused gradient with stratified sampling not implemented!");
    }

  protected:

    SptensorT<ExecSpace> X;
    pool_type rand_pool;
    AlgParams algParams;
    ttb_indx num_samples_nonzeros_value;
    ttb_indx num_samples_zeros_value;
    ttb_indx num_samples_grad;
    ttb_real weight_nonzeros_value;
    ttb_real weight_zeros_value;
    ttb_real weight_grad;
    map_type hash_map;
  };

}
