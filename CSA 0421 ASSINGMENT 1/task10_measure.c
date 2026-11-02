/* Task 10 - Measure and compare. Self-contained: compile with  gcc -O2 task10_measure.c -o m -lpthread  and run. */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/resource.h>

static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec + t.tv_nsec/1e9; }
static void cpu_times(double *u, double *s){ struct rusage r; getrusage(RUSAGE_SELF,&r);
    *u = r.ru_utime.tv_sec + r.ru_utime.tv_usec/1e6; *s = r.ru_stime.tv_sec + r.ru_stime.tv_usec/1e6; }

/* /proc/self/io: syscr = read() calls, syscw = write() calls made by this process (Linux) */
static void io_calls(long *r, long *w){
    *r = *w = -1; FILE *f = fopen("/proc/self/io","r"); if(!f) return; char l[128];
    while (fgets(l,sizeof l,f)) { sscanf(l,"syscr: %ld",r); sscanf(l,"syscw: %ld",w); } fclose(f); }

static long copy(const char *src,const char *dst,size_t bs){
    char *b = malloc(bs); int in = open(src,O_RDONLY), out = open(dst,O_WRONLY|O_CREAT|O_TRUNC,0644);
    long calls = 0; ssize_t n;
    while (1) { n = read(in,b,bs); calls++; if (n <= 0) break; ssize_t d = 0; while (d < n) { ssize_t w = write(out,b+d,n-d); if (w<0) return -1; d += w; } }
    close(in); close(out); free(b); return calls; }

static void *nop(void *a){ return a; }
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER; static pthread_cond_t cv = PTHREAD_COND_INITIALIZER; static int go = 0;
static void *waiter(void *a){ pthread_mutex_lock(&mu); while(!go) pthread_cond_wait(&cv,&mu); pthread_mutex_unlock(&mu); return a; }

static long proc_kb(const char *path,const char *key){      /* read a "Key:   N kB" line */
    FILE *f = fopen(path,"r"); if(!f) return -1; char l[256]; long v = -1;
    while (fgets(l,sizeof l,f)) { size_t k = strlen(key); if (!strncmp(l,key,k)) { sscanf(l+k," %ld",&v); break; } }
    fclose(f); return v; }

int main(void)
{
    printf("=== Measurement A: copy a 4,000,000-byte file with different buffer sizes ===\n");
    FILE *g = fopen("src.dat","w"); for (long i = 0; i < 4000000; i++) fputc('A' + i%26, g); fclose(g);
    printf("%-10s %-12s %-10s %-10s %-10s %-10s %s\n","buffer","read calls","write calls","user(ms)","sys(ms)","wall(ms)","sys share");
    size_t bs[] = {1, 16, 64, 512, 4096, 65536};
    for (int i = 0; i < 6; i++) {
        long r0,w0,r1,w1; double u0,s0,u1,s1; io_calls(&r0,&w0); cpu_times(&u0,&s0); double t = now();
        long calls = copy("src.dat","dst.dat",bs[i]);
        double wall = now() - t; cpu_times(&u1,&s1); io_calls(&r1,&w1);
        double u = (u1-u0)*1e3, s = (s1-s0)*1e3;
        printf("%-10zu %-12ld %-10ld %-10.1f %-10.1f %-10.1f %.0f%%\n", bs[i], calls, w1-w0, u, s, wall*1e3, s+u > 0 ? 100*s/(s+u) : 0);
    }
    unlink("src.dat"); unlink("dst.dat");

    printf("\n=== Measurement B: time to create 1,000 processes vs 1,000 threads ===\n");
    double u0,s0,u1,s1; int st;
    cpu_times(&u0,&s0); double t = now();
    for (int i = 0; i < 1000; i++) { pid_t p = fork(); if (p == 0) _exit(0); waitpid(p,&st,0); }
    double tp = now() - t; cpu_times(&u1,&s1);
    printf("%-26s wall %7.1f ms   user %6.1f ms   sys %6.1f ms\n","1000 x fork()+wait", tp*1e3,(u1-u0)*1e3,(s1-s0)*1e3);
    cpu_times(&u0,&s0); t = now();
    for (int i = 0; i < 1000; i++) { pthread_t th; pthread_create(&th,0,nop,0); pthread_join(th,0); }
    double tt = now() - t; cpu_times(&u1,&s1);
    printf("%-26s wall %7.1f ms   user %6.1f ms   sys %6.1f ms\n","1000 x pthread_create+join", tt*1e3,(u1-u0)*1e3,(s1-s0)*1e3);
    printf("threads were %.1fx faster to create than processes\n", tp/tt);

    printf("\n=== Measurement C: memory used by 100 live processes vs 100 live threads ===\n");
    size_t big = 64u*1024*1024; char *m = malloc(big); memset(m,1,big);          /* parent owns 64 MB */
    long rss0 = proc_kb("/proc/self/status","VmRSS:");
    pid_t kids[100]; int n = 0;
    for (; n < 100; n++) { pid_t p = fork(); if (p == 0) { pause(); _exit(0); } if (p < 0) break; kids[n] = p; }
    usleep(200000);
    long priv = 0, rss_sum = 0;
    for (int i = 0; i < n; i++) { char path[64]; snprintf(path,sizeof path,"/proc/%d/smaps_rollup",kids[i]);
        long pd = proc_kb(path,"Private_Dirty:"), pc = proc_kb(path,"Private_Clean:"), r = proc_kb(path,"Rss:");
        if (pd >= 0) { priv += pd + pc; }
        if (r >= 0) { rss_sum += r; } }
    printf("parent RSS before fork: %ld KB (64 MB array touched)\n", rss0);
    printf("%d children created: sum of their RSS = %ld KB, but PRIVATE (really copied) memory = %ld KB total (%ld KB each)\n",
           n, rss_sum, priv, n ? priv/n : 0);
    printf("-> fork shares the 64 MB copy-on-write; each process only pays for its own page tables, stack and written pages\n");
    for (int i = 0; i < n; i++) kill(kids[i],SIGKILL);
    for (int i = 0; i < n; i++) waitpid(kids[i],&st,0);

    long rss1 = proc_kb("/proc/self/status","VmRSS:"), vm1 = proc_kb("/proc/self/status","VmSize:");
    pthread_t th[100]; int k = 0; go = 0;
    for (; k < 100; k++) if (pthread_create(&th[k],0,waiter,0)) break;
    usleep(200000);
    long rss2 = proc_kb("/proc/self/status","VmRSS:"), vm2 = proc_kb("/proc/self/status","VmSize:");
    printf("%d threads created: process RSS %ld KB -> %ld KB (+%ld KB, %ld KB per thread), virtual size +%ld KB (8 MB stacks reserved)\n",
           k, rss1, rss2, rss2-rss1, k ? (rss2-rss1)/k : 0, vm2-vm1);
    printf("-> threads share the whole address space (code, heap, files); each adds only a stack and a thread record\n");
    pthread_mutex_lock(&mu); go = 1; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&mu);
    for (int i = 0; i < k; i++) pthread_join(th[i],0);
    free(m);
    return 0;
}
