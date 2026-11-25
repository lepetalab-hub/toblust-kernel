// SPDX-License-Identifier: GPL-2.0
/*
 * HMKPM: built-in port of HuntMemory-KPM core (Yervant7).
 * Single read/write via arm64 page-table walk.
 */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/cred.h>
#include <linux/string.h>
#include <linux/hmkpm.h>
#include <asm/pgtable.h>

#define HMKPM_BOUNCE_SIZE    512U
#define HMKPM_MAX_SINGLE     (64ULL << 20)
#define HMKPM_ADDR_MASK      0x00FFFFFFFFFFFFFFULL /* TBI: strip top byte */

struct hmkpm_req {
	s32 pid;
	u32 _pad;
	u64 addr;
	u64 size;
};

static bool hmkpm_active = true;

/* Resolve va -> pa inside mm. Caller holds mmap_read_lock(mm). */
static int hmkpm_walk(struct mm_struct *mm, unsigned long va,
		      phys_addr_t *pa, bool write)
{
	pgd_t *pgd = pgd_offset(mm, va);
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	phys_addr_t base;

	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return -EFAULT;

	p4d = p4d_offset(pgd, va);
	if (p4d_none(*p4d) || p4d_bad(*p4d))
		return -EFAULT;

	pud = pud_offset(p4d, va);
	if (pud_none(*pud))
		return -EFAULT;
	if (pud_huge(*pud)) {                       /* 1GB block */
		if (!pud_present(*pud) || (write && !pud_write(*pud)))
			return -EACCES;
		base = PFN_PHYS(pud_pfn(*pud)) + (va & ~PUD_MASK);
		goto out;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd))
		return -EFAULT;
	if (pmd_huge(*pmd)) {                       /* 2MB block */
		if (!pmd_present(*pmd) || (write && !pmd_write(*pmd)))
			return -EACCES;
		base = PFN_PHYS(pmd_pfn(*pmd)) + (va & ~PMD_MASK);
		goto out;
	}

	pte = pte_offset_kernel(pmd, va);
	if (!pte_present(*pte))
		return -EFAULT;
	if (write && !pte_write(*pte))
		return -EACCES;
	base = PFN_PHYS(pte_pfn(*pte)) + (va & ~PAGE_MASK);

out:
	if (!pfn_valid(base >> PAGE_SHIFT))
		return -EFAULT;
	*pa = base;
	return 0;
}

static ssize_t hmkpm_rw_mm(struct mm_struct *mm, unsigned long va, size_t len,
			   void __user *ubuf, bool write)
{
	u8 bounce[HMKPM_BOUNCE_SIZE];
	ssize_t total = 0;

	while (len) {
		unsigned long off = va & ~PAGE_MASK;
		size_t chunk = min_t(size_t, len,
				     min_t(size_t, PAGE_SIZE - off, HMKPM_BOUNCE_SIZE));
		phys_addr_t pa;
		int rc;

		if (write && copy_from_user(bounce, ubuf, chunk))
			return total ? total : -EFAULT;

		down_read(&mm->mmap_sem);
		rc = hmkpm_walk(mm, va, &pa, write);
		if (!rc) {
			void *kva = __va(pa);

			if (write)
				memcpy(kva, bounce, chunk);
			else
				memcpy(bounce, kva, chunk);
		}
		up_read(&mm->mmap_sem);

		if (rc)
			return total ? total : rc;

		if (!write && copy_to_user(ubuf, bounce, chunk))
			return total ? total : -EFAULT;

		total += chunk;
		ubuf = (char __user *)ubuf + chunk;
		va += chunk;
		len -= chunk;
	}
	return total;
}

long hmkpm_dispatch(u64 magic, void __user *buf, unsigned long total)
{
	struct hmkpm_req req;
	struct task_struct *task;
	struct mm_struct *mm;
	bool write;
	ssize_t ret;

	if (magic == HMKPM_MAGIC)           /* handshake */
		return hmkpm_active ? 0 : -ENOSYS;
	if (!hmkpm_active)
		return -ENOSYS;
	if (magic != HMKPM_MAGIC_READ && magic != HMKPM_MAGIC_WRITE)
		return -ENOSYS;                 /* batch belum diport */

	if (!uid_eq(current_euid(), GLOBAL_ROOT_UID))
		return -EPERM;

	write = (magic == HMKPM_MAGIC_WRITE);

	if (total < sizeof(req))
		return -EINVAL;
	if (copy_from_user(&req, buf, sizeof(req)))
		return -EFAULT;
	if (req.size == 0)
		return total == sizeof(req) ? 0 : -EINVAL;
	if (req.size > HMKPM_MAX_SINGLE)
		return -E2BIG;
	if (total != sizeof(req) + req.size)
		return -EINVAL;
	if (req.pid <= 0)
		return -EINVAL;

	req.addr &= HMKPM_ADDR_MASK;
	if (req.addr > ULONG_MAX - req.size)
		return -EINVAL;

	rcu_read_lock(); task = find_task_by_vpid(req.pid); if (task) get_task_struct(task); rcu_read_unlock();
	if (!task)
		return -ESRCH;
	mm = get_task_mm(task);
	put_task_struct(task);
	if (!mm)
		return -ESRCH;

	ret = hmkpm_rw_mm(mm, (unsigned long)req.addr, (size_t)req.size,
			  (char __user *)buf + sizeof(req), write);
	mmput(mm);

	if (ret < 0)
		return ret;
	return ret == (ssize_t)req.size ? 0 : -EFAULT;
}
