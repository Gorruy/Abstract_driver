#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>


int main(void) 
{
    char ch[4096];
    int ret;

    FILE *file = fopen("/dev/abs_dev-0", "w+r");
    FILE *file1 = fopen("/dev/abs_dev-1", "w+r");
    FILE *file2 = fopen("/sys/devices/platform/abs_platform_device.0/abs_address", "wb");
    FILE *file3 = fopen("/sys/devices/platform/abs_platform_device.0/abs_value", "wb");

    if (!file || !file1 || !file2 || !file3) {
        perror("Failed to open file\n");
        exit(1);
    }

    memset(ch, 0, 4096);
    rewind(file);

    ch[0] = 0;
    ret = fwrite(ch, 1, 4, file2);
    fflush(file2);
    ch[0] = 'x';
    ret = fwrite(ch, 1, 1, file3);
    fflush(file3);
    ch[0] = 1;
    ret = fwrite(ch, 1, 4, file2);
    fflush(file2);
    ch[0] = 'y';
    ret = fwrite(ch, 1, 1, file3);
    fflush(file3);
    ch[0] = 2;
    ret = fwrite(ch, 1, 4, file2);
    fflush(file2);
    ch[0] = 'z';
    ret = fwrite(ch, 1, 1, file3);
    fflush(file3);

    ret = read(fileno(file), ch, 3);
    ch[3] = '\0';

    printf("There should be <xyz>:\n");
    printf("%s\n", ch);

    fprintf(file, "Some text\n");
    fsync(fileno(file));

    rewind(file);

    ret = read(fileno(file), ch, 20);

    printf("Write, read and lseek check. There should be <Some text>:\n");
    printf("%.*s",20, ch);

    memset(ch, 0, 20);

    ret = read(fileno(file1), ch, 20);

    printf("Files independence check. There should be blank line:\n");
    printf("%.*s",20, ch);
    printf("\n");

    memset(ch, 0, 20);

    char *arr;
    arr = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
    arr[0] = 'A';
    arr[1] = 'B';
    arr[2] = 'C';
    arr[3] = 'D';
    arr[4] = '\n';
    arr[5] = 0; 
    fsync(fileno(file));

    rewind(file);

    ret = read(fileno(file), ch, 5);

    printf("Mmap write check. There should be <ABCD>:\n");
    printf("%.*s",5, ch);

    return 0;
}