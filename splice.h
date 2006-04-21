#ifndef SPLICE_H
#define SPLICE_H

#if defined(__i386__)
#define __NR_splice	313
#define __NR_tee	315
#define __NR_vmsplice	316
#elif defined(__x86_64__)
#define __NR_splice	275
#define __NR_tee	276
#define __NR_vmsplice	278
#elif defined(__powerpc__) || defined(__powerpc64__)
#define __NR_splice	283
#define __NR_tee	284
#define __NR_vmsplice	285
#elif defined(__ia64__)
#define __NR_splice	1297
#define __NR_tee	1301
#define __NR_vmsplice	1301
#else
#error unsupported arch
#endif

#ifndef F_SETPSZ
#define	F_SETPSZ	15
#define F_GETPSZ	16
#endif

#define SPLICE_F_MOVE	(0x01)	/* move pages instead of copying */
#define SPLICE_F_NONBLOCK (0x02) /* don't block on the pipe splicing (but */
				 /* we may still block on the fd we splice */
				 /* from/to, of course */
#define SPLICE_F_MORE	(0x04)	/* expect more data */

static inline int splice(int fdin, loff_t *off_in, int fdout, loff_t *off_out,
			 size_t len, unsigned long flags)
{
	return syscall(__NR_splice, fdin, off_in, fdout, off_out, len, flags);

}

static inline int tee(int fdin, int fdout, size_t len, unsigned int flags)
{
	return syscall(__NR_tee, fdin, fdout, len, flags);
}

static inline int vmsplice(int fd, void *buffer, size_t len, unsigned int flags)
{
	return syscall(__NR_vmsplice, fd, buffer, len, flags);
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

#endif
