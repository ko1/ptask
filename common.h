#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "ptask.h"

#define HAVE_STDARG_PROTOTYPES 1

#ifdef HAVE_STDARG_PROTOTYPES
#include <stdarg.h>
#define va_init_list(a,b) va_start(a,b)
#else
#include <varargs.h>
#define va_init_list(a,b) va_start(a)
#endif

#define bug(msg) bug_body(__FILE__, __LINE__, msg)

static void bug_body(const char *file, int line, const char *msg)
{
    fprintf(stderr, "[BUG: ptask] %s:%d %s\n", file, line, msg);
    exit(1);
}


#ifndef xmalloc
static void *
xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == 0) {
	bug("xmalloc@ptask.c: allocation failed\n");
    }
    return ptr;
}
#define xfree free
#endif

#define WRAP_FUNC1(name, type) \
  static void name(type p1) { \
      int r = pthread_##name(p1); \
      if (r != 0) { \
	  fprintf(stderr, "pthread_" #name "() on thread %p returns non-zero (%d)\n", (void *)pthread_self(), r);\
	  exit(1); \
      }}

#define WRAP_FUNC2(name, t1, t2) \
  static void name(t1 p1, t2 p2) { \
      int r = pthread_##name(p1, p2); \
      if (r != 0) { \
	  fprintf(stderr, "pthread_" #name "() on thread %p returns non-zero (%d)\n", (void *)pthread_self(), r); \
	  exit(1); \
      }}

#define LOCK(lock, block) {mutex_lock(lock); {block;}; mutex_unlock(lock);}

WRAP_FUNC2(mutex_init, pthread_mutex_t *, pthread_mutexattr_t *);
WRAP_FUNC1(mutex_lock, pthread_mutex_t *);
WRAP_FUNC1(mutex_unlock, pthread_mutex_t *);

WRAP_FUNC2(cond_init, pthread_cond_t *, pthread_condattr_t *);
// WRAP_FUNC1(cond_signal, pthread_cond_t *);
WRAP_FUNC1(cond_broadcast, pthread_cond_t *);
WRAP_FUNC2(cond_wait, pthread_cond_t *, pthread_mutex_t *);

#ifdef USE_JOIN
WRAP_FUNC2(join, pthread_t, void **);
#endif

#define atomic_inc(v) __sync_fetch_and_add(&(v), 1)
#define atomic_dec(v) __sync_fetch_and_sub(&(v), 1)
#define atomic_cas(v, e, a) __sync_bool_compare_and_swap(&(v), (e), (a))
#define atomic_swap(v, e) __sync_lock_test_and_set(&(v), (e))
#define atomic_pause() {__asm__ __volatile__("pause");}

typedef struct tq_struct {
    int num;
} tq_t;

struct tq_set {
    const char *name;
    tq_t *(*create)(size_t);
    void (*free)(tq_t *);
    int (*enq)(tq_t *, ptask_t *);
    ptask_t *(*deq)(tq_t *);
    ptask_t *(*steal)(tq_t *);
    void (*wait)(tq_t *);
};

#define DEFINE_TQ_SET(name) struct tq_set TQ_##name = { \
    #name, \
    tq_##name##_create, \
    tq_##name##_free, \
    tq_##name##_enq, \
    tq_##name##_deq, \
    tq_##name##_steal, \
    tq_##name##_wait, \
};

