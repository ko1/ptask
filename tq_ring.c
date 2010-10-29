
#ifndef QDBG
#define QDBG 1
#endif

#define Q_LINKEDLIST 1

struct tq_ring {
    tq_t basic;

    ptask_t * volatile tail;
    ptask_t head;

    volatile int wait_thread_num;
    pthread_cond_t cond;
    pthread_mutex_t cond_lock;
};

static tq_t *
tq_ring_create(size_t capa)
{
    struct tq_ring *queue = xmalloc(sizeof(struct tq_ring));
    memset(queue, 0, sizeof(struct tq_ring));

    mutex_init(&queue->cond_lock, 0);
    cond_init(&queue->cond, 0);
    queue->tail = queue->head.next = &queue->head;

    return &queue->basic;
}

static void
tq_ring_free(tq_t *queue)
{
    xfree(queue);
}

static void
tq_ring_show(const char *stage, struct tq_ring *queue, ptask_t *target_task)
{
    if (*stage == '*') {
	ptask_t *task = queue->head.next;
	fprintf(stderr, "* <%s> queue (%p)\n", stage, target_task);

	while (task != &queue->head) {
	    fprintf(stderr, "- %p  ", task);
	    if (&queue->head == task) {
		fprintf(stderr, "<-head ");
	    }
	    if (queue->tail == task) {
		fprintf(stderr, "<-tail ");
	    }
	    if (target_task == task) {
		fprintf(stderr, "<-target ");
	    }
	    fprintf(stderr, "\n");
	    task = task->next;
	}
    }
}

static int
tq_ring_enq(tq_t *tq, ptask_t *task)
{
    struct tq_ring *queue = (struct tq_ring *)tq;

    if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

    task->next = &queue->head;

    while (1) {
	ptask_t *tail = queue->tail;

	if (atomic_cas(tail->next, &queue->head, task)) {
	    tq_ring_show("E0", queue, task);
	    if (atomic_cas(queue->tail, tail, task)) {
		/* success */
		tq_ring_show("E1", queue, task);
		break;
	    }
	}
	else {
	    atomic_cas(queue->tail, tail, tail->next);
	    tq_ring_show("E2", queue, task);
	}
    }

    atomic_inc(queue->basic.num);

    /* profiling */
    queue->basic.enq_num++;
    if (queue->basic.max_num < queue->basic.num) {
	queue->basic.max_num = queue->basic.num;
    }

    // tq_ring_show(queue, task);
    return 1;
}

static ptask_t *
tq_ring_deq(tq_t *tq)
{
    struct tq_ring *queue = (struct tq_ring *)tq;
    ptask_t *task;

  retry:
    tq_ring_show("D0", queue, 0);
    task = queue->head.next;

    if (task == &queue->head) {
	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, empty\n", queue, queue->basic.num);
	queue->basic.deq_miss++;
	return 0;
    }
    else {
	ptask_t *next_task = task->next;
	tq_ring_show("D1", queue, task);

	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

	if (task == &queue->head) bug("assert");

	if (atomic_cas(queue->head.next, task, next_task) == 0) {
	    goto retry;
	}

	atomic_dec(queue->basic.num);
    }

    if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);
    return task;
}

static ptask_t *
tq_ring_steal(tq_t *tq)
{
    struct tq_ring *queue = (struct tq_ring *)tq;
    ptask_t *task = 0;
    queue = 0;
    return task;
}

static void
tq_ring_wait(tq_t *tq)
{
    struct tq_ring *queue = (struct tq_ring *)tq;
    if (QDBG) fprintf(stderr, "wait - q: %p, num: %d\n", queue, queue->basic.num);
    sched_yield();
}

DEFINE_TQ_SET(ring);

