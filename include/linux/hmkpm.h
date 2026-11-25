/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_HMKPM_H
#define _LINUX_HMKPM_H

#include <linux/types.h>
#include <linux/uaccess.h>

#define HMKPM_MAGIC        0x484D4B504DULL
#define HMKPM_MAGIC_READ   (HMKPM_MAGIC + 1)
#define HMKPM_MAGIC_WRITE  (HMKPM_MAGIC + 2)
#define HMKPM_MAGIC_MAX    (HMKPM_MAGIC + 2) /* batch 3/4 belum diport */

static inline bool hmkpm_is_magic(u64 v)
{
	return v >= HMKPM_MAGIC && v <= HMKPM_MAGIC_MAX;
}

long hmkpm_dispatch(u64 magic, void __user *buf, unsigned long total);

#endif
