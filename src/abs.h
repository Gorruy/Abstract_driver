#ifndef ABSD
#define ABSD

#include <linux/types.h>
#include <linux/cdev.h>

#define PAGE_SIZE_IN_BYTES 4096
#define NUMBER_OF_DEVICES 4
#define PLATFORM_DEVICE_NAME "abs_platform_device"

typedef struct abs_platform_data {
  int size;
  int permission;
  char *index;
} abs_platform_data_t;
#endif