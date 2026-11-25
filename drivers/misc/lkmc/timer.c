/* https://cirosantilli.com/linux-kernel-module-cheat#timers */

#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/version.h>

static int i;
/* We would normally mark this as static and give it a more generic name.
 * But let's do it like this this time for the sake of our GDB kernel module step debugging example. */
static void lkmc_timer_callback(struct timer_list *data);
static unsigned long onesec;

static DEFINE_TIMER(lkmc_mytimer, lkmc_timer_callback);

static void lkmc_timer_callback(struct timer_list *data)
{
	pr_info("%d\n", i);
	i++;
	if (i == 10)
		i = 0;
	mod_timer(&lkmc_mytimer, jiffies + onesec);
}

static int myinit(void)
{
	onesec = msecs_to_jiffies(1000);
	mod_timer(&lkmc_mytimer, jiffies + onesec);
	return 0;
}

static void myexit(void)
{
/* v6.15-rc1 8fa7292fee5c8e6682efc5aff2932af1b7edac9e ("treewide: Switch/rename to timer_delete[_sync]()") */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
	timer_delete(&lkmc_mytimer);
#else
	del_timer(&lkmc_mytimer);
#endif
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(__FILE__);
