#include <linux/module.h>
#include <linux/tty.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include "comm.h"
#include "memory.h"
#include "process.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
	MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

long dispatch_ioctl(struct file* const file, unsigned int const cmd,
		    unsigned long const arg)
{
	static COPY_MEMORY cm;
	static MODULE_BASE mb;
	static char name[0x100] = {0};

	switch (cmd) {
	case OP_READ_MEM:
		if (copy_from_user(&cm, (void __user*)arg, sizeof(cm)) != 0)
			return -1;
		if (read_process_memory(cm.pid, cm.addr, cm.buffer, cm.size) == false)
			return -1;
		break;
	case OP_WRITE_MEM:
		if (copy_from_user(&cm, (void __user*)arg, sizeof(cm)) != 0)
			return -1;
		if (write_process_memory(cm.pid, cm.addr, cm.buffer, cm.size) == false)
			return -1;
		break;
	case OP_MODULE_BASE:
		if (copy_from_user(&mb, (void __user*)arg, sizeof(mb)) != 0
		    || copy_from_user(name, (void __user*)mb.name, sizeof(name) - 1) != 0)
			return -1;
		mb.base = get_module_base(mb.pid, name);
		if (copy_to_user((void __user*)arg, &mb, sizeof(mb)) != 0)
			return -1;
		break;
	default:
		break;
	}
	return 0;
}

static struct file_operations dispatch_functions = {
	.owner          = THIS_MODULE,
	.open           = dispatch_open,
	.release        = dispatch_close,
	.unlocked_ioctl = dispatch_ioctl,
};

struct mem_tool_device {
	struct cdev   cdev;
	struct device *dev;
	int            max;
};

static struct mem_tool_device *memdev;
static struct list_head       *prev_module;
static dev_t                   mem_tool_dev_t;
static struct class           *mem_tool_class;
static const char             *devicename;

int dispatch_open(struct inode *node, struct file *file)
{
	file->private_data = memdev;
	prev_module = __this_module.list.prev;
	list_del_init(&__this_module.list); /* sembunyikan dari /proc/modules */
	device_destroy(mem_tool_class, mem_tool_dev_t);
	class_destroy(mem_tool_class);
	pr_debug("kernel_hack: opened\n");
	return 0;
}

int dispatch_close(struct inode *node, struct file *file)
{
	list_add(&__this_module.list, prev_module);
	mem_tool_class = class_create(THIS_MODULE, devicename);
	memdev->dev = device_create(mem_tool_class, NULL, mem_tool_dev_t,
				    NULL, "%s", devicename);
	pr_debug("kernel_hack: closed\n");
	return 0;
}

static int __init driver_entry(void)
{
	int ret;

	devicename = get_rand_str(); /* nama device node acak tiap load */

	ret = alloc_chrdev_region(&mem_tool_dev_t, 0, 1, devicename);
	if (ret < 0) {
		pr_err("kernel_hack: alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}

	memdev = kmalloc(sizeof(*memdev), GFP_KERNEL);
	if (!memdev) {
		ret = -ENOMEM;
		goto err_unregister;
	}
	memset(memdev, 0, sizeof(*memdev));

	cdev_init(&memdev->cdev, &dispatch_functions);
	memdev->cdev.owner = THIS_MODULE;
	memdev->cdev.ops   = &dispatch_functions;

	ret = cdev_add(&memdev->cdev, mem_tool_dev_t, 1);
	if (ret) {
		pr_err("kernel_hack: cdev_add failed: %d\n", ret);
		goto err_free;
	}

	mem_tool_class = class_create(THIS_MODULE, devicename);
	if (IS_ERR(mem_tool_class)) {
		ret = PTR_ERR(mem_tool_class);
		pr_err("kernel_hack: class_create failed: %d\n", ret);
		goto err_cdev;
	}

	memdev->dev = device_create(mem_tool_class, NULL, mem_tool_dev_t,
				    NULL, "%s", devicename);
	if (IS_ERR(memdev->dev)) {
		ret = PTR_ERR(memdev->dev);
		pr_err("kernel_hack: device_create failed: %d\n", ret);
		goto err_class;
	}

	/* Hapus entry proc yang bisa jadi sidik jari */
	if (!IS_ERR(filp_open("/proc/sched_debug", O_RDONLY, 0)))
		remove_proc_subtree("sched_debug", NULL);
	if (!IS_ERR(filp_open("/proc/uevents_records", O_RDONLY, 0)))
		remove_proc_entry("uevents_records", NULL);

	/* Sembunyikan dari /proc/devices */
	unregister_chrdev_region(mem_tool_dev_t, 1);

	pr_debug("kernel_hack: loaded as /dev/%s\n", devicename);
	return 0;

err_class:
	class_destroy(mem_tool_class);
err_cdev:
	cdev_del(&memdev->cdev);
err_free:
	kfree(memdev);
err_unregister:
	unregister_chrdev_region(mem_tool_dev_t, 1);
	return ret;
}

static void __exit driver_unload(void)
{
	device_destroy(mem_tool_class, mem_tool_dev_t);
	class_destroy(mem_tool_class);
	cdev_del(&memdev->cdev);
	kfree(memdev);
	unregister_chrdev_region(mem_tool_dev_t, 1);
	pr_debug("kernel_hack: unloaded\n");
}

module_init(driver_entry);
module_exit(driver_unload);

MODULE_LICENSE("GPL");
