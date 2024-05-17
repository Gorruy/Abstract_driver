#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kdev_t.h>

#include "abs.h"


static void abs_dev_release(struct device *dev)
{
    pr_info("Releasing device\n");
}

static struct platform_device devices[NUMBER_OF_DEVICES];
static abs_platform_data_t dev_data[NUMBER_OF_DEVICES] = {
    { .size = PAGE_SIZE_IN_BYTES, .index = "ABSDEV1"},
    { .size = PAGE_SIZE_IN_BYTES, .index = "ABSDEV2"},
    { .size = PAGE_SIZE_IN_BYTES, .index = "ABSDEV3"},
    { .size = PAGE_SIZE_IN_BYTES, .index = "ABSDEV4"}
};

static void create_devs(void)
{
  size_t i;
  for ( i = 0; i < NUMBER_OF_DEVICES; i++ ) {
    devices[i].id = i;
    devices[i].name = PLATFORM_DEVICE_NAME;
    devices[i].dev.platform_data = &dev_data[i];
    devices[i].dev.release = abs_dev_release;
  }
}

static int __init abs_devices_init(void)
{
  int err_code;
  size_t i;

  create_devs();

  pr_info("Adding devices\n");
  for ( i = 0; i < NUMBER_OF_DEVICES; i++ ) {
    err_code = platform_device_register(&devices[i]);
    if ( err_code < 0 ) {
      pr_info("Device registration failed for %ld device", i);
      goto init_error;
    }
  }

  return 0;

init_error:
  pr_info("Init failed\n");
  return err_code;
}

static void __exit abs_devices_exit(void)
{
  size_t i;
  for ( i = 0; i < NUMBER_OF_DEVICES; i++ ) {
    platform_device_unregister(&devices[i]);
  }
}

module_init(abs_devices_init);
module_exit(abs_devices_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Mimikin vladimirmimikin@gmail.com");
MODULE_DESCRIPTION("Module for testing purposes");