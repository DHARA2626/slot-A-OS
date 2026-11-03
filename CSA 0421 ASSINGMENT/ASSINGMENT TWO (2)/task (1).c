/* Q1 evaluation: cost of 2000 "student sessions" as processes vs as threads. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <time.h>
#include <sched.h>
#define N 2000
static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec/1e9; }
static long rss_kb(const char *key, const char *path){
    FILE *f=fopen(path,"r"); if(!f) return 0; char l[256]; long v=0;
    while(fgets(l,sizeof l,f)) if(!strncmp(l,key,strlen(key))){ sscanf(l+strlen(key),"%ld",&v); break; }
    fclose(f); return v; }
static void *hold(void *a){ (void)a; sleep(3); return NULL; }

/* context-switch ping-pong: two tasks pinned to one CPU pass a byte back and forth */
static int pp_fd[2][2];
static void *pp_thread(void *a){ long n=(long)a; char c;
    for(long i=0;i<n;i++){ if(read(pp_fd[0][0],&c,1)!=1) break; if(write(pp_fd[1][1],&c,1)!=1) break; } return NULL; }
static double pingpong(int use_threads, long n){
    cpu_set_t s; CPU_ZERO(&s); CPU_SET(0,&s); sched_setaffinity(0,sizeof s,&s);
    pipe(pp_fd[0]); pipe(pp_fd[1]); char c='x'; double t0,t1;
    if(use_threads){ pthread_t t; pthread_create(&t,NULL,pp_thread,(void*)n);
        t0=now(); for(long i=0;i<n;i++){ write(pp_fd[0][1],&c,1); read(pp_fd[1][0],&c,1);} t1=now(); pthread_join(t,NULL);
    } else { pid_t p=fork(); if(p==0){ pp_thread((void*)n); _exit(0);} 
        t0=now(); for(long i=0;i<n;i++){ write(pp_fd[0][1],&c,1); read(pp_fd[1][0],&c,1);} t1=now(); waitpid(p,NULL,0); }
    close(pp_fd[0][0]);close(pp_fd[0][1]);close(pp_fd[1][0]);close(pp_fd[1][1]);
    return (t1-t0)/(2.0*n)*1e6;           /* microseconds per switch (2 switches per round trip) */
}

int main(int argc,char**argv){
    if(argc<2) return 1;
    if(!strcmp(argv[1],"procs")){
        static pid_t p[N]; double t0=now(); int made=0;
        for(int i=0;i<N;i++){ p[i]=fork(); if(p[i]==0){ sleep(3); _exit(0);} if(p[i]<0){perror("fork");break;} made++; }
        double t1=now(); usleep(500000);
        long pss=0,rss=0,vm=0; char path[64];
        for(int i=0;i<made;i++){ snprintf(path,64,"/proc/%d/smaps_rollup",p[i]); pss+=rss_kb("Pss:",path);
                                 snprintf(path,64,"/proc/%d/status",p[i]); rss+=rss_kb("VmRSS:",path); vm+=rss_kb("VmSize:",path);}
        printf("processes: %d created in %.3f s (%.1f us each)\n",made,t1-t0,(t1-t0)/made*1e6);
        printf("           sum VmSize=%ld kB, sum VmRSS=%ld kB, sum PSS=%ld kB  -> PSS/process=%.0f kB\n",vm,rss,pss,(double)pss/made);
        for(int i=0;i<made;i++) waitpid(p[i],NULL,0);
    } else if(!strcmp(argv[1],"threads")){
        static pthread_t t[N]; pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at,64*1024);
        long before=rss_kb("VmRSS:","/proc/self/status"), vmb=rss_kb("VmSize:","/proc/self/status");
        double t0=now(); int made=0;
        for(int i=0;i<N;i++){ if(pthread_create(&t[i],&at,hold,NULL)){perror("pthread_create");break;} made++; }
        double t1=now(); usleep(500000);
        long after=rss_kb("VmRSS:","/proc/self/status"), vma=rss_kb("VmSize:","/proc/self/status");
        printf("threads:   %d created in %.3f s (%.1f us each)\n",made,t1-t0,(t1-t0)/made*1e6);
        printf("           RSS grew %ld kB (=%.1f kB/thread), VmSize grew %ld kB (=%.0f kB/thread, 64 kB stack requested)\n",after-before,(double)(after-before)/made,vma-vmb,(double)(vma-vmb)/made);
        for(int i=0;i<made;i++) pthread_join(t[i],NULL);
    } else if(!strcmp(argv[1],"switch")){
        long n=200000;
        double a=pingpong(0,n), b=pingpong(1,n);
        printf("context switch (pinned to 1 CPU, %ld round trips):\n  process <-> process: %.2f us per switch\n  thread  <-> thread : %.2f us per switch\n",n,a,b);
    }
    return 0;
}
