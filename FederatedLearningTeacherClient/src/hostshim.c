#include <errno.h>
#include <zephyr/fs/fs.h>
#include "espconfiguration.h"

const char *teacherclient_dataset_path = "dataset.csv";

void fs_file_t_init(struct fs_file_t *file) {
    file->fp = NULL;
}

int fs_open(struct fs_file_t *file, const char *path, int flags) {
    (void)flags;
    file->fp = fopen(path, "r");
    return file->fp != NULL ? 0 : -errno;
}

ssize_t fs_read(struct fs_file_t *file, void *buffer, size_t size) {
    size_t n = fread(buffer, 1, size, file->fp);
    if (n == 0 && ferror(file->fp)) {
        return -EIO;
    }
    return (ssize_t)n;
}

int fs_close(struct fs_file_t *file) {
    if (file->fp != NULL) {
        fclose(file->fp);
        file->fp = NULL;
    }
    return 0;
}
