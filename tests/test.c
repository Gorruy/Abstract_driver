#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>


int main(void) 
{
    char buf[4096];

    FILE *file = fopen("/dev/abs_dev-0", "w+r");
    FILE *file1 = fopen("/dev/abs_dev-1", "w+r");

    if (!file) {
        perror("Failed to open file\n");
        exit(1);
    }

    fprintf(file, "Some text\n");
    fprintf(file, "\0");
    fsync(fileno(file));

    rewind(file);

    printf("There should be blank line:\n");
    char ch;
    do {
        ch = fgetc(file1);
        if (ch==EOF) {
            printf("\n");
            break;
        }
        printf("%c", ch);
 
    } while (ch != EOF);

    printf("There should be <Some text>:\n");
    do {
        ch = fgetc(file);
        if (ch==EOF) {
            break;
        }
        printf("%c", ch);
 
    } while (ch != EOF);
 
    return 0;
}