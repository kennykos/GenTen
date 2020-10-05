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

#include "Genten_Util.hpp"

// For vtune
#include <sstream>
#include <sys/types.h>
#include <unistd.h>


void Genten::error(std::string s)
{
  std::cerr << "FATAL ERROR: " << s << std::endl;
  throw s;
}

bool  Genten::isEqualToTol(ttb_real  d1,
                           ttb_real  d2,
                           ttb_real  dTol)
{
  // Numerator = fabs(d1 - d2).
  ttb_real  dDiff = fabs(d1 - d2);

  // Denominator  = max(1, fabs(d1), fabs(d2).
  ttb_real  dAbs1 = fabs(d1);
  ttb_real  dAbs2 = fabs(d2);
  ttb_real  dD = 1.0;
  if ((dAbs1 > 1.0) || (dAbs2 > 1.0))
  {
    if (dAbs1 > dAbs2)
      dD = dAbs1;
    else
      dD = dAbs2;
  }

  // Relative difference.
  ttb_real  dRelDiff = dDiff / dD;

  // Compare the relative difference to the tolerance.
  return( dRelDiff < dTol );
}

char *  Genten::getGentenVersion(void)
{
  return( (char *)("Genten Tensor Toolbox 0.0.0") );
}

// Connect executable to vtune for profiling
void Genten::connect_vtune(const int p_rank) {
  std::stringstream cmd;
  pid_t my_os_pid=getpid();
  const std::string vtune_loc =
    "amplxe-cl";
  const std::string output_dir = "./vtune/vtune.";
  cmd << vtune_loc
      << " -collect hotspots -result-dir " << output_dir << p_rank
      << " -target-pid " << my_os_pid << " &";
  if (p_rank == 0)
    std::cout << cmd.str() << std::endl;
  system(cmd.str().c_str());
  system("sleep 10");
}

constexpr const Genten::Solver_Method::type Genten::Solver_Method::types[];
constexpr const char*const Genten::Solver_Method::names[];

constexpr const Genten::MTTKRP_Method::type Genten::MTTKRP_Method::types[];
constexpr const char*const Genten::MTTKRP_Method::names[];

constexpr const Genten::MTTKRP_All_Method::type Genten::MTTKRP_All_Method::types[];
constexpr const char*const Genten::MTTKRP_All_Method::names[];

constexpr const Genten::GCP_LossFunction::type Genten::GCP_LossFunction::types[];
constexpr const char*const Genten::GCP_LossFunction::names[];

constexpr const Genten::GCP_Sampling::type Genten::GCP_Sampling::types[];
constexpr const char*const Genten::GCP_Sampling::names[];

constexpr const Genten::GCP_Step::type Genten::GCP_Step::types[];
constexpr const char*const Genten::GCP_Step::names[];
