/*********************************************************************************************/
/************************	Created by 许莉 on 16/03/14.	******************************/
/*********	 Copyright © 2016年 xuli. All rights reserved.	******************************/
/*********************************************************************************************/
#ifndef __COMM_CACHE_H__
#define __COMM_CACHE_H__

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BASEBUFFERSIZE	(1 << 23)	/*  CACHE里面buff的基本大小 */
#define INCREASE_SIZE	(1 << 22)

struct comm_cache
{
	bool    init;			/* 结构体是否初始化的标志 */
	size_t     head;			/* 有效数据的开始下标 */
	size_t     tail;			/* 有效数据的结束下标 */
	size_t     size;			/* 有效数据的大小 */
	size_t     capacity;		/* 缓冲区的大小 */
	char    base[BASEBUFFERSIZE];	/* 缓冲的基地址 */
	char    *buffer;		/* 缓冲区的地址 */
};

/* 初始化cache */
void commcache_init(struct comm_cache *commcache);

/* 释放cache */
void commcache_free(struct comm_cache *commcache);

/* 获取cache有效数据长度 */
size_t commcache_size(struct comm_cache *commcache);

/* 从cache里面提取数据 @data:待提取的数据 @size:待提取数据的大小 */
bool commcache_export(struct comm_cache *commcache, char *data, size_t size);

/* 往cache里面添加数据 @data:待添加的数据 @size:待添加数据的大小 */
bool commcache_import(struct comm_cache *commcache, const char *data, size_t size);

/* 往cache里面恢复数据 @data:待恢复的数据 @size:待恢复数据的大小 */
bool commcache_resume(struct comm_cache *commcache, const char *data, size_t size);

/* 扩展cache的内存， @expect:期盼需要内存的大小 */
bool commcache_expect(struct comm_cache *commcache, size_t expect);

/* 恢复cache里面的buffer为基地址 */
void commcache_shrink(struct comm_cache *commcache);

/* 清除cache里面的无效数据 */
void commcache_adjust(struct comm_cache *commcache);

/* 清空cache里面的所有数据 */
void commcache_empty(struct comm_cache *commcache);

#ifdef __cplusplus
}
#endif
#endif	/* ifndef __COMM_CACHE_H__ */

