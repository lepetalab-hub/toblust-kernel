#ifndef KHACK_VERSION_CONTROL_H
#define KHACK_VERSION_CONTROL_H

#include <linux/version.h>

#ifdef DEBUG
#define PRINT_DEBUG(fmt, ...) pr_debug("[khack] " fmt, ##__VA_ARGS__)
#else
#define PRINT_DEBUG(fmt, ...) do {} while (0)
#endif

#endif /* KHACK_VERSION_CONTROL_H */
