
#ifndef PTASK_H
#define PTASK_H

#include <stddef.h>

/* API for task internal */
typedef struct ptask_struct ptask_t;
typedef struct ptask_queue_struct ptask_queue_t;
typedef struct ptask_queue_group_struct ptask_queue_group_t;

/* all APIs are limited to called by one thread */
int ptask_setup(void);

void ptask_dispatch(ptask_t *task);
void ptask_dispatch_queue(ptask_t *task, ptask_queue_t *queue);
void ptask_dispatch_group(ptask_t *task, ptask_queue_group_t *group);

ptask_queue_t *ptask_queue_create(int capa);
void ptask_queue_destruct(ptask_queue_t *);
void ptask_queue_add(ptask_queue_t *q, ptask_t *task);

ptask_t *ptask_create(void *(*func)(void *args), void *argv);
int ptask_finished(ptask_t *task);
void ptask_wait(ptask_t *task);
void *ptask_result(ptask_t *task);
size_t ptask_memsize(ptask_t *task);
void ptask_destruct(ptask_t *task);

void *ptask_malloc(size_t size);


/* for performance test */
void ptask_qtest(int n);

#endif /* PTASK_H */
