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

#include "lmdb.h"
#include "lmdb_helpers.h"

#include "eraft_journal.h"

struct lmdb_eraft_journal
{
	int             acceptor_id;
	char            *db_path;
	uint64_t        db_size;

	/* Persistent state for voted_for and term
	 * We store string keys (eg. "term") with int values */
	MDB_dbi         state;

	/* Entries that have been appended to our log
	 * For each log entry we store two things next to each other:
	 *  - TPL serialized raft_entry_t
	 *  - raft_entry_data_t */
	MDB_dbi         entries;

	/* LMDB database environment */
	MDB_env         *db_env;
};

static void __load_db(struct lmdb_eraft_journal *lmdb, char *db_path, int db_size)
{
	mdb_db_env_create(&lmdb->db_env, 0, db_path, db_size);
	mdb_db_create(&lmdb->entries, lmdb->db_env, "entries");
	mdb_db_create(&lmdb->state, lmdb->db_env, "state");
}

static void __drop_db(struct lmdb_eraft_journal *lmdb)
{
	MDB_dbi dbs[] = { lmdb->entries, lmdb->state };

	mdb_drop_dbs(lmdb->db_env, dbs, ARRAY_SIZE(dbs));
}

/*丢弃一个*/
static void _lmdb_drop(char *dbpath, int dbsize)
{
	struct lmdb_eraft_journal lmdb;

	/*加载存储*/
	__load_db(&lmdb, dbpath, dbsize);
	__drop_db(&lmdb);
}

/*加载一个*/
static int _lmdb_load(struct lmdb_eraft_journal *lmdb, char *dbpath, int dbsize)
{
	/*加载存储*/
	__load_db(lmdb, dbpath, dbsize);
	return 0;
}

/*重建一个*/
static int _lmdb_make(struct lmdb_eraft_journal *lmdb, char *dbpath, int dbsize)
{
	_lmdb_drop(dbpath, dbsize);
	return _lmdb_load(lmdb, dbpath, dbsize);
}

/************************************************************/

static int lmdb_eraft_journal_open(void *handle)
{
	struct lmdb_eraft_journal *s = handle;

	/*获取存储路径*/
	size_t  db_env_path_length = strlen(s->db_path) + 16;
	char    *db_env_path = malloc(db_env_path_length);

	snprintf(db_env_path, db_env_path_length, "%s_%d", s->db_path, s->acceptor_id);
	/*加载原有数据*/
	_lmdb_load(s, db_env_path, s->db_size);

	free(db_env_path);
	return 0;
}

static void lmdb_eraft_journal_close(void *handle)
{
	struct lmdb_eraft_journal *s = handle;

	if (s->entries) {
		mdb_close(s->db_env, s->entries);
	}

	if (s->db_env) {
		mdb_env_close(s->db_env);
	}

	printf("lmdb eraft_journal closed successfully");
}

static void *lmdb_eraft_journal_tx_begin(void *handle)
{
	struct lmdb_eraft_journal *s = handle;

	MDB_txn *txn = NULL;
	int     e = mdb_txn_begin(s->db_env, NULL, 0, &txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	return txn;
}

static int lmdb_eraft_journal_tx_commit(void *handle, void *txn)
{
	// struct lmdb_eraft_journal       *s = handle;

	assert(txn);
	int e = mdb_txn_commit(txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	return e;
}

static void lmdb_eraft_journal_tx_abort(void *handle, void *txn)
{
	// struct lmdb_eraft_journal *s = handle;

	if (txn) {
		mdb_txn_abort(txn);
	}
}

static int lmdb_eraft_journal_get(void *handle, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	struct lmdb_eraft_journal *s = handle;

	MDB_val key;

	key.mv_data = &iid;
	key.mv_size = sizeof(iid_t);

	MDB_val val;
	memset(&val, 0, sizeof(val));

	assert(txn);
	int e = mdb_get(txn, s->entries, &key, &val);

	if (e != 0) {
		if (e == MDB_NOTFOUND) {
			printf("There is no record for iid: %d", iid);
		} else {
			printf("Could not find record for iid: %d : %s", iid, mdb_strerror(e));
			mdb_fatal(e);
		}

		return 0;
	}

	eraft_entry_decode(eentry, val.mv_data, val.mv_size);
	assert(iid == eentry->iid);

	return 1;
}

static int lmdb_eraft_journal_set(void *handle, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	struct lmdb_eraft_journal *s = handle;

	size_t  len = eraft_entry_cubage(eentry);
	char    *buf = malloc(len);

	eraft_journal_encode(eentry, buf, len);

	MDB_val key;
	key.mv_data = &iid;
	key.mv_size = sizeof(iid_t);

	MDB_val val;
	val.mv_data = buf;
	val.mv_size = len;

	assert(txn);
	int e = mdb_put(txn, s->entries, &key, &val, 0);

	free(buf);

	if (e != 0) {
		if (e == MDB_MAP_FULL) {
			printf("There is no space for iid: %d", iid);
		} else {
			printf("Could not save record for iid: %d : %s", iid, mdb_strerror(e));
			mdb_fatal(e);
		}

		return 0;
	}

	return 1;
}

#if 0
static iid_t
lmdb_eraft_journal_get_trim_instance(void *handle)
{
	struct lmdb_eraft_journal       *s = handle;
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
lmdb_eraft_journal_put_trim_instance(void *handle, iid_t iid)
{
	struct lmdb_eraft_journal       *s = handle;
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
lmdb_eraft_journal_trim(void *handle, iid_t iid)
{
	struct lmdb_eraft_journal       *s = handle;
	int                             result;
	iid_t                           min = 0;
	MDB_cursor                      *cursor = NULL;
	MDB_val                         key, data;

	if (iid == 0) {
		return 0;
	}

	lmdb_eraft_journal_put_trim_instance(handle, iid);

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

#endif	/* if 0 */

void __pop_newest_log(struct lmdb_eraft_journal *lmdb)
{
#ifdef TEST_NETWORK_ONLY
	return;
#endif
	MDB_val k, v;

	mdb_pop(lmdb->db_env, lmdb->entries, &k, &v);
}

void __pop_oldest_log(struct lmdb_eraft_journal *lmdb)
{
#ifdef TEST_NETWORK_ONLY
	return;
#endif
	MDB_val k, v;

	mdb_poll(lmdb->db_env, lmdb->entries, &k, &v);
}

typedef void (*ERAFT_DSTORE_LOAD_COMMIT_LOG_FCB)(struct eraft_journal *journal, raft_entry_t *entry, void *usr);
/** Load all log entries we have persisted to disk */
int __load_foreach_append_log(struct lmdb_eraft_journal *lmdb, ERAFT_DSTORE_LOAD_COMMIT_LOG_FCB fcb, void *usr)
{
#if 0
	MDB_cursor      *curs;
	MDB_txn         *txn;
	MDB_val         k, v;
	int             e;
	int             n_entries = 0;

	e = mdb_txn_begin(lmdb->db_env, NULL, MDB_RDONLY, &txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	e = mdb_cursor_open(txn, lmdb->entries, &curs);

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

			fcb(lmdb, &ety, usr);
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
#endif	/* if 0 */
}

static int lmdb_eraft_journal_set_state(void *handle, char *key, size_t klen, char *val, size_t vlen)
{
#ifdef TEST_NETWORK_ONLY
	return 0;
#endif
	struct lmdb_eraft_journal *s = handle;

	return mdb_puts_int_commit(s->db_env, s->state, key, *(int *)val);
}

static int lmdb_eraft_journal_get_state(void *handle, char *key, size_t klen, char *val, size_t vlen)
{
	struct lmdb_eraft_journal *s = handle;

	return mdb_gets_int(s->db_env, s->state, key, (int *)val);
}

/************************************************************************************************/
static struct lmdb_eraft_journal *lmdb_eraft_journal_init(int acceptor_id, char *dbpath, uint64_t dbsize)
{
	struct lmdb_eraft_journal *h = calloc(1, sizeof(struct lmdb_eraft_journal));

	h->acceptor_id = acceptor_id;
	h->db_path = strdup(dbpath);
	h->db_size = dbsize;
	return h;
}

static void lmdb_eraft_journal_free(struct lmdb_eraft_journal *h)
{
	free(h->db_path);

	free(h);
}

void eraft_journal_init_lmdb(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize)
{
	j->handle = lmdb_eraft_journal_init(acceptor_id, dbpath, dbsize);

	j->api.open = lmdb_eraft_journal_open;
	j->api.close = lmdb_eraft_journal_close;
	j->api.tx_begin = lmdb_eraft_journal_tx_begin;
	j->api.tx_commit = lmdb_eraft_journal_tx_commit;
	j->api.tx_abort = lmdb_eraft_journal_tx_abort;
	j->api.get = lmdb_eraft_journal_get;
	j->api.set = lmdb_eraft_journal_set;
	// j->api.trim = lmdb_eraft_journal_trim;
	// j->api.get_trim_instance = lmdb_eraft_journal_get_trim_instance;

	j->api.set_state = lmdb_eraft_journal_set_state;
	j->api.get_state = lmdb_eraft_journal_get_state;
}

void eraft_journal_free_lmdb(struct eraft_journal *j)
{
	lmdb_eraft_journal_free((struct lmdb_eraft_journal *)j->handle);
	j->handle = NULL;
}

