#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#define CHUNK (64*1024)
static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec/1e9; }
struct shm { sem_t empty, full; size_t len; char data[CHUNK]; };

static double bench_pipe(size_t total, unsigned long *cs){
    int fd[2]; if (pipe(fd) == -1) { perror("pipe"); exit(1); }
    double s = now(); pid_t p = fork();
    if (p == 0) { close(fd[1]); char *b = malloc(CHUNK); unsigned long sum = 0; ssize_t n;
        while ((n = read(fd[0], b, CHUNK)) > 0) for (ssize_t i=0;i<n;i+=64) sum += (unsigned char)b[i];
        _exit((int)(sum & 0x7f)); }
    close(fd[0]); char *b = malloc(CHUNK); memset(b, 1, CHUNK);
    for (size_t sent = 0; sent < total; sent += CHUNK) if (write(fd[1], b, CHUNK) != CHUNK) perror("write");
    close(fd[1]); int st; waitpid(p,&st,0); *cs = WEXITSTATUS(st); free(b);
    return now() - s;
}
static double bench_shm(size_t total, unsigned long *cs){
    shm_unlink("/ipc_bench");
    int fd = shm_open("/ipc_bench", O_CREAT|O_RDWR, 0600); if (ftruncate(fd, sizeof(struct shm)) == -1) perror("ftruncate");
    struct shm *m = mmap(NULL, sizeof *m, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    sem_init(&m->empty,1,1); sem_init(&m->full,1,0);
    double s = now(); pid_t p = fork();
    if (p == 0) { unsigned long sum = 0;
        for (;;) { sem_wait(&m->full); if (m->len == 0) break;
            for (size_t i=0;i<m->len;i+=64) sum += (unsigned char)m->data[i];
            sem_post(&m->empty); }
        _exit((int)(sum & 0x7f)); }
    char *b = malloc(CHUNK); memset(b, 1, CHUNK);
    for (size_t sent = 0; sent < total; sent += CHUNK) {
        sem_wait(&m->empty); memcpy(m->data, b, CHUNK); m->len = CHUNK; sem_post(&m->full); }
    sem_wait(&m->empty); m->len = 0; sem_post(&m->full);
    int st; waitpid(p,&st,0); *cs = WEXITSTATUS(st); free(b);
    double e = now() - s; munmap(m, sizeof *m); close(fd); shm_unlink("/ipc_bench"); return e;
}
int main(void){
    size_t mb[] = {1, 10, 100};
    printf("%6s %12s %12s %10s\n","MB","pipe (s)","shm+sem (s)","ratio");
    for (int k=0;k<3;k++){ size_t tot = mb[k]*1024*1024; unsigned long c1,c2; double bp=1e9,bs=1e9;
        for (int r=0;r<5;r++){ double a=bench_pipe(tot,&c1); if(a<bp)bp=a; double b=bench_shm(tot,&c2); if(b<bs)bs=b; }
        printf("%6zu %12.4f %12.4f %9.2fx  (checksums %s)\n",mb[k],bp,bs,bp/bs,c1==c2?"match":"DIFFER"); }
    return 0; }
