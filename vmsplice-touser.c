/*
 * Use vmsplice to splice data from a pipe to user space memory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/types.h>

#include "splice.h"

static int do_dump;
static int splice_flags;

int do_vmsplice(int fd, void *buf, int len)
{
	struct pollfd pfd = { .fd = fd, .events = POLLIN, };
	struct iovec iov;
	int written;
	int ret;

	iov.iov_base = buf;
	iov.iov_len = len;
	ret = 0;

	while (len) {
		/*
		 * in a real app you'd be more clever with poll of course,
		 * here we are basically just blocking on output room and
		 * not using the free time for anything interesting.
		 */
		if (poll(&pfd, 1, -1) < 0)
			return error("poll");

		written = svmsplice(fd, &iov, 1, splice_flags);

		if (written < 0)
			return error("vmsplice");
		else if (!written)
			break;

		len -= written;
		ret += written;
		if (len) {
			iov.iov_len -= written;
			iov.iov_base += written;
		}
	}

	return ret;
}

static int usage(char *name)
{
	fprintf(stderr, "| %s [-d(ump)]\n", name);
	return 1;
}

static int parse_options(int argc, char *argv[])
{
	int c, index = 1;

	while ((c = getopt(argc, argv, "d")) != -1) {
		switch (c) {
		case 'd':
			do_dump = 1;
			index++;
			break;
		default:
			return -1;
		}
	}

	return index;
}

static void hexdump(unsigned char *buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		printf("%02x", buf[i]);
	printf("\n");
}

int main(int argc, char *argv[])
{
	unsigned char *buf;
	int ret;

	if (parse_options(argc, argv) < 0)
		return usage(argv[0]);

	if (check_input_pipe())
		return usage(argv[0]);

	buf = malloc(4096);

	memset(buf, 0, 4096);

	ret = do_vmsplice(STDIN_FILENO, buf, 4096);
	if (ret < 0)
		return 1;

	printf("splice %d\n", ret);

	if (do_dump)
		hexdump(buf, ret);

	return 0;
}
