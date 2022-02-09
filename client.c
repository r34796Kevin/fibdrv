#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define FIB_DEV "/dev/fibonacci"
#define bufsize 1

int main()
{
    long long sz;

    char buf[256];
    // char write_buf[] = "testing writing";
    int offset = 1000; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    /*
    FILE *fp1 = fopen("kerneltime.txt", "w");

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = write(fd, write_buf, 0);
        // printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
        fprintf(fp1, "%d %lld\n", i, sz);
    }
    fclose(fp1);
    FILE *fp3 = fopen("kerneltimefast.txt", "w");

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = write(fd, write_buf, 1);
        // printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
        fprintf(fp3, "%d %lld\n", i, sz);
    }
    fclose(fp3);
    */

    FILE *fp1 = fopen("fib_fast.txt", "w");
    // struct timespec start, end;

    for (int i = 0; i <= offset; i++) {
        struct timespec start, end;
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &start);
        sz = write(fd, buf, bufsize);
        clock_gettime(CLOCK_MONOTONIC, &end);
        // printf("writing from " FIB_DEV
        //       " at offset %d, returned the sequence "
        //       "%s.\n",
        //       i, buf);
        long long ut1 = (long long) (end.tv_sec * 1e9 + end.tv_nsec) -
                        (start.tv_sec * 1e9 + start.tv_nsec);
        // 得到的執行時間寫進time.txt
        fprintf(fp1, "%d %lld %lld\n", i, ut1, sz);
    }
    fclose(fp1);

    FILE *fp2 = fopen("fib_n.txt", "w");
    // struct timespec start, end;

    for (int i = 0; i <= offset; i++) {
        struct timespec start, end;
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &start);
        sz = read(fd, buf, bufsize);
        clock_gettime(CLOCK_MONOTONIC, &end);
        // printf("Reading from " FIB_DEV
        //       " at offset %d, returned the sequence "
        //       "%s.\n",
        //       i, buf);
        long long ut = (long long) (end.tv_sec * 1e9 + end.tv_nsec) -
                       (start.tv_sec * 1e9 + start.tv_nsec);
        // 得到的執行時間寫進time.txt
        fprintf(fp2, "%d %lld %lld\n", i, ut, sz);
    }
    fclose(fp2);


    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        // printf("Reading from " FIB_DEV
        //       " at offset %d, returned the sequence "
        //       "%s.\n",
        //       i, buf);
    }


    close(fd);
    return 0;
}
