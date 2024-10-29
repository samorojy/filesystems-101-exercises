#include <solution.h>
#include <errno.h>
#include <sys/stat.h>
#include <liburing.h>
#include "fs_malloc.h"

#define QUEUE_DEPTH 8
#define IO_BLOCK_SIZE (256 * 1024)
#define MAX_READS 4

struct io_task {
    int is_read;
    off_t start_offset, current_offset;
    size_t first_length;
    char *buffer;
};


off_t get_file_size(int fd) {
    struct stat st;
    if (fstat(fd, &st) < 0) return -1;
    if (S_ISREG(st.st_mode)) {
        return st.st_size;
    }
    return -1;
}

int submit_read(int fd, struct io_uring *ring, off_t size, off_t offset) {
    struct io_task *task = fs_xmalloc(sizeof(*task) + size);
    if (!task) return -1;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        free(task);
        return -1;
    }

    task->is_read = true;
    task->current_offset = task->start_offset = offset;
    task->buffer = (char *) (task + 1);
    task->first_length = size;

    io_uring_prep_read(sqe, fd, task->buffer, size, offset);
    io_uring_sqe_set_data(sqe, task);
    return 0;
}

int copy_file(int in, int out, struct io_uring *ring, off_t file_size) {
    int read_operations = 0, write_operations = 0;
    off_t bytes_remaining_to_write = file_size;
    off_t current_offset = 0;

    while (file_size || bytes_remaining_to_write) {
        int read_batch_size = 0;

        for (int i = 0; i < MAX_READS; ++i) {
            off_t size = file_size > IO_BLOCK_SIZE ? IO_BLOCK_SIZE : file_size;
            if (!size || submit_read(in, ring, size, current_offset)) break;
            read_batch_size += size;
            file_size -= size;
            current_offset += size;
            read_operations++;
        }

        if (read_batch_size) {
            if (io_uring_submit(ring) < 0) return -errno;
        }

        while (read_batch_size) {
            struct io_uring_cqe *cqe;
            if (io_uring_wait_cqe(ring, &cqe)) return -errno;

            struct io_task *task = io_uring_cqe_get_data(cqe);

            task->is_read = false;
            task->current_offset = task->start_offset;
            task->buffer = (char *) (task + 1);

            struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
            io_uring_prep_write(sqe, out, task->buffer, task->first_length, task->current_offset);
            io_uring_sqe_set_data(sqe, task);
            if (io_uring_submit(ring) < 0) return -errno;

            bytes_remaining_to_write -= task->first_length;
            read_batch_size -= task->first_length;
            read_operations--;
            write_operations++;

            io_uring_cqe_seen(ring, cqe);
        }

        for (int i = 0; i < write_operations; ++i) {
            struct io_uring_cqe *cqe;
            if (io_uring_wait_cqe(ring, &cqe)) return -errno;

            struct io_task *task = io_uring_cqe_get_data(cqe);
            free(task);
            io_uring_cqe_seen(ring, cqe);
        }
        write_operations = 0;
    }
    return 0;
}

int copy(int in, int out) {
    struct io_uring ring;
    errno = 0;
    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) return -errno;
    off_t file_size = get_file_size(in);
    if (file_size < 0) return -errno;

    int result = copy_file(in, out, &ring, file_size);
    io_uring_queue_exit(&ring);
    return result;
}
