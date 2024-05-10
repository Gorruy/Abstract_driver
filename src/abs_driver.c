#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <sched.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <errno.h>

#define DEV_NAME "abs_driver"
#define NUMBER_OF_DEVICES 10
#define PAGE_SIZE_IN_BYTES 4096


int abs_driver_maj_num;
int abs_driver_min_num;

typedef struct page { 
  uint8_t data[PAGE_SIZE_IN_BYTES];
} page_t;

struct abs_driver_dev {
  page_t *page;
  size_t size;
  struct semaphore sem;
  struct cdev cdev;
};

int abs_driver_open(struct inode *inode, struct file *file)
{
  struct abs_driver_dev *dev;

  dev = container_of( inode->i_cdev, struct abs_driver_dev, cdev );
  file->private_data = dev;

  return 0;
}

ssize_t abs_driver_read(struct file *file,
                        char __user *buf,
                        size_t count,
                        size_t loff_t *f_pos)
{
  struct abs_driver_dev *dev = file->private_data;

  if (down_interruptable(&dev->sem)) {
    return -ERESTARTSYS;
  }

  if ( *f_pos >= dev->size ) {
    return 0;
  }

  if ( *f_pos + count > dev->size ) {
    count = dev->size - *f_pos;
  }

  if (copy_to_user(buf, dev->page->data[*f_pos], count)) {
    return -EFAULT;
  } 

  *f_pos += count;
  up(&dev->sem);

  return count;
}

ssize_t abs_driver_write(struct file *file,
                         const char __user *buf,
                         size_t count ,
                         loff_t *f_pos)
{
  struct abs_driver_dev *dev = file->private_data;
  ssize_t result = -ENOMEM;

  if (down_interruptable(&dev->sem)) {
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

loff_t abs_driver_llseek(struct file,
                         loff_t,
                         int );

unsigned abs_driver_mmap(struct file *);



static struct file_operations abs_driver_fops = {
  .owner = THIS_MODULE,
  .open = abs_driver_open,
  .read = abs_driver_read,
  .write = abs_driver_write,
  .llseek = abs_driver_llseek,
  .mmap_capabilities = abs_driver_mmap,
  .release = abs_driver_release
}

static int setup_dev(struct abs_driver_dev *dev, int index)
{
  int err_code;
  int dev_num = MKDEV(abs_driver_maj_num, abs_driver_min_num + index);

  cdev_init(&dev->cdev, &abs_driver_fops);
  dev->cdev.owner = THIS_MODULE;
  dev->cdev.ops = &abs_driver_fops;
  err_code = cdev_add(cdev, dev_num, 1);

  return err_code;
}

static int __init initialization_function(void)
{
  dev_t dev_num;
  int err_code;

  err_code = alloc_chrdev_region( *dev_num, abs_driver_min_num, NUMBER_OF_DEVICES, DEV_NAME );
  if ( err_code < 0 ) {
    printk("Alloc failed\n");
    goto error;
  }

  abs_driver_maj_num = MAJOR(dev_num);
  return;

error:
  exit_function();
  return err_code;
}

static void __exit exit_function(void)
{
  if ( unregister_chrdev_region( dev_num, NUMBER_OF_DEVICES ) ) {
    printk( KERN_WARNING "Unregister failed\n");
    return;
  }

}

module_init(initialization_function);
module_exit(exit_function);

MODULE_LICENSE("GPLv3");
MODULE_AUTHOR("Vladimir Mimikin vladimirmimikin@gmail.com");
MODULE_DESCRIPTION("Abstract platform driver");

