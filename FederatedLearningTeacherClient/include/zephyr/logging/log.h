#ifndef _hostshim_zephyr_log
#define _hostshim_zephyr_log

/* Host shim of <zephyr/logging/log.h>: log macros go to stdout */

#include <stdio.h>

#define LOG_MODULE_REGISTER(...)
#define LOG_LEVEL_INF 0
#define LOG_INF(fmt, ...) printf("<inf> " fmt "\n", ##__VA_ARGS__)
#define LOG_WRN(fmt, ...) printf("<wrn> " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) printf("<err> " fmt "\n", ##__VA_ARGS__)

#endif
