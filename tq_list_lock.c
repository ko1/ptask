
#ifndef QDBG
#define QDBG 0
#endif

#define Q_LINKEDLIST 1

struct tq_list_lock {
    tq_t basic;

    ptask_t *head, *tail;
    /*
     * head -> <old>
     *           v
     *          ...
     *           v
     * tail -> <new>
     */
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

static tq_t *
tq_list_lock_create(size_t capa)
{
    struct tq_list_lock *queue = xmalloc(sizeof(struct tq_list_lock));
    memset(queue, 0, sizeof(struct tq_list_lock));
    queue->head = queue->tail = 0;
    pthread_mutex_init(&queue->lock, 0);
    pthread_cond_init(&queue->cond, 0);
    return &queue->basic;
}

static void
tq_list_lock_free(tq_t *tq)
{
    struct tq_list_lock *queue = (struct tq_list_lock *)tq;
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->cond);
    xfree(queue);
}

static int
tq_list_lock_enq(tq_t *tq, ptask_t *task)
{
    struct tq_list_lock *queue = (struct tq_list_lock *)tq;

    if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

    task->next = 0;

    LOCK(&queue->lock, {
	if (queue->tail == 0) {
	    queue->head = task;
	    queue->tail = task;

	    cond_broadcast(&queue->cond);
	}
	else {
	    queue->tail->next = task;
	    queue->tail = task;
	}
	queue->basic.num++;
    });

    PTASK_PROFILE_SET_MAX(max_num, queue->basic.num);
    return 1;
}

static ptask_t *
tq_list_lock_deq(tq_t *tq)
{
    struct tq_list_lock *queue = (struct tq_list_lock *)tq;

    if (queue->head) {
	ptask_t *task;

	LOCK(&queue->lock, {
	    task = queue->head;

	    if (task) {
		if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);
		queue->head = task->next;

		if (task->next == 0) {
		    queue->tail = 0;
		}
		queue->basic.num--;
	    }
	});

	return task;
    }

    if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, empty\n", queue, queue->basic.num);
    return 0;
}

static ptask_t *
tq_list_lock_steal(tq_t *tq)
{
    return tq_list_lock_deq(tq);
}

static void
tq_list_lock_wait(tq_t *tq)
{
    struct tq_list_lock *queue = (struct tq_list_lock *)tq;
    if (QDBG) fprintf(stderr, "wait - q: %p, num: %d\n", queue, queue->basic.num);

    if (0) {
	LOCK(&queue->lock, {
	    while (queue->head == 0) {
		PTASK_PROFILE_INC(wait_num);
		cond_wait(&queue->cond, &queue->lock);
	    }
	});
    }
    else {
	PTASK_PROFILE_INC(wait_num);
	sched_yield();
    }
}

DEFINE_TQ_SET(list_lock);

