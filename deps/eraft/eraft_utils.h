/*********************************************************************************************/
/************************	Created by 钱慧奇 on 18/04/23.	******************************/
/*********	 Copyright © 2016年 xuli. All rights reserved.	******************************/
/*********************************************************************************************/
#ifndef __ERAFT_UTILS_H__
#define __ERAFT_UTILS_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GOOGLE_LOG 0

#if GOOGLE_LOG
  #include "loger.h"
#else
  #define loger(fmt, ...)
// #define loger(fmt, ...) fprintf(stdout, "FILENAME:%s | LINE:%d | FUNCTION:%s | MASSAGE: "  fmt, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif

#define BUILD_BUG_ON(condition) ((void)sizeof(struct { int : -!!(condition); }))

/* 编译器版本 */
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

/* gcc version. for example : v4.1.2 is 40102, v3.4.6 is 30406 */
/* 逻辑跳转优化*/
#if GCC_VERSION
/* 条件大多数为真，与if配合使用，直接执行if中语句 */
  #define likely(x)     __builtin_expect(!!(x), 1)
/* 条件大多数为假，与if配合使用，直接执行else中语句 */
  #define unlikely(x)   __builtin_expect(!!(x), 0)
#else
  #define likely(x)     (!!(x))
  #define unlikely(x)   (!!(x))
#endif

#undef  New
#define New(ptr)					     \
	((ptr) = (__typeof__(ptr))calloc(1, sizeof(*(ptr))), \
	(likely((ptr)) ? (ptr) : (errno = ENOMEM, NULL)))

#undef  NewArray
#define NewArray(ptr, size)					\
	((ptr) = (__typeof__(ptr))calloc(size, sizeof(*(ptr))),	\
	(likely((ptr)) ? (ptr) : (errno = ENOMEM, NULL)))

#undef  Free
#define Free(ptr)				     \
	do {					     \
		if (likely((ptr))) {		     \
			free((void *)(ptr));	     \
			(ptr) = (__typeof__(ptr)) 0; \
		}				     \
	} while (0)

#if (GCC_VERSION >= 40100)

/* 内存访问栅 */
  #define barrier()             (__sync_synchronize())
/* 原子获取 */
// #define ATOMIC_GET(ptr)                 (barrier(), ( __typeof__ (*( ptr ))) ( (ATOMIC_GET)((AO_T*)( ptr ))))
  #define ATOMIC_GET(ptr)       ({ __typeof__(*(ptr)) volatile *_val = (ptr); barrier(); (*_val); })
// #define ATOMIC_GET(ptr)                 (barrier(), ( __typeof__ (*( ptr ))) ( *( ptr )))

/* 原子设置 */
  #define ATOMIC_SET(ptr, value)        ((void)__sync_lock_test_and_set((ptr), (value)))

/* 原子交换，如果被设置，则返回旧值，否则返回设置值 */
  #define ATOMIC_SWAP(ptr, value)       ((__typeof__(*(ptr)))__sync_lock_test_and_set((ptr), (value)))

/* 原子比较交换，如果当前值等于旧值，则新值被设置，返回旧值，否则返回新值*/
  #define ATOMIC_CAS(ptr, comp, value)  ((__typeof__(*(ptr)))__sync_val_compare_and_swap((ptr), (comp), (value)))

/* 原子比较交换，如果当前值等于旧指，则新值被设置，返回真值，否则返回假 */
  #define ATOMIC_CASB(ptr, comp, value) (__sync_bool_compare_and_swap((ptr), (comp), (value)) != 0 ? true : false)

/* 原子清零 */
  #define ATOMIC_CLEAR(ptr)             ((void)__sync_lock_release((ptr)))

/* maths/bitop of ptr by value, and return the ptr's new value */
  #define ATOMIC_ADD_F(ptr, value)      ((__typeof__(*(ptr)))__sync_add_and_fetch((ptr), (value)))
  #define ATOMIC_SUB_F(ptr, value)      ((__typeof__(*(ptr)))__sync_sub_and_fetch((ptr), (value)))
  #define ATOMIC_OR_F(ptr, value)       ((__typeof__(*(ptr)))__sync_or_and_fetch((ptr), (value)))
  #define ATOMIC_AND_F(ptr, value)      ((__typeof__(*(ptr)))__sync_and_and_fetch((ptr), (value)))
  #define ATOMIC_XOR_F(ptr, value)      ((__typeof__(*(ptr)))__sync_xor_and_fetch((ptr), (value)))

  #define ATOMIC_F_ADD(ptr, value)      ((__typeof__(*(ptr)))__sync_fetch_and_add((ptr), (value)))
  #define ATOMIC_F_SUB(ptr, value)      ((__typeof__(*(ptr)))__sync_fetch_and_sub((ptr), (value)))
  #define ATOMIC_F_OR(ptr, value)       ((__typeof__(*(ptr)))__sync_fetch_and_or((ptr), (value)))
  #define ATOMIC_F_AND(ptr, value)      ((__typeof__(*(ptr)))__sync_fetch_and_and((ptr), (value)))
  #define ATOMIC_F_XOR(ptr, value)      ((__typeof__(*(ptr)))__sync_fetch_and_xor((ptr), (value)))

#else
printf("%d", GCC_VERSION);

  #error "can not supported atomic operate by gcc(v4.0.1+) buildin function."
#endif	/* if (GCC_VERSION >= 40100) */

#define ATOMIC_INC(ptr)         ((void)ATOMIC_ADD_F((ptr), 1))
#define ATOMIC_DEC(ptr)         ((void)ATOMIC_SUB_F((ptr), 1))
#define ATOMIC_ADD(ptr, val)    ((void)ATOMIC_ADD_F((ptr), (val)))
#define ATOMIC_SUB(ptr, val)    ((void)ATOMIC_SUB_F((ptr), (val)))

#define MIN(a, b)               ((a) < (b) ? (a) : (b))
#define MAX(a, b)               ((a) > (b) ? (a) : (b))

#define ARRAY_SIZE(a)           ((int)(sizeof(a) / sizeof((a)[0])))

/*
 * 返回以osize / bsize 的最大倍数
 */
#undef  CEIL_TIMES
#define CEIL_TIMES(osize, bsize) \
	(((osize) + (bsize) - 1) / (bsize))

/*
 * 返回以bsize为单位对齐的osize
 */
#undef  ADJUST_SIZE
#define ADJUST_SIZE(osize, bsize) \
	(CEIL_TIMES((osize), (bsize)) * (bsize))

#ifdef __cplusplus
}
#endif
#endif	/* ifndef __ERAFT_UTILS_H__ */

