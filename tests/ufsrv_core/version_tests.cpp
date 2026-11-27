/**
 * @file version_tests.cpp
 * @brief Unit tests for the app version accessors — UfsrvVersion(),
 *        UfsrvVersionMajor/Minor/Patch(), UfsrvCompiledTimestamp(),
 *        UfsrvCompiledBy(). Their implementation is emitted by the generated
 *        version.c (src/version.c.sh -> <build>/src/version.c), so these tests
 *        exercise the output of the app version-generation pipeline.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

extern "C" {
#include <version.h>
}

/* Derive the expected values from the SAME macros the pipeline stamps, so the
 * tests remain correct when MAJOR/MINOR/PATCH are bumped in version.h. */
#define STRINGISE_NX(A) #A
#define STRINGISE(A)    STRINGISE_NX(A)

static const char *const kExpectedMajor = STRINGISE(UFSRV_MAJOR);
static const char *const kExpectedMinor = STRINGISE(UFSRV_MINOR);
static const char *const kExpectedPatch = STRINGISE(UFSRV_PATCH);
static const char *const kExpectedFull =
    STRINGISE(UFSRV_MAJOR) "." STRINGISE(UFSRV_MINOR) "." STRINGISE(UFSRV_PATCH);

/* ── Individual components match the header macros ──────────────────────── */

TEST(VersionAccessors, MajorMatchesHeaderMacro) {
  EXPECT_STREQ(UfsrvVersionMajor(), kExpectedMajor);
}

TEST(VersionAccessors, MinorMatchesHeaderMacro) {
  EXPECT_STREQ(UfsrvVersionMinor(), kExpectedMinor);
}

TEST(VersionAccessors, PatchMatchesHeaderMacro) {
  EXPECT_STREQ(UfsrvVersionPatch(), kExpectedPatch);
}

/* ── Composite version string ───────────────────────────────────────────── */

TEST(VersionAccessors, FullEqualsHeaderTriplet) {
  EXPECT_STREQ(UfsrvVersion(), kExpectedFull);
}

TEST(VersionAccessors, FullIsComponentsJoinedByDots) {
  const std::string composed = std::string(UfsrvVersionMajor()) + "." +
                               UfsrvVersionMinor() + "." +
                               UfsrvVersionPatch();
  EXPECT_EQ(composed, UfsrvVersion());
}

/* ── Contract: accessors return non-null, non-empty strings ─────────────── */

TEST(VersionAccessors, AccessorsReturnNonEmpty) {
  ASSERT_NE(UfsrvVersion(), nullptr);
  ASSERT_NE(UfsrvVersionMajor(), nullptr);
  ASSERT_NE(UfsrvVersionMinor(), nullptr);
  ASSERT_NE(UfsrvVersionPatch(), nullptr);
  EXPECT_GT(std::strlen(UfsrvVersion()), 0u);
  EXPECT_GT(std::strlen(UfsrvVersionMajor()), 0u);
  EXPECT_GT(std::strlen(UfsrvVersionMinor()), 0u);
  EXPECT_GT(std::strlen(UfsrvVersionPatch()), 0u);
}

/* ── Stable across repeated calls (backed by static storage) ────────────── */

TEST(VersionAccessors, StableAcrossCalls) {
  EXPECT_STREQ(UfsrvVersion(), UfsrvVersion());
  EXPECT_EQ(UfsrvVersion(), UfsrvVersion());  // same backing pointer
}

/* ── Timestamp and builder getters ──────────────────────────────────────── */

TEST(VersionAccessors, CompiledTimestampIsNonEmpty) {
  ASSERT_NE(UfsrvCompiledTimestamp(), nullptr);
  EXPECT_GT(std::strlen(UfsrvCompiledTimestamp()), 0u);
}

TEST(VersionAccessors, CompiledByIsNonEmpty) {
  ASSERT_NE(UfsrvCompiledBy(), nullptr);
  EXPECT_GT(std::strlen(UfsrvCompiledBy()), 0u);
}

TEST(VersionAccessors, TimestampAndBuilderAreStable) {
  EXPECT_STREQ(UfsrvCompiledTimestamp(), UfsrvCompiledTimestamp());
  EXPECT_STREQ(UfsrvCompiledBy(), UfsrvCompiledBy());
}
