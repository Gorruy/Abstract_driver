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
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/mm.h>
#include <linux/kobject.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

#include "abs.h"


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
    abs_platform_data_t *platform_data;
    struct cdev cdev;
    dev_t dev_num;
    int address_from_sysfs;
    int value_from_sysfs;
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
    dev->dev_num = MKDEV(abs_maj_num, abs_minimal_minor + index);
  
    cdev_init(&dev->cdev, &abs_fops);
    
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &abs_fops;
  
    err_code = cdev_add(&dev->cdev, dev->dev_num, 1);
    if (err_code < 0) {
        pr_warn("Chrdev adding failed!\n");
        goto setup_add_error;
    }
    
    return 0;

setup_add_error:
    cdev_del(&dev->cdev);
    return err_code;
}

static ssize_t abs_show(struct device *dev,
                        struct device_attribute *attr,
                        char *buf)
{
    ssize_t result;
    int addr;
    struct abs_private_dev_data *private_data;
    
    pr_info("Start show\n");
    // No reading from address!!
    if (strcmp(attr->attr.name,"abs_address") == 0) {
        pr_err("Bad read from address attribute");
        result = -EACCES;
        goto show_error;
    }

    private_data = container_of(dev->platform_data, 
                                struct abs_private_dev_data, 
                                platform_data);
    if (IS_ERR(private_data)) {
        pr_warn("Unable to get device private data in abs_show call\n");
        result = -ENOENT;
        goto show_error;
    }

    mutex_lock(&private_data->mtx);

    addr = private_data->address_from_sysfs;

    result = sprintf(buf, "%c", private_data->platform_data->data[addr]);

    mutex_unlock(&private_data->mtx);

    return 0;

show_error:
    return result;
}

static ssize_t abs_store(struct device *dev, 
                         struct device_attribute *attr,
                         const char *buf, 
                         size_t count)
{
    ssize_t result;
    int *addr;
    struct abs_private_dev_data *private_data;

    pr_info("Storing started\n");
    private_data = container_of(dev->platform_data, 
                                struct abs_private_dev_data, 
                                platform_data);
    if (IS_ERR(private_data)) {
        pr_warn("Unable to get device private data in abs_show call\n");
        result = -ENOENT;
        goto store_error;
    }

    mutex_lock(&private_data->mtx);

    addr = &private_data->address_from_sysfs;

    if (strcmp(attr->attr.name,"abs_value") == 0) {
        result = sscanf(buf, "%c", &private_data->platform_data->data[*addr]);
    }
    else {
        result = sscanf(buf,"%d", addr);
        if (private_data->address_from_sysfs > PAGE_SIZE_IN_BYTES) {
            pr_err("Invalid value for address write!\n");
            private_data->address_from_sysfs = 0;
            result = -EINVAL;
        }
    }

    mutex_unlock(&private_data->mtx);

    pr_info("Storing is over, val:%ld", result);
    return result;

store_error:
    return result;
}

DEVICE_ATTR(abs_value, 0644, abs_show, abs_store);
DEVICE_ATTR(abs_address, 0644, abs_show, abs_store);

static int abs_probe(struct platform_device *dev_to_bind) 
{
    int result;
    abs_platform_data_t *platform_data;
    struct abs_private_dev_data *dev_data;
    struct device *abs_dev_fs;
    struct resource *res;

    pr_info("Binding started\n");
  
    dev_data = devm_kzalloc(&dev_to_bind->dev, 
                            sizeof(struct abs_private_dev_data), 
                            GFP_KERNEL);
    if (!dev_data) {
        pr_warn("Memory allocation failed for dev struct!\n");
        result = -ENOMEM;
        goto probe_dev_alloc_error;
    }
  
    platform_data = dev_get_platdata(&dev_to_bind->dev);
    if (platform_data) {
        dev_data->platform_data = platform_data;
        dev_data->platform_data->data = devm_kzalloc(&dev_to_bind->dev,
                                                     PAGE_SIZE_IN_BYTES,
                                                     GFP_DMA);
        if (!dev_data->platform_data->data) {
            pr_warn("Memory allocation failed for dev data!\n");
            result = -ENOMEM;
            goto probe_data_alloc_error;
        }
    }
    else {
        pr_info("No platform data, obtain with get_platdata\n");
        res = platform_get_resource(dev_to_bind, IORESOURCE_MEM, 0);
        if (!request_mem_region(res->start, res->end - res->start, DRIVER_NAME)) {
            pr_warn("Cant access device registers!\n");
            result = -EINVAL;
            goto probe_data_alloc_error;
        }

        dev_data->platform_data->data = devm_ioremap(&dev_to_bind->dev, 
                                                     res->start, 
                                                     res->end - res->start);
        if (!dev_data->platform_data->data) {
            pr_warn("Remap failed\n");
            result = -ENOMEM;
            goto probe_req_reg_error;
        }

        dev_data->address_from_sysfs = 0;
        dev_data->value_from_sysfs = 0;
    }
  
    result = setup_chrdev(dev_data, dev_to_bind->id);
    if ( result < 0 ) {
        pr_warn("Failed to setup chrdev\n");
        goto probe_setup_chd_error;
    }
    
    mutex_init(&dev_data->mtx);

    abs_dev_fs = device_create(abs_class, 
                               NULL, 
                               dev_data->dev_num,
                               dev_data, 
                               "abs_dev-%d",dev_to_bind->id);

    dev_set_drvdata(&dev_to_bind->dev, dev_data);

    if (IS_ERR(abs_dev_fs)) {
        pr_warn("Failed to create /dev file for %d device", dev_to_bind->id);
        goto probe_dev_create_error;
    }

    if (device_create_file(&dev_to_bind->dev, &dev_attr_abs_value)) {
        pr_warn("Failed to create value attr\n");
        goto probe_error;
    }
    if (device_create_file(&dev_to_bind->dev, &dev_attr_abs_address)) {
        pr_warn("Failed to create address attr\n");
        goto probe_error;
    }
  
    pr_info("Device number %d created successfully\n", dev_to_bind->id + 1);
  
    return 0;

probe_error:
    device_destroy(abs_class, dev_data->dev_num);

probe_dev_create_error:
    cdev_del(&dev_data->cdev);

probe_setup_chd_error:
    kfree(dev_data->platform_data->data);

probe_data_alloc_error:
    kfree(dev_data);

probe_dev_alloc_error:
    return result;

probe_req_reg_error:
    kfree(dev_data);
    release_region(res->start, res->end - res->start);
    return result;
}

static int abs_remove(struct platform_device *dev_to_destroy)
{
    struct abs_private_dev_data *pdata;

    pr_info("Device removing started\n");

    pdata = dev_get_drvdata(&dev_to_destroy->dev);
    cdev_del(&pdata->cdev);
    mutex_destroy(&pdata->mtx);
    device_destroy(abs_class, pdata->dev_num);

    pr_info("Device removed\n");

    return 0;
}

static const struct of_device_id abs_match_table[] = {
    {.compatible = "abs," PLATFORM_DEVICE_NAME},
    {}
};
MODULE_DEVICE_TABLE(of, abs_match_table);

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
        err_code = PTR_ERR(abs_class);
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
    
    pr_info("Open file\n");

    private_data = container_of(inode->i_cdev, struct abs_private_dev_data, cdev);
    mutex_lock(&private_data->mtx);
    filp->private_data = private_data;
    try_module_get(THIS_MODULE);
    mutex_unlock(&private_data->mtx);

    pr_info("Opening succeed\n");
  
    return 0;
}

static ssize_t abs_read(struct file *file,
                        char __user *buf,
                        size_t count,
                        loff_t *f_pos)
{
    ssize_t result;
    struct abs_private_dev_data *private_data = file->private_data;

    pr_info("Reading from file, count: %ld\n", count);;
  
    mutex_lock(&private_data->mtx);
  
    if ( *f_pos >= PAGE_SIZE_IN_BYTES / sizeof(loff_t) ) {
        pr_info("EOF\n");
        result = 0; // EOF
        goto read_err;
    }
  
    if ( *f_pos + count > PAGE_SIZE_IN_BYTES / sizeof(loff_t) ) {
        pr_info("Partial read\n");
        count = PAGE_SIZE_IN_BYTES / sizeof(loff_t) - *f_pos;
    }
  
    if (copy_to_user(buf, &private_data->platform_data->data[*f_pos], count)) {
        result = -EFAULT;
        goto read_err;
    } 
  
    *f_pos += count;
    mutex_unlock(&private_data->mtx);

    pr_info("Reading succeed, new fpos = %lld\n", *f_pos);
  
    return count;

read_err:
    pr_warn("Read failed\n");
    mutex_unlock(&private_data->mtx);
    return result;
}

static ssize_t abs_write(struct file *filp,
                         const char __user *buf,
                         size_t count ,
                         loff_t *f_pos)
{
    struct abs_private_dev_data *private_data = filp->private_data;
    ssize_t result = -ENOMEM;

    pr_info("Writing started, count = %ld, fpos = %lld\n", count, *f_pos);

    mutex_lock(&private_data->mtx);
    if ( *f_pos >= PAGE_SIZE_IN_BYTES ) {
        goto end;
    }

    if (copy_from_user(private_data->platform_data->data, buf, count)) {
        result = -EFAULT;
    }

end:
    mutex_unlock(&private_data->mtx);

    pr_info("Writing succeed\n");
    return result;
}

static loff_t abs_llseek(struct file *filp,
                         loff_t offset,
                         int whence)
{
    int max_size;
    loff_t temp;

    struct abs_private_dev_data *private_data = filp->private_data;

    mutex_lock(&private_data->mtx);
    max_size = PAGE_SIZE_IN_BYTES / sizeof(loff_t);
  
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
            temp = filp->f_pos + offset;
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
  
    mutex_unlock(&private_data->mtx);
    pr_info("Lseek is finished\n");
  
    return filp->f_pos;
}

static int abs_mmap( struct file *filp, struct vm_area_struct *area )
{
    int result;
  
    struct abs_private_dev_data *private_data = filp->private_data;

    mutex_lock(&private_data->mtx);
  
    area->vm_pgoff = virt_to_phys(private_data->platform_data->data) >> PAGE_SHIFT;
    result = remap_pfn_range(area, 
                             area->vm_start, 
                             area->vm_pgoff, 
                             area->vm_end - area->vm_start, 
                             area->vm_page_prot);
    if (result) {
        pr_warn("Mmap remap failed\n");
        result = -EIO;
        goto mmap_error;
    }
    SetPageReserved(virt_to_page((unsigned long)private_data->platform_data->data));
  
    mutex_unlock(&private_data->mtx);
    return result;
  
  mmap_error:
    mutex_unlock(&private_data->mtx);
    return result;
}

static int abs_release (struct inode *node, struct file *filp)
{
    module_put(THIS_MODULE);
    return 0;
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Mimikin vladimirmimikin@gmail.com");
MODULE_DESCRIPTION("Abstract platform driver");
MODULE_VERSION("1.0");
