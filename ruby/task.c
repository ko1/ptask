#include "../ptask.h"
#include "ruby/ruby.h"
#include <unistd.h>

/* API for ext */
VALUE rb_task_create(VALUE (*func)(VALUE *args), int argc, ...);

#define GetTaskPtr(obj, tobj) TypedData_Get_Struct(obj, rb_task_t, &task_data_type, tobj)

#ifdef HAVE_STDARG_PROTOTYPES
#include <stdarg.h>
#define va_init_list(a,b) va_start(a,b)
#else
#include <varargs.h>
#define va_init_list(a,b) va_start(a)
#endif

static VALUE rb_cTask;
static int worker_thread_num = 0;

typedef struct task_struct {
    ptask_t *ptask;
    VALUE (*func)(VALUE *args);
    VALUE result;
    void *data;
    VALUE *argv;
    int argc;
} rb_task_t;

static void
task_mark(void *ptr)
{
    if (ptr) {
	rb_task_t *task = (rb_task_t *)ptr;
	int i;

	if (task->result != Qundef) {
	    rb_gc_mark(task->result);
	}

	if (task->argc > 0) {
	    for (i=0; i<task->argc; i++) {
		rb_gc_mark(task->argv[i]);
	    }
	}
    }
}

static void
task_free(void *ptr)
{
    if (ptr) {
	rb_task_t *task = (rb_task_t *)ptr;
	ptask_destruct(task->ptask);
	ruby_xfree(ptr);
    }
}

static size_t
task_memsize(const void *ptr)
{
    if (ptr) {
	rb_task_t *task = (rb_task_t *)ptr;
	return ptask_memsize(task->ptask) + sizeof(VALUE) * task->argc;
    }
    else {
	return 0;
    }
}

static const rb_data_type_t task_data_type = {
    "task",
    {task_mark, task_free, task_memsize,},
};

static void *
task_dispatch_callback(void *ptr)
{
    rb_task_t *task = (rb_task_t *)ptr;
    task->result = task->func(task->argv);
    return 0;
}

VALUE
rb_task_create(VALUE (*func)(VALUE *argv), int argc, ...)
{
    va_list args;

    if (worker_thread_num > 0) {
	rb_task_t *task;
	VALUE *argv = xmalloc(sizeof(VALUE) * argc);
	VALUE obj;
	int i;

	obj = TypedData_Make_Struct(rb_cTask, rb_task_t, &task_data_type, task);

	/* setup argv */
	va_init_list(args, argc);
	for (i=0; i<argc; i++) {
	    argv[i] = va_arg(args, VALUE);
	}
	va_end(args);

	task->func = func;
	task->argc = argc;
	task->argv = argv;
	task->result = Qundef;
	task->ptask = ptask_create(task_dispatch_callback, task);
	ptask_dispatch(task->ptask);
	return obj;
    }
    else {
	/* Do it immediately */
	int i;
	VALUE *argv = ALLOCA_N(VALUE, argc);

	va_init_list(args, argc);
	for (i=0; i<argc; i++) {
	    argv[i] = va_arg(args, VALUE);
	}
	va_end(args);

	return (VALUE)func(argv);
    }
}

static VALUE
task_method_missing(int argc, VALUE *argv, VALUE self)
{
    rb_task_t *task;
    VALUE result;
    GetTaskPtr(self, task);

    /* synchronize */
    while (!ptask_finished(task->ptask)) {
	ptask_wait(task->ptask);
    }
    result = task->result;
    /* TODO: free ptask */
    return rb_funcall(result, SYM2ID(argv[0]), argc-1, argv+1);
}

static void Init_task_test(void);

void
Init_task(void)
{
    rb_cTask = rb_define_class("Task", rb_cObject);
    rb_define_private_method(rb_cTask, "method_missing", task_method_missing, -1);
    RCLASS_SUPER(rb_cTask) = 0;


    /* setup worker native threads */
    {
	int i;

	if (getenv("RUBY_MAX_WORKER_NUM")) {
	    worker_thread_num = atoi(getenv("RUBY_MAX_WORKER_NUM"));
	}

	if (worker_thread_num <= 0) {
	    worker_thread_num = 0;
	}
	else {
	    ptask_setup();

	    for (i=0; i<worker_thread_num; i++) {
		ptask_queue_create(4096);
	    }
	}
    }

    Init_task_test();
}


/* Test task */

static VALUE
func_repeat(VALUE *argv)
{
    int i;
    volatile int n = FIX2INT(argv[0]);
    for (i=0; i<n; i++);

    return INT2FIX(42);
}

static VALUE
m_repeat(VALUE klass, VALUE cost)
{
    return rb_task_create(func_repeat, 1, INT2FIX(NUM2INT(cost)));
}

static VALUE
func_sleep(VALUE *args)
{
    int len = FIX2INT(args[0]);
    sleep(len);
    return INT2FIX(len);
}

static VALUE
m_sleep(VALUE klass, VALUE length)
{
    return rb_task_create(func_sleep, 1, INT2FIX(NUM2INT(length)));
}

static VALUE
func_empty(VALUE *args)
{
    /* empty function */
    return Qnil;
}

static VALUE
m_empty(VALUE klass)
{
    return rb_task_create(func_empty, 0);
}

static void
Init_task_test(void)
{
    VALUE cTaskTest = rb_define_class("TaskTest", rb_cObject);
    rb_define_singleton_method(cTaskTest, "empty", m_empty, 0);
    rb_define_singleton_method(cTaskTest, "repeat", m_repeat, 1);
    rb_define_singleton_method(cTaskTest, "sleep", m_sleep, 1);
}


