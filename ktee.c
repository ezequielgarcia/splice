/*
 * A tee implementation using sys_tee. If only one argument is given,
 * stdin output is stored in that file and sent to stdout as well. If a
 * second argument is given, that must be in the form if host:port - in
 * that case, output is stored in file and sent over the network to the
 * given host at given port.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include "splice.h"

static int do_splice(int infd, int outfd, unsigned int len, char *msg)
{
	while (len) {
		int written = splice(infd, NULL, outfd, NULL, len, 0);

		if (written <= 0)
			return error(msg);

		len -= written;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct stat sb;
	int fd;

	if (argc < 2) {
		fprintf(stderr, "%s: outfile\n", argv[0]);
		return 1;
	}

	if (fstat(STDIN_FILENO, &sb) < 0)
		return error("stat");
	if (!S_ISFIFO(sb.st_mode)) {
		fprintf(stderr, "stdout must be a pipe\n");
		return 1;
	}

	fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return error("open output");

	do {
		int tee_len = tee(STDIN_FILENO, STDOUT_FILENO, INT_MAX, SPLICE_F_NONBLOCK);

		if (tee_len < 0) {
			if (errno == EAGAIN) {
				usleep(1000);
				continue;
			}
			return error("tee");
		} else if (!tee_len)
			break;

		/*
		 * Send output to file, also consumes input pipe.
		 */
		if (do_splice(STDIN_FILENO, fd, tee_len, "splice-file"))
			break;
	} while (1);

	return 0;
}
