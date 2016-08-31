// Copyright (C) ETH Zurich, Wyss Zurich, Zurich Eye - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential

#include <ze/common/test_entrypoint.hpp>

#include <ze/imu/accelerometer_model.hpp>
#include <ze/imu/imu_intrinsic_model.hpp>
#include <ze/imu/imu_noise_model.hpp>

TEST(AccelerometerModelTest, testAccelerometer)
{
  using namespace ze;
  std::shared_ptr<ImuIntrinsicModelCalibrated> intrinsics =
      std::make_shared<ImuIntrinsicModelCalibrated>();
  std::shared_ptr<ImuNoiseNone> noise = std::make_shared<ImuNoiseNone>();

  AccelerometerModel model(intrinsics, noise);

  EXPECT_EQ(intrinsics, model.intrinsicModel());
  EXPECT_EQ(noise, model.noiseModel());
}

ZE_UNITTEST_ENTRYPOINT