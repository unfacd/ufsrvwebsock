#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>
#include <session_auth_guard.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); abort(); } } while (0)

static SessionAuthClaims claims = {.mutex = PTHREAD_MUTEX_INITIALIZER};
static pthread_barrier_t start_barrier, claimed_barrier;
static atomic_int winners;
static int mode;

static void *compete(void *argument)
{
  unsigned long index = (unsigned long)argument;
  char cookie[64];
  snprintf(cookie, sizeof cookie, "cookie-%lu", mode == 0 ? 0 : index);
  SessionAuthClaim claim;
  pthread_barrier_wait(&start_barrier);
  bool entered = SessionAuthClaimBegin(&claims, &claim, cookie);
  bool accepted = entered && SessionAuthClaimUser(&claims, &claim, mode == 2 ? index+1 : 663);
  if (accepted) atomic_fetch_add(&winners, 1);
  pthread_barrier_wait(&claimed_barrier);
  if (entered) SessionAuthClaimEnd(&claims, &claim);
  return NULL;
}

int SessionAuthGuardFixture(void)
{
  CHECK(SessionAuthConnectionIsLive(true, false, 1000, 995, 996, 60, 20));
  CHECK(!SessionAuthConnectionIsLive(false, false, 1000, 995, 996, 60, 20));
  CHECK(!SessionAuthConnectionIsLive(true, false, 1000, 900, 901, 60, 20));
  CHECK(!SessionAuthConnectionIsLive(true, true, 1000, 970, 960, 60, 20));
  CHECK(SessionAuthConnectionIsLive(true, true, 1000, 980, 960, 60, 20));
  CHECK(SessionAuthConnectionIsLive(true, true, 1000, 970, 990, 60, 20));
  CHECK(SessionAuthConnectionIsLive(true, false, 1000, 999, 900, 60, 20));
  CHECK(SessionAuthConnectionIsLive(true, false, 1000, 0, 0, 60, 20));
  CHECK(SessionAuthConnectionIsLive(true, true, 1000, 1100, 1090, 60, 20));
  CHECK(SessionAuthConnectionIsLive(true, false, 1000, 900, 901, 0, 20));
  CHECK(SessionAuthConnectionIsLive(true, true, 1000, 970, 960, 0, 0));
  CHECK(!SessionAuthConnectionIsLive(true, false, 1000, 900, 901, 30, 20));
  int a, b;
  bool conflict;
  CHECK(SessionAuthSelectHolder(NULL, NULL, &conflict) == NULL && !conflict);
  CHECK(SessionAuthSelectHolder(&a, NULL, &conflict) == &a && !conflict);
  CHECK(SessionAuthSelectHolder(NULL, &a, &conflict) == &a && !conflict);
  CHECK(SessionAuthSelectHolder(&a, &a, &conflict) == &a && !conflict);
  CHECK(SessionAuthSelectHolder(&a, &b, &conflict) == NULL && conflict);
  SessionAuthClaim first, second;
  CHECK(!SessionAuthClaimBegin(&claims, &first, ""));
  CHECK(SessionAuthClaimBegin(&claims, &first, "one"));
  CHECK(!SessionAuthClaimBegin(&claims, &second, "one"));
  CHECK(SessionAuthClaimBegin(&claims, &second, "two"));
  CHECK(SessionAuthClaimUser(&claims, &first, 1));
  CHECK(!SessionAuthClaimUser(&claims, &second, 1));
  CHECK(!SessionAuthClaimUser(&claims, &first, 2));
  CHECK(SessionAuthClaimUser(&claims, &first, 1));
  SessionAuthClaimEnd(&claims, &first);
  CHECK(!SessionAuthClaimUser(&claims, &first, 2));
  CHECK(SessionAuthClaimUser(&claims, &second, 1));
  SessionAuthClaimEnd(&claims, &second);
  CHECK(claims.head == NULL);
  pthread_barrier_init(&start_barrier, NULL, 16);
  pthread_barrier_init(&claimed_barrier, NULL, 16);
  for (mode = 0; mode < 3; mode++) {
    for (int repetition = 0; repetition < 100; repetition++) {
      atomic_store(&winners, 0);
      pthread_t threads[16];
      for (unsigned long i = 0; i < 16; i++) CHECK(pthread_create(&threads[i], NULL, compete, (void *)i) == 0);
      for (int i = 0; i < 16; i++) CHECK(pthread_join(threads[i], NULL) == 0);
      CHECK(atomic_load(&winners) == (mode == 2 ? 16 : 1));
      CHECK(claims.head == NULL);
    }
  }
  pthread_barrier_destroy(&start_barrier);
  pthread_barrier_destroy(&claimed_barrier);
  return 0;
}
