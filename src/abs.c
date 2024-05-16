#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <asm/page.h>
#include <asm/io.h
#include <linux/mod_devicetable.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif>

#include "abs.h"

#define DEV_NAME "abs"
#define CLASS_NAME "abs_class"


static struct class *abs_class;
static int abs_maj_num;
static int abs_minimal_minor;

static int abs_open(struct inode*, struct file*);
static ssize_t abs_read(struct file*, char __user*, size_t, loff_t*);
static ssize_t abs_write(struct file*, const char __user*, size_t, loff_t*);
static loff_t abs_llseek(struct file*, loff_t, int);
static int abs_mmap( struct file*, struct vm_area_struct*);
static int abs_release (struct inode*, struct file*);


struct abs_private_dev_data {
    abs_platform_data_t platform_data;
    int size;
    struct cdev cdev;
    dev_t dev_num;
    struct device *sys_fs_device;
    struct mutex mtx;
};

static struct file_operations abs_fops = {
    .owner = THIS_MODULE,
    .open = abs_open,
    .read = abs_read,
    .write = abs_write,
    .llseek = abs_llseek,
    .mmap = abs_mmap,
    .release = abs_release
};

static int setup_chrdev(struct abs_private_dev_data *dev, int index)
{
    int err_code;
    int dev_num = MKDEV(abs_maj_num, abs_minimal_minor + index);
  
    err_code = cdev_init(&dev->cdev, &abs_fops);
    if (err_code < 0) {
      pr_warn("Chrdev init failed!\n");
      goto setup_init_error;
    }
    
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &abs_fops;
  
    err_code = cdev_add(&dev->cdev, dev_num, 1);
    if (err_code < 0) {
      pr_warn("Chrdev adding failed!\n");
      goto setup_add_error;
    }
    
    return 0;

setup_init_error:
    return err_code;

setup_add_error:
    cdev_del(&dev);
    return err_code;
}

static ssize_t abs_show(struct device *dev,
                        struct device_attribute *attr,
                        char *buf)
{
    return 0;
}

static ssize_t abs_store(struct device *dev, 
                         struct device_attribute *attr,
                         const char *buf, 
                         size_t count)
{
    return 0;
}

static int abs_probe(struct platform_device *dev_to_bind) 
{
    pr_info("Binding started\n");
  
    int result;
    abs_platform_data_t *platform_data;
    struct abs_private_dev_data *dev_data;
    struct device *abs_dev_fs;
  
    platform_data = dev_get_platdata(&dev_to_bind->dev);
    if (!platform_data) {
        pr_warn("Can't obtain any platform data from device\n");
        result = -EINVAL;
        goto probe_error;
    }
  
    dev_data = devm_kzalloc(&dev_to_bind->dev, 
                            sizeof(struct abs_private_dev_data), 
                            GFP_KERNEL);
    if (!dev_data) {
        pr_warn("Memory allocation failed for dev struct!\n");
        result = -ENOMEM;
        goto probe_error;
    }
    
    dev_set_drvdata(&dev_to_bind->dev, dev_data);
  
    dev_data->platform_data = *platform_data;
  
    dev_data->platform_data->data = devm_kzalloc(&dev_to_bind->dev, 
                                                 PAGE_SIZE_IN_BYTES, 
                                                 GFP_DMA);
    if (!dev_data->platform_data->data) {
        pr_warn("Memory allocation failed for page allocation\n");
        result = -ENOMEM;
        goto probe_error;
    }
  
    dev_data->dev_num = MKDEV(abs_maj_num, dev_to_bind->id);
    result = setup_chrdev(dev_data, dev_to_bind->id);
    if ( result < 0 ) {
        pr_warn("Failed to setup chrdev\n");
        goto probe_error;
    }
    pr_info("Bounding succeed\n");
  
    abs_dev_fs = device_create(abs_class, 
                               NULL, 
                               dev_data->dev_num,
                               NULL, 
                               "abs_dev-%d",dev_to_bind->id);
    if (IS_ERR(abs_dev_fs)) {
        pr_warn("Failed to create /dev file for %d device", dev_to_bind->id);
        goto probe_error;
    }
    dev_data->sys_fs_device = abs_dev_fs;
  
    pr_info("Device number %d created successfully\n", dev_to_bind->id);
  
    mutex_init(dev_data->mtx);
  
    return result;

probe_error:
    pr_warn("Binding failed\n");
    return result;
}

static int abs_remove(struct platform_device *dev_to_destroy)
{
    mutex_destroy(&dev_to_destroy->mtx);
    device_destroy(abs_class, dev_to_destroy->dev->dev_num);
    cdev_del(&dev_to_destroy->cdev);
    return 0;
}

static const struct of_device_id abs_match_table[] = {
  {.compatible = "abs_device"},
  {}
};

struct platform_driver abs_platform_driver = {
    .probe = abs_probe,
    .remove = abs_remove,
    .driver = {
        .name = PLATFORM_DEVICE_NAME,
        .owner = THIS_MODULE,
        .of_match_table = abs_match_table
    }
};

static void abs_exit(void)
{
    pr_info("Cleaning\n");
    unregister_chrdev_region(MKDEV(abs_maj_num, abs_minimal_minor), NUMBER_OF_DEVICES);
    platform_driver_unregister(&abs_platform_driver);
    class_destroy(abs_class);
}

static int __init abs_init(void)
{
    dev_t dev_num;
    int err_code;
  
    pr_info("Allocating chrdev region\n");
    err_code = alloc_chrdev_region(&dev_num, abs_minimal_minor, NUMBER_OF_DEVICES, DEV_NAME);
    if ( err_code < 0 ) {
        pr_warn("Alloc failed\n");
        goto init_error;
    }
  
    abs_maj_num = MAJOR(dev_num);
  
    abs_class = class_create(THIS_MODULE, CLASS_NAME);
    if ( IS_ERR(abs_class) ) {
        pr_warn("Failed to register class!\n");
        err_code = abs_class;
        goto init_error;
    }
  
    platform_driver_register(&abs_platform_driver);
  
    pr_info("Driver registered\n");
  
    return 0;

init_error:
    abs_exit();
    return err_code;
}

module_init(abs_init);
module_exit(abs_exit);


/*-----------Driver's callbacks for systemcalls:--------------*/

static int abs_open(struct inode *inode, struct file *filp)
{
    struct abs_private_dev_data *private_data;
  
    mutex_lock(&private_data->mtx);
    private_data = container_of(inode->i_cdev, struct abs_private_dev_data, cdev);
    filp->private_data = private_data;
    try_module_get(THIS_MODULE);
    mutex_unlock(&private_data->mtx);
  
    return 0;
}

static ssize_t abs_read(struct file *file,
                        char __user *buf,
                        size_t count,
                        loff_t *f_pos)
{
    struct abs_private_dev_data *dev = file->private_data;
  
    mutex_lock(&private_data->mtx);
  
    if ( *f_pos >= dev->size ) {
        return 0;
    }
  
    if ( *f_pos + count > dev->size ) {
        count = dev->size - *f_pos;
    }
  
    if (copy_to_user(buf, &dev->platform_data->data[*f_pos], count)) {
        return -EFAULT;
    } 
  
    *f_pos += count;
    mutex_unclock(&private_data->mtx);
  
    return count;
}

static ssize_t abs_write(struct file *filp,
                         const char __user *buf,
                         size_t count ,
                         loff_t *f_pos)
{
    struct abs_private_dev_data *private_data = filp->private_data;
    ssize_t result = -ENOMEM;

    mutex_lock(&private_data->mtx);
    if ( *f_pos >= PAGE_SIZE_IN_BYTES ) {
        return result;
    }

    if (copy_from_user(private_data->platform_data->data, buf, count)) {
        result = -EFAULT;
    }

    mutex_unlock(&private_data->mtx);
    return result;
}

static loff_t abs_llseek(struct file *filp,
                         loff_t offset,
                         int whence)
{
    mutex_lock(&file->private_data->mtx);
    struct abs_private_dev_data *private_data = filp->private_data;
    int max_size = private_data->size;
  
    loff_t temp;
  
    pr_info("Starting lseek\n");
    
    switch(whence)
    {
        case SEEK_SET:
            if( (offset > max_size) || (offset < 0) ) {
                return -EINVAL;
            }
            filp->f_pos = offset;
            break;
    
        case SEEK_CUR:
            temp = file->f_pos + offset;
            if ( (temp > max_size) || (temp < 0)) {
                return -EINVAL;
            }
            filp->f_pos = temp;
            break;
    
        case SEEK_END:
            temp = max_size + offset;
            if ( (temp > max_size) || (temp < 0)) {
                return -EINVAL;
            }
            filp->f_pos = temp;
            break;
          
        default:
            return -EINVAL;
    }
  
    mutex_unlock(&file->private_data->mtx);
    pr_info("Lseek is finished\n");
  
    return file->f_pos;
}

static int abs_mmap( struct file *filp, struct vm_area_struct *area )
{
    int result;
  
    mutex_lock(&filp->private_data->mtx);
    struct abs_private_dev_data *private_data = filp->private_data;
  
    area->vm_pgoff = virt_to_phy(private_data->platform_data->data) >> PAGE_SHIFT;
    result = remap_pfn_range(area, 
                             area->vm_start, 
                             area->vm->vm_pgoff, 
                             area->vm_end - area->vm_start, 
                             area->vm_page_prot);
    if (result) {
        pr_warn("Mmap remap failed\n");
        result = -EIO;
        goto mmap_error;
    }
    SetPageReserved(virt_to_page((unsigned long)private_data->platform_data->data));
  
    return result;
  
  mmap_error:
    return result;
}

static int abs_release (struct inode *node, struct file *filp)
{
    int result;

    module_put(THIS_MODULE);
    return result;
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Mimikin vladimirmimikin@gmail.com");
MODULE_DESCRIPTION("Abstract platform driver");
MODULE_VERSION("1.0");
