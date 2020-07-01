//@HEADER
// ************************************************************************
//     C++ Tensor Toolbox
//     Software package for tensor math by Sandia National Laboratories
// 
// Sandia National Laboratories is a multiprogram laboratory operated by
// Sandia Corporation, a wholly owned subsidiary of Lockheed Martin Corporation,
// for the United States Department of Energy's National Nuclear Security
// Administration under contract DE-AC04-94AL85000.
//
// Copyright 2013, Sandia Corporation.
// ************************************************************************
//@HEADER

#include "Genten_Tensor.hpp"

namespace Genten {

namespace Impl {

template <typename ExecSpace>
void copyFromSptensor(const TensorT<ExecSpace>& x,
                      const SptensorT<ExecSpace>& src)
{
  const ttb_indx nnz = src.nnz();
  Kokkos::parallel_for(Kokkos::RangePolicy<ExecSpace>(0,nnz),
                       KOKKOS_LAMBDA(const ttb_indx i)
  {
    const ttb_indx k = x.sub2ind(src.getSubscripts(i));
    x[k] = src.value(i);
  }, "copyFromSptensor");
}

template <typename ExecSpace>
void copyFromKtensor(const TensorT<ExecSpace>& x,
                     const KtensorT<ExecSpace>& src)
{
  typedef Kokkos::TeamPolicy<ExecSpace> Policy;
  typedef typename Policy::member_type TeamMember;
  typedef Kokkos::View< ttb_indx**, Kokkos::LayoutRight, typename ExecSpace::scratch_memory_space , Kokkos::MemoryUnmanaged > TmpScratchSpace;

  /*const*/ ttb_indx ne = x.numel();
  /*const*/ unsigned nd = src.ndims();
  /*const*/ unsigned nc = src.ncomponents();

  // Make VectorSize*TeamSize ~= 256 on Cuda
  static const bool is_cuda = Genten::is_cuda_space<ExecSpace>::value;
  const unsigned VectorSize = is_cuda ? nc : 1;
  const unsigned TeamSize = is_cuda ? (256+nc-1)/nc : 1;
  const ttb_indx N = (ne+TeamSize-1)/TeamSize;

  const size_t bytes = TmpScratchSpace::shmem_size(TeamSize,nd);
  Policy policy(N, TeamSize, VectorSize);
  Kokkos::parallel_for(policy.set_scratch_size(0,Kokkos::PerTeam(bytes)),
                       KOKKOS_LAMBDA(const TeamMember& team)
  {
    // Compute indices for entry "i"
    const unsigned team_rank = team.team_rank();
    const unsigned team_size = team.team_size();
    const ttb_indx i = team.league_rank()*team_size+team_rank;
    if (i >= ne)
      return;

    TmpScratchSpace scratch(team.team_scratch(0), team_size, nd);
    ttb_indx *sub = &scratch(team_rank, 0);
    Kokkos::single(Kokkos::PerThread(team), [&]()
    {
      x.ind2sub(sub, i);
    });

    // Compute Ktensor value for given indices
    ttb_real src_val = 0.0;
    Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(team, nc),
                            [&](const unsigned j, ttb_real& v)
    {
      ttb_real tmp = src.weights(j);
      for (unsigned m=0; m<nd; ++m) {
        tmp *= src[m].entry(sub[m],j);
      }
      v += tmp;
    }, src_val);

    // Write result
    Kokkos::single(Kokkos::PerThread(team), [&]()
    {
      x[i] = src_val;
    });

  }, "copyFromKtensor");
}

}

template <typename ExecSpace>
TensorT<ExecSpace>::
TensorT(const SptensorT<ExecSpace>& src) : siz(src.size())
{
  siz_host = create_mirror_view(siz);
  deep_copy(siz_host, siz);
  values = ArrayT<ExecSpace>(siz_host.prod(), ttb_real(0.0));
  Impl::copyFromSptensor(*this, src);
}

template <typename ExecSpace>
TensorT<ExecSpace>::
TensorT(const KtensorT<ExecSpace>& src) : siz(src.ndims())
{
  siz_host = create_mirror_view(siz);
  const ttb_indx nd = siz_host.size();
  for (ttb_indx i=0; i<nd; ++i)
    siz_host[i] = src[i].nRows();
  deep_copy(siz, siz_host);
  values = ArrayT<ExecSpace>(siz_host.prod());
  Impl::copyFromKtensor(*this, src);
}

}

#define INST_MACRO(SPACE) template class Genten::TensorT<SPACE>;
GENTEN_INST(INST_MACRO)
