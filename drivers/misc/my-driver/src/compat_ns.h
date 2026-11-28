/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MY_DRIVER_COMPAT_NS_H
#define _MY_DRIVER_COMPAT_NS_H

/* MODULE_IMPORT_NS changed semantics in 5.15.
 * On kernels that do not define or expand it correctly, no-op. */
#ifndef MODULE_IMPORT_NS
#define MODULE_IMPORT_NS(ns)
#endif

#endif /* _MY_DRIVER_COMPAT_NS_H */
