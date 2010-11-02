#define USE_JOIN 1

#define PTASK_PROFILE 0
#define PTASK_PROFILE_INC(var) 
#define PTASK_PROFILE_SET_MAX(var, x)

#include "common.h"
#include "benchutils.h"

struct ptask_struct {
    struct ptask_struct *next;
    int status;
};

#define PROGRESS_REPORT 0

#define ptask_status_name(x) ""

static __thread int ptask_worker_id;
static int last_ptask_worker_id = 1;

#define QDBG 0

#include "tq_list_lock.c"
#include "tq_list_atomic.c"
#include "tq_list_nosync.c"
#include "tq_array_lock.c"
#include "tq_array_atomic.c"
#include "tq_array_nosync.c"

struct tq_set *tq = &TQ_list_atomic;
//struct tq_set *tq = &TQ_list_lock;
//struct tq_set *tq = &TQ_list_nosync;
//struct tq_set *tq = &TQ_array_lock;
//struct tq_set *tq = &TQ_array_atomic;
//struct tq_set *tq = &TQ_array_nosync;

#define MAX  100000000
// #define MAX  24000
#define TNUM 1
#define LNUM (MAX/TNUM)
#define WNUM 4

#if TQ_DEBUG
ptask_t *records[MAX+100];
int gen;
int con;
#endif

#define USE_QMEM 0

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

#if TQ_DEBUG
    records[gen++] = task;
#endif
    return task;
}

static void
free_task(ptask_t *task)
{
    // fprintf(stderr, "x");
#if USE_QMEM
    qmem_free(qm, task);
#else
    free(task);
#endif
}

static ptask_t finish_task;
int wnum[100];

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

#if TQ_DEBUG
	if (records[con++] != task) bug("...");
#endif
	if (task == &finish_task) {
	    if (QDBG) fprintf(stderr, "wn: %2d, finish\n", ptask_worker_id);
	    return 0;
	}
	volatile int n = 100; int i;
	for (i=0; i<n; i++);
	
	if (QDBG) fprintf(stderr, "wn: %2d, task: %p free task.\n", ptask_worker_id, task);
	free_task(task);

	wnum[ptask_worker_id]++;
    }
}

static void
test(int wn)
{
    tq_t *q = tq->create(10000);
    int i, j;
    pthread_t *tid;

#if USE_QMEM
    qm = qmem_create(4096 * 16);
#endif

    if (wn > 0) {
	tid = malloc(sizeof(pthread_t) * wn);

	for (i=0; i<wn; i++) {
	    pthread_create(&tid[i], 0, consumer, q);
	}

	for (j=0; j<LNUM; j++) {
	    for (i=0; i<TNUM; i++) {
		ptask_t *task = make_task();

		if (QDBG) fprintf(stderr, "wn: %2d, task: %p enq task.\n", ptask_worker_id, task);

		while (tq->enq(q, task) == 0) {
		    /* sched yield */
		}
	    }
	    if (PROGRESS_REPORT) if (j % (LNUM/80) == 0) fprintf(stderr, ".");
	}

	if (PROGRESS_REPORT) if ((j % (LNUM/80 ? LNUM/80 : 1)) == 0) fprintf(stderr, ".");

	for (i=0; i<wn; i++) {
#if TQ_DEBUG
	    records[gen++] = &finish_task;
#endif
	    tq->enq(q, &finish_task);
	}
	for (i=0; i<wn; i++) {
	    join(tid[i], 0);
	}

	tq->free(q);
	free(tid);
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
	    if (PROGRESS_REPORT) if ((j % (LNUM/80 ? LNUM/80 : 1)) == 0) fprintf(stderr, ".");
	}
    }

    // fprintf(stderr, "finish\n");
#if USE_QMEM
    qmem_print_status(qm);
    qmem_destruct(qm);
#endif
}

struct tq_set *tqs[] = {
    &TQ_list_lock,
    &TQ_list_atomic,
    // &TQ_list_nosync,
    //&TQ_array_lock,
    //&TQ_array_atomic,
    // TQ_array_nosync,
};

int
main(int argc, char **argv)
{
    int i, j;
    for (i=0; i<sizeof(tqs)/sizeof(void *); i++) {
	tq = tqs[i];

	for (j=0; j<8; j++) {
	    benchmark_t *bm;
	    char name[100];
	    fprintf(stderr, "wn: %d, tq: %s\n", j, tq->name);
	    sprintf(name, "%s#%d", tq->name, j);
	    bm = benchmark_start(name);
	    test(j);
	    benchmark_finish(bm);
	    exit(1);
	}
    }
    return 0;
}
