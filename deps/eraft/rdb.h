#pragma once

// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/types.h>
#include <limits.h>

#include "rocksdb/c.h"

struct _rocksdb_stuff
{
	rocksdb_t               *db;
	rocksdb_options_t       *options;
	rocksdb_readoptions_t   *roptions;
	rocksdb_writeoptions_t  *woptions;
	rocksdb_writebatch_t    *wbatch;// not safe.
	char                    dbname[PATH_MAX];
};

/*
 * Initial or create a level-db instance.
 */
struct _rocksdb_stuff   *rdb_initialize(const char *name, size_t block_size, size_t wb_size, size_t lru_size, short bloom_size);

/*
 * Close the level-db instance.
 */
void rdb_close(struct _rocksdb_stuff *rdbs);

/*
 * Destroy the level-db.
 */
void rdb_destroy(struct _rocksdb_stuff *rdbs);

/*
 * Get key's value.
 */
char *rdb_get(struct _rocksdb_stuff *rdbs, const char *key, size_t klen, size_t *vlen);

/*
 * Set record (key value).
 */
int rdb_put(struct _rocksdb_stuff *rdbs, const char *key, size_t klen, const char *value, size_t vlen);

/*
 * Push record (key value).
 */
#define rdb_push rdb_put

/*
 * Pull record (key value).
 */
int rdb_pull(struct _rocksdb_stuff *rdbs, const char *key, size_t klen, char *value, size_t vlen);

/*
 * Delete record.
 */
int rdb_delete(struct _rocksdb_stuff *rdbs, const char *key, size_t klen);

/*
 * Batch set record.
 */
int rdb_batch_put(struct _rocksdb_stuff *rdbs, const char *key, size_t klen, const char *value, size_t vlen);

/*
 * Batch del record.
 */
int rdb_batch_delete(struct _rocksdb_stuff *rdbs, const char *key, size_t klen);

/*
 * Commit batch set.
 * With rdb_batch_put to use.
 */
int rdb_batch_commit(struct _rocksdb_stuff *rdbs);

/*
 * Returns if key exists.
 */
int rdb_exists(struct _rocksdb_stuff *rdbs, const char *key, size_t klen);

/*
 * Compact the database.
 */
void rdb_compact(struct _rocksdb_stuff *rdbs);

const char *rdb_version(void);
