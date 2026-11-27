#include <gtest/gtest.h>
extern "C" int SessionAuthGuardFixture(void);

TEST(SessionAuthGuard, RejectsDuplicateSignonsAndPreservesIndependentAccounts)
{
  EXPECT_EQ(SessionAuthGuardFixture(), 0);
}
