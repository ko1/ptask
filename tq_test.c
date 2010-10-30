#define USE_JOIN 1
#include "common.h"

struct ptask_struct {
    struct ptask_struct *next;
    int status;
};

#define PROGRESS_REPORT 1

#define ptask_status_name(x) ""

static __thread int ptask_worker_id;
static int last_ptask_worker_id = 1;

#define QDBG 0

#include "tq_list_lock.c"
#include "tq_list_nosync.c"
#include "tq_array_lock.c"
#include "tq_array_atomic.c"
#include "tq_array_nosync.c"

// struct tq_set *tq = &TQ_list;
//struct tq_set *tq = &TQ_list_nosync;
//struct tq_set *tq = &TQ_array_lock;
//struct tq_set *tq = &TQ_array_atomic;
struct tq_set *tq = &TQ_array_nosync;

#define MAX  100000000
#define TNUM 500
#define LNUM (MAX/TNUM)
#define WNUM 4

#define USE_QMEM 1

#if USE_QMEM
#include "qmem.h"
qmem_t *qm;
#endif

static ptask_t *
make_task(void)
{
    ptask_t *task;
#if USE_QMEM
    task = qmem_alloc(qm, sizeof(ptask_t));
#else
    task = malloc(sizeof(ptask_t));
#endif
    task->status = 0;
    return task;
}

static void
free_task(ptask_t *task)
{
#if USE_QMEM
    qmem_free(qm, task);
#else
    free(task);
#endif
}

static ptask_t finish_task;

static void *
consumer(void *ptr)
{
    tq_t *q = (tq_t *)ptr;
    ptask_t *task;

    ptask_worker_id = atomic_inc(last_ptask_worker_id);

    if (QDBG) fprintf(stderr, "wn: %2d, start\n", ptask_worker_id);

    while (1) {
	while ((task = tq->deq(q)) == 0) {
	    tq->wait(q);
	}
	if (task == &finish_task) {
	    if (QDBG) fprintf(stderr, "wn: %2d, finish\n", ptask_worker_id);
	    return 0;
	}
	if (QDBG) fprintf(stderr, "wn: %2d, task: %p\n", ptask_worker_id, task);
	free_task(task);
    }
}

int
main(int argc, char **argv)
{
    tq_t *q = tq->create(10000);
    int i, j, wn = WNUM;
    pthread_t *tid;

#if USE_QMEM
    qm = qmem_create(4096 * 16);
#endif

    if (argc > 1) {
	wn = atoi(argv[1]);
    }
    fprintf(stderr, "wn: %d\n", wn);

    if (wn > 0) {
	tid = malloc(sizeof(pthread_t) * wn);

	for (i=0; i<wn; i++) {
	    pthread_create(&tid[i], 0, consumer, q);
	}

	for (j=0; j<LNUM; j++) {
	    for (i=0; i<TNUM; i++) {
		ptask_t *task = make_task();
		while (tq->enq(q, task) == 0) {
		    /* sched yield */
		}
	    }
	    if (PROGRESS_REPORT) if (j % (LNUM/80) == 0) fprintf(stderr, ".");
	}

	if (PROGRESS_REPORT) if (j % (LNUM/80) == 0) fprintf(stderr, "\nwait\n");

	for (i=0; i<wn; i++) {
	    tq->enq(q, &finish_task);
	}
	for (i=0; i<wn; i++) {
	    join(tid[i], 0);
	}
    }
    else {
	int i, j;
	for (j=0; j<LNUM; j++) {
	    for (i=0; i<TNUM; i++) {
		ptask_t *task = make_task();

		if (1) {
		    tq->enq(q, task);
		    task = tq->deq(q);
		}

		free_task(task);
	    }
	    if (PROGRESS_REPORT) if (j % (LNUM/80) == 0) fprintf(stderr, ".");
	}
    }

    fprintf(stderr, "finish\n");
#if USE_QMEM
    qmem_print_status(qm);
    qmem_destruct(qm);
#endif
    return 0;
}
