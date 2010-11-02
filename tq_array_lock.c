
#ifndef QDBG
#define QDBG 0
#endif

struct tq_array_lock {
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
tq_array_lock_create(size_t capa)
{
    struct tq_array_lock *queue = xmalloc(sizeof(struct tq_array_lock));
    memset(queue, 0, sizeof(struct tq_array_lock));

    mutex_init(&queue->cond_lock, 0);
    cond_init(&queue->cond, 0);
    queue->capa = capa;
    if (capa > 0) {
	queue->tasks = xmalloc(sizeof(ptask_t *) * capa);
    }

    return &queue->basic;
}

static void
tq_array_lock_free(tq_t *tq)
{
    struct tq_array_lock *queue = (struct tq_array_lock *)tq;
    xfree(queue->tasks);
}

static int
tq_array_lock_enq(tq_t *tq, ptask_t *task)
{
    struct tq_array_lock *queue = (struct tq_array_lock *)tq;

    if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

    if (queue->num == queue->capa) {
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

	PTASK_PROFILE_SET_MAX(max_num, queue->basic.num);
	return 1;
    }
}

static void
tq_array_lock_wait(tq_t *tq)
{
    struct tq_array_lock *queue = (struct tq_array_lock *)tq;

    if (1) {
	LOCK(&queue->cond_lock, {
	    while (queue->num == 0) {
		if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, sleep\n", queue, queue->basic.num);
		queue->wait_thread_num++;
		PTASK_PROFILE_INC(wait_num);
		cond_wait(&queue->cond, &queue->cond_lock);
		if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, wakeup\n", queue, queue->basic.num);
	    }
	});
    }
    else {
	PTASK_PROFILE_INC(wait_num);
	sched_yield();
    }
}

static ptask_t *
tq_array_lock_deq(tq_t *tq)
{
    struct tq_array_lock *queue = (struct tq_array_lock *)tq;

    if (queue->num == 0) {
	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, deq empty\n", queue, queue->num);
	return 0;
    }
    else {
	ptask_t *task = queue->tasks[queue->tail];
	queue->tail = (queue->tail + 1) % queue->capa;
	atomic_dec(queue->num);

	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

	return task;
    }
}

static ptask_t *
tq_array_lock_steal(tq_t *tq)
{
    struct tq_array_lock *queue = (struct tq_array_lock *)tq;
    ptask_t *task = 0;

    queue = 0;

    return task;
}

DEFINE_TQ_SET(array_lock);
