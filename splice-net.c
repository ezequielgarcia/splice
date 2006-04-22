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
#include <sys/stat.h>
#include <errno.h>

#include "splice.h"

static struct timeval start_time;
static unsigned long long kb_sent;

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

	if (!msecs)
		msecs = 1;

	printf("Throughput: %LuMiB/sec (%Lu MiB in %lu msecs)\n", kb_sent / msecs, kb_sent, msecs);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	unsigned short port;
	int fd, ret;
	struct stat sb;

	if (argc < 3) {
		printf("%s: target port\n", argv[0]);
		return 1;
	}

	if (fstat(STDIN_FILENO, &sb) < 0)
		return error("stat");
	if (!S_ISFIFO(sb.st_mode)) {
		fprintf(stderr, "stdin must be a pipe\n");
		return 1;
	}

	port = atoi(argv[2]);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (inet_aton(argv[1], &addr.sin_addr) != 1) {
		struct hostent *hent = gethostbyname(argv[1]);

		if (!hent)
			return error("gethostbyname");

		memcpy(&addr.sin_addr, hent->h_addr, 4);
	}

	printf("Connecting to %s/%d\n", argv[1], port);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return error("socket");

	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		return error("connect");

	signal(SIGINT, show_rate);
	gettimeofday(&start_time, NULL);

	do {
		ret = splice(STDIN_FILENO, NULL, fd, NULL, SPLICE_SIZE, SPLICE_F_NONBLOCK);
		if (ret < 0) {
			if (errno == EAGAIN) {
				usleep(100);
				continue;
			}
			return error("splice");
		} else if (!ret)
			break;

		kb_sent += ret >> 10;
	} while (1);

	show_rate(0);
	close(fd);
	return 0;
}
