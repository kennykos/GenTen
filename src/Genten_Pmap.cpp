//@header
// ************************************************************************
//     genten: software for generalized tensor decompositions
//     by sandia national laboratories
//
// sandia national laboratories is a multimission laboratory managed
// and operated by national technology and engineering solutions of sandia,
// llc, a wholly owned subsidiary of honeywell international, inc., for the
// u.s. department of energy's national nuclear security administration under
// contract de-na0003525.
//
// copyright 2017 national technology & engineering solutions of sandia, llc
// (ntess). under the terms of contract de-na0003525 with ntess, the u.s.
// government retains certain rights in this software.
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// holder or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.
// ************************************************************************
//@header

#include "Genten_Pmap.hpp"
#include "Genten_DistContext.hpp"
#include "Genten_IOtext.hpp"

#include <boost/serialization/vector.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace Genten {

namespace {
// Silly function to compute divisors
auto divisors(int input) {
  small_vector<int> divisors(1,input);
  int sroot = std::sqrt(input);
  for (auto i = 1; i <= sroot; ++i) {
    if (input % i == 0) {
      divisors.push_back(i);
      if (i != sroot) {
        divisors.push_back(input / i);
      }
    }
  }

  std::sort(divisors.begin(), divisors.end());
  return divisors;
}

// Goal is to count the total storage of the factors for the given grid Storage
// of each factor is the size of the factor matrix times the number of
// processes in the grid that are not in our fiber.
//
// clang-format off
// For example given a grid [2, 3, 5, 7] factor matrices would be distributed over: 
// F0: [_, 3, 5, 7] = 105 of the 210 processes
// F1: [2, _, 5, 7] = 70 of the 210 processes
// F2: [2, 3, _, 7] = 42 of the 210 processes
// F3: [2, 3, 5, _] = 30 of the 210 processes
// clang-format on
//
// Then to compute the total storage you need to multiplie the size of each
// factor matrix times the number of processes it is distributed over.
//
// To keep this code from needing to know about the rank of the factors we will
// return the result for rank 1 factors. The calling code can simply scale this
// result by the rank to figure out the total number of elements
auto nelementsForRank1Factors(small_vector<int> const &grid,
                              std::vector<int> const &tensor_dims) {
  auto nprocs =
      std::accumulate(grid.begin(), grid.end(), 1ll, std::multiplies<>{});

  const auto ndims = grid.size();
  int64_t nelements = 0;
  for (auto i = 0; i < ndims; ++i) {
    const auto replicated_procs = nprocs / grid[i];
    nelements += replicated_procs * tensor_dims[i];
  }

  return nelements;
}

// This function writes the grid with that leads to the minimal storage
// required for the factor matrices
auto recurseMinSpaceGrid(int nprocs, small_vector<int> &grid,
                         std::vector<int> const &tensor_dims,
                         int dims_remaining) {
  assert(dims_remaining >= 1);

  // The last index has no freedom just set it and return
  if (dims_remaining == 1) {
    grid.back() = nprocs;
    return;
  }

  // Current index tells us which position we are in
  const auto current_index = grid.size() - dims_remaining;

  // Make copy for testing on so that we only ever write to grid when we've
  // found a better option
  auto test = grid;
  auto min_storage = std::numeric_limits<int64_t>::max();

  for (auto d : divisors(nprocs)) {
    test[current_index] = d;
    const auto remaining_procs = nprocs / d;
    recurseMinSpaceGrid(remaining_procs, test, tensor_dims, dims_remaining - 1);

    auto test_storage = nelementsForRank1Factors(test, tensor_dims);
    if (test_storage < min_storage) {
      min_storage = test_storage;
      grid = test;
    }
  }
}

auto minFactorSpaceGrid(int nprocs, std::vector<int> const &tensor_dims) {
  const auto ndims = tensor_dims.size();
  auto grid = small_vector<int>(ndims);
  recurseMinSpaceGrid(nprocs, grid, tensor_dims, ndims);
  return grid;
}

auto minAllReduceComm(int nprocs, std::vector<int> const &tensor_dims) {
  return minFactorSpaceGrid(nprocs, tensor_dims);
}

enum class CartGridStratagy { MinAllReduceComm, MinFactorSpace };

auto CartGrid(int nprocs, std::vector<int> const &tensor_dims,
              CartGridStratagy strat = CartGridStratagy::MinAllReduceComm) {
  switch (strat) {
  case CartGridStratagy::MinAllReduceComm:
    return minAllReduceComm(nprocs, tensor_dims);
  case CartGridStratagy::MinFactorSpace:
    return minFactorSpaceGrid(nprocs, tensor_dims);
  }
}

} // namespace

ProcessorMap::ProcessorMap(ptree const &input_tree, TensorInfo const &Ti)
    : pmap_tree_(input_tree.get_child("pmap", ptree{})) {

  // Do initial setup on rank 0
  if (DistContext::rank() == 0) {
    dimension_sizes_ = CartGrid(DistContext::nranks(), Ti.dim_sizes,
                                CartGridStratagy::MinFactorSpace);
  }

  DistContext::Bcast(dimension_sizes_, 0);
  const auto ndims = dimension_sizes_.size();

  // I don't think we need to be periodic
  small_vector<int> periodic(ndims, 0);
  bool reorder = true; // Let MPI be smart I guess
  MPI_Cart_create(DistContext::commWorld(), ndims,
                  dimension_sizes_.data(), periodic.data(), reorder,
                  &cart_comm_);

  MPI_Comm_size(cart_comm_, &grid_nprocs_);
  MPI_Comm_rank(cart_comm_, &grid_rank_);
  coord_.resize(ndims);
  MPI_Cart_coords(cart_comm_, grid_rank_, ndims, coord_.data());

  small_vector<int> dim_filter(ndims, 1);
  sub_maps_.resize(ndims);
  sub_grid_rank_.resize(ndims);
  sub_comm_sizes_.resize(ndims);

  // Get information for the MPI Subgrid for each Dimension
  for (auto i = 0; i < ndims; ++i) {
    dim_filter[i] = 0; // Get all dims except this one
    MPI_Cart_sub(cart_comm_, dim_filter.data(), &sub_maps_[i]);
    dim_filter[i] = 1; // Reset the dim_filter

    MPI_Comm_rank(sub_maps_[i], &sub_grid_rank_[i]);
    MPI_Comm_size(sub_maps_[i], &sub_comm_sizes_[i]);
  }
}

ProcessorMap::~ProcessorMap() {
  if (DistContext::initialized()) {
    for (auto &comm : sub_maps_) {
      if (comm != MPI_COMM_NULL) {
        MPI_Comm_free(&comm);
      }
    }
    if (cart_comm_ != MPI_COMM_NULL) {
      MPI_Comm_free(&cart_comm_);
    }
  }
}
} // namespace Genten
