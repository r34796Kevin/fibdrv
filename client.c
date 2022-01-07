#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define FIB_DEV "/dev/fibonacci"

int main()
{
    long long sz;

    char buf[128];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    FILE *fp1 = fopen("kerneltime.txt", "w");

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        // printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
        fprintf(fp1, "%d %lld\n", i, sz);
    }
    fclose(fp1);
    FILE *fp2 = fopen("usertime.txt", "w");
    struct timespec start, end;
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &start);
        sz = read(fd, buf, 128);
        clock_gettime(CLOCK_MONOTONIC, &end);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
        long long ut = (long long) (end.tv_sec * 1e9 + end.tv_nsec) -
                       (start.tv_sec * 1e9 + start.tv_nsec);
        // long long ut = (long long) (end.tv_nsec - start.tv_nsec);
        // 得到的執行時間寫進time.txt
        fprintf(fp2, "%d %lld\n", i, ut);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    close(fd);
    fclose(fp2);
    return 0;
}
