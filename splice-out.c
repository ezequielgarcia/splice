/*
 * Splice stdout to file
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "splice.h"

static int splice_flags;

static int usage(char *name)
{
	fprintf(stderr, "%s: [-m] out_file\n", name);
	return 1;
}

static int parse_options(int argc, char *argv[])
{
	int c, index = 1;

	while ((c = getopt(argc, argv, "m")) != -1) {
		switch (c) {
		case 'm':
			splice_flags = SPLICE_F_MOVE;
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
	int fd, index;

	index = parse_options(argc, argv);
	if (index == -1 || index + 1 > argc)
		return usage(argv[0]);

	fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return error("open");

	do {
		int ret = splice(STDIN_FILENO, NULL, fd, NULL, SPLICE_SIZE, splice_flags);

		if (ret < 0)
			return error("splice");
		else if (!ret)
			break;
	} while (1);

	close(fd);
	return 0;
}
