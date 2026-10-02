#ifndef _hostshim_zephyr_fs
#define _hostshim_zephyr_fs

/* Host shim of <zephyr/fs/fs.h>: the node file API implemented over stdio (src/hostshim.c) */

#include <stdio.h>
#include <sys/types.h>

struct fs_file_t {
    FILE *fp;
};

#define FS_O_READ 1

void fs_file_t_init(struct fs_file_t *file);
int fs_open(struct fs_file_t *file, const char *path, int flags);
ssize_t fs_read(struct fs_file_t *file, void *buffer, size_t size);
int fs_close(struct fs_file_t *file);

#endif
