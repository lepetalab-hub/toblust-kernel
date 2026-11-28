# SPDX-License-Identifier: GPL-2.0-only
#
# Memory R/W kernel driver (AArch64). Ported from Ghopp3r/Android-Memory-Kernel-Driver.

ifneq ($(CONFIG_MEMDRV_HIDE_KGSL_STRENGTH),0)
ifeq ($(CONFIG_MEMDRV_HIDE_TASK),)
$(error MEMDRV_HIDE_KGSL_STRENGTH >= 1 requires MEMDRV_HIDE_TASK=y)
endif
endif

obj-$(CONFIG_MEMDRV) += memdrv.o

memdrv-y := \
	src/lifecycle.o \
	src/comm.o \
	src/memory.o \
	src/vfs_hijack.o \
	src/input_synth.o \
	src/sensor.o \
	src/harvest.o \
	src/hook_engine.o \
	src/uaccess_target.o \
	src/kallsym.o \
	src/hwbp.o \
	src/user_hook.o

memdrv-$(CONFIG_MEMDRV_HIDE_TASK) += src/dirent_hide.o
memdrv-$(CONFIG_MEMDRV_HIDE_SELF_MODULE) += src/module_hide.o
memdrv-$(filter-out 0,$(CONFIG_MEMDRV_HIDE_KGSL_STRENGTH)) += src/stealth.o

ccflags-y += -I$(src)/include -I$(src)/src
ccflags-y += -Wno-declaration-after-statement
ccflags-y += -Wno-unused-function
ccflags-y += -Wno-unused-variable

ccflags-y += -DDRIVER_NAME='"memdrv"'
ccflags-y += -DKCFG_DRIVER_NAME='"memdrv"'
ccflags-y += -DKCFG_TARGET_PACKAGE='"$(patsubst "%",%,$(CONFIG_MEMDRV_TARGET_PKG))"'
ccflags-y += -DKCFG_REBOOT_MAGIC=$(CONFIG_MEMDRV_REBOOT_MAGIC)u
ccflags-y += -DKCFG_HIDE_SELF_MODULE=$(if $(CONFIG_MEMDRV_HIDE_SELF_MODULE),1,0)
ccflags-y += -DKCFG_HIDE_VMAP=$(if $(CONFIG_MEMDRV_HIDE_VMAP),1,0)
ccflags-y += -DKCFG_HIDE_TASK=$(if $(CONFIG_MEMDRV_HIDE_TASK),1,0)
ccflags-y += -DKCFG_HIDE_KGSL_STRENGTH=$(or $(CONFIG_MEMDRV_HIDE_KGSL_STRENGTH),0)
