#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#ifdef USE_QMEM
#include "qmem.h"
#else
#define qmem_alloc(x, s) malloc(s)
#define qmem_free(x, ptr) ((x = 0), free(ptr))
#define qmem_destruct(qm) ((qm) = 0)
#define qmem_create(size) 0
#define qmem_t int
#define qmem_print_status(qm) if (0) printf("0")
#endif

#define QMEM_BUFFSIZE (4096 * 16)

/* single */
#define BUFF_COUNT 10000

/* thread */
#define MAX  10000000
#define TMAX 1
#define LMAX (MAX/TMAX)

static void
single_test(void)
{
    qmem_t *qm = qmem_create(QMEM_BUFFSIZE);
    char *buff[BUFF_COUNT];
    int i, j, cnt, n;

    for (n = 0;n < BUFF_COUNT; n++) {
	// printf(".");
	cnt = rand() % BUFF_COUNT;

	for (i = 0; i<cnt; i++) {
	    size_t size = rand() % 8 + 1;

	    buff[i] = qmem_alloc(qm, size);

	    for (j=0; j<size; j++) {
		buff[i][j] = 0;
	    }
	    // fprintf(stderr, "alloc: %p (size: %d, qm: %p)\n", buff[i], size, qm);
	}
	for (i = 0; i<cnt; i++) {
	    // fprintf(stderr, "free: %p\n", buff[i]);
	    qmem_free(qm, buff[i]);
	}
    }
    qmem_print_status(qm);
    qmem_destruct(qm);
}

static void
single_destruct_test(void)
{
    int j;

    for (j=0; j<1024; j++) {
	qmem_t *qm = qmem_create(1024);
	int i = 0;
	for (i=0; i<30; i++) {
	    qmem_alloc(qm, 777);
	}
	qmem_destruct(qm);
    }
}

struct tt_data {
    struct tt_data * volatile next;
    qmem_t *qm;
    char *buff;
    size_t size;
} final_tt_data;

size_t worker_count;

static void *
thread_test_worker(void *ptr)
{
    int i;
    struct tt_data *data = (struct tt_data *)ptr, *next;
    qmem_t *qm = data->qm;

    while (1) {
	if (data == &final_tt_data) {
	    break;
	}

	while ((next = data->next) == 0) {
	    sched_yield();
	    /* spin loop */
	}
	// fprintf(stderr, "w: data: %p, data->next: %p\n", data, data->next);

	for (i=0; i<data->size; i++) {
	    data->buff[i] = i; // rand();
	}

#define atomic_inc(v) __sync_fetch_and_add(&(v), 1)
	// atomic_inc(worker_count);

	qmem_free(qm, data->buff);
	qmem_free(qm, data);

	data = next;
    }

    return 0;
}

struct tt_data *
make_data(qmem_t *qm)
{
    struct tt_data *data = qmem_alloc(qm, sizeof(struct tt_data));
    size_t size = 128; // (rand() % 128) + 1;
    data->buff = qmem_alloc(qm, size);
    data->size = size;
    data->next = 0;
    data->qm = qm;
    // fprintf(stderr, "m: %p\n", data);
    return data;
}

static void
thread_test(int tn)
{
    qmem_t *qm = qmem_create(QMEM_BUFFSIZE);
    pthread_t *tid = malloc(sizeof(pthread_t) * tn);
    struct tt_data **data = malloc(sizeof(struct tt_data *) * tn);
    int i, l;

    qmem_print_status(qm);

    for (i=0; i<tn; i++) {
	data[i] = make_data(qm);
	pthread_create(&tid[i], 0, thread_test_worker, data[i]);
    }

    qmem_print_status(qm);

    for (l=0; l<LMAX; l++) {
	for (i=0; i<tn; i++) {
	    struct tt_data * next = make_data(qm);
	    data[i]->next = next;
	    data[i] = next;
	    // qmem_print_status(qm);
	}
    }

    qmem_print_status(qm);

    for (i=0; i<tn; i++) {
	data[i]->next = &final_tt_data;
    }

    for (i=0; i<tn; i++) {
	if (pthread_join(tid[i], 0) != 0) {
	    fprintf(stderr, "pthread_join() error\n");
	}
    }

    qmem_print_status(qm);
    qmem_alloc(qm, 1);
    qmem_print_status(qm);

    qmem_destruct(qm);
}

int
main(int argc, char **argv)
{
    int tn = 0;
    srand(12345);

    if (argc > 1) {
	tn = atoi(argv[1]);
    }
    fprintf(stderr, "tn: %d\n", tn);

    if (tn == 0) {
	single_test();
    }
    else if (tn == -1) {
	single_destruct_test();
    }
    else {
	thread_test(tn);
    }
    return 0;
}

