/*
 * Splice from network to stdout
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/poll.h>

#include "splice.h"

static int usage(char *name)
{
	fprintf(stderr, "%s: port\n", name);
	return 1;
}

static int splice_from_net(int fd)
{
	while (1) {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};
		int ret;

		ret = poll(&pfd, 1, -1);
		if (ret < 0)
			return error("poll");
		else if (!ret)
			continue;

		if (!(pfd.revents & POLLIN))
			continue;

		ret = splice(fd, NULL, STDOUT_FILENO, NULL, SPLICE_SIZE, 0);

		if (ret < 0)
			return error("splice");
		else if (!ret)
			break;
	}

	return 0;
}

static int get_connect(int fd, struct sockaddr_in *addr)
{
	socklen_t socklen = sizeof(*addr);
	int ret, connfd;

	do {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};

		ret = poll(&pfd, 1, -1);
		if (ret < 0)
			return error("poll");
		else if (!ret)
			continue;

		connfd = accept(fd, (struct sockaddr *) addr, &socklen);
		if (connfd < 0)
			return error("accept");
		break;
	} while (1);
			
	return connfd;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	unsigned short port;
	int connfd, opt, fd;

	if (argc < 2)
		return usage(argv[0]);

	if (check_output_pipe())
		return usage(argv[0]);

	port = atoi(argv[1]);

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0)
		return error("socket");

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		return error("setsockopt");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		return error("bind");
	if (listen(fd, 1) < 0)
		return error("listen");

	connfd = get_connect(fd, &addr);
	if (connfd < 0)
		return connfd;

	return splice_from_net(connfd);
}
