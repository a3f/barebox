/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Network filesystem support services.
 *
 * Copyright (C) 2021 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * See:
 *
 *	Documentation/filesystems/netfs_library.rst
 *
 * for a description of the network filesystem interface declared here.
 */

#ifndef _LINUX_NETFS_H
#define _LINUX_NETFS_H

#include <linux/atomic.h>
#include <linux/refcount.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/uio.h>
#include <work.h>

enum netfs_sreq_ref_trace;
typedef struct mempool_s mempool_t;

enum netfs_io_source {
	NETFS_SOURCE_UNKNOWN,
	NETFS_FILL_WITH_ZEROES,
	NETFS_DOWNLOAD_FROM_SERVER,
	NETFS_READ_FROM_CACHE,
	NETFS_INVALID_READ,
	NETFS_UPLOAD_TO_SERVER,
	NETFS_WRITE_TO_CACHE,
	NETFS_INVALID_WRITE,
} __mode(byte);

typedef void (*netfs_io_terminated_t)(void *priv, ssize_t transferred_or_error,
				      bool was_async);

/*
 * Per-inode context.  This wraps the VFS inode.
 */
struct netfs_inode {
	struct inode		inode;		/* The VFS inode */
	struct mutex		wb_lock;	/* Writeback serialisation */
	atomic_t		io_count;	/* Number of outstanding reqs */
	u64			time;		/* Time the inode was allocated */
	unsigned long		flags;
};

/*
 * A netfs group - for instance a ceph snap.  This is marked on dirty pages and
 * pages marked with a group must be flushed before they can be written under
 * the domain of another group.
 */
struct netfs_group {
	refcount_t		ref;
	void (*free)(struct netfs_group *netfs_group);
};

/*
 * Stream of I/O subrequests going to a particular destination, such as the
 * server or the local cache.  This is mainly intended for writing where we may
 * have to write to multiple destinations concurrently.
 */
struct netfs_io_stream {
	/* Submission tracking */
	struct netfs_io_subrequest *construct;	/* Op being constructed */
	size_t			sreq_max_len;	/* Maximum size of a subrequest */
	unsigned int		sreq_max_segs;	/* 0 or max number of segments in an iterator */
	unsigned int		submit_off;	/* Folio offset we're submitting from */
	unsigned int		submit_len;	/* Amount of data left to submit */
	unsigned int		submit_extendable_to; /* Amount I/O can be rounded up to */
	void (*prepare_write)(struct netfs_io_subrequest *subreq);
	void (*issue_write)(struct netfs_io_subrequest *subreq);
	/* Collection tracking */
	struct list_head	subrequests;	/* Contributory I/O operations */
	struct netfs_io_subrequest *front;	/* Op being collected */
	unsigned long long	collected_to;	/* Position we've collected results to */
	size_t			transferred;	/* The amount transferred from this stream */
	enum netfs_io_source	source;		/* Where to read from/write to */
	unsigned short		error;		/* Aggregate error for the stream */
	unsigned char		stream_nr;	/* Index of stream in parent table */
	bool			avail;		/* T if stream is available */
	bool			active;		/* T if stream is active */
	bool			need_retry;	/* T if this stream needs retrying */
	bool			failed;		/* T if this stream failed */
};

/*
 * Descriptor for a single component subrequest.  Each operation represents an
 * individual read/write from/to a server, a cache, a journal, etc..
 *
 * The buffer iterator is persistent for the life of the subrequest struct and
 * the pages it points to can be relied on to exist for the duration.
 */
struct netfs_io_subrequest {
	struct netfs_io_request *rreq;		/* Supervising I/O request */
	struct work_struct	work;
	struct list_head	rreq_link;	/* Link in rreq->subrequests */
	struct iov_iter		io_iter;	/* Iterator for this subrequest */
	unsigned long long	start;		/* Where to start the I/O */
	size_t			len;		/* Size of the I/O */
	size_t			transferred;	/* Amount of data transferred */
	size_t			consumed;	/* Amount of read data consumed */
	size_t			prev_donated;	/* Amount of data donated from previous subreq */
	size_t			next_donated;	/* Amount of data donated from next subreq */
	refcount_t		ref;
	short			error;		/* 0 or error that occurred */
	unsigned short		debug_index;	/* Index in list (for debugging output) */
	unsigned int		nr_segs;	/* Number of segs in io_iter */
	enum netfs_io_source	source;		/* Where to read from/write to */
	unsigned char		stream_nr;	/* I/O stream this belongs to */
	unsigned char		curr_folioq_slot; /* Folio currently being read */
	unsigned char		curr_folio_order; /* Order of folio */
	struct folio_queue	*curr_folioq;	/* Queue segment in which current folio resides */
	unsigned long		flags;
#define NETFS_SREQ_COPY_TO_CACHE	0	/* Set if should copy the data to the cache */
#define NETFS_SREQ_CLEAR_TAIL		1	/* Set if the rest of the read should be cleared */
#define NETFS_SREQ_SEEK_DATA_READ	3	/* Set if ->read() should SEEK_DATA first */
#define NETFS_SREQ_NO_PROGRESS		4	/* Set if we didn't manage to read any data */
#define NETFS_SREQ_ONDEMAND		5	/* Set if it's from on-demand read mode */
#define NETFS_SREQ_BOUNDARY		6	/* Set if ends on hard boundary (eg. ceph object) */
#define NETFS_SREQ_HIT_EOF		7	/* Set if short due to EOF */
#define NETFS_SREQ_IN_PROGRESS		8	/* Unlocked when the subrequest completes */
#define NETFS_SREQ_NEED_RETRY		9	/* Set if the filesystem requests a retry */
#define NETFS_SREQ_RETRYING		10	/* Set if we're retrying */
#define NETFS_SREQ_FAILED		11	/* Set if the subreq failed unretryably */
};

enum netfs_io_origin {
	NETFS_READAHEAD,		/* This read was triggered by readahead */
	NETFS_READPAGE,			/* This read is a synchronous read */
	NETFS_READ_GAPS,		/* This read is a synchronous read to fill gaps */
	NETFS_READ_FOR_WRITE,		/* This read is to prepare a write */
	NETFS_DIO_READ,			/* This is a direct I/O read */
	NETFS_WRITEBACK,		/* This write was triggered by writepages */
	NETFS_WRITETHROUGH,		/* This write was made by netfs_perform_write() */
	NETFS_UNBUFFERED_WRITE,		/* This is an unbuffered write */
	NETFS_DIO_WRITE,		/* This is a direct I/O write */
	NETFS_PGPRIV2_COPY_TO_CACHE,	/* [DEPRECATED] This is writing read data to the cache */
	nr__netfs_io_origin
} __mode(byte);

/*
 * Descriptor for an I/O helper request.  This is used to make multiple I/O
 * operations to a variety of data stores and then stitch the result together.
 */
struct netfs_io_request {
	struct work_struct	work;
	struct inode		*inode;		/* The file being accessed */
	struct address_space	*mapping;	/* The mapping being accessed */
	struct kiocb		*iocb;		/* AIO completion vector */
	struct readahead_control *ractl;	/* Readahead descriptor */
	struct list_head	proc_link;	/* Link in netfs_iorequests */
	struct list_head	subrequests;	/* Contributory I/O operations */
	struct netfs_io_stream	io_streams[2];	/* Streams of parallel I/O operations */
#define NR_IO_STREAMS 2 //wreq->nr_io_streams
	struct netfs_group	*group;		/* Writeback group being written back */
	struct folio_queue	*buffer;	/* Head of I/O buffer */
	struct folio_queue	*buffer_tail;	/* Tail of I/O buffer */
	struct iov_iter		iter;		/* Unencrypted-side iterator */
	struct iov_iter		io_iter;	/* I/O (Encrypted-side) iterator */
	void			*netfs_priv;	/* Private data for the netfs */
	void			*netfs_priv2;	/* Private data for the netfs */
	struct bio_vec		*direct_bv;	/* DIO buffer list (when handling iovec-iter) */
	unsigned int		direct_bv_count; /* Number of elements in direct_bv[] */
	unsigned int		debug_id;
	unsigned int		rsize;		/* Maximum read size (0 for none) */
	unsigned int		wsize;		/* Maximum write size (0 for none) */
	atomic_t		subreq_counter;	/* Next subreq->debug_index */
	unsigned int		nr_group_rel;	/* Number of refs to release on ->group */
	spinlock_t		lock;		/* Lock for queuing subreqs */
	atomic_t		nr_outstanding;	/* Number of ops in progress */
	unsigned long long	submitted;	/* Amount submitted for I/O so far */
	unsigned long long	len;		/* Length of the request */
	size_t			transferred;	/* Amount to be indicated as transferred */
	long			error;		/* 0 or error that occurred */
	enum netfs_io_origin	origin;		/* Origin of the request */
	bool			direct_bv_unpin; /* T if direct_bv[] must be unpinned */
	u8			buffer_head_slot; /* First slot in ->buffer */
	u8			buffer_tail_slot; /* Next slot in ->buffer_tail */
	unsigned long long	i_size;		/* Size of the file */
	unsigned long long	start;		/* Start position */
	atomic64_t		issued_to;	/* Write issuer folio cursor */
	unsigned long long	collected_to;	/* Point we've collected to */
	unsigned long long	cleaned_to;	/* Position we've cleaned folios to */
	pgoff_t			no_unlock_folio; /* Don't unlock this folio after read */
	size_t			prev_donated;	/* Fallback for subreq->prev_donated */
	refcount_t		ref;
	unsigned long		flags;
#define NETFS_RREQ_COPY_TO_CACHE	1	/* Need to write to the cache */
#define NETFS_RREQ_NO_UNLOCK_FOLIO	2	/* Don't unlock no_unlock_folio on completion */
#define NETFS_RREQ_DONT_UNLOCK_FOLIOS	3	/* Don't unlock the folios on completion */
#define NETFS_RREQ_FAILED		4	/* The request failed */
#define NETFS_RREQ_IN_PROGRESS		5	/* Unlocked when the request completes */
#define NETFS_RREQ_UPLOAD_TO_SERVER	8	/* Need to write to the server */
#define NETFS_RREQ_NONBLOCK		9	/* Don't block if possible (O_NONBLOCK) */
#define NETFS_RREQ_BLOCKED		10	/* We blocked */
#define NETFS_RREQ_PAUSE		11	/* Pause subrequest generation */
#define NETFS_RREQ_USE_IO_ITER		12	/* Use ->io_iter rather than ->i_pages */
#define NETFS_RREQ_ALL_QUEUED		13	/* All subreqs are now queued */
#define NETFS_RREQ_NEED_RETRY		14	/* Need to try retrying */
#define NETFS_RREQ_USE_PGPRIV2		31	/* [DEPRECATED] Use PG_private_2 to mark
						 * write to cache on read */
	const struct netfs_request_ops *netfs_ops;
	void (*cleanup)(struct netfs_io_request *req);
};

/*
 * Operations the network filesystem can/must provide to the helpers.
 */
struct netfs_request_ops {
	mempool_t *request_pool;
	mempool_t *subrequest_pool;
	int (*init_request)(struct netfs_io_request *rreq, struct file *file);
	void (*free_request)(struct netfs_io_request *rreq);
	void (*free_subrequest)(struct netfs_io_subrequest *rreq);

	/* Read request handling */
	void (*expand_readahead)(struct netfs_io_request *rreq);
	int (*prepare_read)(struct netfs_io_subrequest *subreq);
	void (*issue_read)(struct netfs_io_subrequest *subreq);
	bool (*is_still_valid)(struct netfs_io_request *rreq);
	void (*done)(struct netfs_io_request *rreq);

	/* Modification handling */
	void (*update_i_size)(struct inode *inode, loff_t i_size);
	void (*post_modify)(struct inode *inode);

	/* Write request handling */
	void (*begin_writeback)(struct netfs_io_request *wreq);
	void (*prepare_write)(struct netfs_io_subrequest *subreq);
	void (*issue_write)(struct netfs_io_subrequest *subreq);
	void (*retry_request)(struct netfs_io_request *wreq, struct netfs_io_stream *stream);
	void (*invalidate_cache)(struct netfs_io_request *wreq);
};

/*
 * How to handle reading from a hole.
 */
enum netfs_read_from_hole {
	NETFS_READ_HOLE_IGNORE,
	NETFS_READ_HOLE_CLEAR,
	NETFS_READ_HOLE_FAIL,
};

/* High-level read API. */
ssize_t netfs_unbuffered_read_iter_locked(struct kiocb *iocb, struct iov_iter *iter);
ssize_t netfs_unbuffered_read_iter(struct kiocb *iocb, struct iov_iter *iter);
ssize_t netfs_buffered_read_iter(struct kiocb *iocb, struct iov_iter *iter);
ssize_t netfs_file_read_iter(struct kiocb *iocb, struct iov_iter *iter);

/* High-level write API */
ssize_t netfs_unbuffered_write_iter(struct kiocb *iocb, struct iov_iter *from);
ssize_t netfs_unbuffered_write_iter_locked(struct kiocb *iocb, struct iov_iter *iter,
					   struct netfs_group *netfs_group);
ssize_t netfs_file_write_iter(struct kiocb *iocb, struct iov_iter *from);

/* FIXME: REMOVEME Address operations API */
struct readahead_control;
struct folio;
struct writeback_control;
int netfs_unpin_writeback(struct inode *inode, struct writeback_control *wbc);
void netfs_clear_inode_writeback(struct inode *inode, const void *aux);

/**
 * netfs_inode - Get the netfs inode context from the inode
 * @inode: The inode to query
 *
 * Get the netfs lib inode context from the network filesystem's inode.  The
 * context struct is expected to directly follow on from the VFS inode struct.
 */
static inline struct netfs_inode *netfs_inode(struct inode *inode)
{
	return container_of(inode, struct netfs_inode, inode);
}

/**
 * netfs_inode_init - Initialise a netfslib inode context
 * @ctx: The netfs inode to initialise
 *
 * Initialise the netfs library context struct.  This is expected to follow on
 * directly from the VFS inode struct.
 */
static inline void netfs_inode_init(struct netfs_inode *ctx)
{
	ctx->time = get_time_ns();
	ctx->flags = 0;
	atomic_set(&ctx->io_count, 0);
}

#define NETFS_INODE_VALID_TIME		(2 * NSEC_PER_SEC)

static inline void netfs_invalidate_inode_attr(struct netfs_inode *ctx)
{
	/* ensure that we always detect the inode to be stale */
	ctx->time = -NETFS_INODE_VALID_TIME;
}

extern const struct dentry_operations netfs_dentry_operations_2sec;

#endif /* _LINUX_NETFS_H */
