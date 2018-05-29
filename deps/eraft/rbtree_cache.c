#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "rbtree.h"
#include "rbtree_cache.h"

#ifndef xzalloc
  #define xzalloc(size) calloc(1, (size))
#endif

#ifndef intcmp
  #define intcmp(x, y)				\
	({					\
		typeof(x)_x = (x);		\
		typeof(y)_y = (y);		\
		(void)(&_x == &_y);		\
		_x < _y ? -1 : _x > _y ? 1 : 0;	\
	})
#endif

typedef struct RBTCacheS
{
	pthread_rwlock_t        rwlock;
	int                     totality;
	struct rb_root          root;
} RBTCacheS;

typedef struct CacheMemberS
{
	struct rb_node  node;
	size_t          klen;
	size_t          vlen;
	char            kvpair[0];
} CacheMemberS;

int RBTCacheCreate(void **rbtCache)
{
	RBTCacheS *cache = (RBTCacheS *)calloc(1, sizeof(RBTCacheS));

	if (NULL == cache) {
		perror("calloc");
		return -1;
	}

	cache->totality = 0;
	INIT_RB_ROOT(&cache->root);

	pthread_rwlock_init(&cache->rwlock, NULL);

	*rbtCache = cache;
	return 0;
}

int RBTCacheDestory(void **rbtCache)
{
	RBTCacheS *cache = (RBTCacheS *)(*rbtCache);

	if (NULL == cache) {
		return 0;
	}

	pthread_rwlock_wrlock(&cache->rwlock);
	/*释放数据*/
	rb_destroy(&cache->root, struct CacheMemberS, node);
	INIT_RB_ROOT(&cache->root);

	pthread_rwlock_unlock(&cache->rwlock);
	pthread_rwlock_destroy(&cache->rwlock);

	cache->totality = 0;
	free(cache);
	*rbtCache = NULL;
	return 0;
}

static int cache_member_cmp(const struct CacheMemberS *a, const struct CacheMemberS *b)
{
	int r = intcmp(a->klen, b->klen);

	if (r == 0) {
		r = memcmp(a->kvpair, b->kvpair, a->klen);
	}

	return r;
}

int RBTCacheSet(void *rbtCache, void *key, size_t klen, void *val, size_t vlen)
{
	return RBTCacheAlter(rbtCache, key, klen, val, vlen, NULL, NULL) ? vlen : 0;
}

int RBTCacheGet(void *rbtCache, void *key, size_t klen, void *val, size_t vlen)
{
	return RBTCacheVisit(rbtCache, key, klen, val, vlen, NULL, NULL);
}

#ifndef MIN
  #define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
int RBTCacheDel(void *rbtCache, void *key, size_t klen, void *val, size_t vlen)
{
	RBTCacheS *cache = (RBTCacheS *)rbtCache;

	CacheMemberS *new = xzalloc(sizeof(*new) + klen);

	rb_init_node(&new->node);
	new->klen = klen;
	new->vlen = 0;
	memcpy(new->kvpair, key, klen);

	pthread_rwlock_wrlock(&cache->rwlock);
	CacheMemberS *old = rb_search(&cache->root, new, node, cache_member_cmp);
	free(new);

	if (NULL == old) {
		/*未找到*/
		pthread_rwlock_unlock(&cache->rwlock);
		return 0;
	} else {
		/*找到该数据*/
		rb_erase(&old->node, &cache->root);
		cache->totality--;
		pthread_rwlock_unlock(&cache->rwlock);

		if (val) {
			int do_size = MIN(old->vlen, vlen);
			memcpy(val, old->kvpair + klen, do_size);
		}

		free(old);
		return 1;
	}
}

bool RBTCacheExist(void *rbtCache, void *key, size_t klen)
{
	RBTCacheS *cache = (RBTCacheS *)rbtCache;

	CacheMemberS *new = xzalloc(sizeof(*new) + klen);

	rb_init_node(&new->node);
	new->klen = klen;
	new->vlen = 0;
	memcpy(new->kvpair, key, klen);

	pthread_rwlock_rdlock(&cache->rwlock);
	CacheMemberS *old = rb_search(&cache->root, new, node, cache_member_cmp);
	free(new);
	pthread_rwlock_unlock(&cache->rwlock);
	return old ? true : false;
}

int RBTCacheVisit(void *rbtCache, void *key, size_t klen, void *val, size_t vlen, TRAVEL_FOR_LOOKUP_FCB lfcb, void *usr)
{
	RBTCacheS *cache = (RBTCacheS *)rbtCache;

	CacheMemberS *new = xzalloc(sizeof(*new) + klen);

	rb_init_node(&new->node);
	new->klen = klen;
	new->vlen = 0;
	memcpy(new->kvpair, key, klen);

	int length = -1;
	pthread_rwlock_rdlock(&cache->rwlock);
	CacheMemberS *old = rb_search(&cache->root, new, node, cache_member_cmp);
	free(new);

	if (old) {
		/*找到该数据*/
		length = old->vlen;

		if (lfcb) {
			lfcb(old->kvpair, old->klen, old->kvpair + old->klen, old->vlen, 0, usr);
		}

		if (val) {
			int do_size = MIN(old->vlen, vlen);
			memcpy(val, old->kvpair + klen, do_size);
		}
	}

	pthread_rwlock_unlock(&cache->rwlock);
	return length;
}

int RBTCacheAlter(void *rbtCache, void *key, size_t klen, void *val, size_t vlen, TRAVEL_FOR_UPDATE_FCB ufcb, void *usr)
{
	RBTCacheS *cache = (RBTCacheS *)rbtCache;

	CacheMemberS *new = xzalloc(sizeof(*new) + klen + vlen);

	rb_init_node(&new->node);
	new->klen = klen;
	new->vlen = vlen;
	memcpy(new->kvpair, key, klen);

	CacheMemberS *old = NULL;
	pthread_rwlock_wrlock(&cache->rwlock);

	if (val) {
		old = rb_insert(&cache->root, new, node, cache_member_cmp);
	} else {
		old = rb_search(&cache->root, new, node, cache_member_cmp);
	}

	bool done = false;

	if (NULL == old) {
		if (val) {
			/*未缓存的数据*/
			memcpy(new->kvpair + klen, val, vlen);
			done = true;

			cache->totality++;
		} else {
			free(new);
		}
	} else {
		/*缓存过的数据*/
#if 0
		free(new);

		if (old->vlen != vlen) {
			CacheMemberS *now = realloc(old, sizeof(*now) + old->klen + vlen);

			if (NULL == now) {
				pthread_rwlock_unlock(&cache->rwlock);
				return 0;
			}

			if (now != old) {
				struct rb_node *parent = rb_parent(&now->node);

				if (parent) {
					if (parent->rb_right == (struct rb_node *)old) {
						parent->rb_right = &now->node;
					}

					if (parent->rb_left == (struct rb_node *)old) {
						parent->rb_left = &now->node;
					}
				} else {
					cache->root.rb_node = &now->node;
				}

				struct rb_node *left = now->node.rb_left;

				if (left) {
					rb_set_parent(left, &now->node);
				}

				struct rb_node *right = now->node.rb_right;

				if (right) {
					rb_set_parent(right, &now->node);
				}
			}

			old = now;
			old->vlen = vlen;
		}
#else
		if (old->vlen == vlen) {
			free(new);
		} else {
			rb_replace_node(&old->node, &new->node, &cache->root);
			free(old);
			old = new;
			old->vlen = vlen;
		}
#endif		/* if 0 */

		if (ufcb) {
			done = ufcb(old->kvpair, old->klen, old->kvpair + old->klen, old->vlen, 0, usr);
		}

		if (!done) {
			if (val) {
				memcpy(old->kvpair + klen, val, vlen);
				done = true;
			}
		}
	}

	pthread_rwlock_unlock(&cache->rwlock);
	return done ? 1 : 0;
}

int RBTCacheTravel(void *rbtCache, TRAVEL_FOR_LOOKUP_FCB lfcb, TRAVEL_FOR_DELETE_FCB dfcb, void *usr)
{
	RBTCacheS *cache = (RBTCacheS *)rbtCache;

	if (dfcb) {
		pthread_rwlock_wrlock(&cache->rwlock);
	} else {
		pthread_rwlock_rdlock(&cache->rwlock);
	}

	CacheMemberS    *entry;
	size_t          idx = 0;
	bool            exec = true;
	rb_for_each_entry(entry, &cache->root, node)
	{
		if (lfcb) {
			exec = lfcb(entry->kvpair, entry->klen, entry->kvpair + entry->klen, entry->vlen, idx, usr);
		}

		if (dfcb) {
			bool discard = dfcb(entry->kvpair, entry->klen, entry->kvpair + entry->klen, entry->vlen, idx, usr);

			if (discard) {
				rb_erase(&entry->node, &cache->root);
				free(entry);
				cache->totality--;
			}
		}

		idx++;

		if (!exec) {
			break;
		}
	}
	pthread_rwlock_unlock(&cache->rwlock);
	return idx;
}

int RBTCacheTravelFrom(void *rbtCache, void *key, size_t klen, size_t numb, TRAVEL_FOR_LOOKUP_FCB lfcb, TRAVEL_FOR_DELETE_FCB dfcb, void *usr)
{
	RBTCacheS *cache = (RBTCacheS *)rbtCache;

	if (dfcb) {
		pthread_rwlock_wrlock(&cache->rwlock);
	} else {
		pthread_rwlock_rdlock(&cache->rwlock);
	}

	CacheMemberS *new = xzalloc(sizeof(*new) + klen);
	rb_init_node(&new->node);
	new->klen = klen;
	new->vlen = 0;
	memcpy(new->kvpair, key, klen);

	struct rb_node  *node = NULL;
	struct rb_node  *next = NULL;
	CacheMemberS    *entry = rb_search(&cache->root, new, node, cache_member_cmp);
	free(new);

	if (entry) {
		node = &entry->node;
		next = rb_next(node);
	} else {
		next = rb_first(&cache->root);
	}

	size_t  done = 0;
	bool    exec = true;
	do {
		node = next;

		if (!node) {
			break;
		}

		next = rb_next(node);

		entry = (CacheMemberS *)node;

		if (lfcb) {
			exec = lfcb(entry->kvpair, entry->klen, entry->kvpair + entry->klen, entry->vlen, done, usr);
		}

		if (dfcb) {
			bool discard = dfcb(entry->kvpair, entry->klen, entry->kvpair + entry->klen, entry->vlen, done, usr);

			if (discard) {
				rb_erase(&entry->node, &cache->root);
				free(entry);
				cache->totality--;
			}
		}

		done++;

		if (!exec) {
			break;
		}
	} while (done < numb);
	pthread_rwlock_unlock(&cache->rwlock);
	return done;
}

