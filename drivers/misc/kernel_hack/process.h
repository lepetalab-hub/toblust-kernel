#include <linux/sched.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 83))
#include <linux/sched/mm.h>
#endif

#define ARC_PATH_MAX 256

static size_t get_module_base(pid_t pid, char *name)
{
    struct task_struct *task;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    char buf[ARC_PATH_MAX];
    char *path_nm;
    size_t base = 0;

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock();
        return 0;
    }
    rcu_read_unlock();

    mm = get_task_mm(task);
    if (!mm) return 0;

    vma = find_vma(mm, 0);
    while (vma) {
        if (vma->vm_file) {
            path_nm = d_path(&vma->vm_file->f_path, buf, ARC_PATH_MAX - 1);
            if (!IS_ERR(path_nm) && !strcmp(kbasename(path_nm), name)) {
                if (strstr(kbasename(path_nm), ".so")) {
                    size_t candidate = (uintptr_t)vma->vm_start;
                    struct vm_area_struct *next = find_vma(mm, vma->vm_end);
                    if (next && next->vm_file) {
                        char *nm2 = d_path(&next->vm_file->f_path,
                                           buf, ARC_PATH_MAX - 1);
                        if (!IS_ERR(nm2) && !strcmp(kbasename(nm2), name)) {
                            base = candidate;
                            break;
                        }
                    }
                    /* try one more hop */
                    if (!base && next) {
                        next = find_vma(mm, next->vm_end);
                        if (next && next->vm_file) {
                            char *nm3 = d_path(&next->vm_file->f_path,
                                               buf, ARC_PATH_MAX - 1);
                            if (!IS_ERR(nm3) && !strcmp(kbasename(nm3), name)) {
                                base = candidate;
                                break;
                            }
                        }
                    }
                } else {
                    base = (uintptr_t)vma->vm_start;
                    break;
                }
            }
        }
        if (vma->vm_end >= ULONG_MAX) break;
        vma = find_vma(mm, vma->vm_end);
    }
    mmput(mm);
    return base;
}
