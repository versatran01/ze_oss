#include <string>
#include <vector>
#include <iostream>

#include <ze/common/benchmark.h>
#include <ze/common/types.h>
#include <ze/common/ringbuffer.h>
#include <ze/common/buffer.h>
#include <ze/common/test_entrypoint.h>

DEFINE_bool(run_benchmark, false, "Benchmark the buffer vs. ringbuffer");

using ze::FloatType;

TEST(RingBufferTest, testTimeAndDataSync)
{
  ze::Ringbuffer<FloatType, 3, 10> buffer;
  for(int i = 1; i < 8; ++i)
  {
    buffer.insert(i, Eigen::Vector3d(i, i, i));
  }

  buffer.lock();

  // check data order
  auto data = buffer.data();
  auto times = buffer.times();

  for (int i = 1; i < 8; ++i)
  {
    EXPECT_EQ(i, times.at(i - 1));
    EXPECT_EQ(i, data(0, i - 1));
  }
  buffer.unlock();

  // close the circle
  for (int i = 8; i < 15; ++i)
  {
    buffer.insert(i, Eigen::Vector3d(i, i, i));
  }

  buffer.lock();
  data = buffer.data();

  EXPECT_EQ(9, times.at(8)); EXPECT_EQ(9, data(0, 8));
  EXPECT_EQ(10, times.at(9)); EXPECT_EQ(10, data(0, 9));
  EXPECT_EQ(11, times.at(0)); EXPECT_EQ(11, data(0, 0));
  EXPECT_EQ(12, times.at(1)); EXPECT_EQ(12, data(0, 1));
  EXPECT_EQ(13, times.at(2)); EXPECT_EQ(13, data(0, 2));
  EXPECT_EQ(14, times.at(3)); EXPECT_EQ(14, data(0, 3));
  EXPECT_EQ(5, times.at(4)); EXPECT_EQ(5, data(0, 4));

  buffer.unlock();
}

TEST(RingBufferTest, testLowerBound)
{
  ze::Ringbuffer<FloatType, 2, 10> buffer;
  for(int i = 1; i < 10; ++i)
  {
    buffer.insert(i, Eigen::Vector2d(i, i));
  }
  buffer.lock();

  EXPECT_EQ(buffer.lower_bound(2), buffer.times().begin() + 1);
  EXPECT_EQ(buffer.lower_bound(11), buffer.times().end());
  EXPECT_EQ(buffer.lower_bound(10), buffer.times().end());
  EXPECT_EQ(buffer.lower_bound(9), buffer.times().end() - 1);
  EXPECT_EQ(buffer.lower_bound(0), buffer.times().begin());
}

TEST(RingBufferTest, testRemoveOlderThanTimestamp)
{
  ze::Ringbuffer<FloatType, 3, 10> buffer;
  for(int i = 1; i < 10; ++i)
  {
    buffer.insert(i, Eigen::Vector3d(i, i, i));
  }

  buffer.lock();
  EXPECT_EQ(9, buffer.times().size());
  buffer.unlock();

  buffer.removeDataBeforeTimestamp(3);
  buffer.lock();
  EXPECT_EQ(3, buffer.times().front());
  EXPECT_EQ(7, buffer.times().size());
  buffer.unlock();
}

TEST(RingBufferTest, testRemoveOlderThan)
{
  ze::Ringbuffer<FloatType, 2, 10> buffer;
  for(int i = 1; i < 10; ++i)
  {
    buffer.insert(ze::secToNanosec(i), Eigen::Vector2d(i, i));
  }

  buffer.removeDataOlderThan(3.0);
  buffer.lock();
  EXPECT_EQ(ze::secToNanosec(6), buffer.times().front());
  EXPECT_EQ(ze::secToNanosec(9), buffer.times().back());
  buffer.unlock();
}

TEST(RingBufferTest, testIterator)
{
  ze::Ringbuffer<FloatType, 2, 10> buffer;
  for(int i = 1; i < 10; ++i)
  {
    buffer.insert(ze::secToNanosec(i), Eigen::Vector2d(i, i));
  }

  buffer.lock();

  // Check before/after
  EXPECT_EQ(*buffer.iterator_equal_or_before(ze::secToNanosec(3.5)),
            ze::secToNanosec(3));
  EXPECT_EQ(*buffer.iterator_equal_or_after(ze::secToNanosec(3.5)),
            ze::secToNanosec(4));

  // Check equal
  EXPECT_EQ(*buffer.iterator_equal_or_before(ze::secToNanosec(3)),
            ze::secToNanosec(3));
  EXPECT_EQ(*buffer.iterator_equal_or_after(ze::secToNanosec(4)),
            ze::secToNanosec(4));

  // Expect out of range:
  EXPECT_EQ(buffer.iterator_equal_or_before(ze::secToNanosec(0.8)),
            buffer.times().end());
  EXPECT_EQ(buffer.iterator_equal_or_before(ze::secToNanosec(9.1)),
            (--buffer.times().end()));
  EXPECT_EQ(buffer.iterator_equal_or_after(ze::secToNanosec(9.1)),
            buffer.times().end());
  EXPECT_EQ(buffer.iterator_equal_or_after(ze::secToNanosec(0.8)),
            buffer.times().begin());

  buffer.unlock();
}

TEST(RingBufferTest, testNearestValue)
{
  ze::Ringbuffer<FloatType, 2, 10> buffer;
  EXPECT_FALSE(std::get<2>(buffer.getNearestValue(ze::secToNanosec(1))));

  for(int i = 1; i < 10; ++i)
  {
    buffer.insert(ze::secToNanosec(i), Eigen::Vector2d(i, i));
  }

  EXPECT_EQ(std::get<1>(buffer.getNearestValue(ze::secToNanosec(1)))[0], 1);
  EXPECT_EQ(std::get<1>(buffer.getNearestValue(ze::secToNanosec(0.4)))[0], 1);
  EXPECT_EQ(std::get<1>(buffer.getNearestValue(ze::secToNanosec(1.4)))[0], 1);
  EXPECT_EQ(std::get<1>(buffer.getNearestValue(ze::secToNanosec(11.0)))[0], 9);
}

TEST(RingBufferTest, testOldestNewestValue)
{
  ze::Ringbuffer<FloatType, 2, 10> buffer;
  EXPECT_FALSE(buffer.getOldestValue().second);
  EXPECT_FALSE(buffer.getNewestValue().second);

  for(int i = 1; i < 10; ++i)
  {
    buffer.insert(ze::secToNanosec(i), Eigen::Vector2d(i, i));
  }

  EXPECT_EQ(buffer.getNewestValue().first[0], 9);
  EXPECT_EQ(buffer.getOldestValue().first[0], 1);
}

TEST(RingBufferTest, testInterpolation)
{
  using namespace ze;

  ze::Ringbuffer<FloatType, 2, 10> buffer;

  for(int i = 0; i < 10; ++i)
  {
    buffer.insert(secToNanosec(i), Vector2(i, i));
  }

  Eigen::Matrix<uint64_t, Eigen::Dynamic, 1> stamps;
  Eigen::Matrix<FloatType, 2, Eigen::Dynamic> values;
  std::tie(stamps, values) = buffer.getBetweenValuesInterpolated(
        secToNanosec(1.2), secToNanosec(5.4));

  EXPECT_EQ(stamps.size(), values.cols());
  EXPECT_EQ(stamps.size(), 6);
  EXPECT_EQ(stamps(0), secToNanosec(1.2));
  EXPECT_EQ(stamps(stamps.size()-1), secToNanosec(5.4));
  EXPECT_DOUBLE_EQ(values(0, 0), 1.2);
  EXPECT_DOUBLE_EQ(values(0, stamps.size()-1), 5.4);

  std::tie(stamps, values) = buffer.getBetweenValuesInterpolated(
        secToNanosec(0), secToNanosec(9));
  EXPECT_EQ(stamps(0), secToNanosec(0));
  EXPECT_EQ(stamps(stamps.size()-1), secToNanosec(9));
  EXPECT_DOUBLE_EQ(values(0, 0), 0);
  EXPECT_DOUBLE_EQ(values(0, stamps.size()-1), 9);

  // "overfill" the ring
  for(int i = 10; i < 15; ++i)
  {
    buffer.insert(secToNanosec(i), Vector2(i, i));
  }
  // cross the buffer boundaries
  std::tie(stamps, values) = buffer.getBetweenValuesInterpolated(
        secToNanosec(8), secToNanosec(12));

  // ensure consistency
  for (int i = 8; i <= 12; ++i)
  {
    EXPECT_EQ(secToNanosec(i), stamps(i - 8));
    EXPECT_EQ(i, values(0, i - 8));
  }

  // cross the buffer boundaries
  std::tie(stamps, values) = buffer.getBetweenValuesInterpolated(
        secToNanosec(7.5), secToNanosec(12.5));

  for (int i = 8; i <= 12; ++i)
  {
    EXPECT_EQ(secToNanosec(i), stamps(i - 8 + 1));
    EXPECT_EQ(i, values(0, i - 8 + 1));
  }
  // interpolated boundaries
  EXPECT_EQ(secToNanosec(7.5), stamps(0));
  EXPECT_EQ(secToNanosec(12.5), stamps(6));
  EXPECT_EQ(7.5, values(0, 0));
  EXPECT_EQ(12.5, values(0, 6));
}

TEST(RingBufferTest, testInterpolationBounds)
{
  using namespace ze;

  ze::Ringbuffer<FloatType, 2, 10> buffer;

  for(int i = 1; i < 10; ++i)
  {
    buffer.insert(secToNanosec(i), Vector2(i, i));
  }

  Eigen::Matrix<uint64_t, Eigen::Dynamic, 1> stamps;
  Eigen::Matrix<FloatType, 2, Eigen::Dynamic> values;
  std::tie(stamps, values) = buffer.getBetweenValuesInterpolated(
        secToNanosec(0), secToNanosec(2));
  EXPECT_EQ(stamps.size(), values.cols());
  EXPECT_EQ(0, stamps.size());
  EXPECT_EQ(0, values.cols());
  std::tie(stamps, values) = buffer.getBetweenValuesInterpolated(
        secToNanosec(5), secToNanosec(15));
  EXPECT_EQ(stamps.size(), values.cols());
  EXPECT_EQ(0, stamps.size());
  EXPECT_EQ(0, values.cols());
  std::tie(stamps, values) = buffer.getBetweenValuesInterpolated(
        secToNanosec(0), secToNanosec(15));
  EXPECT_EQ(stamps.size(), values.cols());
  EXPECT_EQ(0, stamps.size());
  EXPECT_EQ(0, values.cols());
}

TEST(RingBufferTest, benchmarkBufferVsRingBuffer)
{
  if (!FLAGS_run_benchmark) {
    return;
  }

  using namespace ze;

  // generate random data
  Eigen::MatrixXd data(3, 10000);
  data.setRandom();

  Buffer<FloatType, 3> buffer(nanosecToSecTrunc(1024));
  Ringbuffer<FloatType, 3, 1024> ringbuffer;

  //////
  // Insert
  auto insertRingbuffer = [&]()
  {
    for (int i = 0; i < data.cols(); ++i)
    {
      ringbuffer.insert(i, data.col(i));
    }
  };
  auto insertBuffer = [&]()
  {
    for (int i = 0; i < data.cols(); ++i)
    {
      buffer.insert(i, data.col(i));
    }
  };

  FloatType ringbuffer_insert = runTimingBenchmark(insertRingbuffer, 10, 20,
                     "Ringbuffer: Insert", true);
  FloatType buffer_insert = runTimingBenchmark(insertBuffer, 10, 20,
                     "Buffer: Insert", true);

  VLOG(1) << "[Insert]" << "Buffer/Ringbuffer: " <<  buffer_insert / ringbuffer_insert << "\n";

  FloatType oldest, newest;
  std::tie(newest, oldest, std::ignore) = ringbuffer.getOldestAndNewestStamp();

  VLOG(1) << "BufferSize: " << buffer.size() << "\n";
  VLOG(1) << "RingbufferSize: " << ringbuffer.size() << "\n";

  //////
  // get nearest value
  auto getNearestValueRingbuffer = [&]()
  {
    double stamp = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    ringbuffer.getNearestValue(stamp);
  };
  auto getNearestValueBuffer = [&]()
  {
    double stamp = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    buffer.getNearestValue(stamp);
  };

  FloatType ringbuffer_nearest = runTimingBenchmark(getNearestValueRingbuffer, 10, 20,
                     "Ringbuffer: Nearest Value", true);
  FloatType buffer_nearest = runTimingBenchmark(getNearestValueBuffer, 10, 20,
                     "Buffer: Nearest Value", true);

  VLOG(1) << "[NearestValue]" << "Buffer/Ringbuffer: " <<  buffer_nearest / ringbuffer_nearest << "\n";

  //////
  // interpolation
  auto getBetweenValuesInterpolatedRingbuffer = [&]()
  {
    double stamp1 = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    double stamp2 = ((double) rand() / (RAND_MAX)) * (oldest - stamp1) + stamp1 + 1;
    ringbuffer.getBetweenValuesInterpolated(stamp1, stamp2);
  };
  auto getBetweenValuesInterpolatedBuffer = [&]()
  {
    double stamp1 = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    double stamp2 = ((double) rand() / (RAND_MAX)) * (oldest - stamp1) + stamp1 + 1;
    buffer.getBetweenValuesInterpolated(stamp1, stamp2);
  };

  FloatType ringbuffer_interpolate = runTimingBenchmark(getBetweenValuesInterpolatedRingbuffer, 10, 20,
                     "Ringbuffer: Interpolate", true);
  FloatType buffer_interpolate = runTimingBenchmark(getBetweenValuesInterpolatedBuffer, 10, 20,
                     "Buffer: Interpolate", true);

  VLOG(1) << "[Interpolate]" << "Buffer/Ringbuffer: " <<  buffer_interpolate / ringbuffer_interpolate << "\n";

  //////
  // iterator equal or before
  buffer.lock();
  ringbuffer.lock();
  auto iteratorEqRingbuffer = [&]()
  {
    double stamp = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    ringbuffer.iterator_equal_or_before(stamp);
  };
  auto iteratorEqBuffer = [&]()
  {
    double stamp = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    buffer.iterator_equal_or_before(stamp);
  };

  FloatType ringbuffer_iterator = runTimingBenchmark(iteratorEqRingbuffer, 10, 20,
                     "Ringbuffer: Interpolate", true);
  FloatType buffer_iterator = runTimingBenchmark(iteratorEqBuffer, 10, 20,
                     "Buffer: Interpolate", true);
  buffer.unlock();
  ringbuffer.unlock();

  VLOG(1) << "[IteratorBf]" << "Buffer/Ringbuffer: " <<  buffer_iterator / ringbuffer_iterator << "\n";

  //////
  // iterator equal or after
  buffer.lock();
  ringbuffer.lock();
  auto iteratorEqAfRingbuffer = [&]()
  {
    double stamp = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    ringbuffer.iterator_equal_or_after(stamp);
  };
  auto iteratorEqAfBuffer = [&]()
  {
    double stamp = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    buffer.iterator_equal_or_after(stamp);
  };

  FloatType ringbuffer_iterator_af = runTimingBenchmark(iteratorEqAfRingbuffer, 10, 20,
                     "Ringbuffer: Interpolate", true);
  FloatType buffer_iterator_af = runTimingBenchmark(iteratorEqAfBuffer, 10, 20,
                     "Buffer: Interpolate", true);
  buffer.unlock();
  ringbuffer.unlock();

  VLOG(1) << "[IteratorAf]" << "Buffer/Ringbuffer: " <<  buffer_iterator_af / ringbuffer_iterator_af << "\n";

  //////
  // removeDataBeforeTimestamp
  auto removeDataBeforeTimestampRingbuffer = [&]()
  {
    double stamp = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    ringbuffer.removeDataBeforeTimestamp(stamp);
  };
  auto removeDataBeforeTimestampBuffer = [&]()
  {
    double stamp = ((double) rand() / (RAND_MAX)) * (oldest - newest) + newest - 1;
    buffer.removeDataBeforeTimestamp(stamp);
  };

  FloatType ringbuffer_remove = runTimingBenchmark(removeDataBeforeTimestampRingbuffer, 10, 20,
                     "Ringbuffer: Interpolate", true);
  FloatType buffer_remove = runTimingBenchmark(removeDataBeforeTimestampBuffer, 10, 20,
                     "Buffer: Interpolate", true);

  VLOG(1) << "[Remove]" << "Buffer/Ringbuffer: " <<  buffer_remove / ringbuffer_remove << "\n";
}

ZE_UNITTEST_ENTRYPOINT
