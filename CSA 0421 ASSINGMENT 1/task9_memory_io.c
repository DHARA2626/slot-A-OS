/* Task 9 - Memory hierarchy and I/O systems. Self-contained: compile and run. */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>

static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec + t.tv_nsec/1e9; }

/* ---------- Part 1: measured access time for different working-set sizes ---------- */
static double chase_ns(size_t bytes)
{
    size_t n = bytes / sizeof(void *);
    void **a = malloc(n * sizeof(void *));
    size_t *idx = malloc(n * sizeof(size_t));
    for (size_t i = 0; i < n; i++) idx[i] = i;
    srand(1);
    for (size_t i = n - 1; i > 0; i--) { size_t j = rand() % (i + 1); size_t t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    for (size_t i = 0; i < n; i++) a[idx[i]] = &a[idx[(i + 1) % n]];     /* one random cycle */
    long steps = 3000000;
    void **p = &a[idx[0]];
    double t = now();
    for (long i = 0; i < steps; i++) p = (void **)*p;                  /* dependent loads: no overlap */
    double e = now() - t;
    if (p == NULL) puts("");                                           /* keep the loop */
    free(a); free(idx);
    return e / steps * 1e9;
}

/* ---------- Part 2: effect of caching on record look-up ---------- */
#define NREC 20000
#define RECSZ 256
static char cache_tab[NREC][RECSZ];
static int  cached[NREC];

int main(void)
{
    printf("=== Part 1: measured access time vs working-set size (random pointer chasing) ===\n");
    printf("%-14s %-12s %s\n", "set size (KB)", "ns/access", "level it fits in (typical)");
    struct { size_t b; const char *lvl; } L[] = {
        {16*1024, "L1 cache (~32-64 KB, ~1 ns)"}, {128*1024, "L2 cache (~256 KB-2 MB, ~4 ns)"},
        {1024*1024, "L2/L3 cache"}, {8*1024*1024, "L3 cache (~8-64 MB, ~15 ns)"},
        {64*1024*1024, "main memory DRAM (~80-100 ns)"} };
    for (int i = 0; i < 5; i++) {
        double ns = chase_ns(L[i].b);
        printf("%-14zu %-12.1f %s\n", L[i].b / 1024, ns, L[i].lvl);
    }
    printf("(set size in KB; exact boundaries depend on this machine's cache sizes)\n");

    printf("\n=== Part 2: effect of caching on patient-record look-up ===\n");
    int fd = open("records.db", O_RDWR | O_CREAT | O_TRUNC, 0644);
    char rec[RECSZ];
    for (int i = 0; i < NREC; i++) { memset(rec, 'A' + i % 26, RECSZ); if (write(fd, rec, RECSZ) < 0) return 1; }
    fsync(fd);
    long lookups = 200000; unsigned seed = 7; volatile long sink = 0;
    /* hot set: 90% of look-ups go to 5% of the records (typical for active patients) */
    #define PICK() ( ((rand_r(&seed) % 100) < 90) ? (rand_r(&seed) % (NREC/20)) : (rand_r(&seed) % NREC) )

    double t = now();                                   /* A: every look-up is a read() system call */
    for (long i = 0; i < lookups; i++) { int r = PICK(); if (pread(fd, rec, RECSZ, (off_t)r * RECSZ) < 0) return 1; sink += rec[0]; }
    double tA = now() - t;

    memset(cached, 0, sizeof cached); seed = 7; long hits = 0;
    t = now();                                          /* B: small cache in the program's own memory */
    for (long i = 0; i < lookups; i++) {
        int r = PICK();
        if (cached[r]) hits++;
        else { if (pread(fd, cache_tab[r], RECSZ, (off_t)r * RECSZ) < 0) return 1; cached[r] = 1; }
        sink += cache_tab[r][0];
    }
    double tB = now() - t;
    printf("%d records of %d bytes, %ld random look-ups (90%% to the 5%% most active records)\n", NREC, RECSZ, lookups);
    printf("%-38s %8.1f ms  (%.0f ns per look-up)\n", "no cache (read() every time)", tA * 1e3, tA / lookups * 1e9);
    printf("%-38s %8.1f ms  (%.0f ns per look-up, hit ratio %.1f%%)\n", "with in-memory cache", tB * 1e3, tB / lookups * 1e9, 100.0 * hits / lookups);
    printf("speed-up from caching: %.1fx\n", tA / tB);
    close(fd); unlink("records.db");

    double tc = 1.0, tm = 100.0;   /* ns: cache hit, memory (typical) */
    printf("\nEffective access time EAT = h*t_cache + (1-h)*t_memory   (t_cache=%.0f ns, t_memory=%.0f ns)\n", tc, tm);
    double hs[] = {0.50, 0.90, 0.99};
    for (int i = 0; i < 3; i++) printf("  hit ratio %.0f%%  ->  EAT = %.1f ns\n", hs[i] * 100, hs[i] * tc + (1 - hs[i]) * tm);

    printf("\n=== Part 3: MRI image transfer, 500 MB: programmed I/O vs interrupt-per-word vs DMA ===\n");
    /* measure the cost of one trap into the kernel (proxy for interrupt entry/exit cost) */
    long N = 1000000; t = now();
    for (long i = 0; i < N; i++) syscall(SYS_getppid);
    double trap_us = (now() - t) / N * 1e6;
    double bytes = 500.0 * 1024 * 1024;
    double word = 4;                 /* 32-bit word */
    double block = 64 * 1024;        /* DMA block size */
    double irq_cost_us = trap_us < 1.0 ? 1.0 : trap_us;   /* use at least 1 us per interrupt (handler work added) */
    double words = bytes / word, blocks = bytes / block;
    double rate = 1e9;               /* scanner/bus rate: 1 GB/s (assumption) */
    double xfer_s = bytes / rate;
    printf("measured cost of one kernel trap on this machine: %.2f us (interrupt cost assumed >= 1 us)\n", trap_us);
    printf("image = %.0f bytes, word = %.0f bytes, DMA block = %.0f KB, device rate = 1 GB/s (assumption)\n\n", bytes, word, block / 1024);
    printf("%-26s %-16s %-18s %s\n", "method", "interrupts", "CPU time (s)", "buffer memory in RAM");
    printf("%-26s %-16s %-18.2f %s\n", "programmed I/O (polling)", "0", xfer_s, "1 word (CPU 100% busy for whole transfer)");
    printf("%-26s %-16.0f %-18.2f %s\n", "interrupt per word", words, words * irq_cost_us / 1e6, "1 word + image destination");
    printf("%-26s %-16.0f %-18.4f %s\n", "DMA (64 KB blocks)", blocks, blocks * irq_cost_us / 1e6, "64 KB x 2 (double buffer) or image area");
    printf("DMA needs %.0fx fewer interrupts and about %.0fx less CPU time than interrupt-per-word.\n", words / blocks, words / blocks);
    return 0;
}
