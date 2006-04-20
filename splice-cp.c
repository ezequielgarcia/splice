/*
 * Splice cp a file
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "splice.h"

#define BS	SPLICE_SIZE

int main(int argc, char *argv[])
{
	int in_fd, out_fd, pfds[2];
	struct stat sb;

	if (argc < 3) {
		printf("%s: infile outfile\n", argv[0]);
		return 1;
	}

	in_fd = open(argv[1], O_RDONLY);
	if (in_fd < 0)
		return error("open input");

	if (fstat(in_fd, &sb) < 0)
		return error("stat input");

	out_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (out_fd < 0)
		return error("open output");

	if (pipe(pfds) < 0)
		return error("pipe");

	do {
		int this_len = min((off_t) BS, sb.st_size);
		int ret = splice(in_fd, NULL, pfds[1], NULL, this_len, SPLICE_F_NONBLOCK);

		if (ret <= 0)
			return error("splice-in");

		sb.st_size -= ret;
		while (ret > 0) {
			int written = splice(pfds[0], NULL, out_fd, NULL, ret, 0);
			if (written <= 0)
				return error("splice-out");
			ret -= written;
		}
	} while (sb.st_size);

	close(in_fd);
	close(pfds[1]);
	close(out_fd);
	close(pfds[0]);
	return 0;
}
