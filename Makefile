
all: ptask_test qmem_test qmem_test_malloc

CFLAGS=-O3 -g -Wall

tests=ptask_test tq_test qmem_test_malloc qmem_test
clean:
	rm -f $(tests)

ptask_test: ptask.c ptask.h tq_*.c ptask_test.c Makefile
	gcc $(CFLAGS) ptask.c ptask.h ptask_test.c -o ptask_test -lpthread

tq_test: tqtest.c tq_*.c Makefile
	gcc $(CFLAGS) tqtest.c -o tqtest -lpthread

qmem_test: qmem_test.c qmem.c Makefile
	gcc $(CFLAGS) qmem_test.c qmem.c -o qmem_test -lpthread -DUSE_QMEM=1

qmem_test_malloc: qmem_test.c qmem.c Makefile
	gcc $(CFLAGS) qmem_test.c qmem.c -o qmem_test_malloc -lpthread

#####

TARGET=./ptask_test

run: $(tests)
	time $(TARGET)

gdb: $(tests)
	gdb -x run.gdb --args $(TARGET)

pack: ptask.tgz

ptask.tgz: *.c *.h Makefile
	tar cfvz ptask.tgz *.c *.h Makefile

