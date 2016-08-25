// Copyright (C) ETH Zurich, Wyss Zurich, Zurich Eye - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential

#include <random>
#include <ze/common/test_entrypoint.hpp>
#include <ze/common/matrix.hpp>
#include <ze/common/manifold.hpp>
#include <ze/common/types.hpp>
#include <ze/common/transformation.hpp>
#include <ze/geometry/lsq_state.hpp>

TEST(StateTests, testTupleFixedSize)
{
  using namespace ze;
  using Tuple1 = std::tuple<Transformation, real_t, Vector3>;
  using Tuple2 = std::tuple<Transformation, VectorX>;
  EXPECT_TRUE(internal::TupleIsFixedSize<Tuple1>::is_fixed_size);
  EXPECT_FALSE(internal::TupleIsFixedSize<Tuple2>::is_fixed_size);
}

TEST(StateTests, testStateFixedSize)
{
  using namespace ze;

  using MyState = State<Transformation,Vector3,real_t>;
  EXPECT_EQ(MyState::dimension, 10);

  MyState state;
  state.print();

  MyState::TangentVector v;
  state.retract(v);

  EXPECT_EQ(State<Transformation>::dimension, 6);
}

TEST(StateTests, testStateDynamicSize)
{
  using namespace ze;
  using MyState = State<Transformation,VectorX>;

  // Test constructor of dynamic-sized state.
  MyState state;
  VectorX& x = state.at<1>();
  x.resize(5);
  x.setConstant(0.5);
  state.print();

  EXPECT_EQ(state.getDimension(), 11);
  EXPECT_TRUE(state.isDynamicSize());
  EXPECT_FALSE(state.isElementDynamicSize<0>());
  EXPECT_TRUE(state.isElementDynamicSize<1>());

  // Test retract.
  traits<MyState>::TangentVector v;
  v.resize(state.getDimension());
  v.setConstant(1.0);
  state.retract(v);
  state.print();
}

ZE_UNITTEST_ENTRYPOINT
