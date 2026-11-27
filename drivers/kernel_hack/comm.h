#pragma once
#include <linux/slab.h>
#include <linux/random.h>

#define DEVICE_NAME "wanbai"

typedef struct _COPY_MEMORY {
    pid_t pid;
    uintptr_t addr;
    void* buffer;
    size_t size;
} COPY_MEMORY, *PCOPY_MEMORY;

typedef struct _MODULE_BASE {
    pid_t pid;
    char* name;
    uintptr_t base;
} MODULE_BASE, *PMODULE_BASE;

enum OPERATIONS {
    OP_INIT_KEY    = 0x800,
    OP_READ_MEM    = 0x801,
    OP_WRITE_MEM   = 0x802,
    OP_MODULE_BASE = 0x803,
};

char* get_rand_str(void)
{
	static char string[10];
	int lstr, seed, flag, i;
	char *str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	lstr = strlen(str);
	for (i = 0; i < 6; i++) {
		get_random_bytes(&seed, sizeof(int));
		flag = seed % lstr;
		if (flag < 0)
			flag = flag * -1;
		string[i] = str[flag];
	}
	string[6] = '\0';
	return string;
}

int dispatch_open(struct inode *node, struct file *file);
int dispatch_close(struct inode *node, struct file *file);

/* ── Hardware Breakpoint structs ───────────────────────────────────── */
#include <asm/ptrace.h>

#define OP_CMD_HWBP_ADD      0x804
#define OP_CMD_HWBP_GET_HITS 0x805
#define OP_CMD_HWBP_ENABLE   0x806
#define OP_CMD_HWBP_CLEAR    0x807
#define OP_CMD_HWBP_DISABLE  0x809

#define HW_BP_TYPE_READ     1
#define HW_BP_TYPE_WRITE    2
#define HW_BP_TYPE_RW       3
#define HW_BP_TYPE_EXECUTE  4

enum HW_BP_ACTION {
	HW_BP_ADD    = 0,
	HW_BP_REMOVE = 1,
};

typedef struct _HW_BREAKPOINT_CTL {
	pid_t     pid;
	uintptr_t addr;
	int       type;
	int       len;
	int       action; /* HW_BP_ADD / HW_BP_REMOVE */
} HW_BREAKPOINT_CTL, *PHW_BREAKPOINT_CTL;

typedef struct _HW_BREAKPOINT_GET_HITS_CTL {
	pid_t        pid;
	uintptr_t    addr;
	void __user *buffer;
	int          count;
} HW_BREAKPOINT_GET_HITS_CTL, *PHW_BREAKPOINT_GET_HITS_CTL;

typedef struct _HW_BREAKPOINT_HIT_INFO {
	pid_t               pid;
	uint64_t            timestamp;
	uint64_t            addr;
	struct user_pt_regs regs;
} HW_BREAKPOINT_HIT_INFO, *PHW_BREAKPOINT_HIT_INFO;
