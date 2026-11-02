/* Task 8 - CPU modes, protection, polling vs interrupts. Self-contained. */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <sys/resource.h>

static sigjmp_buf jb;
static void on_fault(int s) { (void)s; siglongjmp(jb, 1); }
static int x = 1;

static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec + t.tv_nsec/1e9; }
static double cpu(void){ struct rusage r; getrusage(RUSAGE_SELF,&r);
    return r.ru_utime.tv_sec + r.ru_stime.tv_sec + (r.ru_utime.tv_usec + r.ru_stime.tv_usec)/1e6; }

int main(void)
{
    signal(SIGSEGV, on_fault);
    signal(SIGILL, on_fault);

    printf("===== Part 1: privileged instruction in user mode =====\n");
    if (sigsetjmp(jb, 1) == 0) {
        printf("user program executes 'cli' (disable interrupts)...\n"); fflush(stdout);
#if defined(__x86_64__) || defined(__i386__)
        __asm__ volatile("cli");
#elif defined(__aarch64__)
        __asm__ volatile("msr daifset, #2");
#endif
        printf("cli succeeded (this should never print)\n");
    } else
        printf("-> CPU raised a protection fault, instruction NOT executed (user mode cannot disable interrupts)\n");

    printf("\n===== Part 2: memory protection =====\n");
    printf("parent: &x = %p, x = %d\n", (void *)&x, x); fflush(stdout);
    pid_t p = fork();
    if (p == 0) { x = 99; printf("child : &x = %p, x = %d (child changed it)\n", (void *)&x, x); fflush(stdout); _exit(0); }
    waitpid(p, 0, 0);
    printf("parent: &x = %p, x = %d (unchanged: separate address space)\n", (void *)&x, x);
    if (sigsetjmp(jb, 1) == 0) {
        volatile char *k = (volatile char *)0xffffffff81000000UL;   /* kernel address */
        printf("user program reads kernel address %p ...\n", (void *)k); fflush(stdout);
        char c = *k; printf("read %d (should never print)\n", c);
    } else
        printf("-> MMU fault: user mode may not read kernel memory\n");

    printf("\n===== Part 3: polling vs interrupt-driven (blocking) wait =====\n");
    printf("%-34s %12s %12s %12s\n", "method", "worst(ms)", "avg(ms)", "CPU(ms)");
    const char *name[] = {"busy polling (no sleep)", "polling every 10 ms", "polling every 100 ms", "blocking read (interrupt-driven)"};
    int us[] = {0, 10000, 100000, -1};
    double ev[] = {0.13, 0.27, 0.41};                 /* abnormal reading happens at these times (s) */
    for (int m = 0; m < 4; m++) {
        double worst = 0, sum = 0, c0 = cpu();
        for (int e = 0; e < 3; e++) {
            int fd[2]; if (pipe(fd) < 0) return 1;
            pid_t q = fork();
            if (q == 0) { close(fd[0]);
                struct timespec d = {0, (long)(ev[e] * 1e9)}; nanosleep(&d, 0);
                double t = now(); if (write(fd[1], &t, sizeof t) < 0) _exit(1); _exit(0); }
            close(fd[1]); double tev;
            if (us[m] < 0) { if (read(fd[0], &tev, sizeof tev) < 0) return 1; }
            else { fcntl(fd[0], F_SETFL, O_NONBLOCK);
                   while (read(fd[0], &tev, sizeof tev) != sizeof tev) if (us[m]) usleep(us[m]); }
            double d = (now() - tev) * 1e3; if (d > worst) worst = d; sum += d;
            waitpid(q, 0, 0); close(fd[0]);
        }
        printf("%-34s %12.2f %12.2f %12.1f\n", name[m], worst, sum / 3, (cpu() - c0) * 1e3);
    }
    printf("\nPolling delay is up to the polling interval; interrupts react almost at once and use little CPU.\n");
    return 0;
}
