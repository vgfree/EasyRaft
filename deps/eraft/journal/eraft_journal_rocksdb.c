/*
 * Copyright (c) 2014, Xiaoguang Sun <sun dot xiaoguang at yoyosys dot com>
 * Copyright (c) 2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>

#include "rdb.h"
#include "eraft_journal.h"

#define ERAFT_RDB_BLK_SIZE        (32 * 1024)
#define ERAFT_RDB_WB_SIZE         (64 * 1024 * 1024)
#define ERAFT_RDB_LRU_SIZE        (64 * 1024 * 1024)
#define ERAFT_RDB_BLOOM_SIZE      (10)

struct rocksdb_eraft_journal
{
	int     acceptor_id;
	char *db_path;
	uint64_t db_size;

	struct _rocksdb_stuff *rdbs;
};

static void __load_db(struct rocksdb_eraft_journal *rocksdb, char *db_path, int db_size)
{
	struct _rocksdb_stuff *rdbs = rdb_initialize(db_path,
			ERAFT_RDB_BLK_SIZE,
			ERAFT_RDB_WB_SIZE,
			ERAFT_RDB_LRU_SIZE,
			ERAFT_RDB_BLOOM_SIZE);
	assert(rdbs);

	rocksdb->rdbs = rdbs;
}

static void __drop_db(struct rocksdb_eraft_journal *rocksdb)
{
	rdb_destroy(rocksdb->rdbs);

	rocksdb->rdbs = NULL;
}

/*丢弃一个*/
static void _rocksdb_drop(char *dbpath, int dbsize)
{
	struct rocksdb_eraft_journal rocksdb;

	/*加载存储*/
	__load_db(&rocksdb, dbpath, dbsize);
	__drop_db(&rocksdb);
}

/*加载一个*/
static int _rocksdb_load(struct rocksdb_eraft_journal *rocksdb, char *dbpath, int dbsize)
{
	/*加载存储*/
	__load_db(rocksdb, dbpath, dbsize);
	return 0;
}

/*重建一个*/
static int _rocksdb_make(struct rocksdb_eraft_journal *rocksdb, char *dbpath, int dbsize)
{
	_rocksdb_drop(dbpath, dbsize);
	return _rocksdb_load(rocksdb, dbpath, dbsize);
}

/************************************************************/

static int rocksdb_eraft_journal_open(void *handle)
{
	struct rocksdb_eraft_journal       *s = handle;

	/*获取存储路径*/
	size_t                          db_env_path_length = strlen(s->db_path) + 16;
	char                            *db_env_path = malloc(db_env_path_length);
	snprintf(db_env_path, db_env_path_length, "%s_%d", s->db_path, s->acceptor_id);
	/*加载原有数据*/
	_rocksdb_load(s, db_env_path, s->db_size);

	free(db_env_path);
	return 0;
}

static void rocksdb_eraft_journal_close(void *handle)
{
	struct rocksdb_eraft_journal *s = handle;

	if (s->rdbs) {
		rdb_close(s->rdbs);
	}

	printf("rocksdb eraft_journal closed successfully");
}

static void *rocksdb_eraft_journal_tx_begin(void *handle)
{
	//struct rocksdb_eraft_journal *s = handle;
	return NULL;
}

static int rocksdb_eraft_journal_tx_commit(void *handle, void *txn)
{
	struct rocksdb_eraft_journal       *s = handle;

	int e = rdb_batch_commit(s->rdbs);
	assert (0 == e);
	return e;
}

static void rocksdb_eraft_journal_tx_abort(void *handle, void *txn)
{
	//struct rocksdb_eraft_journal *s = handle;
}

static int rocksdb_eraft_journal_get(void *handle, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	struct rocksdb_eraft_journal       *s = handle;

	size_t vlen = 0;
	char *val = rdb_get(s->rdbs, (const char *)&iid, sizeof(iid_t), &vlen);

	if (NULL == val) {
		printf("There is no record for iid: %d", iid);
		return 0;
	}

	eraft_entry_decode(eentry, val, vlen);
	assert(iid == eentry->iid);

	rocksdb_free(val);

	return 1;
}

static int rocksdb_eraft_journal_set(void *handle, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	struct rocksdb_eraft_journal       *s = handle;

	size_t  len = eraft_entry_cubage(eentry);
	char *buf = malloc(len);
	eraft_journal_encode(eentry, buf, len);


	int e = rdb_put(s->rdbs, (const char *)&iid, sizeof(iid_t), buf, len);

	free(buf);

	if (e != 0) {
		printf("There is no space for iid: %d?", iid);
		return 0;
	}

	return 1;
}

#if 0
static iid_t
rocksdb_eraft_journal_get_trim_instance(void *handle)
{
	struct rocksdb_eraft_journal       *s = handle;
	int                             result;
	iid_t                           iid = 0, k = 0;
	MDB_val                         key, data;

	key.mv_data = &k;
	key.mv_size = sizeof(iid_t);

	if ((result = mdb_get(txn, s->entries, &key, &data)) != 0) {
		if (result != MDB_NOTFOUND) {
			paxos_log_error("mdb_get failed: %s", mdb_strerror(result));
			assert(result == 0);
		} else {
			iid = 0;
		}
	} else {
		iid = *(iid_t *)data.mv_data;
	}

	return iid;
}

static int
rocksdb_eraft_journal_put_trim_instance(void *handle, iid_t iid)
{
	struct rocksdb_eraft_journal       *s = handle;
	iid_t                           k = 0;
	int                             result;
	MDB_val                         key, data;

	key.mv_data = &k;
	key.mv_size = sizeof(iid_t);

	data.mv_data = &iid;
	data.mv_size = sizeof(iid_t);

	result = mdb_put(txn, s->entries, &key, &data, 0);

	if (result != 0) {
		paxos_log_error("%s\n", mdb_strerror(result));
	}

	assert(result == 0);

	return 0;
}

static int
rocksdb_eraft_journal_trim(void *handle, iid_t iid)
{
	struct rocksdb_eraft_journal       *s = handle;
	int                             result;
	iid_t                           min = 0;
	MDB_cursor                      *cursor = NULL;
	MDB_val                         key, data;

	if (iid == 0) {
		return 0;
	}

	rocksdb_eraft_journal_put_trim_instance(handle, iid);

	if ((result = mdb_cursor_open(txn, s->entries, &cursor)) != 0) {
		paxos_log_error("Could not create cursor. %s", mdb_strerror(result));
		goto cleanup_exit;
	}

	key.mv_data = &min;
	key.mv_size = sizeof(iid_t);

	do {
		if ((result = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
			assert(key.mv_size = sizeof(iid_t));
			min = *(iid_t *)key.mv_data;
		} else {
			goto cleanup_exit;
		}

		if ((min != 0) && (min <= iid)) {
			if (mdb_cursor_del(cursor, 0) != 0) {
				paxos_log_error("mdb_cursor_del failed. %s",
					mdb_strerror(result));
				goto cleanup_exit;
			}
		}
	} while (min <= iid);

cleanup_exit:

	if (cursor) {
		mdb_cursor_close(cursor);
	}

	return 0;
}
#endif

static void __pop_newest_log(struct rocksdb_eraft_journal *rocksdb)
{
#ifdef TEST_NETWORK_ONLY
	return;
#endif
	//MDB_val k, v;

	//mdb_pop(rocksdb->db_env, rocksdb->entries, &k, &v);
}

static void __pop_oldest_log(struct rocksdb_eraft_journal *rocksdb)
{
#ifdef TEST_NETWORK_ONLY
	return;
#endif
	//MDB_val k, v;

	//mdb_poll(rocksdb->db_env, rocksdb->entries, &k, &v);
}

typedef void (*ERAFT_DSTORE_LOAD_COMMIT_LOG_FCB)(struct eraft_journal    *journal, raft_entry_t *entry, void *usr);
/** Load all log entries we have persisted to disk */
static int __load_foreach_append_log(struct rocksdb_eraft_journal *rocksdb, ERAFT_DSTORE_LOAD_COMMIT_LOG_FCB fcb, void *usr)
{
#if 0
	MDB_cursor      *curs;
	MDB_txn         *txn;
	MDB_val         k, v;
	int             e;
	int             n_entries = 0;

	e = mdb_txn_begin(rocksdb->db_env, NULL, MDB_RDONLY, &txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	e = mdb_cursor_open(txn, rocksdb->entries, &curs);

	if (0 != e) {
		mdb_fatal(e);
	}

	e = mdb_cursor_get(curs, &k, &v, MDB_FIRST);
	switch (e)
	{
		case 0:
			break;

		case MDB_NOTFOUND:
			return 0;

		default:
			mdb_fatal(e);
	}

	raft_entry_t ety;
	ety.id = 0;

	do {
		if (!(*(int *)k.mv_data & 1)) {
			/* entry metadata */
			__deserialize(&ety, v.mv_data, v.mv_size);
		} else {
			/* entry data for FSM */
			ety.data.buf = v.mv_data;
			ety.data.len = v.mv_size;

			fcb(rocksdb, &ety, usr);
			n_entries++;
		}

		e = mdb_cursor_get(curs, &k, &v, MDB_NEXT);
	} while (0 == e);

	mdb_cursor_close(curs);

	e = mdb_txn_commit(txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	return n_entries;
#else
	return 0;
#endif
}


static int rocksdb_eraft_journal_set_state(void *handle, char *key, size_t klen, char *val, size_t vlen)
{
#ifdef TEST_NETWORK_ONLY
	return 0;
#endif
	struct rocksdb_eraft_journal       *s = handle;

	return rdb_push(s->rdbs, key, klen, val, vlen);
}

static int rocksdb_eraft_journal_get_state(void *handle, char *key, size_t klen, char *val, size_t vlen)
{
	struct rocksdb_eraft_journal       *s = handle;

	return rdb_pull(s->rdbs, key, klen, val, vlen);
}

/************************************************************************************************/
static struct rocksdb_eraft_journal *rocksdb_eraft_journal_init(int acceptor_id, char *dbpath, uint64_t dbsize)
{
	struct rocksdb_eraft_journal *h = calloc(1, sizeof(struct rocksdb_eraft_journal));

	h->acceptor_id = acceptor_id;
	h->db_path = strdup(dbpath);
	h->db_size = dbsize;
	return h;
}
static void rocksdb_eraft_journal_free(struct rocksdb_eraft_journal *h)
{
	free(h->db_path);

	free(h);
}


void eraft_journal_init_rocksdb(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize)
{
	j->handle = rocksdb_eraft_journal_init(acceptor_id, dbpath, dbsize);

	j->api.open = rocksdb_eraft_journal_open;
	j->api.close = rocksdb_eraft_journal_close;
	j->api.tx_begin = rocksdb_eraft_journal_tx_begin;
	j->api.tx_commit = rocksdb_eraft_journal_tx_commit;
	j->api.tx_abort = rocksdb_eraft_journal_tx_abort;
	j->api.get = rocksdb_eraft_journal_get;
	j->api.set = rocksdb_eraft_journal_set;
	//j->api.trim = rocksdb_eraft_journal_trim;
	//j->api.get_trim_instance = rocksdb_eraft_journal_get_trim_instance;

	j->api.set_state = rocksdb_eraft_journal_set_state;
	j->api.get_state = rocksdb_eraft_journal_get_state;
}

void eraft_journal_free_rocksdb(struct eraft_journal *j)
{
	rocksdb_eraft_journal_free((struct rocksdb_eraft_journal *)j->handle);
	j->handle = NULL;
}
