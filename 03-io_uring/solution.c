#include <liburing.h>
#include <errno.h>

const int READ_QUEUE = 4;
const int IO_BLOCK_SIZE = 256 * 1024;

int copy(int in, int out)
{
    struct io_uring ring;
    if (io_uring_queue_init(READ_QUEUE, &ring, 0) < 0)
    {
        return -1;
    }

    char buffer[READ_QUEUE][IO_BLOCK_SIZE];
    struct io_uring_sqe* sqe;
    struct io_uring_cqe* cqe;
    off_t read_offset = 0;
    off_t write_offset = 0;
    int ret, submit_count = 0;

    for (int i = 0; i < READ_QUEUE; ++i)
    {
        sqe = io_uring_get_sqe(&ring);
        if (!sqe)
        {
            ret = -ENOMEM;
            goto cleanup;
        }
        io_uring_prep_read(sqe, in, buffer[i], IO_BLOCK_SIZE, read_offset);
        sqe->user_data = (uint64_t)&buffer[i];
        read_offset += IO_BLOCK_SIZE;
        submit_count++;
    }

    ret = io_uring_submit(&ring);
    if (ret < 0)
    {
        ret = -errno;
        goto cleanup;
    }

    while (submit_count > 0)
    {
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
        {
            ret = -errno;
            goto cleanup;
        }

        if (cqe->res < 0)
        {
            ret = -cqe->res;
            goto cleanup;
        }

        if (cqe->res == 0)
        {
            io_uring_cqe_seen(&ring, cqe);
            break;
        }

        int bytes_read = cqe->res;
        char* buffer_ptr = (char*)cqe->user_data;

        sqe = io_uring_get_sqe(&ring);
        if (!sqe)
        {
            ret = -ENOMEM;
            goto cleanup;
        }
        io_uring_prep_write(sqe, out, buffer_ptr, bytes_read, write_offset);
        write_offset += bytes_read;

        ret = io_uring_submit(&ring);
        if (ret < 0)
        {
            ret = -errno;
            goto cleanup;
        }

        io_uring_cqe_seen(&ring, cqe);
        submit_count--;

        if (bytes_read < IO_BLOCK_SIZE)
        {
            break;
        }

        sqe = io_uring_get_sqe(&ring);
        if (!sqe)
        {
            ret = -ENOMEM;
            goto cleanup;
        }
        io_uring_prep_read(sqe, in, buffer_ptr, IO_BLOCK_SIZE, read_offset);
        sqe->user_data = (uint64_t)buffer_ptr;
        read_offset += IO_BLOCK_SIZE;
        submit_count++;

        ret = io_uring_submit(&ring);
        if (ret < 0)
        {
            ret = -errno;
            goto cleanup;
        }
    }

    ret = 0;

cleanup:
    io_uring_queue_exit(&ring);
    return ret;
}
