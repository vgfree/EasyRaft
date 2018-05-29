#pragma once

#include <stdbool.h>
#include <pthread.h>

/**
 * 创建RBT
 * interface name: RBTCacheCreate
 * interface desc: create RBT cache
 * parameter desc:
 *     rbtCache: output parameter，新建缓存句柄
 * return value:
 *     success, return 0.
 *     failure, return -1.
 */
int RBTCacheCreate(void **rbtCache);

/**
 * 销毁RBT
 * interface name: RBTCacheDestroy
 * interface desc: destroy RBT cache
 * parameter desc:
 *     rbtCache: input parameter, cache handle
 * return value:
 *     success, return 0.
 *     failure, return -1.
 */
int RBTCacheDestory(void **rbtCache);

/**
 * 写操作
 * interface name: RBTCacheSet
 * interface desc: put the data into RBT cache.
 * parameter desc:
 *     rbtCache: input parameter, cache handle.
 *     key: input parameter, data index
 *     val: input parameter, data content
 * return value: return real copy length.
 *     success, return vlen.
 *     failure, return 0.
 */
int RBTCacheSet(void *rbtCache, void *key, size_t klen, void *val, size_t vlen);

/**
 * 读操作
 * interface name: RBTCacheGet
 * interface desc: get data from cache
 * parameter desc:
 *    rbtCache: input parameter, cache handle
 *    key: input parameter, data index.
 * return value: return value length.
 *     success: return >= 0.
 *     failure: return -1.
 */
int RBTCacheGet(void *rbtCache, void *key, size_t klen, void *val, size_t vlen);

/**
 * 删操作
 * interface name: RBTCacheDel
 * interface desc: del data from cache
 * parameter desc:
 *    rbtCache: input parameter, cache handle
 *    key: input parameter, data index.
 * return value: return del key count.
 *     success: return 1.
 *     failure: return 0.
 */
int RBTCacheDel(void *rbtCache, void *key, size_t klen, void *val, size_t vlen);

bool RBTCacheExist(void *rbtCache, void *key, size_t klen);

#ifndef TRAVEL_FOR_UPDATE_FCB
typedef bool (*TRAVEL_FOR_UPDATE_FCB)(const void *key, size_t klen, void *val, size_t vlen, size_t idx, void *usr);
#endif
#ifndef TRAVEL_FOR_DELETE_FCB
typedef bool (*TRAVEL_FOR_DELETE_FCB)(const void *key, size_t klen, void *val, size_t vlen, size_t idx, void *usr);
#endif
#ifndef TRAVEL_FOR_LOOKUP_FCB
typedef bool (*TRAVEL_FOR_LOOKUP_FCB)(const void *key, size_t klen, void *val, size_t vlen, size_t idx, void *usr);
#endif

/**
 * 查操作
 * interface name: RBTCacheVisit
 * interface desc: visit data from cache
 * parameter desc:
 *    rbtCache: input parameter, cache handle
 *    key: input parameter, data index.
 * return value: return visit key count.
 *     success: return 1.
 *     failure: return 0.
 */
int RBTCacheVisit(void *rbtCache, void *key, size_t klen, void *val, size_t vlen, TRAVEL_FOR_LOOKUP_FCB lfcb, void *usr);

/**
 * 改操作
 * interface name: RBTCacheAlter
 * interface desc: alter data from cache
 * parameter desc:
 *    rbtCache: input parameter, cache handle
 *    key: input parameter, data index.
 * return value: return alter key count.
 *     success: return 1.
 *     failure: return 0.
 */
int RBTCacheAlter(void *rbtCache, void *key, size_t klen, void *val, size_t vlen, TRAVEL_FOR_UPDATE_FCB ufcb, void *usr);

/**
 * 遍历操作
 * interface name: RBTCacheTravel
 * interface desc: travel data from cache
 * parameter desc:
 *    rbtCache: input parameter, cache handle
 * return value: return count travel.
 */
int RBTCacheTravel(void *rbtCache, TRAVEL_FOR_LOOKUP_FCB lfcb, TRAVEL_FOR_DELETE_FCB dfcb, void *usr);

/**
 * 遍历操作
 * interface name: RBTCacheTravelFrom
 * interface desc: travel data from cache
 * parameter desc:
 *    rbtCache: input parameter, cache handle
 * return value: return <= numb.
 */
int RBTCacheTravelFrom(void *rbtCache, void *key, size_t klen, size_t numb, TRAVEL_FOR_LOOKUP_FCB lfcb, TRAVEL_FOR_DELETE_FCB dfcb, void *usr);

