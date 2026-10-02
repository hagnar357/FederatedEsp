#ifndef _hostshim_zephyr_kernel
#define _hostshim_zephyr_kernel

/* Host shim of <zephyr/kernel.h> for the node sources compiled on Linux */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

static inline void k_msleep(int ms) { usleep((useconds_t)ms * 1000); }

/* Kconfig symbols used by knowledgedistillation.c (same defaults as the node's prj.conf) */
#ifndef CONFIG_FL_KD_TEMPERATURE_X10
#define CONFIG_FL_KD_TEMPERATURE_X10 30
#endif
#ifndef CONFIG_FL_KD_ALPHA_PERCENT
#define CONFIG_FL_KD_ALPHA_PERCENT 50
#endif
/* distillation starts disabled; main.c enables it at runtime unless --no-kd is given */
#define IS_ENABLED(x) 0

#endif
