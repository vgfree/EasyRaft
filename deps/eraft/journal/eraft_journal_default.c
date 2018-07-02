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

#include "eraft_journal.h"

struct default_eraft_journal
{
	int             acceptor_id;
	char            *db_path;
	uint64_t        db_size;

	int             fd_entries;
	int             fd_state;
};

static void __load_db(struct default_eraft_journal *s, char *db_path, int db_size)
{}

static void __drop_db(struct default_eraft_journal *s)
{
	/*获取存储路径*/
	size_t  db_env_path_length = strlen(s->db_path) + 16;
	char    *db_env_path = malloc(db_env_path_length);

	snprintf(db_env_path, db_env_path_length, "%s_%d", s->db_path, s->acceptor_id);
	unlink(db_env_path);

	free(db_env_path);
}

/*丢弃一个*/
static void _default_drop(char *dbpath, int dbsize)
{
	struct default_eraft_journal s;

	/*加载存储*/
	__load_db(&s, dbpath, dbsize);
	__drop_db(&s);
}

/*加载一个*/
static int _default_load(struct default_eraft_journal *s, char *dbpath, int dbsize)
{
	/*加载存储*/
	__load_db(s, dbpath, dbsize);
	return 0;
}

/*重建一个*/
static int _default_make(struct default_eraft_journal *s, char *dbpath, int dbsize)
{
	_default_drop(dbpath, dbsize);
	return _default_load(s, dbpath, dbsize);
}

/************************************************************/

static int default_eraft_journal_open(void *handle)
{
	struct default_eraft_journal *s = handle;

	/*获取存储路径*/
	size_t  db_env_path_length = strlen(s->db_path) + 16;
	char    *db_env_path = malloc(db_env_path_length);

	snprintf(db_env_path, db_env_path_length, "%s_%d", s->db_path, s->acceptor_id);
	/*加载原有数据*/
	_default_load(s, db_env_path, s->db_size);

	s->fd_entries = open(db_env_path, O_CREAT | O_APPEND | O_WRONLY | O_SYNC, 0644);

	free(db_env_path);

	if (unlikely(s->fd_entries < 0)) {
		return -1;
	}

	/*获取存储路径*/
	size_t  db_state_path_length = strlen(s->db_path) + 16;
	char    *db_state_path = malloc(db_state_path_length);
	snprintf(db_state_path, db_state_path_length, "%s_%d.s", s->db_path, s->acceptor_id);

	s->fd_state = open(db_state_path, O_CREAT | O_RDWR | O_SYNC, 0644);	// TODO: fix to O_DIRECT

	free(db_state_path);

	if (unlikely(s->fd_state < 0)) {
		return -1;
	}

	return 0;
}

static void default_eraft_journal_close(void *handle)
{
	struct default_eraft_journal *s = handle;

	if (s->fd_entries) {
		close(s->fd_entries);
	}

	if (s->fd_state) {
		close(s->fd_state);
	}

	printf("default eraft_journal closed successfully");
}

static void *default_eraft_journal_tx_begin(void *handle)
{
	// struct default_eraft_journal *s = handle;
	return NULL;
}

static int default_eraft_journal_tx_commit(void *handle, void *txn)
{
	// struct default_eraft_journal       *s = handle;
	return 0;
}

static void default_eraft_journal_tx_abort(void *handle, void *txn)
{
	// struct default_eraft_journal *s = handle;
}

static int default_eraft_journal_get(void *handle, void *txn, iid_t iid, struct eraft_entry *eentry)
{
#if 0
	struct default_eraft_journal *s = handle;

	size_t  vlen = 0;
	char    *val = rdb_get(s->rdbs, (const char *)&iid, sizeof(iid_t), &vlen);

	if (NULL == val) {
		printf("There is no record for iid: %d", iid);
		return 0;
	}

	eraft_entry_decode(eentry, val, vlen);
	assert(iid == eentry->iid);

	default_free(val);
#endif

	return 1;
}

static int default_eraft_journal_set(void *handle, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	struct default_eraft_journal *s = handle;

	size_t  len = eraft_entry_cubage(eentry);
	char    *buf = malloc(len);

	eraft_journal_encode(eentry, buf, len);

	char    *wdata = buf;
	size_t  wsize = len;

	while (wsize) {
		ssize_t wbyte = write(s->fd_entries, wdata, wsize);

		if (wbyte < 0) {
			printf("There is no space for iid: %d?", iid);
			free(buf);
			return 0;
		} else {
			wdata += wbyte;
			wsize -= wbyte;
		}
	}

	free(buf);

	return 1;
}

#if 0
static iid_t
default_eraft_journal_get_trim_instance(void *handle)
{
	struct default_eraft_journal    *s = handle;
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
default_eraft_journal_put_trim_instance(void *handle, iid_t iid)
{
	struct default_eraft_journal    *s = handle;
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
default_eraft_journal_trim(void *handle, iid_t iid)
{
	struct default_eraft_journal    *s = handle;
	int                             result;
	iid_t                           min = 0;
	MDB_cursor                      *cursor = NULL;
	MDB_val                         key, data;

	if (iid == 0) {
		return 0;
	}

	default_eraft_journal_put_trim_instance(handle, iid);

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

static void __pop_newest_log(struct default_eraft_journal *s)
{
#ifdef TEST_NETWORK_ONLY
	return;
#endif
	// MDB_val k, v;

	// mdb_pop(s->db_env, s->entries, &k, &v);
}

static void __pop_oldest_log(struct default_eraft_journal *s)
{
#ifdef TEST_NETWORK_ONLY
	return;
#endif
	// MDB_val k, v;

	// mdb_poll(s->db_env, s->entries, &k, &v);
}

typedef void (*ERAFT_DSTORE_LOAD_COMMIT_LOG_FCB)(struct eraft_journal *journal, raft_entry_t *entry, void *usr);
/** Load all log entries we have persisted to disk */
static int __load_foreach_append_log(struct default_eraft_journal *s, ERAFT_DSTORE_LOAD_COMMIT_LOG_FCB fcb, void *usr)
{
	// TODO:消除不完成的数据
#if 0
	MDB_cursor      *curs;
	MDB_txn         *txn;
	MDB_val         k, v;
	int             e;
	int             n_entries = 0;

	e = mdb_txn_begin(s->db_env, NULL, MDB_RDONLY, &txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	e = mdb_cursor_open(txn, s->entries, &curs);

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

			fcb(s, &ety, usr);
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

static int default_eraft_journal_set_state(void *handle, char *key, size_t klen, char *val, size_t vlen)
{
#ifdef TEST_NETWORK_ONLY
	return 0;
#endif
#if 0
	struct default_eraft_journal *s = handle;

	// FIXME:
	ssize_t wbyte = write(s->fd_state, key, klen);

	if (wbyte < 0) {
		return -1;
	}

	wbyte = write(s->fd_state, val, vlen);

	if (wbyte < 0) {
		return -1;
	}
#endif

	return 0;
}

static int default_eraft_journal_get_state(void *handle, char *key, size_t klen, char *val, size_t vlen)
{
	// struct default_eraft_journal       *s = handle;

	return 0;
}

/************************************************************************************************/
static struct default_eraft_journal *default_eraft_journal_init(int acceptor_id, char *dbpath, uint64_t dbsize)
{
	struct default_eraft_journal *h = calloc(1, sizeof(struct default_eraft_journal));

	h->acceptor_id = acceptor_id;
	h->db_path = strdup(dbpath);
	h->db_size = dbsize;
	return h;
}

static void default_eraft_journal_free(struct default_eraft_journal *h)
{
	free(h->db_path);

	free(h);
}

void eraft_journal_init_default(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize)
{
	j->handle = default_eraft_journal_init(acceptor_id, dbpath, dbsize);

	j->api.open = default_eraft_journal_open;
	j->api.close = default_eraft_journal_close;
	j->api.tx_begin = default_eraft_journal_tx_begin;
	j->api.tx_commit = default_eraft_journal_tx_commit;
	j->api.tx_abort = default_eraft_journal_tx_abort;
	j->api.get = default_eraft_journal_get;
	j->api.set = default_eraft_journal_set;
	// j->api.trim = default_eraft_journal_trim;
	// j->api.get_trim_instance = default_eraft_journal_get_trim_instance;

	j->api.set_state = default_eraft_journal_set_state;
	j->api.get_state = default_eraft_journal_get_state;
}

void eraft_journal_free_default(struct eraft_journal *j)
{
	default_eraft_journal_free((struct default_eraft_journal *)j->handle);
	j->handle = NULL;
}

