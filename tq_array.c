
#ifndef QDBG
#define QDBG 0
#endif

struct tq_array {
    tq_t basic;
    ptask_t **tasks;

    int capa;
    volatile int num;

    int head;
    int tail;

    volatile int wait_thread_num;
    pthread_cond_t cond;
    pthread_mutex_t cond_lock;
};

static tq_t *
tq_array_create(size_t capa)
{
    struct tq_array *queue = xmalloc(sizeof(struct tq_array));
    memset(queue, 0, sizeof(struct tq_array));

    mutex_init(&queue->cond_lock, 0);
    cond_init(&queue->cond, 0);
    queue->capa = capa;
    if (capa > 0) {
	queue->tasks = xmalloc(sizeof(ptask_t *) * capa);
    }

    return &queue->basic;
}

static void
tq_array_free(tq_t *tq)
{
    struct tq_array *queue = (struct tq_array *)tq;
    xfree(queue->tasks);
}

static int
tq_array_enq(tq_t *tq, ptask_t *task)
{
    struct tq_array *queue = (struct tq_array *)tq;

    if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

    if (queue->num == queue->capa) {
	queue->basic.enq_miss++;
	return 0;
    }
    else {
	int next_index = (queue->head + 1) % queue->capa;

	queue->tasks[queue->head] = task;
	queue->head = next_index;

	LOCK(&queue->cond_lock, {
	    int old_num = queue->num;
	    atomic_inc(queue->num);

	    if (queue->wait_thread_num > 0) {
		if (QDBG) fprintf(stderr, "enq-broadcast - old_num: %d, num: %d, wait_num: %d\n",
				  old_num, queue->num, queue->wait_thread_num);
		cond_broadcast(&queue->cond);
		queue->wait_thread_num--;
	    }
	});

	/* profiling */
	queue->basic.enq_num++;
	if (queue->basic.max_num < queue->basic.num) {
	    queue->basic.max_num = queue->basic.num;
	}
	return 1;
    }
}

static void
tq_array_wait(tq_t *tq)
{
    struct tq_array *queue = (struct tq_array *)tq;

    if (1) {
	LOCK(&queue->cond_lock, {
	    while (queue->num == 0) {
		if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, sleep\n", queue, queue->basic.num);
		queue->wait_thread_num++;
		queue->basic.cond_wait_num++;
		cond_wait(&queue->cond, &queue->cond_lock);
		if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, wakeup\n", queue, queue->basic.num);
	    }
	});
    }
    else {
	sched_yield();
    }
}

static ptask_t *
tq_array_deq(tq_t *tq)
{
    struct tq_array *queue = (struct tq_array *)tq;

    if (queue->num == 0) {
	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, deq empty\n", queue, queue->num);
	queue->basic.deq_miss++;
	return 0;
    }
    else {
	ptask_t *task = queue->tasks[queue->tail];
	queue->tail = (queue->tail + 1) % queue->capa;
	queue->basic.deq_num++;

	atomic_dec(queue->num);

	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

	return task;
    }
}

static ptask_t *
tq_array_steal(tq_t *tq)
{
    struct tq_array *queue = (struct tq_array *)tq;
    ptask_t *task = 0;

    queue = 0;

    return task;
}

DEFINE_TQ_SET(array);
