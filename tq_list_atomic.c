
#ifndef QDBG
#define QDBG 1
#endif

struct tq_list_atomic {
    tq_t basic;
    ptask_t * volatile head, * volatile tail;

    /*
     * head -> <old>
     *           v
     *          ...
     *           v
     * tail -> <new>
     */
};

static tq_t *
tq_list_atomic_create(size_t capa)
{
    struct tq_list_atomic *queue = xmalloc(sizeof(struct tq_list_atomic));
    memset(queue, 0, sizeof(struct tq_list_atomic));
    queue->head = queue->tail = 0;
    return &queue->basic;
}

static void
tq_list_atomic_free(tq_t *tq)
{
    struct tq_list_atomic *queue = (struct tq_list_atomic *)tq;
    xfree(queue);
}

static int
tq_list_atomic_enq(tq_t *tq, ptask_t *task)
{
    struct tq_list_atomic *queue = (struct tq_list_atomic *)tq;
    ptask_t *tail;

    //fprintf(stderr, "enq: %p\n", task);

    if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

    task->next = 0;
    tail = atomic_swap(queue->tail, task);

    if (tail == 0) {
	queue->head = task;
	/* TODO: wakeup */
    }
    else {
	tail->next = task;
    }

    return 1;
}

static ptask_t *
tq_list_atomic_deq(tq_t *tq)
{
    struct tq_list_atomic *queue = (struct tq_list_atomic *)tq;
    ptask_t * const MEDIATOR = ((void *)~0UL);

  retry:
    if (queue->head && queue->head != MEDIATOR) {
	volatile ptask_t *task;

	task = atomic_swap(queue->head, MEDIATOR);

	if (task == MEDIATOR) {
	    // bug("...");
	    atomic_pause();
	    goto retry;
	}
	else if (task == 0) {
	    // fprintf(stderr, "empty\n");
	    atomic_cas(queue->head, MEDIATOR, 0);
	    goto deq_fail;
	}
	else {
	    ptask_t *next = task->next;

	    if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

	    if (next == 0) {
		queue->head = 0;
		// fprintf(stderr, "next is zero\n");

		if (atomic_cas(queue->tail, task, 0) != 0) {
		    // fprintf(stderr, "zero-clear: %p\n", task);
		    return (ptask_t *)task;
		}
		while ((next = task->next) == 0) {
		    /* short */
		    atomic_pause();
		}
		// fprintf(stderr, "new-head: %p\n", task);
	    }
	    queue->head = (ptask_t *)next;
	    //fprintf(stderr, "return: %p, next: %p\n", task, next);
	    return (ptask_t *)task;
	}
    }

  deq_fail:
    if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, empty\n", queue, queue->basic.num);
    return 0;
}

static ptask_t *
tq_list_atomic_steal(tq_t *tq)
{
    return tq_list_atomic_deq(tq);
}

static void
tq_list_atomic_wait(tq_t *tq)
{
    struct tq_list_atomic *queue = (struct tq_list_atomic *)tq;
    if (QDBG) fprintf(stderr, "wait - q: %p, num: %d\n", queue, queue->basic.num);

    if (queue->head == 0) {
	sched_yield();
    }
}

DEFINE_TQ_SET(list_atomic);

