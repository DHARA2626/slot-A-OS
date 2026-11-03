#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#define TRAINS 8
#define COACHES 10
#define SEATS 40
#define THREADS 16
#define BOOKINGS 400                 /* per thread */
#define DB_US 100                    /* simulated database/payment latency inside the critical section */
static char seat[TRAINS][COACHES][SEATS];   /* 0 free, 1 sold */
static long sold_attempts[TRAINS][COACHES][SEATS];
static pthread_mutex_t g = PTHREAD_MUTEX_INITIALIZER, tl[TRAINS], cl[TRAINS][COACHES];
static int mode;                     /* 0 global, 1 per train, 2 per coach */
static long sold_total;
static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec/1e9; }
static pthread_mutex_t *pick(int t,int c){ return mode==0?&g: mode==1?&tl[t]:&cl[t][c]; }
static void *agent(void *a){
    unsigned s = (unsigned)(long)a * 7919u + 1;
    for (int i=0;i<BOOKINGS;i++){
        int t=rand_r(&s)%TRAINS, c=rand_r(&s)%COACHES, k=rand_r(&s)%SEATS;
        pthread_mutex_t *m = pick(t,c);
        pthread_mutex_lock(m);
        int free_ = (seat[t][c][k]==0);             /* check */
        usleep(DB_US);                               /* slow work between check and update */
        if (free_) { seat[t][c][k]=1; __atomic_fetch_add(&sold_attempts[t][c][k],1,__ATOMIC_RELAXED); __atomic_fetch_add(&sold_total,1,__ATOMIC_RELAXED); }
        pthread_mutex_unlock(m);
    } return NULL; }
int main(void){
    for(int t=0;t<TRAINS;t++){ pthread_mutex_init(&tl[t],NULL); for(int c=0;c<COACHES;c++) pthread_mutex_init(&cl[t][c],NULL);}
    const char *name[]={"one lock for whole system","one lock per train","one lock per coach"};
    for (mode=0;mode<3;mode++){
        memset(seat,0,sizeof seat); memset(sold_attempts,0,sizeof sold_attempts); sold_total=0;
        pthread_t th[THREADS]; double s=now();
        for(long i=0;i<THREADS;i++) pthread_create(&th[i],NULL,agent,(void*)i);
        for(int i=0;i<THREADS;i++) pthread_join(th[i],NULL);
        double e=now()-s; long dbl=0, sold=0;
        for(int t=0;t<TRAINS;t++)for(int c=0;c<COACHES;c++)for(int k=0;k<SEATS;k++){ if(sold_attempts[t][c][k]>1)dbl++; sold+=seat[t][c][k]; }
        printf("%-27s %6.2f s  %7.0f bookings/s  double-sold seats=%ld  seats sold=%ld\n",name[mode],e,THREADS*BOOKINGS/e,dbl,sold);
    } return 0; }
