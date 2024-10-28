#include <liburing.h>
#include <errno.h>

const int READ_QUEUE = 4;
const int IO_BLOCK_SIZE = 256 * 1024;

static int submit_read(struct io_uring* ring, int in, char buffer[READ_QUEUE][IO_BLOCK_SIZE],
                       off_t* read_offset, int* inflight_reads)
{
    for (int i = 0; i < READ_QUEUE; ++i)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe)
        {
            return -ENOMEM;
        }
        io_uring_prep_read(sqe, in, buffer[i], IO_BLOCK_SIZE, *read_offset);
        sqe->user_data = (uint64_t)&buffer[i];
        *read_offset += IO_BLOCK_SIZE;
        (*inflight_reads)++;
    }

    int ret = io_uring_submit(ring);
    if (ret < 0)
    {
        return -errno;
    }
    return 0;
}

static int process_write(struct io_uring* ring, struct io_uring_cqe* cqe, int out, off_t* write_offset,
                         int* submit_count)
{
    int bytes_read = cqe->res;
    char* buffer_ptr = (char*)cqe->user_data;

    if (bytes_read > 0)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe)
        {
            return -ENOMEM;
        }
        io_uring_prep_write(sqe, out, buffer_ptr, bytes_read, *write_offset);
        *write_offset += bytes_read;
        (*submit_count)++;

        int ret = io_uring_submit(ring);
        if (ret < 0)
        {
            return -errno;
        }
    }

    io_uring_cqe_seen(ring, cqe);
    return bytes_read == IO_BLOCK_SIZE ? 1 : 0;
}

static int wait_requests(struct io_uring* ring, int* submit_count)
{
    struct io_uring_cqe* cqe;
    int ret;

    while (*submit_count > 0)
    {
        ret = io_uring_wait_cqe(ring, &cqe);
        if (ret < 0)
        {
            return -errno;
        }

        if (cqe->res < 0)
        {
            return -cqe->res;
        }

        io_uring_cqe_seen(ring, cqe);
        (*submit_count)--;
    }

    return 0;
}

int copy(int in, int out)
{
    struct io_uring ring;
    if (io_uring_queue_init(READ_QUEUE, &ring, 0) < 0)
    {
        return -1;
    }

    char buffer[READ_QUEUE][IO_BLOCK_SIZE];
    off_t read_offset = 0, write_offset = 0;
    int submit_count = 0, inflight_reads = 0;

    int ret = submit_read(&ring, in, buffer, &read_offset, &inflight_reads);
    if (ret < 0)
    {
        goto cleanup;
    }

    while (inflight_reads > 0)
    {
        struct io_uring_cqe* cqe;
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

        inflight_reads--;

        ret = process_write(&ring, cqe, out, &write_offset, &submit_count);
        if (ret < 0)
        {
            goto cleanup;
        }

        if (ret > 0)
        {
            struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
            if (!sqe)
            {
                ret = -ENOMEM;
                goto cleanup;
            }
            io_uring_prep_read(sqe, in, (char*)cqe->user_data, IO_BLOCK_SIZE, read_offset);
            sqe->user_data = cqe->user_data;
            read_offset += IO_BLOCK_SIZE;
            inflight_reads++;

            ret = io_uring_submit(&ring);
            if (ret < 0)
            {
                ret = -errno;
                goto cleanup;
            }
        }
    }

    ret = wait_requests(&ring, &submit_count);

cleanup:
    io_uring_queue_exit(&ring);
    return ret;
}
