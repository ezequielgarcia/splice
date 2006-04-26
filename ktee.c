/*
 * A tee implementation using sys_tee. Stores stdin input in the given file
 * and duplicates that to stdout.
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

static int usage(char *name)
{
	fprintf(stderr, "... | %s: outfile\n", name);
	return 1;
}

int main(int argc, char *argv[])
{
	struct stat sb;
	int fd;

	if (argc < 2)
		return usage(argv[0]);

	if (fstat(STDIN_FILENO, &sb) < 0)
		return error("stat");
	if (!S_ISFIFO(sb.st_mode)) {
		fprintf(stderr, "stdout must be a pipe\n");
		return usage(argv[0]);
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
