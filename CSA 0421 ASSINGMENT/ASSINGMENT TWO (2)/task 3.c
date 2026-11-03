#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#define CAMERAS 64
#define WORK 40000000UL            /* simulated "plate reading" work per frame */

static atomic_int next_cam;        /* work queue index for the pool */
static volatile unsigned long sink[CAMERAS];
static int processed_unsafe = 0;   /* shared counter, NO protection */
static int processed_mutex = 0;    /* shared counter, mutex */
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec/1e9; }

static void process_frame(int cam){        /* CPU-bound, private data only */
    unsigned long x = cam + 1;
    for (unsigned long i = 0; i < WORK; i++) x = x * 6364136223846793005UL + 1442695040888963407UL;
    sink[cam] = x;
    processed_unsafe++;                    /* data race on purpose */
    pthread_mutex_lock(&m); processed_mutex++; pthread_mutex_unlock(&m);
}
static void *one_cam(void *a){ process_frame((int)(long)a); return NULL; }
static void *pool_worker(void *a){
    (void)a;
    for (;;) { int c = atomic_fetch_add(&next_cam, 1); if (c >= CAMERAS) break; process_frame(c); }
    return NULL;
}
static double run_threads(int nthreads, int per_camera){
    processed_unsafe = processed_mutex = 0; atomic_store(&next_cam, 0);
    pthread_t *t = malloc(sizeof(pthread_t) * (per_camera ? CAMERAS : nthreads));
    double s = now();
    if (per_camera) { for (long i=0;i<CAMERAS;i++) pthread_create(&t[i],NULL,one_cam,(void*)i);
                      for (int i=0;i<CAMERAS;i++) pthread_join(t[i],NULL); }
    else            { for (int i=0;i<nthreads;i++) pthread_create(&t[i],NULL,pool_worker,NULL);
                      for (int i=0;i<nthreads;i++) pthread_join(t[i],NULL); }
    double e = now() - s; free(t); return e;
}
int main(void){
    double s = now();
    processed_unsafe = processed_mutex = 0;
    for (int c=0;c<CAMERAS;c++) process_frame(c);
    double seq = now() - s;
    printf("%-22s %8.3f s  speed-up %5.2f  (unsafe=%d mutex=%d)\n","sequential",seq,1.0,processed_unsafe,processed_mutex);
    int pools[] = {4,8,16};
    for (int k=0;k<3;k++){ double t = run_threads(pools[k],0);
        printf("pool of %-2d threads     %8.3f s  speed-up %5.2f  (unsafe=%d mutex=%d)\n",pools[k],t,seq/t,processed_unsafe,processed_mutex);}
    double t = run_threads(0,1);
    printf("one thread per camera  %8.3f s  speed-up %5.2f  (unsafe=%d mutex=%d)\n",t,seq/t,processed_unsafe,processed_mutex);
    return 0;
}
