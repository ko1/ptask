#include <stdio.h>
#include <stdlib.h>
#include "ptask.h"

#define TNUM 10000
#define LNUM 1000
#define QNUM 1
static void *
empty(void *ptr)
{
    return 0;
}

static void *
loop(void *ptr)
{
    volatile int n = 1000;
    volatile int i, j;

    for(i=0; i<n; i++) {
	for(j=0; j<n; j++) {
	}
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int i, j;
    int qn = QNUM;
    void *(* volatile f)(void *) = empty;

    if (argc > 1) {
	qn = atoi(argv[1]);
	if (qn < 0) qn = 0;
	printf("qn: %d\n", qn);
    }
    if (argc > 2) {
	switch(argv[2][0]) {
	  case 'l': f = loop; break;
	  case 'e': f = empty; break;
	  default:
	    break;
	}
    }

    if (qn > 0) {
	ptask_queue_t **qs = (ptask_queue_t **)malloc(sizeof(ptask_queue_t *) * qn);
	ptask_t *ts[TNUM];

	ptask_setup();

	for (i=0; i<qn; i++) {
	    qs[i] = ptask_queue_create(1000);
	}

	for (j=0; j<LNUM; j++) {
	    for (i=0; i<TNUM; i++) {
		ptask_t *task;
		ts[i] = task = ptask_create(f, 0);

		if ((i % 7) == 0) {
		    ptask_destruct(task);
		    ts[i] = 0;
		}
		else {
		    ptask_dispatch(task);

		    if ((i % 10) == 0) {
			// fprintf(stderr, "destruct: %p (%d)\n", task, i);
			ptask_destruct(task);
			ts[i] = 0;
		    }
		}
	    }

	    for (i=0; i<TNUM; i++) {
		if (ts[i]) {
		    ptask_wait(ts[i]);
		    ptask_destruct(ts[i]);
		}
	    }
	    printf("."); fflush(stdout);
	}

	for (i=0; i<qn; i++) {
	    ptask_queue_destruct(qs[i]);
	}
	free(qs);
    }
    else {
	for (j=0; j<LNUM; j++) {
	    for (i=0; i<TNUM; i++) {
		f(0);
	    }
	}
    }
    return 0;
}
