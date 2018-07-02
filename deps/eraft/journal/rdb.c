/*
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdb.h"

/*
 * Open the specified database, if no exists, then create it.
 * Args:
 *      1. name: db name
 *      2. block_size: block size
 *      3. wb_size: write buffer size
 *      4. lru_size: lru cache size
 *      5. bloom_size: bloom key size
 * Return:
 *      _rocksdb_stuff: rocksdb handler.
 */
struct _rocksdb_stuff *rdb_initialize(const char *name, size_t block_size, size_t wb_size, size_t lru_size, short bloom_size)
{
	struct _rocksdb_stuff   *rdbs = NULL;
	rocksdb_cache_t         *cache;
	// rocksdb_filterpolicy_t  *policy;
	char *err = NULL;

	rdbs = malloc(sizeof(struct _rocksdb_stuff));
	memset(rdbs, 0, sizeof(struct _rocksdb_stuff));

	rdbs->options = rocksdb_options_create();

	snprintf(rdbs->dbname, sizeof(rdbs->dbname), "%s", name);

	cache = rocksdb_cache_create_lru(lru_size);
	// policy = rocksdb_filterpolicy_create_bloom(bloom_size);

	// rocksdb_options_set_filter_policy(rdbs->options, policy);
	// rocksdb_options_set_cache(rdbs->options, cache);
	// rocksdb_options_set_block_size(rdbs->options, block_size);
	rocksdb_options_set_write_buffer_size(rdbs->options, wb_size);
#if defined(OPEN_COMPRESSION)
	rocksdb_options_set_compression(rdbs->options, rocksdb_snappy_compression);
#else
	rocksdb_options_set_compression(rdbs->options, rocksdb_no_compression);
#endif
	/* R */
	rdbs->roptions = rocksdb_readoptions_create();
	rocksdb_readoptions_set_verify_checksums(rdbs->roptions, 1);
	rocksdb_readoptions_set_fill_cache(rdbs->roptions, 1);	/* set 1 is faster */

	/* W */
	rdbs->woptions = rocksdb_writeoptions_create();
#define SYNC_PUT
#ifdef SYNC_PUT
	rocksdb_writeoptions_set_sync(rdbs->woptions, 1);
#else
	rocksdb_writeoptions_set_sync(rdbs->woptions, 0);
#endif

	/* B */
	rdbs->wbatch = rocksdb_writebatch_create();

	rocksdb_options_set_create_if_missing(rdbs->options, 1);
	rdbs->db = rocksdb_open(rdbs->options, rdbs->dbname, &err);

	if (err) {
		fprintf(stderr, "%s", err);
		rocksdb_free(err);
		err = NULL;
		free(rdbs);
		return NULL;
	}

	return rdbs;
}

/*
 * Close the rdb.
 */
void rdb_close(struct _rocksdb_stuff *rdbs)
{
	rocksdb_close(rdbs->db);
	rocksdb_options_destroy(rdbs->options);
	rocksdb_readoptions_destroy(rdbs->roptions);
	rocksdb_writeoptions_destroy(rdbs->woptions);
	rocksdb_writebatch_destroy(rdbs->wbatch);
}

/*
 * Destroy the rdb.
 */
void rdb_destroy(struct _rocksdb_stuff *rdbs)
{
	char *err = NULL;

	rocksdb_close(rdbs->db);
	rocksdb_destroy_db(rdbs->options, rdbs->dbname, &err);

	if (err) {
		fprintf(stderr, "%s\n", err);
		rocksdb_free(err);
		err = NULL;
	}

	rocksdb_options_destroy(rdbs->options);
	rocksdb_readoptions_destroy(rdbs->roptions);
	rocksdb_writeoptions_destroy(rdbs->woptions);
	rocksdb_writebatch_destroy(rdbs->wbatch);
	free(rdbs);
}

char *rdb_get(struct _rocksdb_stuff *rdbs, const char *key, size_t klen, size_t *vlen)
{
	char    *err = NULL;
	char    *val = NULL;

	val = rocksdb_get(rdbs->db, rdbs->roptions, key, klen, vlen, &err);

	if (err) {
		fprintf(stderr, "%s\n", err);
		rocksdb_free(err);
		err = NULL;
	}

	return val;
}

inline int rdb_put(struct _rocksdb_stuff *rdbs, const char *key, size_t klen, const char *value, size_t vlen)
{
	char *err = NULL;

	rocksdb_put(rdbs->db, rdbs->woptions, key, klen, value, vlen, &err);

	if (err) {
		fprintf(stderr, "%s\n", err);
		rocksdb_free(err);
		err = NULL;
		return -1;
	} else {
		return 0;
	}
}

int rdb_pull(struct _rocksdb_stuff *rdbs, const char *key, size_t klen, char *value, size_t vlen)
{
	size_t  len = 0;
	char    *err = NULL;
	char    *val = NULL;

	val = rocksdb_get(rdbs->db, rdbs->roptions, key, klen, &len, &err);

	if (err) {
		fprintf(stderr, "%s\n", err);
		rocksdb_free(err);
		err = NULL;
		return -1;
	} else {
		if (!val) {
			return -1;
		}

		int ok = (vlen == len) ? 0 : -1;

		if (!ok) {
			memcpy(value, val, vlen);
		}

		rocksdb_free(val);
		return ok;
	}
}

inline int rdb_delete(struct _rocksdb_stuff *rdbs, const char *key, size_t klen)
{
	char    *err = NULL;
	char    *val = NULL;
	size_t  vlen = 0;

	val = rocksdb_get(rdbs->db, rdbs->roptions, key, klen, (size_t *)&vlen, &err);

	if (err) {
		fprintf(stderr, "%s\n", err);
		rocksdb_free(err);
		err = NULL;
		return -1;
	}

	rocksdb_free(val);

	/* if not found, then return 0. */
	if (vlen == 0) {
		return 0;
	}

	/* if found, delete it, then return 1. */
	rocksdb_delete(rdbs->db, rdbs->woptions, key, klen, &err);

	if (err) {
		fprintf(stderr, "%s\n", err);
		rocksdb_free(err);
		err = NULL;
		return -1;
	}

	return 1;
}

inline int rdb_batch_put(struct _rocksdb_stuff *rdbs, const char *key, size_t klen, const char *value, size_t vlen)
{
	rocksdb_writebatch_put(rdbs->wbatch, key, klen, value, vlen);
	return 0;
}

inline int rdb_batch_delete(struct _rocksdb_stuff *rdbs, const char *key, size_t klen)
{
	rocksdb_writebatch_delete(rdbs->wbatch, key, klen);
	return 0;
}

inline int rdb_batch_commit(struct _rocksdb_stuff *rdbs)
{
	char *err = NULL;

	rocksdb_write(rdbs->db, rdbs->woptions, rdbs->wbatch, &err);
	rocksdb_writebatch_clear(rdbs->wbatch);

	if (err) {
		fprintf(stderr, "%s\n", err);
		rocksdb_free(err);
		err = NULL;
		return -1;
	} else {
		return 0;
	}
}

inline void rdb_batch_rollback(struct _rocksdb_stuff *rdbs)
{
	rocksdb_writebatch_clear(rdbs->wbatch);
}

inline int rdb_exists(struct _rocksdb_stuff *rdbs, const char *key, size_t klen)
{
	char    *err = NULL;
	char    *val = NULL;
	size_t  vlen = 0;

	val = rocksdb_get(rdbs->db, rdbs->roptions, key, klen, (size_t *)&vlen, &err);

	if (err) {
		fprintf(stderr, "%s\n", err);
		rocksdb_free(err);
		err = NULL;
		return -1;
	}

	rocksdb_free(val);

	return vlen ? 1 : 0;
}

inline void rdb_compact(struct _rocksdb_stuff *rdbs)
{
	rocksdb_compact_range(rdbs->db, NULL, 0, NULL, 0);
}

const char *rdb_version(void)
{
	static char version[32];

	if (!version[0]) {
		snprintf(version, sizeof(version),
			"rocksdb-%d-%d",
			5,
			13);
	}

	return version;
}

