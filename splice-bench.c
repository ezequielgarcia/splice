#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sched.h>

#include "splice.h"

static int nr_clients = 8;
static int net_port = 8888;
static int client_loops = 10;
static int bind_cpu;
static int write_to_null;
static int same_file;
static int splice_size = SPLICE_SIZE;
static char *filename = "splice-file";

static int nr_cpus;

static int usage(char *name)
{
	fprintf(stderr, "Usage %s [options] [filename]:\n", name);
	fprintf(stderr, "\t[-n] (number of clients]\n");
	fprintf(stderr, "\t[-p] (port number)\n");
	fprintf(stderr, "\t[-l] (number of loops)\n");
	fprintf(stderr, "\t[-z] (write to /dev/null)\n");
	fprintf(stderr, "\t[-s] (use 1 file for all)\n");
	fprintf(stderr, "\t[-a] (set CPU affinity)\n");
	fprintf(stderr, "\t[-b] (splice chunk size)\n");
	return 1;
}

static int parse_options(int argc, char *argv[])
{
	int c, index = 1;

	while ((c = getopt(argc, argv, "n:p:l:azsb:")) != -1) {
		switch (c) {
		case 'n':
			nr_clients = atoi(optarg);
			index++;
			break;
		case 'p':
			net_port = atoi(optarg);
			index++;
			break;
		case 'l':
			client_loops = atoi(optarg);
			index++;
			break;
		case 'a':
			bind_cpu = 1;
			index++;
			break;
		case 'z':
			write_to_null = 1;
			index++;
			break;
		case 's':
			same_file = 1;
			index++;
			break;
		case 'b':
			splice_size = atoi(optarg);
			index++;
			break;
		default:
			return -1;
		}
	}

	return index;
}

int bind_to_cpu(int index)
{
	cpu_set_t cpu_mask;
	pid_t pid;
	int cpu;

	if (!bind_cpu || nr_cpus == 1)
		return 0;

	cpu = index % nr_cpus;

	CPU_ZERO(&cpu_mask);
	CPU_SET((cpu), &cpu_mask);

	pid = getpid();
	if (sched_setaffinity(pid, sizeof(cpu_mask), &cpu_mask) == -1)
		return error("set affinity");

	return 0;
}

int accept_loop(int listen_sk)
{
	unsigned long received;
	struct sockaddr addr;
	unsigned int len = sizeof(addr);
	int sk;

again:
	sk = accept(listen_sk, &addr, &len);
	if (sk < 0)
		return error("accept");

	/* read forever */
	received = 0;
	for (;;) {
		int ret = recv(sk, NULL, 128*1024*1024, MSG_TRUNC);
		if (ret > 0) {
			received += ret;
			continue;
		}
		if (!ret)
			break;
		if (errno == EAGAIN || errno == EINTR)
			continue;
		break;
	}

	close(sk);
	goto again;
}

int server(int offset)
{
	struct sockaddr addr;
	struct sockaddr_in saddr_in;
	unsigned int len;
	int sk;

	bind_to_cpu(offset);
	nice(-20);

	sk = socket(PF_INET, SOCK_STREAM, 0);
	if (sk < 0)
		return error("socket");

	saddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr_in.sin_port = htons(net_port + offset);

	if (bind(sk, (struct sockaddr*)&saddr_in, sizeof(saddr_in)) < 0)
		return error("bind");

	if (listen(sk, 1) < 0)
		return error("listen");

	len = sizeof(addr);
	if (getsockname(sk, &addr, &len) < 0)
		return error("getsockname");

	return accept_loop(sk);
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

int client_loop(int out_fd, int fd, int *pfd, int offset)
{
	struct timeval start;
	unsigned long long size;
	unsigned long msecs;
	struct stat sb;
	int loops = client_loops;
	loff_t off;

	if (fstat(fd, &sb) < 0)
		return error("fstat");

	gettimeofday(&start, NULL);

again:
	size = sb.st_size;
	off = 0;

	do {
		int ret = splice(fd, &off, pfd[1], NULL, min(size, (unsigned long long) splice_size), 0);

		if (ret <= 0)
			return error("splice-in");

		size -= ret;
		while (ret > 0) {
			int flags = size ? SPLICE_F_MORE : 0;
			int written = splice(pfd[0], NULL, out_fd, NULL, ret, flags);

			if (written <= 0)
				return error("splice-out");

			ret -= written;
		}
	} while (size);

	if (--loops)
		goto again;

	size = sb.st_size >> 10;
	size *= client_loops;
	msecs = mtime_since_now(&start);
	fprintf(stdout, "Client%d: %Lu MiB/sec\n", offset, size / (unsigned long long) msecs);
	return 0;
}

static int client_open_net(int offset)
{
	int sk = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in s_to;
	struct hostent *hp;

	hp = gethostbyname("localhost");
	if (!hp)
		return error("gethostbyname");

	bzero((char *) &s_to, sizeof (s_to));
	bcopy((char *) hp->h_addr, (char *) &(s_to.sin_addr), hp->h_length);
	s_to.sin_family = hp->h_addrtype;
	s_to.sin_port = htons(net_port + offset);

	if (connect(sk, (struct sockaddr *)&s_to, sizeof(s_to)) < 0)
		return error("connect");

	return sk;
}

int client(int offset)
{
	int file_fd, out_fd;
	char fname[64];
	int pfd[2];

	bind_to_cpu(offset);
	nice(-20);

	if (!write_to_null)
		out_fd = client_open_net(offset);
	else
		out_fd = open("/dev/null", O_WRONLY);

	if (out_fd < 0)
		return error("socket");

	sprintf(fname, "%s%d", filename, same_file ? 0 : offset);
	file_fd = open(filename, O_RDONLY);
	if (file_fd < 0)
		return error("open");

	if (pipe(pfd) < 0)
		return error("pipe");
	
	return client_loop(out_fd, file_fd, pfd, offset);
}

int main(int argc, char *argv[])
{
	pid_t *spids, *cpids;
	int i, index;

	index = parse_options(argc, argv);
	if (index < 0)
		return usage(argv[0]);

	if (index < argc)
		filename = argv[index];

	spids = malloc(nr_clients * sizeof(pid_t));
	cpids = malloc(nr_clients * sizeof(pid_t));
	memset(spids, 0, nr_clients * sizeof(pid_t));
	memset(cpids, 0, nr_clients * sizeof(pid_t));

	nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (nr_cpus < 0)
		return error("_SC_NPROCESSORS_ONLN");

	fprintf(stdout,"Processors: %d\n", nr_cpus);

	/*
	 * fork servers
	 */
	if (!write_to_null) {
		for (i = 0; i < nr_clients; i++) {
			pid_t pid = fork();

			if (pid)
				spids[i] = pid;
			else {
				server(i);
				spids[i] = 0;
				exit(0);
			}
		}
		sleep(1); /* should have servers started now */
	}

	/*
	 * fork clients
	 */
	for (i = 0; i < nr_clients; i++) {
		pid_t pid = fork();

		if (pid)
			cpids[i] = pid;
		else {
			client(i);
			cpids[i] = 0;
			exit(0);
		}
	}

	/*
	 * wait for clients to exit
	 */
	fprintf(stdout, "Waiting for clients\n");
	for (i = 0; i < nr_clients; i++) {
		if (cpids[i]) {
			waitpid(cpids[i], NULL, 0);
			cpids[i] = 0;
		}
	}

	/*
	 * then kill servers
	 */
	for (i = 0; i < nr_clients; i++) {
		if (spids[i]) {
			kill(spids[i], SIGKILL);
			waitpid(spids[i], NULL, 0);
			spids[i] = 0;
		}
	}

	return 0;
}
