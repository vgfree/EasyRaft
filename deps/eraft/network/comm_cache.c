/*********************************************************************************************/
/************************	Created by 许莉 on 16/03/14.	******************************/
/*********	 Copyright © 2016年 xuli. All rights reserved.	******************************/
/*********************************************************************************************/
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "comm_cache.h"

#ifndef likely
  #define likely(x)     __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
  #define unlikely(x)   __builtin_expect(!!(x), 0)
#endif

/*返回以bsize为单位对齐的osize*/
#ifndef ADJUST_SIZE
  #define ADJUST_SIZE(osize, bsize) \
	((((osize) + (bsize) - 1) / (bsize)) * (bsize))
#endif

void commcache_init(struct comm_cache *commcache)
{
	assert(commcache);
	memset(commcache, 0, sizeof(struct comm_cache));
	commcache->capacity = BASEBUFFERSIZE;
	commcache->buffer = commcache->base;
	commcache->init = true;
}

void commcache_free(struct comm_cache *commcache)
{
	if (commcache && commcache->init) {
		if (commcache->buffer != commcache->base) {
			free(commcache->buffer);
		}

		commcache->init = false;
	}
}

size_t commcache_size(struct comm_cache *commcache)
{
	return commcache->size;
}

bool commcache_export(struct comm_cache *commcache, char *data, size_t size)
{
	assert(commcache && commcache->init);
	// if (size < 0) {
	//	size = commcache->size;
	// }

	if (unlikely(size > commcache->size)) {
		return false;
	}

	/*有可能起始地址和目标地址相同*/
	if (data) {
		if (commcache->head + size <= commcache->capacity) {
			memmove(data, &commcache->buffer[commcache->head], size);
		} else {
			size_t  front = commcache->capacity - commcache->head;
			size_t  after = (commcache->head + size) % commcache->capacity;
			memmove(data, &commcache->buffer[commcache->head], front);
			memmove(data + front, &commcache->buffer[0], after);
		}
	}

	commcache->head += size;
	commcache->head %= commcache->capacity;
	commcache->size -= size;

	/*进行缩容*/
	if (commcache->size <= (BASEBUFFERSIZE / 2)) {
		/*越懒越好*/
		commcache_shrink(commcache);
	}

#if 0
	/*进行调整*/
	if (commcache->head >= (BASEBUFFERSIZE / 2)) {
		/*越懒越好*/
		commcache_adjust(commcache);
	}
#endif
	return true;
}

bool commcache_import(struct comm_cache *commcache, const char *data, size_t size)
{
	assert(commcache && commcache->init);
	// if (size < 0) {
	//	size = strlen(data);
	// }

	if (unlikely(!commcache_expect(commcache, size))) {
		return false;
	}

	/*有可能起始地址和目标地址相同*/
	if (commcache->tail + size <= commcache->capacity) {
		memmove(&commcache->buffer[commcache->tail], data, size);
	} else {
		size_t  front = commcache->capacity - commcache->tail;
		size_t  after = (commcache->tail + size) % commcache->capacity;
		memmove(&commcache->buffer[commcache->tail], data, front);
		memmove(&commcache->buffer[0], data + front, after);
	}

	commcache->tail += size;
	commcache->tail %= commcache->capacity;
	commcache->size += size;
	return true;
}

bool commcache_resume(struct comm_cache *commcache, const char *data, size_t size)
{
	assert(commcache && commcache->init);
	// if (size < 0) {
	//	size = strlen(data);
	// }

	if (unlikely(!commcache_expect(commcache, size))) {
		return false;
	}

	/*有可能起始地址和目标地址相同*/
	if (commcache->head >= size) {
		memmove(&commcache->buffer[commcache->head - size], data, size);
		commcache->head -= size;
	} else {
		size_t  front = size - commcache->head;
		size_t  after = commcache->head;
		memmove(&commcache->buffer[commcache->capacity - front], data, front);
		memmove(&commcache->buffer[0], data + front, after);
		commcache->head = commcache->capacity - front;
	}

	commcache->size += size;
	return true;
}

void commcache_adjust(struct comm_cache *commcache)
{
	assert(commcache && commcache->init);

	if (likely(commcache->head != 0)) {
		assert(commcache->size >= 0);

		if (likely(commcache->size > 0)) {
			size_t size = commcache->size;

			if (commcache->head + size <= commcache->capacity) {
				memmove(&commcache->buffer[0], &commcache->buffer[commcache->head], size);
			} else {
				size_t  front = commcache->capacity - commcache->head;
				size_t  after = (commcache->head + size) % commcache->capacity;

				if (front <= after) {
					char *data = malloc(front);
					assert(data);
					memcpy(data, &commcache->buffer[commcache->head], front);
					memmove(&commcache->buffer[front], &commcache->buffer[0], after);
					memcpy(&commcache->buffer[0], data, front);
					free(data);
				} else {
					char *data = malloc(after);
					assert(data);
					memcpy(data, &commcache->buffer[0], after);
					memmove(&commcache->buffer[0], &commcache->buffer[commcache->head], front);
					memcpy(&commcache->buffer[front], data, after);
					free(data);
				}
			}
		}

		commcache->tail = commcache->size % commcache->capacity;
		commcache->head = 0;
	}
}

static bool commcache_expand(struct comm_cache *commcache, size_t expand)
{
	assert(commcache && commcache->init && (expand > 0));

	size_t capacity = commcache->capacity + ADJUST_SIZE(expand, INCREASE_SIZE);

	char *buffer = calloc(capacity, sizeof(char));

	if (buffer) {
		size_t size = commcache->size;

		if (commcache->head + size <= commcache->capacity) {
			memcpy(buffer, &commcache->buffer[commcache->head], size);
		} else {
			size_t  front = commcache->capacity - commcache->head;
			size_t  after = (commcache->head + size) % commcache->capacity;
			memcpy(buffer, &commcache->buffer[commcache->head], front);
			memcpy(buffer + front, &commcache->buffer[0], after);
		}

		if (commcache->buffer != commcache->base) {
			free(commcache->buffer);
		}

		commcache->buffer = buffer;
		commcache->capacity = capacity;

		commcache->tail = commcache->size % commcache->capacity;
		commcache->head = 0;

		return true;
	} else {
		abort();
		return false;
	}
}

bool commcache_expect(struct comm_cache *commcache, size_t expect)
{
	assert(commcache && commcache->init && (expect >= 0));
#if 0
	/*进行调整*/
	if (commcache->head >= (BASEBUFFERSIZE / 2)) {
		/*越懒越好*/
		commcache_adjust(commcache);
	}
#endif
	size_t remain = commcache->capacity - commcache->size;

	if (remain >= expect) {
		return true;
	}

	/*进行扩容*/
	size_t  expand = expect - remain;
	bool    ok = commcache_expand(commcache, expand);
	assert(ok);
	return ok;
}

void commcache_shrink(struct comm_cache *commcache)
{
	assert(commcache && commcache->init);

	if (commcache->buffer != commcache->base) {
		if (commcache->size < BASEBUFFERSIZE) {
			if (commcache->size > 0) {
				size_t size = commcache->size;

				if (commcache->head + size <= commcache->capacity) {
					memcpy(commcache->base, &commcache->buffer[commcache->head], size);
				} else {
					size_t  front = commcache->capacity - commcache->head;
					size_t  after = (commcache->head + size) % commcache->capacity;
					memcpy(commcache->base, &commcache->buffer[commcache->head], front);
					memcpy(commcache->base + front, &commcache->buffer[0], after);
				}
			}

			free(commcache->buffer);
			commcache->buffer = commcache->base;
			commcache->capacity = BASEBUFFERSIZE;
			commcache->tail = commcache->size % commcache->capacity;
			commcache->head = 0;
			// printf("restore cache capacity:%d\n", commcache->capacity);
			return;
		}
	}
}

void commcache_empty(struct comm_cache *commcache)
{
	assert(commcache && commcache->init);

	if (commcache->buffer != commcache->base) {
		free(commcache->buffer);
		commcache->buffer = commcache->base;
		commcache->capacity = BASEBUFFERSIZE;
	}

	commcache->tail = 0;
	commcache->head = 0;
	commcache->size = 0;
	// printf("restore cache capacity:%d\n", commcache->capacity);
}

