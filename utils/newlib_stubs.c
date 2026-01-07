#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/unistd.h>
#include <stddef.h>
#include <stdint.h>

#undef errno
extern int errno;

/* Minimal environment variables */
char *__env[1] = {0};
char **environ = __env;

/* Semihosting operation codes */
#define SYS_WRITE 0x05
#define SYS_READ 0x06

/* Symbols defined by the linker script */
extern char __bss_end;	 /* End of .bss, start of heap */
extern char __stack_top; /* End of RAM (top of stack) */

/* Track the current end of the heap */
static char *heap_end = &__bss_end;

/* Helper for ARM semihosting calls */
static inline int __semihost(int cmd, void *arg)
{
	int result;
	__asm__ volatile(
		"mov r0, %1\n"
		"mov r1, %2\n"
		"bkpt 0xAB\n"
		"mov %0, r0\n"
		: "=r"(result)
		: "r"(cmd), "r"(arg)
		: "r0", "r1", "memory");
	return result;
}

/*** System call stubs required by newlib ***/

void _exit(int status)
{
	(void)status;
	while (1)
	{
		/* Infinite loop */
	}
}

int _close(int file)
{
	(void)file;
	return -1;
}

int _execve(char *name, char **argv, char **env)
{
	(void)name;
	(void)argv;
	(void)env;
	errno = ENOMEM;
	return -1;
}

int _fork(void)
{
	errno = EAGAIN;
	return -1;
}

int _fstat(int file, struct stat *st)
{
	(void)file;
	st->st_mode = S_IFCHR;
	return 0;
}

int _getpid(void)
{
	return 1;
}

int _isatty(int file)
{
	switch (file)
	{
	case STDOUT_FILENO:
	case STDERR_FILENO:
	case STDIN_FILENO:
		return 1;
	default:
		errno = EBADF;
		return 0;
	}
}

int _kill(int pid, int sig)
{
	(void)pid;
	(void)sig;
	errno = EINVAL;
	return -1;
}

int _link(char *oldpath, char *newpath)
{
	(void)oldpath;
	(void)newpath;
	errno = EMLINK;
	return -1;
}

int _lseek(int file, int ptr, int dir)
{
	(void)file;
	(void)ptr;
	(void)dir;
	return 0;
}

/*
 * _sbrk()
 * Simple heap implementation for malloc and newlib.
 * Extends heap upwards until stack region is reached.
 */
void *_sbrk(int incr)
{
	char *prev_heap_end = heap_end;
	char *next_heap = heap_end + incr;

	if (next_heap >= &__stack_top)
	{
		errno = ENOMEM;
		return (void *)-1;
	}

	heap_end = next_heap;
	return (void *)prev_heap_end;
}

/*
 * _read()
 * Read data from stdin using semihosting.
 */
int _read(int file, char *ptr, int len)
{
	struct
	{
		int fd;
		char *buf;
		int len;
	} args = {file, ptr, len};

	__semihost(SYS_READ, &args);
	return len;
}

/*
 * _stat()
 * Return a dummy file status.
 */
int _stat(const char *filepath, struct stat *st)
{
	(void)filepath;
	st->st_mode = S_IFCHR;
	return 0;
}

/*
 * _times()
 * Return timing info (not implemented).
 */
clock_t _times(struct tms *buf)
{
	(void)buf;
	return (clock_t)-1;
}

/*
 * _unlink()
 * File deletion not supported.
 */
int _unlink(char *name)
{
	(void)name;
	errno = ENOENT;
	return -1;
}

/*
 * _wait()
 * Wait for a child process (not supported).
 */
int _wait(int *status)
{
	(void)status;
	errno = ECHILD;
	return -1;
}

/*
 * _write()
 * Write to stdout/stderr using semihosting.
 */
int _write(int file, char *ptr, int len)
{
	struct
	{
		int fd;
		const char *buf;
		int len;
	} args = {file, ptr, len};

	__semihost(SYS_WRITE, &args);
	return len;
}
