
#ifndef QDBG
#define QDBG 0
#endif

#define Q_LINKEDlist_nosync 1

struct tq_list_nosync {
    tq_t basic;

    ptask_t *head, *tail;
    /*
     * head -> <old>
     *           v
     *          ...
     *           v
     * tail -> <new>
     */
};

static tq_t *
tq_list_nosync_create(size_t capa)
{
    struct tq_list_nosync *queue = xmalloc(sizeof(struct tq_list_nosync));
    memset(queue, 0, sizeof(struct tq_list_nosync));
    queue->head = queue->tail = 0;
    return &queue->basic;
}

static void
tq_list_nosync_free(tq_t *tq)
{
    struct tq_list_nosync *queue = (struct tq_list_nosync *)tq;
    xfree(queue);
}

static int
tq_list_nosync_enq(tq_t *tq, ptask_t *task)
{
    struct tq_list_nosync *queue = (struct tq_list_nosync *)tq;

    if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

    task->next = 0;

    if (queue->tail == 0) {
	queue->head = task;
	queue->tail = task;
    }
    else {
	queue->tail->next = task;
	queue->tail = task;
    }

    /* profiling */
    queue->basic.enq_num++;
    if (queue->basic.max_num < queue->basic.num) {
	queue->basic.max_num = queue->basic.num;
    }

    // tq_list_nosync_show(queue, task);
    return 1;
}

static ptask_t *
tq_list_nosync_deq(tq_t *tq)
{
    struct tq_list_nosync *queue = (struct tq_list_nosync *)tq;

    if (queue->head) {
	ptask_t *task;

	task = queue->head;

	if (task) {
	    if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);
	    queue->head = task->next;

	    if (task->next == 0) {
		queue->tail = 0;
	    }
	    queue->basic.num--;
	    return task;
	}
    }

    if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, empty\n", queue, queue->basic.num);
    queue->basic.deq_miss++;
    return 0;
}

static ptask_t *
tq_list_nosync_steal(tq_t *tq)
{
    struct tq_list_nosync *queue = (struct tq_list_nosync *)tq;
    ptask_t *task = 0;
    queue = 0;
    return task;
}

static void
tq_list_nosync_wait(tq_t *tq)
{
    struct tq_list_nosync *queue = (struct tq_list_nosync *)tq;
    if (QDBG) fprintf(stderr, "wait - q: %p, num: %d\n", queue, queue->basic.num);

    sched_yield();
}

DEFINE_TQ_SET(list_nosync);

