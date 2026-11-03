/* Task 5: how a program obtains OS services.
   mode "raw"   : open()/read()/write()/close() system calls directly
   mode "stdio" : fopen()/fread()/fwrite() C library functions            */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define FILE_NAME "patient_log.txt"

static void make_file(void)                 /* 10,000-byte input file */
{
    FILE *f = fopen(FILE_NAME, "w");
    for (int i = 0; i < 10000; i++) fputc('A' + i % 26, f);
    fclose(f);
}

static long raw_count(size_t b)             /* count bytes with read() */
{
    char *buf = malloc(b);
    int fd = open(FILE_NAME, O_RDONLY);     /* system call: open  */
    long total = 0; ssize_t n;
    while ((n = read(fd, buf, b)) > 0)      /* system call: read  */
        total += n;
    close(fd);                              /* system call: close */
    free(buf);
    return total;
}

static long stdio_count(size_t b)           /* count bytes with fread() */
{
    char *buf = malloc(b);
    FILE *f = fopen(FILE_NAME, "r");        /* library: wraps openat */
    long total = 0; size_t n;
    while ((n = fread(buf, 1, b, f)) > 0)   /* library: buffered, wraps read */
        total += n;
    fclose(f);
    free(buf);
    return total;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s raw|stdio bufsize\n", argv[0]); return 1; }
    size_t b = strtoul(argv[2], NULL, 10);
    make_file();
    long total = strcmp(argv[1], "raw") == 0 ? raw_count(b) : stdio_count(b);
    printf("mode=%s buffer=%zu bytes_read=%ld\n", argv[1], b, total);
    unlink(FILE_NAME);
    return 0;
}
