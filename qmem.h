
/* Queued memory allocator */

#include <stdlib.h>

typedef struct qmem_struct qmem_t;

qmem_t *qmem_create(size_t default_size);
void qmem_destruct(qmem_t *qm);
void *qmem_alloc(qmem_t *qm, size_t size);
void qmem_free(qmem_t *qm, void *ptr);

int qmem_set_buffer_size(qmem_t *qm, size_t default_size);
int qmem_set_recycle_size(qmem_t *qm, size_t recycle_size);

void qmem_print_status(qmem_t *qm);
