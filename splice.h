#ifndef SPLICE_H
#define SPLICE_H

#include <errno.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <linux/unistd.h>

#if defined(__i386__)
#define __NR_sys_splice		313
#define __NR_sys_tee		315
#define __NR_sys_vmsplice	316
#elif defined(__x86_64__)
#define __NR_sys_splice		275
#define __NR_sys_tee		276
#define __NR_sys_vmsplice	278
#elif defined(__powerpc__) || defined(__powerpc64__)
#define __NR_sys_splice		283
#define __NR_sys_tee		284
#define __NR_sys_vmsplice	285
#elif defined(__ia64__)
#define __NR_sys_splice		1297
#define __NR_sys_tee		1301
#define __NR_sys_vmsplice	1302
#else
#error unsupported arch
#endif

#define SPLICE_F_MOVE	(0x01)	/* move pages instead of copying */
#define SPLICE_F_NONBLOCK (0x02) /* don't block on the pipe splicing (but */
				 /* we may still block on the fd we splice */
				 /* from/to, of course */
#define SPLICE_F_MORE	(0x04)	/* expect more data */
#define SPLICE_F_GIFT   (0x08)  /* pages passed in are a gift */

_syscall6(int, sys_splice, int, fdin, loff_t *, off_in, int, fdout, loff_t *, off_out, size_t, len, unsigned int, flags);
_syscall4(int, sys_vmsplice, int, fd, const struct iovec *, iov, unsigned long, nr_segs, unsigned int, flags);
_syscall4(int, sys_tee, int, fdin, int, fdout, size_t, len, unsigned int, flags);

static inline int splice(int fdin, loff_t *off_in, int fdout, loff_t *off_out,
			 size_t len, unsigned long flags)
{
	return sys_splice(fdin, off_in, fdout, off_out, len, flags);
}

static inline int tee(int fdin, int fdout, size_t len, unsigned int flags)
{
	return sys_tee(fdin, fdout, len, flags);
}

static inline int vmsplice(int fd, const struct iovec *iov,
			   unsigned long nr_segs, unsigned int flags)
{
	return sys_vmsplice(fd, iov, nr_segs, flags);
}

#define SPLICE_SIZE	(64*1024)

#define BUG_ON(c) assert(!(c))

#define min(x,y) ({ \
        typeof(x) _x = (x);     \
        typeof(y) _y = (y);     \
        (void) (&_x == &_y);            \
        _x < _y ? _x : _y; })

#define max(x,y) ({ \
        typeof(x) _x = (x);     \
        typeof(y) _y = (y);     \
        (void) (&_x == &_y);            \
        _x > _y ? _x : _y; })

static inline int error(const char *n)
{
	perror(n);
	return -1;
}

static int __check_pipe(int pfd)
{
	struct stat sb;

	if (fstat(pfd, &sb) < 0)
		return error("stat");
	if (!S_ISFIFO(sb.st_mode))
		return 1;

	return 0;
}

static inline int check_input_pipe(void)
{
	if (!__check_pipe(STDIN_FILENO))
		return 0;

	fprintf(stderr, "stdin must be a pipe\n");
	return 1;
}

static inline int check_output_pipe(void)
{
	if (!__check_pipe(STDOUT_FILENO))
		return 0;

	fprintf(stderr, "stdout must be a pipe\n");
	return 1;
}

#endif
