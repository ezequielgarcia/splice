/*
 * Splice argument file to stdout
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "splice.h"

static int usage(char *name)
{
	fprintf(stderr, "%s: infile | ...\n", name);
	return 1;
}

int main(int argc, char *argv[])
{
	struct stat sb;
	int fd;

	if (argc < 2)
		return usage(argv[0]);

	if (check_output_pipe())
		return usage(argv[0]);

	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		return error("open input");

	if (fstat(fd, &sb) < 0)
		return error("stat input");

	do {
		int ret = splice(fd, NULL, STDOUT_FILENO, NULL, sb.st_size, 0);

		if (ret < 0)
			return error("splice");
		else if (!ret)
			break;

		sb.st_size -= ret;
	} while (1);

	close(fd);
	return 0;
}
