// SPDX-License-Identifier: GPL-2.0
/*
 * HMKPM: built-in port of HuntMemory-KPM v3.0.0 core I/O.
 * Ops: single R/W, batch R/W. Control via /sys/kernel/hmkpm/enable.
 */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kstrtox.h>
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

#define HMKPM_BOUNCE_SIZE        512U
#define HMKPM_MAX_SINGLE         (64ULL << 20)
#define HMKPM_MAX_ENTRY          (64ULL << 20)
#define HMKPM_MAX_BATCH_ENTRIES  65536U
#define HMKPM_MAX_BATCH_TOTAL    (128ULL << 20)
#define HMKPM_BATCH_CHUNK        32U
#define HMKPM_ADDR_MASK          0x00FFFFFFFFFFFFFFULL /* TBI: strip top byte */

struct hmkpm_req {
	s32 pid;
	u32 _pad;
	u64 addr;
	u64 size;
};

struct hmkpm_batch_hdr {
	s32 pid;
	u32 _pad;
	u64 count;
	u64 data_total;
};

struct hmkpm_batch_entry {
	u64 addr;
	u64 size;
};

static bool hmkpm_active = true;

/* Resolve va -> pa inside mm. Caller holds mmap_sem (read). */
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
	if (pud_sect(*pud)) {
		if (!pud_present(*pud) || (write && !pud_write(*pud)))
			return -EACCES;
		base = PFN_PHYS(pud_pfn(*pud)) + (va & ~PUD_MASK);
		goto out;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd))
		return -EFAULT;
	if (pmd_sect(*pmd)) {
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

static struct mm_struct *hmkpm_get_mm(pid_t pid)
{
	struct task_struct *task;
	struct mm_struct *mm;

	rcu_read_lock();
	task = find_task_by_vpid(pid);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();

	if (!task)
		return NULL;
	mm = get_task_mm(task);
	put_task_struct(task);
	return mm;
}

static long hmkpm_single(u64 magic, void __user *buf, unsigned long total)
{
	struct hmkpm_req req;
	struct mm_struct *mm;
	bool write = (magic == HMKPM_MAGIC_WRITE);
	ssize_t ret;

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
	if (req.addr > U64_MAX - req.size)
		return -EINVAL;

	mm = hmkpm_get_mm(req.pid);
	if (!mm)
		return -ESRCH;

	ret = hmkpm_rw_mm(mm, (unsigned long)req.addr, (size_t)req.size,
			  (char __user *)buf + sizeof(req), write);
	mmput(mm);

	if (ret < 0)
		return ret;
	return ret == (ssize_t)req.size ? 0 : -EFAULT;
}

static long hmkpm_batch(u64 magic, void __user *ubuf, unsigned long total)
{
	struct hmkpm_batch_hdr hdr;
	struct hmkpm_batch_entry chunk[HMKPM_BATCH_CHUNK];
	struct mm_struct *mm;
	bool write = (magic == HMKPM_MAGIC_WRITE_BATCH);
	u64 entries_end, io_end, data_start, data_off = 0;
	u32 done = 0;
	long rc = 0;

	if (total < sizeof(hdr))
		return -EINVAL;
	if (copy_from_user(&hdr, ubuf, sizeof(hdr)))
		return -EFAULT;
	if (hdr.pid <= 0)
		return -EINVAL;

	if (hdr.count == 0) {
		if (hdr.data_total != 0)
			return -EINVAL;
		if (write && total != sizeof(hdr))
			return -EINVAL;
		return 0;
	}
	if (hdr.count > HMKPM_MAX_BATCH_ENTRIES)
		return -E2BIG;
	if (hdr.data_total > HMKPM_MAX_BATCH_TOTAL)
		return -E2BIG;

	entries_end = sizeof(hdr) + hdr.count * sizeof(struct hmkpm_batch_entry);
	io_end = entries_end + hdr.data_total;
	if (write) {
		if (total != io_end)
			return -EINVAL;
	} else if (total < io_end) {
		return -ERANGE;
	}

	mm = hmkpm_get_mm(hdr.pid);
	if (!mm)
		return -ESRCH;

	data_start = entries_end;

	while (done < hdr.count) {
		u32 n = (u32)min_t(u64, hdr.count - done, HMKPM_BATCH_CHUNK);
		u64 chunk_bytes = (u64)n * sizeof(chunk[0]);
		void __user *entry_src = (char __user *)ubuf + sizeof(hdr) +
					 (u64)done * sizeof(chunk[0]);
		u32 i;

		if (copy_from_user(chunk, entry_src, chunk_bytes)) {
			rc = -EFAULT;
			goto out;
		}

		for (i = 0; i < n; i++) {
			struct hmkpm_batch_entry *e = &chunk[i];
			u64 req = e->size;
			u64 va = e->addr & HMKPM_ADDR_MASK;
			void __user *dp = (char __user *)ubuf + data_start + data_off;
			ssize_t ret;

			if (req == 0)
				continue;
			if (req > HMKPM_MAX_ENTRY) {
				rc = -E2BIG;
				goto out;
			}
			if (req > hdr.data_total - data_off) {
				rc = -EINVAL;
				goto out;
			}

			if (va > U64_MAX - req) {
				e->size = 0;
				if (!write)
					(void)clear_user(dp, req);
				data_off += req;
				continue;
			}

			ret = hmkpm_rw_mm(mm, (unsigned long)va, (size_t)req, dp, write);
			if (ret < 0) {
				e->size = 0;
				if (!write)
					(void)clear_user(dp, req);
			} else if ((u64)ret < req) {
				e->size = (u64)ret;
				if (!write)
					(void)clear_user((char __user *)dp + ret, req - (u64)ret);
			} else {
				e->size = req;
			}
			data_off += req;
		}

		if (copy_to_user(entry_src, chunk, chunk_bytes)) {
			rc = -EFAULT;
			goto out;
		}

		done += n;
		cond_resched();
	}

	if (data_off != hdr.data_total)
		rc = -EINVAL;

out:
	mmput(mm);
	return rc;
}

long hmkpm_dispatch(u64 magic, void __user *buf, unsigned long total)
{
	if (magic == HMKPM_MAGIC)           /* handshake */
		return READ_ONCE(hmkpm_active) ? 0 : -ENOSYS;
	if (!READ_ONCE(hmkpm_active))
		return -ENOSYS;
	if (!uid_eq(current_euid(), GLOBAL_ROOT_UID))
		return -EPERM;

	switch (magic) {
	case HMKPM_MAGIC_READ:
	case HMKPM_MAGIC_WRITE:
		return hmkpm_single(magic, buf, total);
	case HMKPM_MAGIC_READ_BATCH:
	case HMKPM_MAGIC_WRITE_BATCH:
		return hmkpm_batch(magic, buf, total);
	default:
		return -ENOSYS;
	}
}

/* ---- Control: /sys/kernel/hmkpm/enable ---- */

static ssize_t enable_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 READ_ONCE(hmkpm_active) ? "active" : "inactive");
}

static ssize_t enable_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	bool v;

	if (!strncmp(buf, "toggle", 6)) {
		v = !READ_ONCE(hmkpm_active);
	} else if (kstrtobool(buf, &v)) {
		return -EINVAL;
	}
	WRITE_ONCE(hmkpm_active, v);
	return count;
}

static struct kobj_attribute enable_attr = __ATTR(enable, 0644,
						  enable_show, enable_store);
static struct kobject *hmkpm_kobj;

static int __init hmkpm_init(void)
{
	int ret;

	hmkpm_kobj = kobject_create_and_add("hmkpm", kernel_kobj);
	if (!hmkpm_kobj)
		return -ENOMEM;
	ret = sysfs_create_file(hmkpm_kobj, &enable_attr.attr);
	if (ret)
		kobject_put(hmkpm_kobj);
	return ret;
}
late_initcall(hmkpm_init);
