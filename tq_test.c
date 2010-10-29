
#include "common.h"

struct ptask_struct {
    struct ptask_struct *next;
};

#define ptask_status_name(x) ""

#include "tq_pthread.c"
#include "tq_array.c"
#include "tq_list.c"

//struct tq_set *tq = &TQ_list;
struct tq_set *tq = &TQ_array;
//struct tq_set *tq = &TQ_pthread;

#define MAX  100000
#define TNUM 500
#define LNUM (MAX/TNUM)

static void *
consumer(void *ptr)
{
    tq_t *q = (tq_t *)ptr;
    int i, j;
    for (j=0; j<LNUM; j++) {
	for (i=0; i<TNUM; i++) {
	    while (tq->deq(q) == 0) {
		/* */
	    }
	    // fprintf(stderr, ".");
	}
    }
    return 0;
}

int
main(int argc, char **argv)
{
    tq_t *q = tq->create(100);
    ptask_t tasks[MAX];
    int i, j;
    pthread_t tid;

    pthread_create(&tid, 0, consumer, q);

    if (0) {
	for (i=0; i<MAX; i++) {
#if 0
	    volatile int j;
	    for (j=0; j<1000; j++) {
	    }
#elif 0
	    fprintf(stderr, ".");
#endif
	    while(tq->enq(q, &tasks[i]) == 0) {
	    }
	}
	pthread_join(tid, 0);
	return 0;
    }

    for (j=0; j<LNUM; j++) {
	for (i=0; i<TNUM; i++) {
	    while (tq->enq(q, &tasks[LNUM * j + i]) == 0) {
		sched_yield();
	    }
	}
	fprintf(stderr, ".");
    }

    join(tid, 0);
    printf("finish\n");
    return 0;
}
