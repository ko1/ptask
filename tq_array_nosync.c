
#ifndef QDBG
#define QDBG 0
#endif

#define LOCK_COST_TEST 0
#define ATOMIC_COST_TEST 0
#define ATOMIC_LOCK_COST_TEST 1

#if ATOMIC_LOCK_COST_TEST
static void
atomic_lock(volatile int *lock)
{
#if 1
    while (__sync_bool_compare_and_swap(lock, 0, 1)) {
	// spin
    }
#else
    while (*lock != 0) {
    }
    *lock = 1;
#endif
}

static void
atomic_unlock(volatile int *lock)
{
    *lock = 0;
}

#define ATOMIC_LOCK(l, body) {atomic_lock(l); body; atomic_unlock(l);}

#else
#define ATOMIC_LOCK(l, body) body
#endif

struct tq_array_nosync {
    tq_t basic;
    ptask_t **tasks;

    int capa;

    int head;
    int tail;

#if LOCK_COST_TEST
    pthread_mutex_t lock;
#endif

    int volatile atomic_num;
};

static tq_t *
tq_array_nosync_create(size_t capa)
{
    struct tq_array_nosync *queue = xmalloc(sizeof(struct tq_array_nosync));
    memset(queue, 0, sizeof(struct tq_array_nosync));

    queue->capa = capa;
    if (capa > 0) {
	queue->tasks = xmalloc(sizeof(ptask_t *) * capa);
    }

#if LOCK_COST_TEST
    pthread_mutex_init(&queue->lock, 0);
#endif
    return &queue->basic;
}

static void
tq_array_nosync_free(tq_t *tq)
{
    struct tq_array_nosync *queue = (struct tq_array_nosync *)tq;
    xfree(queue->tasks);
}

static int
tq_array_nosync_enq(tq_t *tq, ptask_t *task)
{
    struct tq_array_nosync *queue = (struct tq_array_nosync *)tq;

    if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

    if (queue->basic.num == queue->capa) {
	return 0;
    }
    else {
	int next_index = (queue->head + 1) % queue->capa;

	queue->tasks[queue->head] = task;
	queue->head = next_index;
	queue->basic.num++;

#if LOCK_COST_TEST
	LOCK(&queue->lock, 0);
#endif

#if ATOMIC_COST_TEST
	atomic_inc(queue->atomic_num);
#endif

	ATOMIC_LOCK(&queue->atomic_num, 0);

	PTASK_PROFILE_SET_MAX(max_num, queue->basic.num);
	return 1;
    }
}

static void
tq_array_nosync_wait(tq_t *tq)
{
    PTASK_PROFILE_INC(wait_num);
    sched_yield();
}

static ptask_t *
tq_array_nosync_deq(tq_t *tq)
{
    struct tq_array_nosync *queue = (struct tq_array_nosync *)tq;

    if (queue->basic.num == 0) {
	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, deq empty\n", queue, queue->basic.num);
	return 0;
    }
    else {
	ptask_t *task = queue->tasks[queue->tail];
	queue->tail = (queue->tail + 1) % queue->capa;
	queue->basic.num--;

#if LOCK_COST_TEST
	LOCK(&queue->lock, 0);
#endif
#if ATOMIC_COST_TEST
	atomic_dec(queue->atomic_num);
#endif
	ATOMIC_LOCK(&queue->atomic_num, 0);

	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

	return task;
    }
}

static ptask_t *
tq_array_nosync_steal(tq_t *tq)
{
    struct tq_array_nosync *queue = (struct tq_array_nosync *)tq;
    ptask_t *task = 0;

    queue = 0;

    return task;
}

DEFINE_TQ_SET(array_nosync);
