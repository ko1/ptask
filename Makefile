
all: ptask_test qmem_test qmem_test_malloc ptask_bench tq_test

CFLAGS=-O3 -g -Wall

tests=ptask_test ptask_bench tq_test qmem_test_malloc qmem_test

clean:
	rm -f $(tests)

ptask_test: ptask.c ptask.h tq_*.c ptask_test.c Makefile
	gcc $(CFLAGS) ptask.c ptask.h ptask_test.c -o ptask_test -lpthread

ptask_bench: ptask.c ptask.h tq_*.c ptask_bench.c Makefile
	gcc $(CFLAGS) ptask.c ptask.h ptask_bench.c -o ptask_bench -lpthread

tq_test: tq_test.c tq_*.c Makefile qmem.c qmem.h
	gcc $(CFLAGS) tq_test.c qmem.c -o tq_test -lpthread

qmem_test: qmem_test.c qmem.c qmem.h Makefile
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
