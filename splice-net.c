/*
 * Splice stdin to net
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

#include "splice.h"

static struct timeval start_time;
static unsigned long long kb_sent;
static unsigned long iters;

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

void show_rate(int sig)
{
	unsigned long msecs = mtime_since_now(&start_time);

	printf("Throughput: %LuMiB/sec (%Lu MiB in %lu msecs)\n", kb_sent / msecs, kb_sent, msecs);
	printf("avg put: %Lu\n", kb_sent / iters);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	unsigned short port;
	int fd, pfd[2], ffd, ret;
	unsigned long long b_sent = 0;
	int bla = 1;

	if (argc < 4) {
		printf("%s: file target port\n", argv[0]);
		return 1;
	}

	port = atoi(argv[3]);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (inet_aton(argv[2], &addr.sin_addr) != 1) {
		struct hostent *hent = gethostbyname(argv[2]);

		if (!hent) {
			perror("gethostbyname");
			return 1;
		}

		memcpy(&addr.sin_addr, hent->h_addr, 4);
	}

	printf("Connecting to %s/%d\n", argv[2], port);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return 1;
	}

	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("connect");
		return 1;
	}

	if (pipe(pfd) < 0) {
		perror("pipe");
		return 1;
	}

	ffd = open(argv[1], O_RDWR);
	if (ffd < 0) {
		perror("open input");
		return 1;
	}

	signal(SIGINT, show_rate);
	gettimeofday(&start_time, NULL);

	do {
		if (kb_sent >= 128*1024) {
			b_sent += 40000;
			ret = ftruncate(ffd, b_sent);
			printf("trunc file a little %d\n", ret);
			bla = 0;
		}
			
		ret = splice(ffd, NULL, pfd[1], NULL, SPLICE_SIZE, 0x02);

		if (!bla)
			printf("spliced %d\n", ret);

		if (ret < 0) {
			perror("splice");
			break;
		} else if (!ret) {
			break;
		}
		b_sent += ret;
		kb_sent += ret >> 10;
		iters++;
		while (ret > 0) {
			int flags = 0;
			int written = splice(pfd[0], NULL, fd, NULL, ret, flags);
			if (written < 0) {
				perror("splice-out");
				break;
			} else if (!written)
				break;
			ret -= written;
		}
	} while (kb_sent < 512 * 1024 && bla);

	show_rate(0);
	close(fd);
	return 0;
}
