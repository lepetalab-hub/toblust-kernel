/* SPDX-License-Identifier: GPL-2.0 */
/* Compatibility shims for building FinaSU on kernels older than 5.8. */
#ifndef __KSU_COMPAT_419_H
#define __KSU_COMPAT_419_H

#include <linux/version.h>
#include <linux/build_bug.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#ifndef static_assert
#define KSU_SA_CAT_(a, b) a##b
#define KSU_SA_CAT(a, b) KSU_SA_CAT_(a, b)
#define static_assert(expr, ...) \
	typedef char KSU_SA_CAT(ksu_sa_, __LINE__)[(expr) ? 1 : -1] __attribute__((unused))
#endif

#include <asm/unistd.h>
#ifndef SECCOMP_ARCH_NATIVE_NR
/* Pre-5.x seccomp headers lack this; __NR_syscalls is the native syscall count. */
#define SECCOMP_ARCH_NATIVE_NR __NR_syscalls
#endif

#ifndef untagged_addr
#define untagged_addr(addr) (addr)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)

static inline int copy_from_kernel_nofault(void *dst, const void *src, size_t size)
{
	return probe_kernel_read(dst, src, size);
}

static inline long strncpy_from_user_nofault(char *dst, const void __user *src, long count)
{
	return strncpy_from_user(dst, (const char __user *)src, count);
}

static inline int copy_from_user_nofault(void *dst, const void __user *src, size_t size)
{
	int ret;

	pagefault_disable();
	ret = __copy_from_user_inatomic(dst, src, size);
	pagefault_enable();
	return ret ? -EFAULT : 0;
}

static inline int copy_to_user_nofault(void __user *dst, const void *src, size_t size)
{
	int ret;

	pagefault_disable();
	ret = __copy_to_user_inatomic(dst, src, size);
	pagefault_enable();
	return ret ? -EFAULT : 0;
}

static inline int copy_to_kernel_nofault(void *dst, const void *src, size_t size)
{
	return probe_kernel_write(dst, src, size);
}

#endif /* LINUX_VERSION_CODE < 5.8 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
/* task_work_add(task, cb, bool notify) before 5.11; TWA_RESUME meant notify=true. */
#define TWA_RESUME true
#endif

static inline ssize_t strscpy_pad(char *dest, const char *src, size_t count)
{
	ssize_t r = strscpy(dest, src, count);

	if (r >= 0 && (size_t)r < count)
		memset(dest + r, 0, count - r);
	return r;
}

#ifndef fallthrough
#define fallthrough __attribute__((__fallthrough__))
#endif

#endif /* __KSU_COMPAT_419_H */
