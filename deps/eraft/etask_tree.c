#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include <sys/eventfd.h>

#include "rbtree_cache.h"
#include "etask_tree.h"

#if GCC_VERSION
  #define likely(x)     __builtin_expect(!!(x), 1)
  #define unlikely(x)   __builtin_expect(!!(x), 0)
#else
  #define likely(x)     (!!(x))
  #define unlikely(x)   (!!(x))
#endif

static int eventfd_xrecv(int efd, eventfd_t *value)
{
	int ret;

	do {
		ret = eventfd_read(efd, value);
	} while (unlikely(ret < 0) && errno == EINTR);

	if ((ret != 0) && unlikely(errno != EAGAIN)) {
		// x_printf(E, "eventfd_read() failed");
		abort();
	}

	return ret;
}

static int eventfd_xsend(int efd, eventfd_t value)
{
	int ret;

	do {
		ret = eventfd_write(efd, value);
	} while (unlikely(ret < 0) && (errno == EINTR || errno == EAGAIN));

	if (unlikely(ret < 0)) {
		// x_printf(E, "eventfd_write() failed");
		abort();
	}

	return ret;
}

/*
 * timeout 毫秒
 */
static int eventfd_xwait(int efds[], int nums, int timeout)
{
	int             max = nums;
	struct pollfd   pfds[max];

	for (int i = 0; i < max; i++) {
		pfds[i].fd = efds[i];
		pfds[i].events = POLLIN;
	}

	do {
		int have = poll(pfds, max, timeout);

		if (have < 0) {
			if ((errno == EINTR) || (errno == EAGAIN)) {
				continue;
			}

			assert(0);
		} else if (have == 0) {
			/*timeout*/
			return 0;
		} else {
			int done = 0;

			/* An event on one of the fds has occurred. */
			for (int i = 0; i < max; i++) {
				int ev = pfds[i].revents;

				if (ev & (POLLERR | POLLHUP | POLLNVAL)) {
					assert(0);
				}

				if (ev & POLLIN) {
					efds[done] = pfds[i].fd;

					done++;

					if (done == have) {
						break;
					}
				}
			}

			return have;
		}
	} while (1);
}

void *etask_tree_make(void)
{
	void    *handle = NULL;
	int     ret = RBTCacheCreate(&handle);

	assert(ret == 0);
	return handle;
}

void etask_tree_free(void *tree)
{
	int ret = RBTCacheDestory(&tree);

	assert(ret == 0);
}

int etask_tree_make_task(void *tree, void *key, size_t klen)
{
	int     efd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
	int     ret = RBTCacheSet(tree, key, klen, &efd, sizeof(efd));

	assert(ret && (efd != -1));
	return efd;
}

int etask_tree_await_task(void *tree, void *key, size_t klen, int efd, int msec)
{
	bool have = eventfd_xwait(&efd, 1, msec) ? true : false;

	if (have) {
		eventfd_t val = 0;
		eventfd_xrecv(efd, &val);
		assert(val);
	}

	int     val = 0;
	int     ret = RBTCacheDel(tree, key, klen, &val, sizeof(val));
	assert(ret);
	assert(val == efd);
	close(efd);
	return have ? 0 : -1;
}

static bool _find_lfcb(const void *key, size_t klen, void *val, size_t vlen, size_t idx, void *usr)
{
	int efd = 0;

	assert(vlen == sizeof(efd));
	memcpy(&efd, val, sizeof(efd));

	eventfd_xsend(efd, 1);
	return true;
}

void etask_tree_awake_task(void *tree, void *key, size_t klen)
{
	RBTCacheVisit(tree, key, klen, NULL, 0, _find_lfcb, NULL);
}

