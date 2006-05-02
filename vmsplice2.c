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

int do_vmsplice(int fd, struct iovec *iov, unsigned long nr_vecs)
{
	struct pollfd pfd = { .fd = fd, .events = POLLOUT, };
	long written;

	while (nr_vecs) {
		/*
		 * in a real app you'd be more clever with poll of course,
		 * here we are basically just blocking on output room and
		 * not using the free time for anything interesting.
		 */
		if (poll(&pfd, 1, -1) < 0)
			return error("poll");

		written = vmsplice(fd, iov, nr_vecs, 0);

		if (written <= 0)
			return error("vmsplice");

		while (written) {
			int this_len = iov->iov_len;

			if (this_len > written)
				this_len = written;

			iov->iov_len -= this_len;
			if (!iov->iov_len) {
				nr_vecs--;
				iov++;
			}

			written -= this_len;
		}
	}

	return 0;
}

static int usage(char *name)
{
	fprintf(stderr, "%s | ...\n", name);
	return 1;
}

#define S1	"header header header header header header header header"
#define S2	"body body body body body body body body body body body"
#define S3	"footer footer footer footer footer footer footer footer"

int main(int argc, char *argv[])
{
	struct iovec vecs[3];
#if 0
	/* Dangerous on-stack usage! */
	char h[] = S1;
	char b[] = S2;
	char f[] = S3;

	vecs[0].iov_base = h;
	vecs[1].iov_base = b;
	vecs[2].iov_base = f;
#else
	vecs[0].iov_base = strdup(S1);
	vecs[1].iov_base = strdup(S2);
	vecs[2].iov_base = strdup(S3);

	vecs[0].iov_len = strlen(vecs[0].iov_base);
	vecs[1].iov_len = strlen(vecs[1].iov_base);
	vecs[2].iov_len = strlen(vecs[2].iov_base);
#endif
		
	if (check_output_pipe())
		return usage(argv[0]);

	return do_vmsplice(STDOUT_FILENO, vecs, 3);
}
