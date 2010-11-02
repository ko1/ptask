
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <stdio.h>

typedef struct {
    // struct rusage rusage;
    char *name;
    struct tms tms;
    struct timeval tv;
} benchmark_t;

static benchmark_t *
benchmark_start(const char *name)
{
    benchmark_t *bm = malloc(sizeof(benchmark_t));
    // getrusage(RUSAGE_SELF, &bm->rusage);
    bm->name = malloc(strlen(name) + 1);
    strcpy(bm->name, name);

    times(&bm->tms);
    gettimeofday(&bm->tv, 0);
    return bm;
}

static double
timeval2double(struct timeval *tv)
{
    return (tv->tv_sec * 1000000 + tv->tv_usec) / 1000000.0;
}

static void
benchmark_finish(benchmark_t *start_bm)
{
    benchmark_t finish_bm;
    double sys, user, real;
    long clock_tick;

    // getrusage(RUSAGE_SELF, &finish_bm.rusage);
    times(&finish_bm.tms);
    gettimeofday(&finish_bm.tv, 0);

    clock_tick = sysconf(_SC_CLK_TCK);

    sys = (finish_bm.tms.tms_stime - start_bm->tms.tms_stime) / (double)clock_tick;
    user = (finish_bm.tms.tms_utime - start_bm->tms.tms_utime) / (double)clock_tick;
    real = timeval2double(&finish_bm.tv) - timeval2double(&start_bm->tv);
    fprintf(stdout, "%s\t%f\t%f\t%f\n", start_bm->name, user, sys, real);

    free(start_bm->name);
    free(start_bm);
}
