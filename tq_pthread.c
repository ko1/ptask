
#ifndef QDBG
#define QDBG 0
#endif

struct tq_pthread {
    tq_t basic;

    ptask_t *head, *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    volatile int wait_thread_num;
};

static tq_t *
tq_pthread_create(size_t capa)
{
    struct tq_pthread *queue = xmalloc(sizeof(struct tq_pthread));
    memset(queue, 0, sizeof(struct tq_pthread));
    mutex_init(&queue->lock, 0);
    cond_init(&queue->cond, 0);
    queue->head = queue->tail = 0;
    return &queue->basic;
}

static void
tq_pthread_free(tq_t *tq)
{
    xfree(tq);
}

static int
tq_pthread_enq(tq_t *tq, ptask_t *task)
{
    struct tq_pthread *queue = (struct tq_pthread *)tq;

    task->next = 0;

    LOCK(&queue->lock, {
	if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p (%s)\n",
			  queue, queue->basic.num, task, ptask_status_name(task));

	ptask_t *tail = queue->tail;

	if (tail) {
	    tail->next = task;
	}
	else {
	    queue->head = task;
	}
	queue->tail = task;
	queue->basic.num++;

	if (queue->wait_thread_num > 0) {
	    cond_signal(&queue->cond);
	}
    });

    /* profiling */
    queue->basic.enq_num++;
    if (queue->basic.max_num < queue->basic.num) {
	queue->basic.max_num = queue->basic.num;
    }
    return 1;
}

static ptask_t *
tq_pthread_deq(tq_t *tq)
{
    struct tq_pthread *queue = (struct tq_pthread *) tq;
    ptask_t *task;

  first:

    if (0) {
	if (queue->head == 0) {
	    sched_yield();
	    goto first;
	}
    }
    else {
	LOCK(&queue->lock, {
	    while (queue->head == 0) {
		queue->wait_thread_num++;
		queue->basic.cond_wait_num++;
		if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, sleep\n", queue, queue->basic.num);
		cond_wait(&queue->cond, &queue->lock);
		if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, wakeup\n", queue, queue->basic.num);
		queue->wait_thread_num--;
	    }
	});
    }

    LOCK(&queue->lock, {
	task = queue->head;

	if (task) {
	    queue->head = task->next;
	    if (task->next == 0) {
		queue->tail = 0;
	    }
	    queue->basic.num--;
	}

	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p (%s)\n", queue, queue->basic.num,
			  task, task ? ptask_status_name(task) : "(;;)");
    });

    if (task == 0) {
	goto first;
    }
    queue->basic.deq_num++;
    return task;
}

static void
tq_pthread_wait(tq_t *tq)
{
}

static ptask_t *
tq_pthread_steal(tq_t *queue)
{
    ptask_t *task = 0;
    return task;
}

DEFINE_TQ_SET(pthread);

#if 0
struct tq_set TQ_PTHREAD = {
    tq_pthread_create,
    tq_pthread_free,
    tq_pthread_enq,
    tq_pthread_deq,
    tq_pthread_steal,
    tq_pthread_wait,
};
#endif
