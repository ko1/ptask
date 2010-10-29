
#ifndef QDBG
#define QDBG 0
#endif

#define Q_LINKEDLIST 1

struct tq_list {
    tq_t basic;

    ptask_t * volatile tail;
    ptask_t head;

    volatile int wait_thread_num;
    pthread_cond_t cond;
    pthread_mutex_t cond_lock;
};

static tq_t *
tq_list_create(size_t capa)
{
    struct tq_list *queue = xmalloc(sizeof(struct tq_list));
    memset(queue, 0, sizeof(struct tq_list));

    mutex_init(&queue->cond_lock, 0);
    cond_init(&queue->cond, 0);
    queue->tail = &queue->head;

    return &queue->basic;
}

static void
tq_list_free(tq_t *queue)
{
    xfree(queue);
}

static void
tq_list_show(const char *stage, struct tq_list *queue, ptask_t *target_task)
{
    if (*stage == '*') {
	ptask_t *task = &queue->head;
	fprintf(stderr, "* <%s> queue (%p)\n", stage, target_task);

	while (task) {
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
tq_list_enq(tq_t *tq, ptask_t *task)
{
    struct tq_list *queue = (struct tq_list *)tq;

    if (QDBG) fprintf(stderr, "enq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

    task->next = 0;

    while (1) {
	ptask_t *tail = queue->tail;

	if (atomic_cas(tail->next, 0, task)) {
	    tq_list_show("E0", queue, task);
	    if (atomic_cas(queue->tail, tail, task)) {
		/* success */
		tq_list_show("E1", queue, task);
		break;
	    }
	}
	else {
	    fprintf(stderr, "!!\n");
	    atomic_cas(queue->tail, tail, tail->next);
	    tq_list_show("E2", queue, task);
	}
    }

    atomic_inc(queue->basic.num);

    /* profiling */
    queue->basic.enq_num++;
    if (queue->basic.max_num < queue->basic.num) {
	queue->basic.max_num = queue->basic.num;
    }

    // tq_list_show(queue, task);
    return 1;
}

static ptask_t *
tq_list_deq(tq_t *tq)
{
    struct tq_list *queue = (struct tq_list *)tq;
    ptask_t *task;

  retry:
    tq_list_show("D0", queue, 0);
    task = queue->head.next;

    if (task == 0) {
	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, empty\n", queue, queue->basic.num);
	queue->basic.deq_miss++;
	return 0;
    }
    else {
	ptask_t *next_task = task->next;
	tq_list_show("D1", queue, task);

	if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);

	if (task == &queue->head) bug("assert");

	/*
	  head.next -> task -> next_task
	  tail -> ?
	 */

	if (atomic_cas(queue->head.next, task, next_task) == 0) {
	    goto retry;
	}

	/*
	  head.next -> next_task
	  tail -> ?
	  task->next -> next_task
	 */

	atomic_dec(queue->basic.num);

	tq_list_show("D2", queue, task);

	if (next_task == 0) {
	    /* task is the last task. tail pointed me */

	    /*
	      head.next -> 0
	      tail -> task
	      task->next -> 0
	     */

	    if (queue->head.next != 0) bug("assert");
	    if (0) {
		ptask_t *t = task;
		int i = 0;
		while (t != queue->tail) {
		    fprintf(stderr, "<%d> %p (head: %p, head.next:%p, tail: %p)\n", i++, t,
			    &queue->head, queue->head.next, queue->tail);
		    if (t)
		      t = t->next;
		    else {
			break;
		    }
		}
	    }

	    tq_list_show("D3-1", queue, task);
	    tq_list_enq(tq, &queue->head);

	    fprintf(stderr, "<%d> %p (head: %p, head.next:%p, tail: %p)\n", -1, task,
		    &queue->head, queue->head.next, queue->tail);

	    tq_list_show("D3-2", queue, task);

	    /*
	      head.next -> 0
	      tail -> head
	      task->next -> head
	     */

	    if (task->next == &queue->head) {
		task->next = 0;
	    }
	    else {
		fprintf(stderr, "* ... %p\n", task);

		{
		    ptask_t *t = task->next;
		    int i = 0;
		    while (t != &queue->head) {
			fprintf(stderr, "%2d> %p\n", i++, t);
			t = t->next;
		    }
		}

		ptask_t *t1 = task->next, *t2;
		task->next = 0;
		while (1) {
		    // fprintf(stderr, "> %p\n", t1);
		    if (t1 == &queue->head) {
			// fprintf(stderr, "> end!\n");
			break;
		    }
		    t2 = t1->next;

		    fprintf(stderr, "> %p\n", t1);
		    //tq_list_show("*D4-1", queue, t1);
		    tq_list_enq(tq, t1);
		    //tq_list_show("*D4-2", queue, t1);
		    t1 = t2;
		}
	    }
	}
    }

    if (QDBG) fprintf(stderr, "deq - q: %p, num: %d, task: %p\n", queue, queue->basic.num, task);
    return task;
}

static ptask_t *
tq_list_steal(tq_t *tq)
{
    struct tq_list *queue = (struct tq_list *)tq;
    ptask_t *task = 0;
    queue = 0;
    return task;
}

static void
tq_list_wait(tq_t *tq)
{
    struct tq_list *queue = (struct tq_list *)tq;
    if (QDBG) fprintf(stderr, "wait - q: %p, num: %d\n", queue, queue->basic.num);
    sched_yield();
}

DEFINE_TQ_SET(list);

