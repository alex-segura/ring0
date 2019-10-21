#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include <asm/pgtable.h>
#include <asm/pgtable_types.h>
#include <asm/uaccess.h>

MODULE_DESCRIPTION("Test kernel hax");
MODULE_AUTHOR("Me");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0");

#define DEVICE_NAME "ring0"
#define CODE_SIZE 0x1000

#define R0_COPY _IOW('a', 0, char *)
#define R0_EXEC _IOW('a', 1, long *)


static struct class *dev_class;
static struct cdev ring0_cdev;

static char *code;
static int device_open;

static int __init ring0_init(void);
static void __exit ring0_exit(void);
static int ring0_open(struct inode *, struct file *);
static int ring0_release(struct inode *, struct file *);
static ssize_t ring0_read(struct file *, char *, size_t, loff_t *);
static ssize_t ring0_write(struct file *, const char *, size_t, loff_t *);
static long ring0_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = ring0_read,
	.write = ring0_write,
	.open = ring0_open,
	.unlocked_ioctl = ring0_ioctl,
	.release = ring0_release
};

static ssize_t ring0_read(struct file *filep, char *buffer, size_t len,
    loff_t *offset)
{
	return -EINVAL;
}

static ssize_t ring0_write(struct file *filep, const char *buffer, size_t len,
    loff_t *offset)
{
	return -EINVAL;
}

static int ring0_open(struct inode *inode, struct file *file)
{
	if (device_open > 0)
		return -EBUSY;
	device_open++;
	try_module_get(THIS_MODULE);
	return 0;
}

static int ring0_release(struct inode *inode, struct file *file)
{
	device_open--;
	module_put(THIS_MODULE);
	return 0;
}

static long ring0_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
	case R0_COPY: {
		unsigned long n;
		n = copy_from_user(code, (const void __user*) arg, CODE_SIZE);
		if (n < 0)
			return n;
		return 0;
	}
	case R0_EXEC: {
		unsigned long n = ((long (*)(void))code)();
		printk(KERN_INFO "Returned: %lx\n", n);
		return n;
	}
	default:
		return -EINVAL;
	}
}

static int __init ring0_init(void)
{
	code = __vmalloc(CODE_SIZE, GFP_KERNEL, PAGE_KERNEL_EXEC);
	if (code == NULL) {
		printk(KERN_ALERT "Couldn't allocate executable page\n");
		return -ENOMEM;
	}
	if ((alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME)) <0) {
		printk(KERN_INFO "Couldn't allocate major number\n");
		return -1;
	}

	cdev_init(&ring0_cdev, &fops);

	if (cdev_add(&ring0_cdev, dev, 1) < 0) {
		printk(KERN_INFO "Cannot add device to system\n");
		goto r_class;
	}

	if ((dev_class = class_create(THIS_MODULE, "ring0_class")) == NULL) {
		printk(KERN_INFO "Cannot create the struct class\n");
		goto r_class;
	}

	if ((device_create(dev_class, NULL, dev, NULL, DEVICE_NAME)) == NULL) {
		printk(KERN_INFO "Cannot create the device\n");
		goto r_device;
	}

	printk(KERN_INFO "Ready to run!\n");
	return 0;

r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev, 1);
	return -1;
}

static void __exit ring0_exit(void)
{
	if (code != NULL)
		vfree(code);
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&ring0_cdev);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "See ya next time!\n");
}

module_init(ring0_init);
module_exit(ring0_exit);
