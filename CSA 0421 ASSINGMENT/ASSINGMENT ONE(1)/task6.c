#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

/* Copy src to dst with the read() and write() system calls, using a buffer of
   bufsize bytes. Returns the number of read() calls made, or -1 on error. */
long copy_file(const char *src, const char *dst, size_t bufsize)
{
    char buf[8192];
    if (bufsize == 0 || bufsize > sizeof buf) return -1;   /* protect the buffer */
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { close(in); return -1; }
    long calls = 0;
    ssize_t n;

    /* TODO 1 (completed) */
    while (1) {
        n = read(in, buf, bufsize);      /* one read() system call */
        calls++;                         /* count every read call, including the last one */
        if (n <= 0) break;               /* 0 = end of file, -1 = error */
        ssize_t done = 0;                /* write exactly the n bytes that were read */
        while (done < n) {               /* write() may write fewer bytes, so loop */
            ssize_t w = write(out, buf + done, n - done);
            if (w < 0) { close(in); close(out); return -1; }
            done += w;
        }
    }

    close(in);
    close(out);
    return (n < 0) ? -1 : calls;
}

int main(void)
{
    FILE *f = fopen("records.dat", "w");   /* a 10,000-byte test file */
    for (int i = 0; i < 10000; i++) fputc('A' + i % 26, f);
    fclose(f);

    size_t sizes[] = {1, 512, 4096};
    for (int i = 0; i < 3; i++)
        printf("buffer %4zu bytes: %5ld read() calls\n", sizes[i],
               copy_file("records.dat", "copy.dat", sizes[i]));

    unlink("records.dat");
    unlink("copy.dat");
    return 0;
}
