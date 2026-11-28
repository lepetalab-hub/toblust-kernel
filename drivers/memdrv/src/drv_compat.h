// SPDX-License-Identifier: GPL-2.0-only
// Kernel-version shims. The driver targets 5.10..6.12; this tree is 4.19,
// so the handful of APIs that moved are bridged here, once.
#ifndef DRIVER_COMPAT_H
#define DRIVER_COMPAT_H

#include <linux/version.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#ifndef __nocfi
#define __nocfi
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#include <linux/mmap_lock.h>
#else
/* mmap_lock_* wrappers arrived in 5.8; before that the rwsem is mm->mmap_sem. */
#define mmap_read_lock(mm)   down_read(&(mm)->mmap_sem)
#define mmap_read_unlock(mm) up_read(&(mm)->mmap_sem)
#define mmap_write_lock(mm)   down_write(&(mm)->mmap_sem)
#define mmap_write_unlock(mm) up_write(&(mm)->mmap_sem)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
/* copy_from_kernel_nofault() (5.8+) equals probe_kernel_read() on 4.19. */
#define copy_from_kernel_nofault(dst, src, size) probe_kernel_read((dst), (src), (size))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
/*
 * task_work_add() took a plain bool notify flag before 5.11. The enum below
 * keeps the call sites identical; TWA_RESUME == 1 == true, so the value
 * passed through the kallsyms-resolved pointer is ABI-equivalent.
 */
enum task_work_notify_mode {
	TWA_NONE = 0,
	TWA_RESUME = 1,
	TWA_SIGNAL = 2,
};
#endif


/* ktime_get_boottime_ns() arrived in 5.3 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
#include <linux/timekeeping.h>
#include <linux/ktime.h>
static inline u64 ktime_get_boottime_ns(void) {
	return ktime_to_ns(ktime_get_boottime());
}
#endif


/* <linux/pgtable.h> arrived in 5.8; before that the content is in <asm/pgtable.h> */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
#include <asm/pgtable.h>
#endif

/* pud_leaf / pmd_leaf: aliases for huge-page detection, added in 5.5.
 * On arm64 4.19: a PUD/PMD is a "leaf" (block descriptor) if its TABLE_BIT
 * is clear but it is present. This matches the logic in pud_bad / pmd_bad.
 * We use the same bit-tests the upstream macros use on 5.5+. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
#include <asm/pgtable-hwdef.h>
static inline int pud_leaf(pud_t pud)
{
	return (pud_val(pud) & PUD_TYPE_MASK) == PUD_TYPE_SECT;
}
static inline int pmd_leaf(pmd_t pmd)
{
	return (pmd_val(pmd) & PMD_TYPE_MASK) == PMD_TYPE_SECT;
}
#endif

/* access_ok() gained a two-argument form in 5.0, dropping the type arg.
 * Redefine the two-arg form to call the three-arg version on 4.19. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
#ifdef access_ok
#undef access_ok
#endif
#define access_ok(addr, size) __range_ok((addr), (size))
#endif
#endif /* DRIVER_COMPAT_H */
