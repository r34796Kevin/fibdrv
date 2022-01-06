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
    FILE *fp = fopen("time.txt", "w");

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        struct timespec start, end;
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_REALTIME, &start);
        sz = read(fd, buf, sizeof(buf));
        clock_gettime(CLOCK_REALTIME, &end);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
        long long ut = (long long) (end.tv_sec * 1e9 + end.tv_nsec) -
                       (start.tv_sec * 1e9 + start.tv_nsec);
        //得到的執行時間寫進time.txt
        fprintf(fp, "%d %lld %lld\n", i, ut, atoll(buf));
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
    fclose(fp);
    return 0;
}
