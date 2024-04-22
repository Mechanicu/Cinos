#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define DEVICE_NAME adddev
#define __DEVICE_OPS(name, op) name##_##op
#define _DEVICE_OPS(name, op) __DEVICE_OPS(name, op)
#define DEVICE_OPEN _DEVICE_OPS(DEVICE_NAME, open)
#define DEVICE_CLOSE _DEVICE_OPS(DEVICE_NAME, close)
#define DEVICE_READ _DEVICE_OPS(DEVICE_NAME, read)
#define DEVICE_WRITE _DEVICE_OPS(DEVICE_NAME, write)
#define DEVICE_INIT _DEVICE_OPS(DEVICE_NAME, init)
#define DEVICE_EXIT _DEVICE_OPS(DEVICE_NAME, exit)
#define DEVICE_OPS _DEVICE_OPS(DEVICE_NAME, ops)
#define DEVICE _DEVICE_OPS(DEVICE_NAME, device)
#define _DEVICE_STR(name) #name
#define DEVICE_STR(str) _DEVICE_STR(str)

#define devcachesize (32 << 2)
#define devcount 2

char dev_registers[devcount][devcachesize];
struct cdev DEVICE_NAME;
unsigned int baseminor = 0;
dev_t devno;

int DEVICE_OPEN(struct inode *inode, struct file *file)
{
    int num = MINOR(inode->i_rdev);
    file->private_data = dev_registers[num];
    printk(KERN_INFO "%s:Open, subdev id:%d\n", DEVICE_STR(DEVICE_NAME), num);
    return 0;
}

ssize_t DEVICE_READ(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    unsigned long p = *ppos;
    /*get devcache for current file*/
    char *register_addr = file->private_data; 
    /*check read pos*/
    if (p >= devcachesize)
        return 0;
    if (size > devcachesize - p)
        size = devcachesize - p;
    printk(KERN_INFO "%s:Read, rsize:0x%lx, pos:%lld\n", DEVICE_STR(DEVICE_NAME), size, *ppos);

    /*copy data to user space*/
    if (!(ret = copy_to_user(buf, register_addr + p, size)))
    {
        ret = size;
    }
    printk(KERN_INFO "%s:Read, ret:%d\n", DEVICE_STR(DEVICE_NAME), ret);
    return ret;
}

ssize_t DEVICE_WRITE(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    unsigned long p = *ppos;
    /*get devcache for current file*/
    char *register_addr = file->private_data;
    /*check write pos*/
    if (p >= devcachesize)
        return 0;
    if (size > devcachesize - p)
        size = devcachesize - p;
    printk(KERN_INFO "%s:Write, wsize:0x%lx, pos:%lld\n", DEVICE_STR(DEVICE_NAME), size, *ppos);

    /*copy data to dev cache*/
    if (!(ret = copy_from_user(register_addr + p, buf, size)))
    {
        /*add*/
        char* tmp = register_addr + p;
        *tmp += 1;
        ret = size;
    }  
    printk(KERN_INFO "%s:Write, ret:%d\n", DEVICE_STR(DEVICE_NAME), ret);
    return ret;
}

int DEVICE_CLOSE(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "%s:Close\n", DEVICE_STR(DEVICE_NAME));
    return 0;
}

static const struct file_operations DEVICE_OPS = {
    .owner = THIS_MODULE,
    .open = DEVICE_OPEN,
    .release = DEVICE_CLOSE,
    .read = DEVICE_READ,
    .write = DEVICE_WRITE,
};

static int __init DEVICE_INIT(void)
{
    int res = 0;
    printk(KERN_INFO "%s:Init\n", DEVICE_STR(DEVICE_NAME));
    /*init chr dev*/
    cdev_init(&DEVICE_NAME, &DEVICE_OPS);
    /*register chr dev*/
    if ((res = alloc_chrdev_region(&devno, baseminor, devcount, DEVICE_STR(DEVICE_NAME))) != 0)
    {
        printk(KERN_INFO "%s:Register chr dev failed, base minor:%u, dev count:%u\n", DEVICE_STR(DEVICE_NAME), baseminor, devcount);
        return res;
    }
    /*add dev to system with dev number*/
    if ((res = cdev_add(&DEVICE_NAME, devno, devcount)) != 0)
    {
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
module_init(adddev_init);
module_exit(adddev_exit);