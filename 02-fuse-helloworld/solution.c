#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static const char *root_entries[] = { "hello", NULL };

static const struct stat hello_stat = {
    .st_mode = S_IFREG | S_IRUSR,
    .st_nlink = 1,
    .st_uid = 9091,
    .st_gid = 9091,
    .st_size = 32,
};

static const struct stat dir_stat = {
    .st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH,
    .st_nlink = 2,
    .st_uid = 9091,
    .st_gid = 9091,
};

static int hello_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        *stbuf = dir_stat;
    } else if (strcmp(path, "/hello") == 0) {
        *stbuf = hello_stat;
    } else {
        return -ENOENT;
    }
    return 0;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
    for (const char **entry = root_entries; *entry != NULL; entry++) {
        filler(buf, *entry, NULL, 0, FUSE_FILL_DIR_PLUS);
    }

    return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/hello") != 0) {
        return -ENOENT;
    }

    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EROFS;
    }

    return 0;
}

static int hello_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) fi;

    if (strcmp(path + 1, "hello") != 0) {
        return -ENOENT;
    }

    char hello_file_content[32];
    snprintf(hello_file_content, sizeof(hello_file_content),"hello, %d\n", fuse_get_context()->pid);
    size_t length = strlen(hello_file_content);

    if (offset < (off_t) length) {
        if (size + (size_t) offset > length) {
            size = length - (size_t) offset;
        }
        memcpy(buf, hello_file_content + offset, size);
    } else {
        size = 0;
    }

    return size;
}

static int hello_write(const char* path, const char* buf,
    size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) path;
    (void) buf;
    (void) size;
    (void) offset;
    (void) fi;
    return -EROFS;
}

static int hello_mkdir(const char* path, mode_t mode) {
    (void) path;
    (void) mode;
    return -EROFS;
}

static int hello_mknod(const char* path, mode_t mode, dev_t dev) {
    (void) path;
    (void) mode;
    (void) dev;
    return -EROFS;
}

static int hello_write_buf(const char* path, struct fuse_bufvec* buf, off_t offset, struct fuse_file_info* fi) {
    (void) path;
    (void) buf;
    (void) offset;
    (void) fi;
    return -EROFS;
}

static int hello_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    (void) path;
    (void) mode;
    (void) fi;
    return -EROFS;
}

static int hello_truncate(const char* path, off_t offset, struct fuse_file_info* fi) {
    (void) path;
    (void) offset;
    (void) fi;
    return -EROFS;
}

static int hello_symlink(const char* path, const char* link) {
    (void) path;
    (void) link;
    return -EROFS;
}

static struct fuse_operations hellofs_ops = {
    .getattr = hello_getattr,
    .readdir = hello_readdir,
    .open = hello_open,
    .read = hello_read,
    .write = hello_write,
    .mkdir = hello_mkdir,
    .mknod = hello_mknod,
    .write_buf = hello_write_buf,
    .create = hello_create,
    .truncate = hello_truncate,
    .symlink = hello_symlink
};

int helloworld(const char *mntp) {
    char *argv[] = {"exercise", "-f", (char *)mntp, NULL};
    return fuse_main(3, argv, &hellofs_ops, NULL);
}
