#include <gmock/gmock.h>

TEST(Test, Elementary) { EXPECT_THAT(1 + 1, ::testing::Eq(3)); }