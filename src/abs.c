#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>

#include "abs.h"

#define DEV_NAME "abs"
#define CLASS_NAME "abs_class"


static struct class *abs_class;
static int abs_maj_num;
static int abs_minimal_minor;

typedef struct abs_page {
  uint8_t data[PAGE_SIZE_IN_BYTES];
} page_t;

struct abs_private_dev_data {
  abs_platform_data_t platform_data;
  page_t *page;
  uint64_t size;
  struct semaphore sem;
  struct cdev cdev;
  dev_t dev_num;
};

static int abs_open(struct inode *inode, struct file *file)
{
  struct abs_private_dev_data *dev;

  dev = container_of( inode->i_cdev, struct abs_private_dev_data, cdev );
  file->private_data = dev;

  return 0;
}

static ssize_t abs_read(struct file *file,
                               char __user *buf,
                               size_t count,
                               loff_t *f_pos)
{
  struct abs_private_dev_data *dev = file->private_data;

  if (down_interruptible(&dev->sem)) {
    return -ERESTARTSYS;
  }

  if ( *f_pos >= dev->size ) {
    return 0;
  }

  if ( *f_pos + count > dev->size ) {
    count = dev->size - *f_pos;
  }

  if (copy_to_user(buf, &dev->page->data[*f_pos], count)) {
    return -EFAULT;
  } 

  *f_pos += count;
  up(&dev->sem);

  return count;
}

static ssize_t abs_write(struct file *file,
                                const char __user *buf,
                                size_t count ,
                                loff_t *f_pos)
{
  struct abs_private_dev_data *dev = file->private_data;
  ssize_t result = -ENOMEM;

  if (down_interruptible(&dev->sem)) {
    return -ERESTARTSYS;
  }

  if ( *f_pos >= PAGE_SIZE_IN_BYTES ) {
    return result;
  }

  if (copy_from_user(dev->page->data, buf, count)) {
    result = -EFAULT;
  }

  up(&dev->sem);
  return result;
  
}

static loff_t abs_llseek(struct file *file,
                                loff_t offset,
                                int whence)
{
  struct abs_private_dev_data *private_data = file->private_data;
  int max_size = private_data->size;

  loff_t temp;

  pr_info("Starting lseek\n");
  
  switch(whence)
  {
    case SEEK_SET:
      if( (offset > max_size) || (offset < 0) ) {
        return -EINVAL;
      }
      file->f_pos = offset;
      break;

    case SEEK_CUR:
      temp = file->f_pos + offset;
      if ( (temp > max_size) || (temp < 0)) {
        return -EINVAL;
      }
      file->f_pos = temp;
      break;

    case SEEK_END:
      temp = max_size + offset;
      if ( (temp > max_size) || (temp < 0)) {
        return -EINVAL;
      }
      file->f_pos = temp;
      break;
    
    default:
      return -EINVAL;
  }

  pr_info("Lseek is finished\n");

  return file->f_pos;
}

static int abs_mmap(struct file *file,
                            struct vm_area_struct *area)
{
  int result;
  return result;
}

static int abs_release (struct inode *node,
                                struct file *file)
{
  int result;
  return result;
}

static struct file_operations abs_fops = {
  .owner = THIS_MODULE,
  .open = abs_open,
  .read = abs_read,
  .write = abs_write,
  .llseek = abs_llseek,
  .mmap = abs_mmap,
  .release = abs_release
};

static int setup_dev(struct abs_private_dev_data *dev, int index)
{
  int err_code;
  int dev_num = MKDEV(abs_maj_num, abs_minimal_minor + index);

  cdev_init(&dev->cdev, &abs_fops);
  dev->cdev.owner = THIS_MODULE;
  dev->cdev.ops = &abs_fops;
  err_code = cdev_add(&dev->cdev, dev_num, 1);

  return err_code;
}

static int abs_probe(struct platform_device *dev_to_bind) 
{
  int result;

  struct abs_private_dev_data *dev_data;
  abs_platform_data_t *platform_data;

  platform_data = dev_get_platdata(&dev_to_bind->dev);
  if (!platform_data) {
    pr_info("Can't obtain any platform data from device\n");
    return -EINVAL;
  }

  dev_data = devm_kzalloc( &dev_to_bind->dev, sizeof(struct abs_private_dev_data), GFP_KERNEL );
  if (!dev_data) {
    pr_info("Memory allocation failed!\n");
    return -ENOMEM;
  }
  
  dev_set_drvdata(&dev_to_bind->dev, dev_data);

  dev_data->platform_data = *platform_data;
  dev_data->page = devm_kzalloc( &dev_to_bind->dev, PAGE_SIZE_IN_BYTES, GFP_KERNEL );

  dev_data->dev_num = MKDEV(abs_maj_num, dev_to_bind->id);
  result = setup_dev(dev_data, dev_to_bind->id);
  if ( result < 0 ) {
    pr_err("Failed to setup chrdev\n");
    goto probe_error;
  }

  

  return result;

probe_error:
  return result;
}
static int abs_remove(struct platform_device *dev)
{
  int result;
  return result;
}

struct platform_driver abs_platform_driver = {
  .probe = abs_probe,
  .remove = abs_remove,
  .driver = {
    .name = PLATFORM_DEVICE_NAME
  }
};

static void abs_cleanup(void)
{
  pr_info("Cleaning\n");
  unregister_chrdev_region( MKDEV(abs_maj_num, abs_minimal_minor), NUMBER_OF_DEVICES );
  platform_driver_unregister(&abs_platform_driver);
  class_destroy(abs_class);
}

static int __init abs_init(void)
{
  dev_t dev_num;
  int err_code;

  pr_info("Allocating chrdev region\n");
  err_code = alloc_chrdev_region( &dev_num, abs_minimal_minor, NUMBER_OF_DEVICES, DEV_NAME );
  if ( err_code < 0 ) {
    pr_info("Alloc failed\n");
    goto error;
  }

  abs_maj_num = MAJOR(dev_num);

  abs_class = class_create( THIS_MODULE, CLASS_NAME );
  if ( IS_ERR(abs_class) ) {
    pr_info("Failed to register class!\n");
    goto error;
  }

  platform_driver_register(&abs_platform_driver);

  pr_info("Driver registered\n");

  return err_code;

error:
  abs_cleanup();
  return err_code;
}

module_init(abs_init);
module_exit(abs_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Mimikin vladimirmimikin@gmail.com");
MODULE_DESCRIPTION("Abstract platform driver");
