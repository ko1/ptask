
#ifndef QDBG
#undef  QDBG
#define QDBG 1
#endif

#include <semaphore.h>
#include <errno.h>

struct tq_array_atomic {
    tq_t basic;
    ptask_t **tasks;

    int capa;

    int head;
    union array_atomic_tagged_tail {
	struct {
	    unsigned int index:16;
	    unsigned int tag:16;
	} tagged;
	int number;
    } tail;

    volatile int wait_thread_num;
    sem_t sem;
};

static tq_t *
tq_array_atomic_create(size_t capa)
{
    struct tq_array_atomic *queue = xmalloc(sizeof(struct tq_array_atomic));
    memset(queue, 0, sizeof(struct tq_array_atomic));

    queue->capa = capa;
    if (capa > 0) {
	queue->tasks = xmalloc(sizeof(ptask_t *) * capa);
    }
    sem_init(&queue->sem, 0, 0);
    return &queue->basic;
}

static void
tq_array_atomic_free(tq_t *tq)
{
    struct tq_array_atomic *queue = (struct tq_array_atomic *)tq;
    sem_destroy(&queue->sem);
    xfree(queue->tasks);
}

static int
tq_array_atomic_enq(tq_t *tq, ptask_t *task)
{
    struct tq_array_atomic *queue = (struct tq_array_atomic *)tq;

    if (QDBG) fprintf(stderr, "wn: %2d enq - q: %p, num: %d, task: %p\n", ptask_worker_id, queue, queue->basic.num, task);

    if (queue->basic.num == queue->capa) {
	queue->basic.enq_miss++;
	return 0;
    }
    else {
	int next_index = (queue->head + 1) % queue->capa;

	queue->tasks[queue->head] = task;
	queue->head = next_index;

	atomic_inc(queue->basic.num);

	if (queue->wait_thread_num > 0) {
	    atomic_dec(queue->wait_thread_num);
	    sem_post(&queue->sem);
	}

	/* profiling */
	queue->basic.enq_num++;
	if (queue->basic.max_num < queue->basic.num) {
	    queue->basic.max_num = queue->basic.num;
	}
	return 1;
    }
}

static ptask_t *
tq_array_atomic_deq(tq_t *tq)
{
    struct tq_array_atomic *queue = (struct tq_array_atomic *)tq;

  retry:
    if (queue->basic.num == 0) {
	if (QDBG) fprintf(stderr, "wn: %2d deq - q: %p, num: %d, deq empty\n",
			  ptask_worker_id, queue, queue->basic.num);
	queue->basic.deq_miss++;
	return 0;
    }
    else {
	ptask_t *task;
	union array_atomic_tagged_tail tail = queue->tail;
	union array_atomic_tagged_tail next_tail;
	next_tail.tagged.index = (tail.tagged.index + 1) % queue->capa;
	if (tail.tagged.index == 0) next_tail.tagged.tag = tail.tagged.tag + 1;

	task = queue->tasks[tail.tagged.index];

	if (!atomic_cas(queue->tail.number, tail.number, next_tail.number)) {
	    fprintf(stderr, "wn: %2d deq - index: %d, current: %d\n",
		    ptask_worker_id, tail.tagged.index, queue->tail.tagged.index);
	    goto retry;
	}

	atomic_dec(queue->basic.num);

	if (QDBG) fprintf(stderr, "wn: %2d deq - q: %p, num: %d, task: %p\n",
			  ptask_worker_id, queue, queue->basic.num, task);

	if (!atomic_cas(task->status, 0, ptask_worker_id)) {
	    fprintf(stderr, "wn: %2d: task: %p, task->status: %2d\n", ptask_worker_id, task, task->status);
	    bug("...");
	}

	return task;
    }
}

static void
tq_array_atomic_wait(tq_t *tq)
{
    struct tq_array_atomic *queue = (struct tq_array_atomic *)tq;
    int i;

    if (QDBG); fprintf(stderr, "wn: %2d wait - q: %p, num: %d, start\n", ptask_worker_id, queue, queue->basic.num);

  retry:
    queue->basic.cond_wait_num++;

    atomic_inc(queue->wait_thread_num);

    for (i=0; i<5; i++) {
	/* busy loop */
	if (sem_trywait(&queue->sem) == 0) {
	    goto last;
	}
	sched_yield();
    }

    if (sem_wait(&queue->sem) != 0) {
	switch (errno) {
	  case EINTR: goto retry;
	  case EINVAL: bug("sem_wait: EINVAL");
	  default:
	    bug("sem_wait: unreachable");
	}
    }

  last:
    if (QDBG); fprintf(stderr, "wn: %2d wait - q: %p, num: %d, end\n", ptask_worker_id, queue, queue->basic.num);
}

static ptask_t *
tq_array_atomic_steal(tq_t *tq)
{
    struct tq_array_atomic *queue = (struct tq_array_atomic *)tq;
    ptask_t *task = 0;

    queue = 0;

    return task;
}

DEFINE_TQ_SET(array_atomic);

