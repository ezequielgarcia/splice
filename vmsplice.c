/*
 * Use vmsplice to fill some user memory into a pipe. vmsplice writes
 * to stdout, so that must be a pipe.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <getopt.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "splice.h"

#define ALIGN_BUF

#ifdef ALIGN_BUF
#define ALIGN_MASK	(65535)	/* 64k-1, should just be PAGE_SIZE - 1 */
#define ALIGN(buf)	(void *) (((unsigned long) (buf) + ALIGN_MASK) & ~ALIGN_MASK)
#else
#define ALIGN_MASK	(0)
#define ALIGN(buf)	(buf)
#endif

static int do_clear;

int do_vmsplice(int fd, void *buffer, int len)
{
	struct pollfd pfd = { .fd = fd, .events = POLLOUT, };
	int written;

	while (len) {
		/*
		 * in a real app you'd be more clever with poll of course,
		 * here we are basically just blocking on output room and
		 * not using the free time for anything interesting.
		 */
		if (poll(&pfd, 1, -1) < 0)
			return error("poll");

		written = vmsplice(fd, buffer, min(SPLICE_SIZE, len), 0);

		if (written <= 0)
			return error("vmsplice");

		len -= written;
		buffer += written;
	}

	return 0;
}

static int usage(char *name)
{
	fprintf(stderr, "%s: [-c]\n", name);
	return 1;
}

static int parse_options(int argc, char *argv[])
{
	int c, index = 1;

	while ((c = getopt(argc, argv, "m")) != -1) {
		switch (c) {
		case 'c':
			do_clear = 1;
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
	unsigned char *buffer;
	struct stat sb;
	long page_size;
	int i, ret;

	if (parse_options(argc, argv) < 0)
		return usage(argv[0]);

	if (fstat(STDOUT_FILENO, &sb) < 0)
		return error("stat");
	if (!S_ISFIFO(sb.st_mode)) {
		fprintf(stderr, "stdout must be a pipe\n");
		return 1;
	}

	ret = fcntl(STDOUT_FILENO, F_GETPSZ);
	if (ret < 0)
		return error("F_GETPSZ");

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size < 0)
		return error("_SC_PAGESIZE");

	fprintf(stderr, "Pipe size: %d pages / %ld bytes\n", ret, ret * page_size);

	buffer = ALIGN(malloc(2 * SPLICE_SIZE + ALIGN_MASK));
	for (i = 0; i < 2 * SPLICE_SIZE; i++)
		buffer[i] = (i & 0xff);

	do {
		/*
		 * vmsplice the first half of the buffer into the pipe
		 */
		if (do_vmsplice(STDOUT_FILENO, buffer, SPLICE_SIZE))
			break;

		/*
		 * first half is now in pipe, but we don't quite know when
		 * we can reuse it.
		 */

		/*
		 * vmsplice second half
		 */
		if (do_vmsplice(STDOUT_FILENO, buffer + SPLICE_SIZE, SPLICE_SIZE))
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
		if (do_clear)
			memset(buffer, 0x00, SPLICE_SIZE);
			
	} while (0);

	return 0;
}
