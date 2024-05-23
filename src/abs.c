#include <linux/slab.h>
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
#include <linux/dma-direct.h>

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
        // will hold offset to which data from abs_value attr will be written
        unsigned int address_from_sysfs;
        char value_from_sysfs; 
        struct mutex mtx;
        struct device *devp;
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

static int setup_chrdev(struct abs_private_dev_data *private_dev_data, int index)
{
        /* 
         * Helper function for device initialization during probe
         * todo: move to probe
         */
        int err_code;
        private_dev_data->dev_num = MKDEV(abs_maj_num, abs_minimal_minor + index);
      
        cdev_init(&private_dev_data->cdev, &abs_fops);
        
        private_dev_data->cdev.owner = THIS_MODULE;
        private_dev_data->cdev.ops = &abs_fops;
      
        err_code = cdev_add(&private_dev_data->cdev, private_dev_data->dev_num, 1);
        if (err_code < 0) {
                dev_warn(private_dev_data->devp, "Chrdev adding failed!\n");
                goto setup_add_error;
        }
        
        return 0;

setup_add_error:
        cdev_del(&private_dev_data->cdev);
        return err_code;
}

static ssize_t abs_show(struct device *dev,
                        struct device_attribute *attr,
                        char *buf)
{
        /* Read callback for sysfs*/
        ssize_t result;
        int addr;
        char val;
        struct abs_private_dev_data *private_data;
        
        dev_dbg(dev, "Start show %s\n", attr->attr.name);
        
        // Get device specific data (mutex, platform_data->data etc)
        private_data = dev_get_drvdata(dev);
        if (IS_ERR(private_data)) {
                dev_warn(dev, "Unable to get device private data in abs_show call\n");
                result = -ENOENT;
                goto show_error;
        }
    
        mutex_lock(&private_data->mtx);
    
        addr = private_data->address_from_sysfs;
        val = private_data->platform_data->data[addr];
        dev_dbg(dev, "Expected addr:%d, value:%d\n", addr, val);
    
        if (strcmp(attr->attr.name, "abs_value") == 0) {
                result = scnprintf(buf, PAGE_SIZE_IN_BYTES, "value:%d\n", val);
        }
        else {
                result = scnprintf(buf, PAGE_SIZE_IN_BYTES, "addr:%d\n", addr);
        }
    
        mutex_unlock(&private_data->mtx);
    
        dev_dbg(dev, "Show succeed\n");
        return result;

show_error:
        return result;
}

static ssize_t abs_store(struct device *dev, 
                         struct device_attribute *attr,
                         const char *buf, 
                         size_t count)
{
        /* Write callback for sysfs */
        ssize_t result;
        unsigned int *addrp;
        unsigned int val_buf;
        struct abs_private_dev_data *private_data;
    
        dev_dbg(dev, "Storing started\n");
    
        private_data = dev_get_drvdata(dev);
        if (IS_ERR(private_data)) {
                dev_warn(dev, "Unable to get device private data in abs_show call\n");
                result = -ENOENT;
                goto store_end;
        }
    
        mutex_lock(&private_data->mtx);
    
        addrp = &private_data->address_from_sysfs;

        if (sscanf(buf, "%d %n", &val_buf, (int*)&count) != 1) {
                dev_err(dev, "Can't read value!\n");
                result = -EINVAL;
                goto store_end;
        }
        result = count;

        if (strcmp(attr->attr.name, "abs_value") == 0) {
                if (val_buf > U8_MAX) {
                        result = -EINVAL;
                        goto store_end;
                }
                private_data->platform_data->data[*addrp] = val_buf;
                
        } else {
                if (val_buf > PAGE_SIZE_IN_BYTES) {
                        dev_err(dev, "Invalid value for address write!\n");
                        result = -EINVAL;
                        goto store_end;
                }
                *addrp = val_buf;
        }
    
    
        dev_dbg(dev, "Storing is over, val:%d, addr:%d\n", 
                private_data->platform_data->data[*addrp], 
                *addrp);

store_end:
        mutex_unlock(&private_data->mtx);
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
        int already_bound;

        already_bound = 0;
        dev_dbg(&dev_to_bind->dev, "Binding started\n");
      
        dev_data = devm_kzalloc(&dev_to_bind->dev, 
                                sizeof(struct abs_private_dev_data), 
                                GFP_KERNEL);
        if (!dev_data) {
                dev_warn(&dev_to_bind->dev, "Memory allocation failed for dev struct!\n");
                result = -ENOMEM;
                goto probe_dev_alloc_error;
        }
      
        platform_data = dev_get_platdata(&dev_to_bind->dev);

        if (platform_data) {
                dev_data->platform_data = platform_data;
                if (platform_data->data) {
                        already_bound = 1;
                } else {
                        dev_data->platform_data->data = devm_kmalloc(&dev_to_bind->dev, 
                                                             PAGE_SIZE_IN_BYTES, 
                                                             GFP_KERNEL);
                }

                if (!dev_data->platform_data->data) {
                        dev_warn(&dev_to_bind->dev, "Dev data alloc failed!\n");
                        result = -ENOMEM;
                        goto probe_data_alloc_error;
                }
        } else {
                dev_dbg(&dev_to_bind->dev, "No platform data, obtain with get_platdata\n");
                res = platform_get_resource(dev_to_bind, IORESOURCE_MEM, 0);
                if (!request_mem_region(res->start, res->end - res->start, DRIVER_NAME)) {
                        dev_warn(&dev_to_bind->dev, "Cant access device registers!\n");
                        result = -EINVAL;
                        goto probe_data_alloc_error;
                }
        
                dev_data->platform_data->data = devm_ioremap(&dev_to_bind->dev, 
                                                             res->start, 
                                                             res->end - res->start);
                if (!dev_data->platform_data->data) {
                        dev_warn(&dev_to_bind->dev, "Remap failed\n");
                        result = -ENOMEM;
                        goto probe_req_reg_error;
                }
        
        }

        dev_data->address_from_sysfs = 0;
        dev_data->value_from_sysfs = 0;
      
        result = setup_chrdev(dev_data, dev_to_bind->id);
        if (result < 0) {
                dev_warn(&dev_to_bind->dev, "Failed to setup chrdev\n");
                goto probe_setup_chd_error;
        }
        
        mutex_init(&dev_data->mtx);
    
        abs_dev_fs = device_create(abs_class, 
                                   NULL, 
                                   dev_data->dev_num,
                                   dev_data, 
                                   "abs_dev-%d",dev_to_bind->id);

        if (IS_ERR(abs_dev_fs)) {
                dev_warn(&dev_to_bind->dev, "Failed to create /dev file for %d device", 
                         dev_to_bind->id);
                goto probe_dev_create_error;
        }
    
        dev_data->devp = &dev_to_bind->dev;
    
        dev_set_drvdata(&dev_to_bind->dev, dev_data);
        if (already_bound) {
                device_remove_file(&dev_to_bind->dev, &dev_attr_abs_address);
                device_remove_file(&dev_to_bind->dev, &dev_attr_abs_value);
        }

        if (device_create_file(&dev_to_bind->dev, &dev_attr_abs_value)) {
                dev_warn(&dev_to_bind->dev, "Failed to create value attr\n");
                goto probe_value_error;
        }
        if (device_create_file(&dev_to_bind->dev, &dev_attr_abs_address)) {
                dev_warn(&dev_to_bind->dev, "Failed to create address attr\n");
                goto probe_addr_error;
        }
      
        dev_dbg(&dev_to_bind->dev, "Device number %d created successfully\n", dev_to_bind->id + 1);
        return 0;

probe_addr_error:
        device_remove_file(&dev_to_bind->dev, &dev_attr_abs_value);
probe_value_error:
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
    
        dev_dbg(&dev_to_destroy->dev, "Device removing started\n");
        pdata = dev_get_drvdata(&dev_to_destroy->dev);
        ClearPageReserved(virt_to_page((unsigned long)pdata->platform_data->data));
        cdev_del(&pdata->cdev);
        mutex_destroy(&pdata->mtx);
        device_destroy(abs_class, pdata->dev_num);
    
        pr_debug("Device removed\n");
    
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
        pr_debug("Cleaning\n");
        unregister_chrdev_region(MKDEV(abs_maj_num, 
                                       abs_minimal_minor), 
                                       NUMBER_OF_DEVICES);
        platform_driver_unregister(&abs_platform_driver);
        class_destroy(abs_class);
}

static __init int abs_init(void)
{
        dev_t dev_num;
        int err_code;
      
        pr_debug("Allocating chrdev region\n");
        err_code = alloc_chrdev_region(&dev_num, 
                                       abs_minimal_minor,
                                       NUMBER_OF_DEVICES, 
                                       DEV_NAME);
        if (err_code < 0) {
                pr_warn("Alloc failed\n");
                goto init_error;
        }
      
        abs_maj_num = MAJOR(dev_num);
      
        abs_class = class_create(THIS_MODULE, CLASS_NAME);
        if (IS_ERR(abs_class)) {
                pr_warn("Failed to register class!\n");
                err_code = PTR_ERR(abs_class);
                goto init_error;
        }
      
        platform_driver_register(&abs_platform_driver);
      
        pr_debug("Driver registered\n");
      
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
    
        // Get device data
        private_data = container_of(inode->i_cdev, struct abs_private_dev_data, cdev);

        dev_dbg(private_data->devp, "Open file\n");

        mutex_lock(&private_data->mtx);

        filp->private_data = private_data;
        filp->f_pos = 0;

        try_module_get(THIS_MODULE);

        mutex_unlock(&private_data->mtx);
    
        dev_dbg(private_data->devp, "Opening succeed\n");
      
        return 0;
}

static ssize_t abs_read(struct file *file,
                        char __user *buf,
                        size_t count,
                        loff_t *f_pos)
{
        ssize_t result;
        struct abs_private_dev_data *private_data = file->private_data;
    
        dev_dbg(private_data->devp, "Reading from file, count: %ld\n", count);;
      
        mutex_lock(&private_data->mtx);
      
        // trying to read outside memory
        if (*f_pos >= PAGE_SIZE_IN_BYTES / sizeof(loff_t)) {
                dev_warn(private_data->devp, "EOF\n");
                result = 0; // EOF
                goto read_err;
        }
      
        // trying to read more than we can
        if (*f_pos + count > PAGE_SIZE_IN_BYTES) {
                dev_warn(private_data->devp, "Partial read\n");
                count = PAGE_SIZE_IN_BYTES - *f_pos;
        }
      
        if (copy_to_user(buf, &private_data->platform_data->data[*f_pos], count)) {
                result = -EFAULT;
                goto read_err;
        } 
      
        *f_pos += count;
        mutex_unlock(&private_data->mtx);
    
        dev_dbg(private_data->devp, "Reading succeed, new fpos = %lld\n", *f_pos);
      
        return count;

read_err:
        dev_dbg(private_data->devp, "Read failed\n");
        mutex_unlock(&private_data->mtx);
        return result;
}

static ssize_t abs_write(struct file *filp,
                         const char __user *buf,
                         size_t count ,
                         loff_t *f_pos)
{
        struct abs_private_dev_data *private_data = filp->private_data;
        ssize_t result = 0;
    
        dev_dbg(private_data->devp, "Writing started, count = %ld, fpos = %lld\n", count, *f_pos);
    
        mutex_lock(&private_data->mtx);

        if (*f_pos >= PAGE_SIZE_IN_BYTES) {
                result = 0;
                goto end;
        }

        //trying to write more than we can
        if (count + *f_pos > PAGE_SIZE_IN_BYTES) {
                count = PAGE_SIZE_IN_BYTES - *f_pos;
        }
    
        if (copy_from_user(private_data->platform_data->data, buf, count)) {
                result = -EFAULT;
                goto end;
        }

        result = count;
        *f_pos += count;
end:
        mutex_unlock(&private_data->mtx);
        dev_dbg(private_data->devp, "Writing succeed\n");

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
      
        dev_dbg(private_data->devp, "Starting lseek\n");
        
        switch(whence) {
        case SEEK_SET:
                if((offset > max_size) || (offset < 0))
                        return -EINVAL;
                filp->f_pos = offset;
                break;
    
        case SEEK_CUR:
                temp = filp->f_pos + offset;
                if ( (temp > max_size) || (temp < 0))
                        return -EINVAL;
                filp->f_pos = temp;
                break;
    
        case SEEK_END:
                temp = max_size + offset;
                if ( (temp > max_size) || (temp < 0)) 
                        return -EINVAL;
                filp->f_pos = temp;
                break;
          
        default:
                return -EINVAL;
        }
      
        mutex_unlock(&private_data->mtx);
        dev_dbg(private_data->devp, "Lseek is finished\n");
      
        return filp->f_pos;
}

static int abs_mmap(struct file *filp, struct vm_area_struct *area) 
{
        int result;
        int pfn;
        unsigned long area_size;
      
        struct abs_private_dev_data *private_data = filp->private_data;
    
        mutex_lock(&private_data->mtx);
        dev_dbg(private_data->devp, "Mmap started\n");

        area_size = area->vm_end - area->vm_start;
        if (area_size > PAGE_SIZE_IN_BYTES) {
                result = -ENOMEM;
                dev_warn(private_data->devp, "Mmap failed due to big size of area\n");
                goto mmap_error;
        }
      
        pfn = virt_to_phys(private_data->platform_data->data) >> PAGE_SHIFT;
        area->vm_file = filp;
        area->vm_flags |= VM_READ | VM_SHARED | VM_WRITE;
        area->vm_pgoff = pfn;
        result = remap_pfn_range(area, 
                                 area->vm_start,
                                 area->vm_pgoff, 
                                 area_size, 
                                 area->vm_page_prot);
        if (result) {
                dev_warn(private_data->devp, "Mmap remap failed\n");
                result = -EIO;
                goto mmap_error;
        }
        SetPageReserved(virt_to_page((unsigned long)private_data->platform_data->data));
        dev_dbg(private_data->devp, "Mmap succeed\n");

        mutex_unlock(&private_data->mtx);
        return 0;
  
mmap_error:
        dev_dbg(private_data->devp, "Mmap failed\n");
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
