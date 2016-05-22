#pragma once

#include <memory>
#include <cuda_runtime_api.h>
#include <imp/core/image_base.hpp>
#include <imp/cu_core/cu_image_gpu.cuh>
#include <imp/cu_core/cu_utils.hpp>
#include <ze/common/macros.h>

namespace ze {
namespace cu {

// forward declarations
class Texture2D;

struct VariationalDenoisingParams
{
  float lambda = 10.f;
  std::uint16_t max_iter = 100;
  std::uint16_t primal_dual_energy_check_iter = 0;
  double primal_dual_gap_tolerance = 0.0;
};

/**
 * @brief The VariationalDenoising class
 */
class VariationalDenoising
{
public:
  ZE_POINTER_TYPEDEFS(VariationalDenoising);
  typedef ze::cu::Fragmentation<16> Fragmentation;
  typedef std::shared_ptr<Fragmentation> FragmentationPtr;

public:
  VariationalDenoising();
  virtual ~VariationalDenoising();

  virtual void  __host__  denoise(const ze::ImageBase::Ptr& dst,
                                  const ze::ImageBase::Ptr& src) = 0;

  inline dim3 dimGrid() {return fragmentation_->dimGrid;}
  inline dim3 dimBlock() {return fragmentation_->dimBlock;}
  virtual inline VariationalDenoisingParams& params() { return params_; }


  friend std::ostream& operator<<(std::ostream& os,
                                  const VariationalDenoising& rhs);

protected:
  virtual void init(const Size2u& size);
  inline virtual void print(std::ostream& os) const
  {
    //os << "  size: " << this->size_ << std::endl
    os << "  lambda: " << this->params_.lambda << std::endl
       << "  max_iter: " << this->params_.max_iter << std::endl
       << "  primal_dual_energy_check_iter: " << this->params_.primal_dual_energy_check_iter << std::endl
       << "  primal_dual_gap_tolerance: " << this->params_.primal_dual_gap_tolerance << std::endl
       << std::endl;
  }


  ze::cu::ImageGpu32fC1::Ptr u_;
  ze::cu::ImageGpu32fC1::Ptr u_prev_;
  ze::cu::ImageGpu32fC2::Ptr p_;

  // cuda textures
  std::shared_ptr<ze::cu::Texture2D> f_tex_;
  std::shared_ptr<ze::cu::Texture2D> u_tex_;
  std::shared_ptr<ze::cu::Texture2D> u_prev_tex_;
  std::shared_ptr<ze::cu::Texture2D> p_tex_;

  Size2u size_;
  FragmentationPtr fragmentation_;

  // algorithm parameters
  VariationalDenoisingParams params_;
};

inline std::ostream& operator<<(std::ostream& os,
                                const VariationalDenoising& rhs)
{
  rhs.print(os);
  return os;
}


} // namespace cu
} // namespace ze
