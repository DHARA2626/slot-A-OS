/* Q1: create child processes with fork() and observe them with ps and /proc.
   Children 0,1 compute (state R), children 2,3 sleep (state S = waiting for I/O/timer),
   child 4 exits at once and is NOT waited for yet (state Z = zombie, "terminated"). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define NCHILD 5

static void burn(double secs)
{
    struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
    volatile unsigned long x = 0;
    do { for (int i = 0; i < 100000; i++) x += i; clock_gettime(CLOCK_MONOTONIC, &b); }
    while ((b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9 < secs);
}

int main(void)
{
    pid_t kids[NCHILD];
    printf("parent pid=%d\n", getpid());
    fflush(stdout);
    for (int i = 0; i < NCHILD; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {                       /* child */
            if (i < 2)       burn(6.0);       /* ready/running */
            else if (i < 4)  sleep(6);        /* waiting       */
            _exit(i);                         /* i==4 exits immediately */
        }
        kids[i] = pid;
        printf("created child %d pid=%d\n", i, pid);
    }
    fflush(stdout);
    sleep(3);                                 /* window for ps and /proc observation */
    for (int i = 0; i < NCHILD; i++) {
        int st; waitpid(kids[i], &st, 0);
        printf("child pid=%d exited with status %d\n", kids[i], WEXITSTATUS(st));
    }
    return 0;
}
