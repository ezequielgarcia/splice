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

int main(int argc, char *argv[])
{
	struct stat sb;
	int fd;

	if (argc < 2) {
		printf("%s: infile\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (fstat(fd, &sb) < 0) {
		perror("stat");
		return 1;
	}

	do {
		int ret = splice(fd, NULL, STDOUT_FILENO, NULL, sb.st_size, 0);

		if (ret < 0) {
			perror("splice");
			break;
		} else if (!ret)
			break;

		sb.st_size -= ret;
	} while (1);

	close(fd);
	return 0;
}
