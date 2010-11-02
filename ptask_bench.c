#include <stdio.h>
#include <stdlib.h>
#include "ptask.h"

#define SHOW_PROGRESS 0

#define QNUM 1

#define MAX  1000000
#define TNUM 10000
#define COST 1000
#define LNUM (MAX/COST)



static void *
loop(void *ptr)
{
    volatile int n = *(int *)ptr;
    int i;

    for(i=0; i<n; i++) {
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int i, j;
    int qn = QNUM;
    int cost = COST;
    srand(123);

    if (argc > 1) {
	qn = atoi(argv[1]);
    }
    if (argc > 2) {
	cost = atoi(argv[2]);
    }

    fprintf(stderr, "qn: %d, cost: %d, tnum: %d, lnum: %d\n", qn, cost, TNUM, LNUM);

    if (qn > 0) {
	ptask_queue_t **qs = (ptask_queue_t **)malloc(sizeof(ptask_queue_t *) * qn);
	ptask_t *ts[TNUM];
	ptask_setup();

	for (i=0; i<qn; i++) {
	    qs[i] = ptask_queue_create(2000);
	}

	for (j=0; j<LNUM; j++) {
	    if (1) {
		for (i=0; i<TNUM; i++) {
		    ptask_t *task;
		    ts[i] = task = ptask_create(loop, &cost);
		    ptask_dispatch(task);
		}

		for (i=0; i<TNUM; i++) {
		    ptask_wait(ts[i]);
		    ptask_destruct(ts[i]);
		}
	    }
	    if (SHOW_PROGRESS && (j % (LNUM/100)) == 0) fprintf(stderr, ".");
	}

	for (i=0; i<qn; i++) {
	    ptask_queue_destruct(qs[i]);
	}
	free(qs);
    }
    else {
	for (j=0; j<LNUM; j++) {
	    for (i=0; i<TNUM; i++) {
		loop(&cost);
	    }
	    if (SHOW_PROGRESS && (j % (LNUM/100)) == 0) fprintf(stderr, ".");
	}
    }
    return 0;
}
