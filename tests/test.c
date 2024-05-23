#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define DEV_1 "/dev/abs_dev-0"
#define DEV_2 "/dev/abs_dev-1"
#define DEV_3 "/dev/abs_dev-2"
#define DEV_4 "/dev/abs_dev-3"

#define PAGE_SIZE 4096


int mmap_check(void)
{
    void *buf;
    int res;
    
    char buf_to_read[PAGE_SIZE];
    memset(buf_to_read, 0, PAGE_SIZE);

    char string[] = "Some string to put in mem\n";
    size_t str_size = strlen(string);

    FILE *filp1 = fopen(DEV_1, "r+w");
    FILE *filp2 = fopen(DEV_2, "r+w");
    FILE *filp3 = fopen(DEV_3, "r+w");
    FILE *filp4 = fopen(DEV_4, "r+w");

    rewind(filp2);
    res = write(fileno(filp2), buf_to_read, PAGE_SIZE);
    if (res < 0) {
        perror("Writing failed\n");
        return -1;
    }

    fsync(fileno(filp2));

    buf = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(filp2), 0);
    if (buf == MAP_FAILED) {
        perror("Mmap failed\n");
        return -1;
    }
    
    memcpy(buf, string, str_size);
    msync(buf, PAGE_SIZE, MS_SYNC);

    rewind(filp2);

    memset(buf_to_read, 0, PAGE_SIZE);
    res = read(fileno(filp2), buf_to_read, str_size);
    if (res < 0 || res != str_size) {
        fprintf(stderr, "Failed to read from mapped mem,\n");
        return -1;
    }

    if (strcmp(string, buf_to_read) != 0) {
        fprintf(stderr, "Wrong data in file after mmap\n");
        return -1;
    }

    memset(buf_to_read, 'A', 100);
    rewind(filp2);
    res = write(fileno(filp2), buf_to_read, 100);

    msync(buf, PAGE_SIZE, MS_SYNC);
    fsync(fileno(filp2));

    for (int i = 0; i < 100; i++) {
        if (((char*)buf)[i] != 'A') {
            fprintf(stderr, "Wrong data in mapped memory after writing to file\n");
            return -1;
        }
    }

    return 0;
}

int write_read_check(void) 
{
    char buf[PAGE_SIZE];
    char buf1[PAGE_SIZE];

    memset(buf, 1, PAGE_SIZE);
    memset(buf1, 0, PAGE_SIZE);
    int ret;

    FILE *filp1 = fopen(DEV_1, "r+w");
    FILE *filp2 = fopen(DEV_2, "r+w");

    if (!filp1 || !filp2) {
        perror("Failed to open file\n");
        return -1;
    }

    ret = write(fileno(filp1), buf, PAGE_SIZE);
    if (ret < 0 || ret != PAGE_SIZE) {
        perror("Failed to write in first dev\n");
        return -1;
    }


    // Check if write increments file offset
    ret = write(fileno(filp1), buf, PAGE_SIZE);
    if (ret != 0) {
        perror("Writing to end of file done wrong!\n");
        return -1;
    }

    rewind(filp1);

    ret = read(fileno(filp1), buf1, PAGE_SIZE);
    if (ret < 0 || ret != PAGE_SIZE) {
        perror("Failed to read from file\n");
        return -1;
    }

    for (int i = 0; i < PAGE_SIZE; i++) {
        if (buf1[i] != 1) {
            fprintf(stderr, "Data in %d element is wrong", i);
            return -1;
        }
    }

    ret = write(fileno(filp2), buf, PAGE_SIZE * 2);
    if (ret < 0 || ret != PAGE_SIZE) {
        perror("Failed to write to second file\n");
        return -1;
    }

    fclose(filp1);
    fclose(filp2);

    return 0;

}

int lseek_check(void) {

    FILE *filp1 = fopen(DEV_1, "r+w");
    FILE *filp2 = fopen(DEV_2, "r+w");

    
}

int sysfs_check(void) {

    FILE *filp1 = fopen("/sys/devices/platform/abs_platform_device.0/abs_value", "wb+");
    FILE *filp2 = fopen("/sys/devices/platform/abs_platform_device.0/abs_address", "wb+");

    char *ch[4];
    memset(ch, 0, 4);

    ch[0] = 100;

    fwrite(ch, 1, 4, filp2);
    fflush(filp1);

}

int main(void) 
{
    if (write_read_check()) {
        exit(1);
    }

    if (mmap_check()) {
        exit(1);
    }

    sysfs_check();
    printf("All tests passed!\n");
    exit(0);
}