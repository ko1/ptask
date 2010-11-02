#include "qmem.h"

#define atomic_inc(v) __sync_fetch_and_add(&(v), 1)
#define atomic_dec(v) __sync_fetch_and_sub(&(v), 1)
#define atomic_cas(v, e, a) __sync_bool_compare_and_swap(&(v), (e), (a))

#ifndef QMEM_PROFILE
#define QMEM_PROFILE 0
#endif

#if 0
#include "ruby/ruby.h"
#define QMEM_MALLOC ruby_malloc
#define QMEM_FREE   ruby_free
#define QMEM_ERROR  rb_bug

#else

#define QMEM_MALLOC malloc
#define QMEM_FREE   free

#include <stdio.h>

void
QMEM_ERROR(const char *str)
{
    fprintf(stderr, "%s\n", str);
    exit(1);
}
#endif

#define QMEM_ALIGN                    8
#define QMEM_DEFAULT_RECYCLE_SIZE  0x10

struct qmem_buffer {
    struct qmem_buffer *next;

    size_t size;
    char *buff;

    size_t free;
    size_t used;
};

struct qmem_struct {
    struct qmem_buffer *head;
    struct qmem_buffer *last;
    size_t free_alloc_count;

    struct qmem_buffer *recycle;
    size_t recycle_buffer_count;

    size_t default_size;
    size_t recycle_size;

#if QMEM_PROFILE
    size_t cnt_alloc;
    size_t cnt_free;
    size_t cnt_free_bottom;
#endif
};

static struct qmem_buffer*
qmem_buffer_create(qmem_t *qm)
{
    struct qmem_buffer *qmb;

    if (qm->recycle_buffer_count > 0) {
	qmb = qm->recycle;
	qm->recycle = qmb->next;
	qm->recycle_buffer_count--;
    }
    else {
	qmb = (struct qmem_buffer *)QMEM_MALLOC(sizeof(struct qmem_buffer));
	qmb->buff = (char *)QMEM_MALLOC(qm->default_size);
	qmb->size = qm->default_size;
    }

    qmb->next = 0;
    qmb->used = 0;
    qmb->free = 0;
    return qmb;
}

static void
qmem_buffer_free(struct qmem_buffer *qmb)
{
    if (qmb) {
	QMEM_FREE(qmb->buff);
	QMEM_FREE(qmb);
    }
}

static void
qmem_buffer_recycle(qmem_t *qm, struct qmem_buffer *qmb)
{
    if (qm->recycle_buffer_count < qm->recycle_size) {
	qmb->next = qm->recycle;
	qm->recycle = qmb;
	qm->recycle_buffer_count++;
    }
    else {
	qmem_buffer_free(qmb);
    }
}

static void
destruct_buffers(struct qmem_buffer *qmb)
{
    struct qmem_buffer *tqmb;
    while (qmb) {
	tqmb = qmb->next;
	qmem_buffer_free(qmb);
	qmb = tqmb;
    }
}

void
qmem_destruct(qmem_t *qm)
{
    destruct_buffers(qm->head);
    destruct_buffers(qm->recycle);
    QMEM_FREE(qm);
}

qmem_t *
qmem_create(size_t default_size)
{
    qmem_t *qm = QMEM_MALLOC(sizeof(struct qmem_struct));

    qm->default_size = default_size;
    qm->recycle_size = QMEM_DEFAULT_RECYCLE_SIZE;
    qm->recycle_buffer_count = 0;
    qm->free_alloc_count = 0;
    qm->recycle = 0;

    qm->head = qm->last = qmem_buffer_create(qm);

#if QMEM_PROFILE
    qm->cnt_alloc = 0;
    qm->cnt_free = 0;
    qm->cnt_free_bottom = 0;
#endif
    return qm;
}

static void
set_free_flag(qmem_t *qm, void *ptr)
{
    struct qmem_buffer *qmb = qm->head;
    size_t *alloc_ptr = (size_t *)ptr - 1;

    alloc_ptr[0] = alloc_ptr[0] | 0x01;

    if (0) fprintf(stderr, "set_free_flag: head: %p, alloc_ptr: %p, alloc_ptr[0]: %p\n",
		   qmb->buff + qmb->free, alloc_ptr, (void *)alloc_ptr[0]);

    atomic_inc(qm->free_alloc_count);
}

static int
check_free_flag(struct qmem_buffer *qmb, size_t *alloc_ptr)
{
    if (0) fprintf(stderr, "check_free_flag: head: %p, alloc_ptr: %p, alloc_ptr[0]: %p\n",
		   qmb->buff + qmb->free, alloc_ptr, (void *)alloc_ptr[0]);

    if (alloc_ptr[0] & 0x01) {
	return 1;
    }
    else {
	return 0;
    }
}

static int
free_bottom(qmem_t *qm)
{
    struct qmem_buffer *qmb = qm->head;
    size_t alloc_size;
    size_t *alloc_ptr = (size_t *)(qmb->buff + qmb->free);

    if (check_free_flag(qmb, alloc_ptr)) {
	atomic_dec(qm->free_alloc_count);

#if QMEM_PROFILE
	qm->cnt_free_bottom++;
#endif

	alloc_size = alloc_ptr[0] & ~0x01;
	qmb->free += alloc_size;

	if (qmb->free == qmb->used) {
	    if (qmb->next) {
		qm->head = qmb->next;
		qmem_buffer_recycle(qm, qmb);
	    }
	    else {
		qmb->used = 0;
		qmb->free = 0;
		return 0; /* don't continue */
	    }
	}
	return 1; /* continue */
    }
    else {
	return 0; /* don't continue */
    }
}

static void
check_free(qmem_t *qm)
{
    if (qm->free_alloc_count > 0) {
	while (free_bottom(qm));
    }
}

void *
qmem_alloc(qmem_t *qm, size_t size)
{
    struct qmem_buffer *qmb = qm->last;
    size_t *buff, alloc_size = sizeof(size_t) + size;

    check_free(qm);

    /* check */
    if (qm->default_size < size) {
	QMEM_ERROR("qmem_alloc: over default size.");
    }

    /* alignment */
    if (alloc_size % QMEM_ALIGN != 0) {
	alloc_size = QMEM_ALIGN * (alloc_size / QMEM_ALIGN + 1);
    }

    if (qmb->size - qmb->used > alloc_size) {
	/* do nothing */
    }
    else {
	qmb = qmem_buffer_create(qm);
	qm->last->next = qmb;
	qm->last = qmb;
    }

    buff = (size_t *)(qmb->buff + qmb->used);
    qmb->used += alloc_size;

    // fprintf(stderr, "qmem_alloc: allo_size: %d\n", alloc_size);
    if ((alloc_size & 0x01) != 0) QMEM_ERROR("qmem_alloc: not aligned.");

#if QMEM_PROFILE
    qm->cnt_alloc++;
#endif

    buff[0] = alloc_size;
    return &buff[1];
}

void
qmem_free(qmem_t *qm, void *ptr)
{
    set_free_flag(qm, ptr);

#if QMEM_PROFILE
    atomic_inc(qm->cnt_free);
#endif
}

void
qmem_print_status(qmem_t *qm)
{
    struct qmem_buffer *qmb;
    size_t buffer_num = 0;

    qmb = qm->head;
    while (qmb) {buffer_num++; qmb = qmb->next;}

    fprintf(stderr, "* qmem_print_status\n");
    fprintf(stderr, "- buffer number    : %d\n", (int)buffer_num);
    fprintf(stderr, "- default_size     : %d\n", (int)qm->default_size);
    fprintf(stderr, "- recycle_size     : %d\n", (int)qm->recycle_size);
    fprintf(stderr, "- free_alloc_count : %d\n", (int)qm->free_alloc_count);
    fprintf(stderr, "- recycle_buff_cnt : %d\n", (int)qm->recycle_buffer_count);

#if QMEM_PROFILE
    fprintf(stderr, "- cnt_alloc        : %d\n", (int)qm->cnt_alloc);
    fprintf(stderr, "- cnt_free         : %d\n", (int)qm->cnt_free);
    fprintf(stderr, "- cnt_free_bottom  : %d\n", (int)qm->cnt_free_bottom);
#endif
}

int
qmem_set_buffer_size(qmem_t *qm, size_t default_size)
{
    qm->default_size = default_size;

    destruct_buffers(qm->recycle);
    qm->recycle = 0;
    return 1;
}

int
qmem_set_recycle_size(qmem_t *qm, size_t recycle_size)
{
    qm->recycle_size = recycle_size;

    destruct_buffers(qm->recycle);
    qm->recycle = 0;
    return 1;
}
