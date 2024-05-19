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
    char buf[4096];
    char ch[20];

    FILE *file = fopen("/dev/abs_dev-0", "w+r");
    FILE *file1 = fopen("/dev/abs_dev-1", "w+r");

    if (!file || !file1) {
        perror("Failed to open file\n");
        exit(1);
    }

    fprintf(file, "Some text\n");
    fprintf(file, "\0");
    fsync(fileno(file));

    rewind(file);

    read(fileno(file), ch, 20);

    printf("Write, read and lseek check. There should be <Some text>:\n");
    printf("%.*s",20, ch);

    memset(ch, 0, 20);

    read(fileno(file1), ch, 20);

    printf("Files independence check. There should be blank line:\n");
    printf("%.*s",20, ch);
    printf("\n");

    memset(ch, 0, 20);

    char *arr;
    arr = mmap(arr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
    arr[0] = 'A';
    arr[1] = 'B';
    arr[2] = 'C';
    arr[3] = 'D';
    arr[4] = '\n'; 
    fsync(fileno(file));

    rewind(file);

    printf("Mmap write check. There should be <ABCD>:\n");
    printf("%.*s",5, arr);

    return 0;
}