/*
 * Splice stdout to file
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "splice.h"

int main(int argc, char *argv[])
{
	int fd;

	if (argc < 2) {
		printf("%s: outfile\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	do {
		int ret = splice(STDIN_FILENO, NULL, fd, NULL, SPLICE_SIZE, 0);

		if (ret < 0) {
			perror("splice");
			break;
		} else if (!ret)
			break;
	} while (1);

	close(fd);
	return 0;
}
