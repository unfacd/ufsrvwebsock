/**
 * @file session_auth_lookup_tests.cpp
 * @brief Exercise production authentication lookup error, absence and reference transfer.
 */
#include <gtest/gtest.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <thread>

extern "C" {
#include <session_auth_guard.h>
#include <uflib/adt/adt_hashtable.h>
void *__real_malloc(size_t size);
}

static thread_local bool fail_next_malloc;
extern "C" void *__wrap_malloc(size_t size)
{
  if (fail_next_malloc) {
    fail_next_malloc = false;
    errno = ENOMEM;
    return nullptr;
  }
  return __real_malloc(size);
}

namespace {
struct Item { const char *key; int references; };
void Retain(RecyclerClientData *data, int count)
{
  static_cast<Item *>(static_cast<void *>(data))->references += count;
}

class SessionAuthLookup : public ::testing::Test {
protected:
  HashTable table{};
  Item item{"present", 0};
  void SetUp() override
  {
    ASSERT_EQ(HashTableLockingInstantiate(&table, offsetof(Item, key), KEY_SIZE_ZERO,
              HASH_ITEM_IS_PTR_TYPE, "auth-lookup", HASHTABLE_DEFAULT_EXTRACTOR), &table);
    ASSERT_EQ(AddToHash(&table, &item), &item);
  }
  void TearDown() override
  {
    fail_next_malloc = false;
    HashTableDestruct(&table, false);
  }
};

TEST_F(SessionAuthLookup, FoundAndAbsentAreSuccessfulButDistinct)
{
  void *holder = nullptr;
  ASSERT_TRUE(SessionAuthLookupHolder(&table, "present", Retain, &holder));
  EXPECT_EQ(holder, &item);
  EXPECT_EQ(item.references, 1);
  item.references--;
  ASSERT_TRUE(SessionAuthLookupHolder(&table, "absent", Retain, &holder));
  EXPECT_EQ(holder, nullptr);
  EXPECT_EQ(item.references, 0);
}

TEST_F(SessionAuthLookup, ContendedIndexIsErrorForBothPresentAndAbsentKeys)
{
  ASSERT_EQ(pthread_rwlock_wrlock(&table.hashtable_rwlock), 0);
  bool present_ok = true, absent_ok = true;
  void *present = &item, *absent = &item;
  int present_error = 0, absent_error = 0;
  auto start = std::chrono::steady_clock::now();
  std::thread reader([&] {
    present_ok = SessionAuthLookupHolder(&table, "present", Retain, &present);
    present_error = errno;
    absent_ok = SessionAuthLookupHolder(&table, "absent", Retain, &absent);
    absent_error = errno;
  });
  reader.join();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();
  EXPECT_LT(elapsed_ms, 500) << "a contended index must fail on a bounded budget, not block";
  EXPECT_EQ(pthread_rwlock_unlock(&table.hashtable_rwlock), 0);
  EXPECT_FALSE(present_ok);
  EXPECT_FALSE(absent_ok);
  EXPECT_EQ(present, nullptr);
  EXPECT_EQ(absent, nullptr);
  EXPECT_EQ(present_error, EBUSY);
  EXPECT_EQ(absent_error, EBUSY);
  EXPECT_EQ(item.references, 0);
  ASSERT_TRUE(SessionAuthLookupHolder(&table, "present", Retain, &present));
  EXPECT_EQ(present, &item);
  EXPECT_EQ(item.references, 1);
  item.references--;
}

TEST_F(SessionAuthLookup, ContendedReadSucceedsWhenTheWriterReleasesWithinBudget)
{
  std::atomic<bool> attempting{false};
  bool ok = false;
  void *holder = nullptr;
  int error = -1;
  ASSERT_EQ(pthread_rwlock_wrlock(&table.hashtable_rwlock), 0);
  std::thread reader([&] {
    attempting.store(true, std::memory_order_release);
    ok = SessionAuthLookupHolder(&table, "present", Retain, &holder);
    error = errno;
  });
  while (!attempting.load(std::memory_order_acquire)) std::this_thread::yield();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(pthread_rwlock_unlock(&table.hashtable_rwlock), 0);
  reader.join();
  EXPECT_TRUE(ok);
  EXPECT_EQ(error, 0);
  EXPECT_EQ(holder, &item);
  EXPECT_EQ(item.references, 1);
  item.references--;
}

TEST_F(SessionAuthLookup, ContendedAbsentKeyIsAbsenceOnceTheLockClears)
{
  std::atomic<bool> attempting{false};
  bool ok = false;
  void *holder = &item;
  ASSERT_EQ(pthread_rwlock_wrlock(&table.hashtable_rwlock), 0);
  std::thread reader([&] {
    attempting.store(true, std::memory_order_release);
    ok = SessionAuthLookupHolder(&table, "absent", Retain, &holder);
  });
  while (!attempting.load(std::memory_order_acquire)) std::this_thread::yield();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(pthread_rwlock_unlock(&table.hashtable_rwlock), 0);
  reader.join();
  EXPECT_TRUE(ok);
  EXPECT_EQ(holder, nullptr);
  EXPECT_EQ(item.references, 0);
}

TEST_F(SessionAuthLookup, AllocationFailureCannotLookLikeAbsenceOrAcquirePin)
{
  void *holder = &item;
  fail_next_malloc = true;
  bool result = SessionAuthLookupHolder(&table, "present", Retain, &holder);
  EXPECT_FALSE(result);
  EXPECT_FALSE(fail_next_malloc);
  EXPECT_EQ(errno, ENOMEM);
  EXPECT_EQ(holder, nullptr);
  EXPECT_EQ(item.references, 0);
}

TEST_F(SessionAuthLookup, InvalidArgumentsAreErrorsAndClearOutput)
{
  void *holder = &item;
  EXPECT_FALSE(SessionAuthLookupHolder(nullptr, "present", Retain, &holder));
  EXPECT_EQ(errno, EINVAL);
  EXPECT_EQ(holder, nullptr);
  EXPECT_FALSE(SessionAuthLookupHolder(&table, "present", Retain, nullptr));
  EXPECT_EQ(errno, EINVAL);
  EXPECT_EQ(item.references, 0);
}

TEST_F(SessionAuthLookup, RepeatedLookupTransfersExactlyOnePinPerHit)
{
  for (int i = 0; i < 1000; ++i) {
    void *holder = nullptr;
    ASSERT_TRUE(SessionAuthLookupHolder(&table, i%2 ? "present" : "absent", Retain, &holder));
    if (i%2) {
      EXPECT_EQ(holder, &item);
      EXPECT_EQ(item.references, 1);
      item.references--;
    } else {
      EXPECT_EQ(holder, nullptr);
      EXPECT_EQ(item.references, 0);
    }
  }
}
}
