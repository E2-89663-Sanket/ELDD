// implementation of pseudo char device driver 
//   multiple device instances   
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/wait.h>

// number of devices
#define DEVCNT 4
// initial kfifo buffer size
#define SIZE 32

// function declarations
static int pchar_open(struct inode *pinode, struct file *pfile);
static int pchar_close(struct inode *pinode, struct file *pfile);
static ssize_t pchar_read(struct file *pfile, char __user *ubuf, size_t bufsize, loff_t *poffset);
static ssize_t pchar_write(struct file *pfile, const char __user *ubuf, size_t bufsize, loff_t *poffset);

// psuedo device -- private data structure
typedef struct pchar_device {
    struct kfifo buffer;
    struct cdev cdev;
    wait_queue_head_t wr_wq;
    wait_queue_head_t rd_wq; 
} pchar_device_t;

// global variables

// memory for keeping device private struct
// array elements 0, 1, ... corresponds to devices pchar0, pchar1, ... 
static pchar_device_t devices[DEVCNT];

static int major;
static struct class *pclass;
static struct file_operations pchar_ops = {
    .owner = THIS_MODULE,
    .open = pchar_open,
    .release = pchar_close,
    .read = pchar_read,
    .write = pchar_write
};

// module initialization function
static int __init pchar_init(void) {
    int ret, i;
    dev_t devno;
    struct device *pdevice;
    pr_info("%s: pchar_init() called.\n", THIS_MODULE->name);
    // allocate device numbers e.g. 235/0, 235/1, 235/2, 235/3
    ret = alloc_chrdev_region(&devno, 0, DEVCNT, "pchar");
    if(ret < 0) {
        pr_err("%s: alloc_chrdev_region() failed.\n", THIS_MODULE->name);
        goto alloc_chrdev_region_failed;
    }
    major = MAJOR(devno);
    pr_info("%s: device number = %d/%d.\n", THIS_MODULE->name, major, MINOR(devno));
    // create device class
    pclass = class_create("pchar_class");
    if(IS_ERR(pclass)) {
        pr_err("%s: class_create() failed.\n", THIS_MODULE->name);
        ret = -1;
        goto class_create_failed;
    }
    pr_info("%s: device class is created.\n", THIS_MODULE->name);
    // create device file
    for(i=0; i<DEVCNT; i++) {
        devno = MKDEV(major, i);
        pdevice = device_create(pclass, NULL, devno, NULL, "pchar%d", i);
        if(IS_ERR(pdevice)) {
            pr_err("%s: device_create() failed for pchar%d.\n", THIS_MODULE->name, i);
            ret = -1;
            goto device_create_failed;
        }
        pr_info("%s: device file pchar%d is created.\n", THIS_MODULE->name, i);
    }
    // init cdev struct and add into kernel db
    for(i=0; i<DEVCNT; i++) {
        cdev_init(&devices[i].cdev, &pchar_ops);
        devno = MKDEV(major, i);
        ret = cdev_add(&devices[i].cdev, devno, 1);
        if(ret < 0) {
            pr_err("%s: cdev_add() failed for pchar%d.\n", THIS_MODULE->name, i);
            goto cdev_add_failed;
        }
        pr_info("%s: device cdev for pchar%d is added in kernel.\n", THIS_MODULE->name, i);
    }
    // allocate device buffer (kfifo)
    for(i=0; i<DEVCNT; i++) {
        ret = kfifo_alloc(&devices[i].buffer, SIZE, GFP_KERNEL);
        if(ret) {
            pr_err("%s: kfifo_alloc() failed for pchar%d.\n", THIS_MODULE->name, i);
            goto kfifo_alloc_failed;
        }
        pr_info("%s: allocated device buffer for pchar%d of size %d.\n", THIS_MODULE->name, i, kfifo_size(&devices[i].buffer));
    }
    // initialize wait queues
    for(i=0; i<DEVCNT; i++) {
        init_waitqueue_head(&devices[i].wr_wq);
        pr_info("%s: initialized wait queue for pchar%d.\n", THIS_MODULE->name, i);
         init_waitqueue_head(&devices[i].rd_wq);
    pr_info("%s: initialized reader wait queue for pchar%d.\n", THIS_MODULE->name, i);
    }
    return 0; // module initialized successfully.

kfifo_alloc_failed:
    for(i = i-1; i >= 0; i--)
        kfifo_free(&devices[i].buffer);
    i = DEVCNT;
cdev_add_failed:
    for(i = i-1; i >= 0; i--)
        cdev_del(&devices[i].cdev);
    i = DEVCNT;
device_create_failed:
    for(i = i-1; i >= 0; i--) {
        devno = MKDEV(major, i);
        device_destroy(pclass, devno);
    }
    class_destroy(pclass);
class_create_failed:
    unregister_chrdev_region(devno, DEVCNT);
alloc_chrdev_region_failed:
    return ret;
}


// module de-initialization function
static void __exit pchar_exit(void) {
    int i;
    dev_t devno;
    pr_info("%s: pchar_exit() called.\n", THIS_MODULE->name);
    // deallocate device buffer (kfifo)
    for(i = DEVCNT-1; i >= 0; i--) {
        kfifo_free(&devices[i].buffer);
        pr_info("%s: device buffer for pchar%d is released.\n", THIS_MODULE->name, i);
    }
    // remove device cdev from the kernel db.
    for(i = DEVCNT-1; i >= 0; i--) {
        cdev_del(&devices[i].cdev);
        pr_info("%s: device cdev of pchar%d is removed from kernel.\n", THIS_MODULE->name, i);
    }
    // destroy device files
    for(i = DEVCNT-1; i >= 0; i--) {
        devno = MKDEV(major, i);
        device_destroy(pclass, devno);
        pr_info("%s: device file is destroyed.\n", THIS_MODULE->name);
    }
    // destroy device class
    class_destroy(pclass);
    pr_info("%s: device class is destroyed.\n", THIS_MODULE->name);
    // unallocate device number
    unregister_chrdev_region(devno, DEVCNT);
    pr_info("%s: device number released.\n", THIS_MODULE->name);
}

// pchar file operations
static int pchar_open(struct inode *pinode, struct file *pfile) {
    pr_info("%s: pchar_open() called.\n", THIS_MODULE->name);
    // find addr of private struct of device associated with the current device
    //  file inode and assign it to struct file's (OFT entry) private data
    pfile->private_data = container_of(pinode->i_cdev, pchar_device_t, cdev);
    return 0;
}

static int pchar_close(struct inode *pinode, struct file *pfile) {
    pr_info("%s: pchar_close() called.\n", THIS_MODULE->name);
    return 0;
}
/*
static ssize_t pchar_read(struct file *pfile, char __user *ubuf, size_t bufsize, loff_t *poffset) {
    int nbytes=0, ret;
    pchar_device_t *dev = (pchar_device_t *)pfile->private_data;
    pr_info("%s: pchar_read() called.\n", THIS_MODULE->name);
    // write data from device buffer (kfifo) to the user buffer
    ret = kfifo_to_user(&dev->buffer, ubuf, bufsize, &nbytes);
    if(ret) {
        pr_err("%s: failed to copy device buffer into user buffer.\n", THIS_MODULE->name);
        return ret;
    }
    pr_info("%s: pchar_read() %d bytes read.\n", THIS_MODULE->name, nbytes);
    // if one/more bytes read from the buffer, wake up blocked writer process if any
    if(nbytes > 0) {
        pr_info("%s: waking up writer process if any.\n", THIS_MODULE->name);
        wake_up_interruptible(&dev->wr_wq);
    }
    // returns num of bytes successfully read.
    return nbytes;  
}
*/
static ssize_t pchar_read(struct file *pfile, char __user *ubuf, size_t bufsize, loff_t *poffset) {
    int nbytes = 0, ret;
    pchar_device_t *dev = (pchar_device_t *)pfile->private_data;

    pr_info("%s: pchar_read() called.\n", THIS_MODULE->name);

    // Block if buffer is empty
    ret = wait_event_interruptible(dev->rd_wq, !kfifo_is_empty(&dev->buffer));
    if (ret < 0) {
        pr_err("%s: pchar_read() reader wakeup due to signal.\n", THIS_MODULE->name);
        return ret;
    }

    // Read from buffer
    ret = kfifo_to_user(&dev->buffer, ubuf, bufsize, &nbytes);
    if (ret) {
        pr_err("%s: failed to copy device buffer into user buffer.\n", THIS_MODULE->name);
        return ret;
    }

    pr_info("%s: pchar_read() %d bytes read.\n", THIS_MODULE->name, nbytes);

    // Wake up writers if any
    if (nbytes > 0) {
        pr_info("%s: waking up writer process if any.\n", THIS_MODULE->name);
        wake_up_interruptible(&dev->wr_wq);
    }

    return nbytes;
}




static ssize_t pchar_write(struct file *pfile, const char __user *ubuf, size_t bufsize, loff_t *poffset) {
    int nbytes, ret;
    pchar_device_t *dev = (pchar_device_t *) pfile->private_data;
    pr_info("%s: pchar_write() called.\n", THIS_MODULE->name);
    if (nbytes > 0) {
        pr_info("%s: waking up reader process if any.\n", THIS_MODULE->name);
        wake_up_interruptible(&dev->rd_wq);  // <--- ADD THIS
    }
    
    // if buffer is full, block the current process in writer's wait queue
    //wait_event(dev->wr_wq, !kfifo_is_full(&dev->buffer)); // uninterruptible sleep
    ret = wait_event_interruptible(dev->wr_wq, !kfifo_is_full(&dev->buffer));
    if(ret < 0) { // wake up due to signal
        pr_err("%s: pchar_write() writer wakeup due to signal.\n", THIS_MODULE->name);
        return ret; // -ERESTARTSYS i.e. restart the syscall.
    }

    // write user buffer data into device "buffer" (kfifo)
    ret = kfifo_from_user(&dev->buffer, ubuf, bufsize, &nbytes);
    if(ret) {
        pr_err("%s: failed to copy user buffer into device buffer.\n", THIS_MODULE->name);
        return ret;
    }
    pr_info("%s: pchar_write() %d bytes written.\n", THIS_MODULE->name, nbytes);
    // returns num of bytes successfully written.
    return nbytes;
}



module_init(pchar_init);
module_exit(pchar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SANKET");
MODULE_DESCRIPTION("Hello Char Device Driver");
