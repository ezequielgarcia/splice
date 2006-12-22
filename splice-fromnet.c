/*
 * Splice from network to pipe. Currently splicing from a socket is not
 * supported, so this test case demonstrates how to use read + vmsplice/splice
 * to achieve the desired effect. This still involves a copy, so it's
 * not as fast as real splice from socket would be.
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

#include "splice.h"

#define PSIZE	4096
#define MASK	(PSIZE - 1)

#define ALIGN(buf)	(void *) (((unsigned long) (buf) + MASK) & ~MASK)

static int splice_flags;

static int usage(char *name)
{
	fprintf(stderr, "%s: [-g] port\n", name);
	return 1;
}

static unsigned long mtime_since(struct timeval *s, struct timeval *e)
{
	double sec, usec;

	sec = e->tv_sec - s->tv_sec;
	usec = e->tv_usec - s->tv_usec;
	if (sec > 0 && usec < 0) {
		sec--;
		usec += 1000000;
	}

	sec *= (double) 1000;
	usec /= (double) 1000;

	return sec + usec;
}

static unsigned long mtime_since_now(struct timeval *s)
{
	struct timeval t;

	gettimeofday(&t, NULL);
	return mtime_since(s, &t);
}

static int recv_loop(int fd, char *buf)
{
	unsigned long kb_recv = 0, spent;
	struct timeval s;
	struct iovec iov;
	int idx = 0;

	gettimeofday(&s, NULL);

	do {
		char *ptr = buf + idx * SPLICE_SIZE;
		int ret;

		ret = recv(fd, ptr, SPLICE_SIZE, MSG_WAITALL);
		if (ret < 0) {
			perror("recv");
			return 1;
		} else if (ret != SPLICE_SIZE)
			break;

		iov.iov_base = ptr;
		iov.iov_len = SPLICE_SIZE;
		ret = vmsplice(STDOUT_FILENO, &iov, 1, splice_flags);
		if (ret < 0) {
			perror("vmsplice");
			return 1;
		} else if (ret != SPLICE_SIZE) {
			fprintf(stderr, "bad vmsplice %d\n", ret);
			return 1;
		}
		idx = (idx + 1) & 0x01;
		kb_recv += (SPLICE_SIZE / 1024);
	} while (1);

	spent = mtime_since_now(&s);
	fprintf(stderr, "%lu MiB/sec (%luKiB in %lu msec)\n", kb_recv / spent, kb_recv, spent);

	return 0;
}

static int parse_options(int argc, char *argv[])
{
	int c, index = 1;

	while ((c = getopt(argc, argv, "g")) != -1) {
		switch (c) {
		case 'g':
			splice_flags = SPLICE_F_GIFT;
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
	struct sockaddr_in addr;
	unsigned short port;
	unsigned int len;
	int fd, opt, sk, index;
	char *buf;

	if (check_output_pipe())
		return usage(argv[0]);

	index = parse_options(argc, argv);
	if (index == -1 || index + 1 > argc)
		return usage(argv[0]);

	port = atoi(argv[index]);

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return error("socket");

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		return error("setsockopt");

	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		return error("bind");

	if (listen(fd, 1) < 0)
		return error("listen");

	len = sizeof(addr);
	sk = accept(fd, &addr, &len);
	if (sk < 0)
		return error("accept");

	fprintf(stderr, "Connected\n");

	buf = ALIGN(malloc(2 * SPLICE_SIZE - 1));

	return recv_loop(sk, buf);

	close(fd);
	return 0;
}
