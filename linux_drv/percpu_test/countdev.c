#include <linux/cdev.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define DEVICE_NAME            countdev
#define __DEVICE_OPS(name, op) name##_##op
#define _DEVICE_OPS(name, op)  __DEVICE_OPS(name, op)
#define DEVICE_UNLOCK_IOCTL    _DEVICE_OPS(DEVICE_NAME, unlock_ioctl)
#define DEVICE_OPEN            _DEVICE_OPS(DEVICE_NAME, open)
#define DEVICE_CLOSE           _DEVICE_OPS(DEVICE_NAME, close)
#define DEVICE_READ            _DEVICE_OPS(DEVICE_NAME, read)
#define DEVICE_WRITE           _DEVICE_OPS(DEVICE_NAME, write)
#define DEVICE_INIT            _DEVICE_OPS(DEVICE_NAME, init)
#define DEVICE_EXIT            _DEVICE_OPS(DEVICE_NAME, exit)
#define DEVICE_OPS             _DEVICE_OPS(DEVICE_NAME, ops)
#define DEVICE                 _DEVICE_OPS(DEVICE_NAME, device)
#define _DEVICE_STR(name)      #name
#define DEVICE_STR(str)        _DEVICE_STR(str)

#define devcount               2
#define PERCPU_VAR_NAME        percpu_count

struct cdev  DEVICE_NAME;
unsigned int baseminor = 0;
dev_t        devno;

atomic_t atomic_count = {0};
DEFINE_PER_CPU(uint64_t, PERCPU_VAR_NAME);

int DEVICE_OPEN(struct inode *inode, struct file *file)
{
    int num = MINOR(inode->i_rdev);
    int cpu = 0;
    for_each_online_cpu(cpu)
        per_cpu(PERCPU_VAR_NAME, cpu) = 0;
    printk(KERN_INFO "%s:Open, subdev id:%d\n", DEVICE_STR(DEVICE_NAME), num);
    return 0;
}

ssize_t DEVICE_READ(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    printk(KERN_WARNING "%s:Read, no implementation\n", DEVICE_STR(DEVICE_NAME));
    return ret;
}

ssize_t DEVICE_WRITE(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    printk(KERN_WARNING "%s:Write, no implementation\n", DEVICE_STR(DEVICE_NAME));
    return ret;
}

int DEVICE_CLOSE(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "%s:Close\n", DEVICE_STR(DEVICE_NAME));
    return 0;
}

long DEVICE_UNLOCK_IOCTL(struct file *file, unsigned int cmd, unsigned long arg)
{
    this_cpu_inc(PERCPU_VAR_NAME);
    int res = this_cpu_read(PERCPU_VAR_NAME);
    printk(KERN_INFO "%s:Unlock ioctl, times:%d\n", DEVICE_STR(DEVICE_NAME), res);
    return res;
}

static const struct file_operations DEVICE_OPS = {
    .owner          = THIS_MODULE,
    .open           = DEVICE_OPEN,
    .release        = DEVICE_CLOSE,
    .read           = DEVICE_READ,
    .write          = DEVICE_WRITE,
    .unlocked_ioctl = DEVICE_UNLOCK_IOCTL};

static int __init DEVICE_INIT(void)
{
    int res = 0;
    printk(KERN_INFO "%s:Init\n", DEVICE_STR(DEVICE_NAME));
    /*init chr dev*/
    cdev_init(&DEVICE_NAME, &DEVICE_OPS);
    /*register chr dev*/
    if ((res = alloc_chrdev_region(&devno, baseminor, devcount, DEVICE_STR(DEVICE_NAME))) != 0) {
        printk(KERN_INFO "%s:Register chr dev failed, base minor:%u, dev count:%u\n", DEVICE_STR(DEVICE_NAME), baseminor, devcount);
        return res;
    }
    /*add dev to system with dev number*/
    if ((res = cdev_add(&DEVICE_NAME, devno, devcount)) != 0) {
        printk(KERN_INFO "%s:Add chr dev failed, dev num:%u, dev count:%u\n", DEVICE_STR(DEVICE_NAME), devno, devcount);
        unregister_chrdev_region(devno, devcount);
        return res;
    }
    printk(KERN_INFO "%s:Add chr dev success, dev num:%u, dev count:%u\n", DEVICE_STR(DEVICE_NAME), devno, devcount);
    return res;
}

static void __exit DEVICE_EXIT(void)
{
    printk(KERN_INFO "%s:Exit\n", DEVICE_STR(DEVICE_NAME));
    /*delete dev from system*/
    cdev_del(&DEVICE_NAME);
    /*release dev number*/
    unregister_chrdev_region(devno, devcount);
}

MODULE_LICENSE("GPL");
module_init(countdev_init);
module_exit(countdev_exit);