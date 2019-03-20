#include <iterator>
#include "gtest/gtest.h"
#include "tuple_iterator.h"

class TupleIteratorTest : public ::testing::Test {
  protected:
    using Tuple = std::tuple<int, float, std::string>;
    using TupleIterator = tuple_ext::TupleIterator<Tuple>;
    using TupleRange = tuple_ext::TupleRange<Tuple>;

    Tuple tuple_{1, 2.5, "inf"};
    TupleRange tuple_range_{tuple_};
};

TEST_F(TupleIteratorTest, CopyConstructibleConcept) {
    const TupleIterator tuple_begin = tuple_range_.begin();

    // { T a = b; } -> { a == b; }
    TupleIterator tuple_begin_copy = tuple_begin;
    ASSERT_EQ(tuple_begin_copy, tuple_begin);

    // { T(a) == a; }
    ASSERT_EQ(TupleIterator{tuple_begin}, tuple_begin);
}

TEST_F(TupleIteratorTest, CopyAssignableConcept) {
    const TupleIterator tuple_begin = tuple_range_.begin();

    TupleIterator tuple_iter = tuple_range_.end();
    ASSERT_NE(tuple_iter, tuple_begin);

    // { a = b; } -> { a == b }
    tuple_iter = tuple_begin;
    ASSERT_EQ(tuple_iter, tuple_begin);
}

TEST_F(TupleIteratorTest, SwappableConcept) {
    ASSERT_NE(tuple_range_.begin(), tuple_range_.end());
    TupleIterator iter_a = tuple_range_.begin();
    TupleIterator iter_b = tuple_range_.end();

    using std::swap;
    swap(iter_a, iter_b);

    ASSERT_EQ(iter_a, tuple_range_.end());
    ASSERT_EQ(iter_b, tuple_range_.begin());
    ASSERT_NE(iter_a, iter_b);
}

TEST_F(TupleIteratorTest, IteratorConcept) {
    TupleIterator iter = tuple_range_.begin();

    using reference = typename std::iterator_traits<TupleIterator>::reference;
    EXPECT_TRUE((std::is_same_v<decltype(*iter), reference>));
    EXPECT_TRUE((std::is_same_v<decltype(++iter), TupleIterator&>));
}