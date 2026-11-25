/* https://cirosantilli.com/linux-kernel-module-cheat#procfs */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h> /* seq_read, seq_lseek, single_open, single_release */
#include <uapi/linux/stat.h> /* S_IRUSR */

static const char *filename = "lkmc_procfs";

static int show(struct seq_file *m, void *v)
{
	seq_printf(m, "abcd\n");
	return 0;
}

static int open(struct inode *inode, struct  file *file)
{
	return single_open(file, show, NULL);
}

/* 4.19: proc_ops (5.6+) does not exist yet, use file_operations. */
static const struct file_operations pops = {
	.llseek = seq_lseek,
	.open = open,
	.read = seq_read,
	.release = single_release,
};

static int myinit(void)
{
	proc_create(filename, 0, NULL, &pops);
	return 0;
}

static void myexit(void)
{
	remove_proc_entry(filename, NULL);
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(__FILE__);

