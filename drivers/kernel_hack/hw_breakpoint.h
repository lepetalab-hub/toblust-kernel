#ifndef KHACK_HW_BREAKPOINT_H
#define KHACK_HW_BREAKPOINT_H

#include <linux/perf_event.h>
#include "comm.h"
#include "version_control.h"

#ifdef CONFIG_HW_BREAKPOINT_MODE

int  khack_hw_breakpoint_init(struct perf_event_attr *attr);
int  khack_hw_bp_module_init(void);
void khack_hw_bp_module_exit(void);
int  handle_hw_breakpoint_control(PHW_BREAKPOINT_CTL ctl);
int  handle_hw_breakpoint_get_hits(PHW_BREAKPOINT_GET_HITS_CTL ctl,
				   unsigned long arg);
int  handle_hw_breakpoint_clear_all(void);

#else /* !CONFIG_HW_BREAKPOINT_MODE */

static inline int  khack_hw_bp_module_init(void)                              { return 0; }
static inline void khack_hw_bp_module_exit(void)                              {}
static inline int  handle_hw_breakpoint_control(PHW_BREAKPOINT_CTL c)         { return -ENODEV; }
static inline int  handle_hw_breakpoint_get_hits(PHW_BREAKPOINT_GET_HITS_CTL c,
						  unsigned long a)             { return -ENODEV; }
static inline int  handle_hw_breakpoint_clear_all(void)                       { return -ENODEV; }

#endif /* CONFIG_HW_BREAKPOINT_MODE */

#endif /* KHACK_HW_BREAKPOINT_H */
