/*
 * Use vmsplice to fill some user memory into a pipe. vmsplice writes
 * to stdout, so that must be a pipe.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <getopt.h>
#include <sys/poll.h>
#include <sys/types.h>

#include "splice.h"

#define ALIGN(buf)	(void *) (((unsigned long) (buf) + align_mask) & ~align_mask)

static int do_clear;
static int align_mask = 65535;

int do_vmsplice(int fd, void *b1, void *b2, int len)
{
	struct pollfd pfd = { .fd = fd, .events = POLLOUT, };
	struct iovec iov[] = {
		{
			.iov_base = b1,
			.iov_len = len / 2,
		},
		{
			.iov_base = b2,
			.iov_len = len / 2,
		},
	};
	int written, idx = 0;

	while (len) {
		/*
		 * in a real app you'd be more clever with poll of course,
		 * here we are basically just blocking on output room and
		 * not using the free time for anything interesting.
		 */
		if (poll(&pfd, 1, -1) < 0)
			return error("poll");

		written = vmsplice(fd, &iov[idx], 2 - idx, 0);

		if (written <= 0)
			return error("vmsplice");

		len -= written;
		if (written >= iov[idx].iov_len) {
			int extra = written - iov[idx].iov_len;

			idx++;
			iov[idx].iov_len -= extra;
			iov[idx].iov_base += extra;
		} else {
			iov[idx].iov_len -= written;
			iov[idx].iov_base += written;
		}
	}

	return 0;
}

static int usage(char *name)
{
	fprintf(stderr, "%s: [-c(lear)] [-u(nalign)] | ...\n", name);
	return 1;
}

static int parse_options(int argc, char *argv[])
{
	int c, index = 1;

	while ((c = getopt(argc, argv, "cu")) != -1) {
		switch (c) {
		case 'c':
			do_clear = 1;
			index++;
			break;
		case 'u':
			align_mask = 0;
			index++;
			break;
		default:
			return -1;
		}
	}

	return index;
}

int main(int argc, char *argv[])
{
	unsigned char *b1, *b2;

	if (parse_options(argc, argv) < 0)
		return usage(argv[0]);

	if (check_output_pipe())
		return usage(argv[0]);

	b1 = ALIGN(malloc(SPLICE_SIZE + align_mask));
	b2 = ALIGN(malloc(SPLICE_SIZE + align_mask));

	memset(b1, 0xaa, SPLICE_SIZE);
	memset(b2, 0xbb, SPLICE_SIZE);

	do {
		int half = SPLICE_SIZE / 2;

		/*
		 * vmsplice the first half of the buffer into the pipe
		 */
		if (do_vmsplice(STDOUT_FILENO, b1, b2, SPLICE_SIZE))
			break;

		/*
		 * first half is now in pipe, but we don't quite know when
		 * we can reuse it.
		 */

		/*
		 * vmsplice second half
		 */
		if (do_vmsplice(STDOUT_FILENO, b1 + half, b2 + half, SPLICE_SIZE))
			break;

		/*
		 * We still don't know when we can reuse the second half of
		 * the buffer, but we do now know that all parts of the first
		 * half have been consumed from the pipe - so we can reuse that.
		 */

		/*
		 * Test option - clear the first half of the buffer, should
		 * be safe now
		 */
		if (do_clear) {
			memset(b1, 0x00, SPLICE_SIZE);
			memset(b2, 0x00, SPLICE_SIZE);
		}
	} while (0);

	return 0;
}
