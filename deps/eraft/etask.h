#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__LINUX__) || defined(__linux__)
  #include <sys/eventfd.h>
#else
  #define EFD_SEMAPHORE (1)
  #define EFD_NONBLOCK  (04000)
  #define eventfd_t     uint64_t
static inline int eventfd_write(int fd, eventfd_t value)
{
	return write(fd, &value, sizeof(eventfd_t)) !=
	       sizeof(eventfd_t) ? -1 : 0;
}

static inline int eventfd_read(int fd, eventfd_t *value)
{
	return read(fd, value, sizeof(eventfd_t)) !=
	       sizeof(eventfd_t) ? -1 : 0;
}

static inline int eventfd(unsigned int initval, int flags)
{
	return syscall(__NR_eventfd2, initval, flags);
}

#endif	/* if defined(__LINUX__) || defined(__linux__) */

/*
 * Return 0 on success, or -1 if efd has been made nonblocking and
 * errno is EAGAIN.  If efd has been marked blocking or the eventfd counter is
 * not zero, this function doesn't return error.
 */
int eventfd_xrecv(int efd, eventfd_t *value);

int eventfd_xsend(int efd, eventfd_t value);

int eventfd_xwait(int efds[], int nums, int timeout);

struct etask
{
	int     efd;
	bool    freeable;
};

struct etask    *etask_make(struct etask *etask);

void etask_free(struct etask *etask);

void etask_awake(struct etask *etask);

void etask_sleep(struct etask *etask);

/*msec小于0为无限等待*/
bool etask_twait(struct etask *etask, int msec);

