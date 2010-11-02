/*
 * ptask: parallel task scheduler
 *
 * Created by Koichi Sasada
 */

#define PTASK_PROFILE 1

#include "common.h"
#include "ptask.h"
#define Q_LINKEDLIST 1

#if PTASK_PROFILE
#define PTASK_PROFILE_INC(var)     ((task_workers[ptask_worker_id].var)++)
#define PTASK_PROFILE_SET_MAX(var, x)  ((task_workers[ptask_worker_id].var) = \
					(task_workers[ptask_worker_id].var) < (x) ? (x) : \
					(task_workers[ptask_worker_id].var))
#else
#define PTASK_PROFILE_INC(var)
#define PTASK_PROFILE_SET_MAX(var, x)
#endif

static __thread int ptask_worker_id = 0;

#define PTASK_DEBUG 0

#if PTASK_DEBUG
static pthread_mutex_t debug_lock = PTHREAD_MUTEX_INITIALIZER;

static void
ptask_debug_body(const char *file, int line, const char *msg, void *ptr)
{
    mutex_lock(&debug_lock);
    fprintf(stderr, "ptask_debug (w: %d) - ", ptask_worker_id);
    fprintf(stderr, msg, ptr);
    mutex_unlock(&debug_lock);
}
#define ptask_debug(level, msg, param) if (level >= PTASK_DEBUG) ptask_debug_body(__FILE__, __LINE__, msg, (void *)param)

#else
#define ptask_debug(level, msg, param) if (0) printf(msg, param)
#endif

#define ptask_debug2(level, msg, param) fprintf(stderr, msg, param);

enum task_status {
    TASK_WAIT= 1,
    TASK_WAIT_Q,
    TASK_RUN,
    TASK_RUN_Q,
    TASK_FINISH,
    TASK_FINISH_Q,
    TASK_FREE_Q,
};

struct ptask_struct {
#if Q_LINKEDLIST
    ptask_t * volatile next;
#endif
    ptask_queue_t *chained_queue;
    volatile enum task_status status;
    void *(*func)(void *ptr);
    void *argv;
    void *result;
};

typedef struct ptask_worker_struct {
    int id;
    int exec_count;
    ptask_queue_t *queue;
    pthread_t tid;

#if PTASK_PROFILE
    int max_num;

    int enq_num;
    int enq_miss;

    int deq_num;
    int deq_miss;

    int wait_num;

    int acquire_miss;
    int steal_num;
    int steal_miss;
#endif
} ptask_worker_t;

struct ptask_queue_struct {
    ptask_queue_group_t *group;
    ptask_worker_t *worker;
    tq_t *tq;
};

#define QUEUE_GROUP_MAX 0x100
struct ptask_queue_group_struct {
    ptask_queue_t *queues[QUEUE_GROUP_MAX]; /* TODO: should be variable list */
    int num;
    int roundrobin;
    int wait;
};

static const char *ptask_status_name(ptask_t *task);

static int task_worker_count;
static ptask_worker_t task_workers[QUEUE_GROUP_MAX + 1];
static ptask_queue_group_t ptask_default_queue_group;

#define QDBG 0

#if 1
#include "tq_list_lock.c"
#include "tq_list_atomic.c"
#include "tq_array_lock.c"
#include "tq_array_atomic.c"

//const struct tq_set *tq = &TQ_array_lock;
//const struct tq_set *tq = &TQ_array_atomic;
//const struct tq_set *tq = &TQ_list_lock;
const struct tq_set * const tq = &TQ_list_atomic;

#define tq_name   tq->name
#define tq_create tq->create
#define tq_free   tq->free
#define tq_enq    tq->enq
#define tq_deq    tq->deq
#define tq_steal  tq->steal
#define tq_wait   tq->wait
#else
#include "tq_list_lock.c"
#define tq_name   "list_loc"
#define tq_create tq_list_lock_create
#define tq_free   tq_list_lock_free
#define tq_enq    tq_list_lock_enq
#define tq_deq    tq_list_lock_deq
#define tq_steal  tq_list_lock_steal
#define tq_wait   tq_list_lock_wait
#endif

/* task control */

static ptask_queue_t *
tqg_next(ptask_queue_group_t *group)
{
    ptask_queue_t *queue;

    if (group->num == 0) {
	bug("next_queue: no queues.");
    }

    queue = group->queues[group->roundrobin];

    if (queue->tq->num < 5) {
	return queue;
    }
    else {
	group->roundrobin = (group->roundrobin + 1) % group->num;
	return group->queues[group->roundrobin];
    }

#if 0
    int next;
    /* round robin */
    if (group->wait > 0) {
	group->wait--;
	next = group->roundrobin;
    }

    else {
	group->wait = 0;
	group->roundrobin = (group->roundrobin + 1) % group->num;
    }
    // fprintf(stderr, "group->roundrobin: %d, wait: %d\n", group->roundrobin, group->wait);
    return group->queues[group->roundrobin];
#endif
}

static ptask_queue_t *
tqg_max_queue(ptask_queue_group_t *group)
{
    int i, qn = -1, num = 0, n = group->num;

    for (i=0; i<n; i++) {
	if (group->queues[i]->tq && group->queues[i]->tq->num > num) {
	    qn = i;
	    num = group->queues[i]->tq->num;
	}
    }

    if (qn == -1) {
	return 0;
    }
    else {
	return group->queues[qn];;
    }
}

static ptask_t *ptask_queue_deq(ptask_queue_t *queue);

static ptask_t *
ptask_group_steal(ptask_queue_group_t *group)
{
    ptask_t *task;
    ptask_queue_t *queue = tqg_max_queue(group);

    if (queue && (task = ptask_queue_deq(queue)) != 0) {
	PTASK_PROFILE_INC(steal_num);
	return task;
    }
    else {
	PTASK_PROFILE_INC(steal_miss);
	return 0;
    }
}

/* task management */

static const char *
status_name(enum task_status status)
{
    switch (status) {
#define E(name) case TASK_##name: return #name;
	E(WAIT);
	E(WAIT_Q);
	E(RUN);
	E(RUN_Q);
	E(FINISH);
	E(FINISH_Q);
	E(FREE_Q);
#undef E
      default:
	fprintf(stderr, "status: %d\n", status);
	bug("unrechable");
	return 0;
    }
}

static const char *
ptask_status_name(ptask_t *task)
{
    return status_name(task->status);
}

static int
ptask_set_status(ptask_t *task, enum task_status expect_status, enum task_status status)
{
    if (__sync_bool_compare_and_swap(&task->status, expect_status, status)) {
	// fprintf(stderr, "wn: %2d task (%p): %s -> %s\n", ptask_worker_id, task, status_name(expect_status), status_name(status));
	return 1;
    }
    else {
	return 0;
    }
}

static void ptask_free(ptask_t *task);

static int
ptask_acquire(ptask_t *task)
{
    enum task_status status = task->status;

    if (status == TASK_WAIT) {
	if (ptask_set_status(task, status, TASK_RUN)) {
	    return 1;
	}
    }
    else if (status == TASK_WAIT_Q) {
	if (ptask_set_status(task, status, TASK_RUN_Q)) {
	    return 1;
	}
    }
    return 0;
}

static int
ptask_release(ptask_t *task)
{
    /* ptask_queue_t *queue = task->chained_queue; */
    enum task_status finish_status;
    task->chained_queue = 0;

    while (1) {
	enum task_status current_status = task->status;

	switch (current_status) {
	  case TASK_RUN:
	    finish_status = TASK_FINISH;
	    break;
	  case TASK_RUN_Q:
	    finish_status = TASK_FINISH_Q;
	    break;
	  case TASK_FREE_Q:
	    ptask_free(task);
	    return 0;
	  case TASK_FINISH:
	  case TASK_FINISH_Q:
	  case TASK_WAIT:
	  case TASK_WAIT_Q:
	    bug("unreachable");
	    break;
	}

	if (ptask_set_status(task, current_status, finish_status)) {
	    break;
	}
    }
    return 1;
}

/* task should be acquired */
static void
ptask_execute(ptask_t *task)
{
    enum task_status status = task->status;

    if (status != TASK_RUN && status != TASK_RUN_Q) {
	fprintf(stderr, "status: %s\n", status_name(status));
	bug("task_execute: task is not acquired");
    }

    ptask_debug(1, "task_execute: %p\n", task);
    task->result = task->func(task->argv);
    ptask_release(task);
    task_workers[ptask_worker_id].exec_count++;
}

static int
ptask_queue_enq(ptask_queue_t *queue, ptask_t *task)
{
  retry:
    switch (task->status) {
      case TASK_WAIT:
	if (ptask_set_status(task, TASK_WAIT, TASK_WAIT_Q)) break;
	goto retry;
      case TASK_RUN:
	if (ptask_set_status(task, TASK_RUN, TASK_RUN_Q)) break;
	goto retry;
      case TASK_FINISH:
	/* do not enq */
	return 1;

      case TASK_FINISH_Q:
      case TASK_WAIT_Q:
      case TASK_RUN_Q:
      case TASK_FREE_Q:
      default:
	fprintf(stderr, "task %p: %s\n", task, ptask_status_name(task));
	bug("unreachable");
    }

    task->chained_queue = queue;
    while (tq_enq(queue->tq, task) == 0) {
	PTASK_PROFILE_INC(enq_miss);
	sched_yield();
    }
    PTASK_PROFILE_INC(enq_num);
    return 1;
}

static ptask_t *
ptask_queue_deq(ptask_queue_t *queue)
{
    while (1) {
	ptask_t *task = tq_deq(queue->tq);

	if (task) {
	  retry:
	    PTASK_PROFILE_INC(deq_num);

	    switch(task->status) {
	      case TASK_RUN_Q:
		if (ptask_set_status(task, TASK_RUN_Q, TASK_RUN)) break;
		goto retry;

	      case TASK_WAIT_Q:
		if (ptask_set_status(task, TASK_WAIT_Q, TASK_RUN)) return task;
		goto retry;

	      case TASK_FINISH_Q:
		if (ptask_set_status(task, TASK_FINISH_Q, TASK_FINISH)) break;
		goto retry;

	      case TASK_FREE_Q:
		ptask_debug(1, "deq free (%p)\n", task);
		ptask_free(task);
		break;

	      case TASK_WAIT:
	      case TASK_FINISH:
	      case TASK_RUN:
	      default:
		fprintf(stderr, "ptask_queue_deq - task %p: %s\n", task, ptask_status_name(task));
		bug("unreachable");
		break;
	    }
	    PTASK_PROFILE_INC(acquire_miss);
	}
	else {
	    PTASK_PROFILE_INC(deq_miss);
	    return 0;
	}
    }
}

/* called by master. */
void
ptask_wait(ptask_t *task)
{
    // fprintf(stderr, "wn: task (%p): %s\n", task, ptask_status_name(task));

    while (1) {
	switch (task->status) {
	  case TASK_WAIT_Q:
	  case TASK_WAIT: {
	      if (ptask_acquire(task)) {
		  ptask_execute(task);
		  return;
	      }
	      break;
	  }
	  case TASK_RUN:
	  case TASK_RUN_Q: {
	      /* TODO: wait.  now, simply polling. */
	      break;
	  }
	  case TASK_FINISH:
	  case TASK_FINISH_Q:
	    return;
	  case TASK_FREE_Q:
	    bug("ptask_wait: status should not be TASK_FREE");
	    break;
	  default:
	    bug("unreachable");
	    break;
	}

	{
	    ptask_queue_t *next_queue = task->chained_queue;
	    ptask_t *t;

	    if (next_queue && (t = ptask_queue_deq(next_queue)) != 0) {
		ptask_execute(t);
	    }
	    else if (next_queue && (t = ptask_group_steal(next_queue->group)) != 0) {
		ptask_execute(t);
	    }
	    else {
		sched_yield();
	    }
	}
    }
    bug("unreachable");
}

static void *
ptask_worker_func(void *ptr)
{
    ptask_worker_t *worker = (ptask_worker_t *)ptr;

    ptask_worker_id = worker->id;
    ptask_queue_t *queue = worker->queue;

    while (1) {
	ptask_t *task = ptask_queue_deq(queue);
	if (task) {
	    ptask_execute(task);
	}
	else {
	    if ((task = ptask_group_steal(queue->group)) != 0) {
		ptask_execute(task);
	    }
	    else {
		tq_wait(queue->tq);
	    }
	}
    }

    return 0;
}

static ptask_worker_t *
ptask_worker_create(ptask_queue_t *queue)
{
    int r;
    int i = ++task_worker_count;
    ptask_worker_t *worker = &task_workers[i];

    worker->id = i;
    worker->queue = queue;

    r = pthread_create(&worker->tid, 0, ptask_worker_func, worker);
    if (r != 0) {
	fprintf(stderr, "ptask_worker_create: pthread_create() returns non-zero (%d)", r);
	exit(1);
    }

    return worker;
}

static void
ptask_profile(void)
{
#if PTASK_PROFILE
    int i, t = 0;
#endif

    fprintf(stderr,  "[task profile]\n");
    fprintf(stderr,  "worker num = %d, tq: %s\n", ptask_default_queue_group.num, tq_name);

#if PTASK_PROFILE
    if (ptask_default_queue_group.num > 0) {
	fprintf(stderr, "id\texec\tmax_num\tenq_num\tdeq_num\tacq_mis\tenq_mis\tdeq_mis\twai_num\tste_num\tste_mis\n");
	for (i=0; i<ptask_default_queue_group.num + 1; i++) {
	    t += task_workers[i].exec_count;
	    fprintf(stderr,  "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
		    task_workers[i].id,
		    task_workers[i].exec_count,
		    task_workers[i].max_num,
		    task_workers[i].enq_num,
		    task_workers[i].deq_num,
		    task_workers[i].acquire_miss,
		    task_workers[i].enq_miss,
		    task_workers[i].deq_miss,
		    task_workers[i].wait_num,
		    task_workers[i].steal_num,
		    task_workers[i].steal_miss
		    );
	}
	fprintf(stderr,  "total exec = %d\n", t);
    }
#endif
}

static void
sig_exit(int sig)
{
    exit(1);
}

int
ptask_setup(void)
{
    /* setup default */
    task_workers[0].queue = ptask_queue_create(0);

    /* setup profile output */
    atexit(ptask_profile);
    signal(SIGINT, sig_exit);
    return 1;
}

static void
ptask_queue_group_add_default(ptask_queue_t *queue)
{
    queue->group = &ptask_default_queue_group;
    ptask_default_queue_group.queues[ptask_default_queue_group.num++] = queue;
}

/**
  Create a queue.

  capa: capacity of queue.
 */
ptask_queue_t *
ptask_queue_create(int capa)
{
    ptask_queue_t *queue = (ptask_queue_t *)xmalloc(sizeof(ptask_queue_t));
    queue->tq = tq_create(capa);

    if (capa > 0) {
	ptask_queue_group_add_default(queue);
	ptask_worker_create(queue);
    }
    return queue;
}

void
ptask_queue_destruct(ptask_queue_t *queue)
{
    tq_free(queue->tq);
}

ptask_t *
ptask_create(void *(*func)(void *args), void *argv)
{
    ptask_t *task = xmalloc(sizeof(ptask_t));
    task->status = TASK_WAIT;
    task->chained_queue = 0;
    task->func = func;
    task->argv = argv;
    return task;
}

static void
ptask_free(ptask_t *task)
{
    // fprintf(stderr, "wn: %2d task: %p", ptask_worker_id, task);
    ptask_debug(1, "ptask_free: %p\n", task);
    xfree(task);
}

void
ptask_destruct(ptask_t *task)
{
    while (1) {
	enum task_status status = task->status;
	ptask_debug(1, "ptask_destruct: %p\n", task);

	switch (status) {
	  case TASK_WAIT_Q:
	  case TASK_FINISH_Q:
	    if (ptask_set_status(task, status, TASK_FREE_Q)) return;
	    break;

	  case TASK_RUN:
	  case TASK_RUN_Q:
	    ptask_wait(task);
	    break;

	  case TASK_WAIT:
	  case TASK_FINISH:
	    ptask_free(task);
	    return;

	  case TASK_FREE_Q:
	  default:
	    bug("ptask_destruct: unreachable");
	    break;
	}
    }
}

void
ptask_dispatch_queue(ptask_t *task, ptask_queue_t *queue)
{
    if (ptask_queue_enq(queue, task) == 0) {
	bug("ptask_dispatch_queue: unreachable");
#if 0
	LOCK(&queue->cond_lock, {
	    if (queue->wait_thread_num > 0 && queue->num > 0) {
		fprintf(stderr, "wait_thread_num: %d\n", queue->wait_thread_num);
		bug("ptask_dispatch_queue: deadlock");
	    }
	});
#endif
	sched_yield();
    }
}

void
ptask_dispatch_group(ptask_t *task, ptask_queue_group_t *group)
{
    ptask_dispatch_queue(task, tqg_next(group));
}

void
ptask_dispatch(ptask_t *task)
{
    ptask_dispatch_group(task, &ptask_default_queue_group);
}

size_t
ptask_memsize(ptask_t *task)
{
    return sizeof(ptask_t);
}

static void*
empty(void *ptr)
{
    return 0;
}

void
ptask_qtest(int n)
{
    ptask_t *task = ptask_create(empty, 0);
    int i;

    ptask_queue_create(10000);
    // ptask_wait(task);

    for (i=0; i<n; i++) {
	task->status = TASK_WAIT;
	ptask_dispatch(task);
	ptask_wait(task);
    }
}
