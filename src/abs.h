#ifndef ABSD
#define ABSD

#include <linux/types.h>
#include <linux/cdev.h>

#define PAGE_SIZE_IN_BYTES 4096
#define NUMBER_OF_DEVICES 4
#define PLATFORM_DEVICE_NAME "abs_platform_device"
#define DEV_NAME "abs"
#define INDEX_SIZE 3 + 3
#define CLASS_NAME "abs_class"
#define DRIVER_NAME "abs_driver"

typedef struct abs_platform_data {
    uint8_t *data;
    char *index;
    size_t size;
} abs_platform_data_t;
#endif