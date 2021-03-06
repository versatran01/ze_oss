// Copyright (c) 2015-2016, ETH Zurich, Wyss Zurich, Zurich Eye
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the ETH Zurich, Wyss Zurich, Zurich Eye nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL ETH Zurich, Wyss Zurich, Zurich Eye BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include <imp/cu_correspondence/stereo_ctf_warping.hpp>

#include <memory>

#include <glog/logging.h>

#include <imp/cu_correspondence/solver_stereo_huber_l1.cuh>
#include <imp/cu_correspondence/solver_stereo_precond_huber_l1.cuh>
#include <imp/cu_correspondence/solver_stereo_precond_huber_l1_weighted.cuh>
#include <imp/cu_correspondence/solver_epipolar_stereo_precond_huber_l1.cuh>

#include <imp/cu_core/cu_utils.hpp>

namespace ze {
namespace cu {

//------------------------------------------------------------------------------
StereoCtFWarping::StereoCtFWarping(Parameters::Ptr params)
  : params_(params)
{
}

//------------------------------------------------------------------------------
StereoCtFWarping::~StereoCtFWarping()
{
  // thanks to managed ptrs
}

//------------------------------------------------------------------------------
void StereoCtFWarping::init()
{
  CHECK(!image_pyramids_.empty());
  for (size_t i=params_->ctf.finest_level; i<=params_->ctf.coarsest_level; ++i)
  {
    Size2u sz = image_pyramids_.front()->size(i);
    switch (params_->solver)
    {
    case StereoPDSolver::HuberL1:
      levels_.emplace_back(new SolverStereoHuberL1(params_, sz, i));
      break;
    case StereoPDSolver::PrecondHuberL1:
      levels_.emplace_back(new SolverStereoPrecondHuberL1(params_, sz, i));
      break;
    case StereoPDSolver::PrecondHuberL1Weighted:
      levels_.emplace_back(new SolverStereoPrecondHuberL1Weighted(params_, sz, i));
      break;
    case StereoPDSolver::EpipolarPrecondHuberL1:
    {
      if (!depth_proposal_)
      {
        depth_proposal_.reset(new ImageGpu32fC1(image_pyramids_.front()->size(0)));
        depth_proposal_->setValue(0.f);
      }
      if (!depth_proposal_sigma2_)
      {
        depth_proposal_sigma2_.reset(new ImageGpu32fC1(image_pyramids_.front()->size(0)));
        depth_proposal_sigma2_->setValue(0.f);
      }

      levels_.emplace_back(new SolverEpipolarStereoPrecondHuberL1(
                             params_, sz, i, cams_, F_, T_mov_fix_,
                             *depth_proposal_, *depth_proposal_sigma2_));
    }
      break;
    }
  }
}

//------------------------------------------------------------------------------
bool StereoCtFWarping::ready()
{
  // check if all vectors are of the same length and not empty
  size_t desired_num_levels =
      params_->ctf.coarsest_level - params_->ctf.finest_level + 1;

  if (images_.empty() || image_pyramids_.empty() || levels_.empty() ||
      params_->ctf.coarsest_level < params_->ctf.finest_level ||
      images_.size() < 2 || // at least two images -> maybe adapt to the algorithm?
      image_pyramids_.front()->numLevels() < desired_num_levels ||
      levels_.size() < desired_num_levels)
  {
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------
void StereoCtFWarping::addImage(const ImageGpu32fC1::Ptr& image)
{
  // generate image pyramid
  ImagePyramid32fC1::Ptr pyr =
      createImagePyramidGpu<Pixel32fC1>(image, params_->ctf.scale_factor);

  // update number of levels
  if (params_->ctf.levels > pyr->numLevels())
  {
    params_->ctf.levels = pyr->numLevels();
  }
  if (params_->ctf.coarsest_level > params_->ctf.levels - 1)
  {
    params_->ctf.coarsest_level = params_->ctf.levels - 1;
  }

  images_.push_back(image);
  image_pyramids_.push_back(pyr);

  VLOG(1) << "we have now " << images_.size() << " images and "
          <<  image_pyramids_.size() << " pyramids in the CTF instance. "
           << "params_->ctf.levels: " << params_->ctf.levels
           << " (" << params_->ctf.coarsest_level
           << " -> " << params_->ctf.finest_level << ")";
}

//------------------------------------------------------------------------------
void StereoCtFWarping::reset()
{
  images_.clear();
  image_pyramids_.clear();
}

//------------------------------------------------------------------------------
void StereoCtFWarping::solve()
{
  if (levels_.empty())
  {
    this->init();
  }
  CHECK(this->ready()) << "not initialized correctly; bailing out;";

  // the image vector that is used as input for the level solvers
  std::vector<ImageGpu32fC1::Ptr> lev_images;


  // the first level is initialized differently so we solve this one first
  size_t lev = params_->ctf.coarsest_level;
  levels_.at(lev)->init();
  // gather images of current scale level
  lev_images.clear();
  for (auto pyr : image_pyramids_)
  {
    lev_images.push_back(pyr->atShared(lev)->as<ImageGpu32fC1>());
  }
  levels_.at(lev)->solve(lev_images);

  // and then loop until we reach the finest level
  // note that we loop with +1 idx as we would result in a buffer underflow
  // due to operator-- on size_t which is an unsigned type.
  for (; lev > params_->ctf.finest_level; --lev)
  {
    levels_.at(lev-1)->init(*levels_.at(lev));

    // gather images of current scale level
    lev_images.clear();
    for (auto pyr : image_pyramids_)
    {
      lev_images.push_back(pyr->atShared(lev-1)->as<ImageGpu32fC1>());
    }
    levels_.at(lev-1)->solve(lev_images);
  }
}

//------------------------------------------------------------------------------
ImageGpu32fC1::Ptr StereoCtFWarping::computePrimalEnergy(size_t level)
{
  CHECK(this->ready()) << "not initialized correctly; bailing out;";
  level = max(params_->ctf.finest_level,
              min(params_->ctf.coarsest_level, level));
  return levels_.at(level)->computePrimalEnergy();
}

//------------------------------------------------------------------------------
StereoCtFWarping::ImageGpu32fC1::Ptr StereoCtFWarping::getDisparities(size_t level)
{
  CHECK(this->ready()) << "not initialized correctly; bailing out;";
  level = max(params_->ctf.finest_level,
              min(params_->ctf.coarsest_level, level));
  return levels_.at(level)->getDisparities();
}

//------------------------------------------------------------------------------
StereoCtFWarping::ImageGpu32fC1::Ptr StereoCtFWarping::getOcclusion(size_t level)
{
  CHECK(this->ready()) << "not initialized correctly; bailing out;";
  level = max(params_->ctf.finest_level,
              min(params_->ctf.coarsest_level, level));
  return levels_.at(level)->getOcclusion();
}

} // namespace cu
} // namespace ze

