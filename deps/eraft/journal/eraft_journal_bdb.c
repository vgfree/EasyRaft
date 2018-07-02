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

#include "db.h"
#include "eraft_journal.h"

struct bdb_eraft_journal
{
	int             acceptor_id;
	char            *db_path;
	uint64_t        db_size;

	DB              *dbp;
	DB_ENV          *dbenv;
};

/* DB的函数执行完成后，返回0代表成功，否则失败 */
static void print_error(int ret)
{
	if (ret != 0) {
		printf("ERROR: %s\n", db_strerror(ret));
	}
}

/* 数据结构DBT在使用前，应首先初始化，否则编译可通过但运行时报参数错误 */
static void init_DBT(DBT *key, DBT *val)
{
	memset(key, 0, sizeof(DBT));
	memset(val, 0, sizeof(DBT));
}

static void __load_db(struct bdb_eraft_journal *bdb, char *db_path, int db_size)
{
	DB_ENV  *dbenv = NULL;
	int     ret = db_env_create(&dbenv, 0);

	print_error(ret);

	uint32_t flags = DB_CREATE | DB_INIT_MPOOL;
	flags |= DB_INIT_TXN | DB_INIT_LOCK;
	ret = dbenv->open(dbenv, db_path, flags, 0);
	print_error(ret);

	bdb->dbenv = dbenv;

	/* 首先创建数据库句柄 */
	DB *dbp;
	ret = db_create(&dbp, dbenv, 0);
	print_error(ret);
	assert(dbp);

	dbp->set_errfile(dbp, stderr);
	dbp->set_errpfx(dbp, "xxx");
#if 1
	dbenv = dbp->get_env(dbp);
	ret = dbenv->set_flags(dbenv, DB_DSYNC_DB, 1);
	print_error(ret);
	/* 设置DB查找数据库文件的目录 */
	// ret = dbenv->set_data_dir(dbenv, db_path);
	// print_error(ret);
#endif
	bdb->dbp = dbp;

	/* 创建一个名为entrys.db的数据库 */
	DB_TXN *txnp = NULL;
	ret = dbenv->txn_begin(dbenv, NULL, &txnp, 0);
	print_error(ret);

	flags = DB_CREATE;
	ret = dbp->open(dbp, txnp, "entrys.db", NULL, DB_RECNO, flags, 0);
	print_error(ret);

	ret = txnp->commit(txnp, 0);
	txnp = NULL;
	print_error(ret);
}

static void __drop_db(struct bdb_eraft_journal *bdb)
{
	// xxxx(bdb->dbp);FIXME

	bdb->dbenv = NULL;
	bdb->dbp = NULL;
}

/*丢弃一个*/
static void _bdb_drop(char *dbpath, int dbsize)
{
	struct bdb_eraft_journal bdb;

	/*加载存储*/
	__load_db(&bdb, dbpath, dbsize);
	__drop_db(&bdb);
}

/*加载一个*/
static int _bdb_load(struct bdb_eraft_journal *bdb, char *dbpath, int dbsize)
{
	/*加载存储*/
	__load_db(bdb, dbpath, dbsize);
	return 0;
}

/*重建一个*/
static int _bdb_make(struct bdb_eraft_journal *bdb, char *dbpath, int dbsize)
{
	_bdb_drop(dbpath, dbsize);
	return _bdb_load(bdb, dbpath, dbsize);
}

/************************************************************/

static int bdb_eraft_journal_open(void *handle)
{
	struct bdb_eraft_journal *s = handle;

	/*获取存储路径*/
	size_t  db_env_path_length = strlen(s->db_path) + 16;
	char    *db_env_path = malloc(db_env_path_length);

	snprintf(db_env_path, db_env_path_length, "%s_%d", s->db_path, s->acceptor_id);
	/*加载原有数据*/
	_bdb_load(s, db_env_path, s->db_size);

	free(db_env_path);
	return 0;
}

static void bdb_eraft_journal_close(void *handle)
{
	struct bdb_eraft_journal *s = handle;

	if (s->dbp) {
		s->dbp->close(s->dbp, 0);
	}

	printf("bdb eraft_journal closed successfully");
}

static void *bdb_eraft_journal_tx_begin(void *handle)
{
	struct bdb_eraft_journal *s = handle;

	DB_TXN  *txnp = NULL;
	int     ret = s->dbenv->txn_begin(s->dbenv, NULL, &txnp, 0);

	print_error(ret);
	return txnp;
}

static int bdb_eraft_journal_tx_commit(void *handle, void *txn)
{
	// struct bdb_eraft_journal       *s = handle;

	DB_TXN *txnp = (DB_TXN *)txn;
	/* Commit the inserted batch of records. */
	int ret = txnp->commit(txnp, 0);

	/*
	 * The transaction handle must not be referenced after
	 * a commit (or abort); null out the handle before
	 * checking the return code.
	 */
	txnp = NULL;
	print_error(ret);
	return 0;
}

static void bdb_eraft_journal_tx_abort(void *handle, void *txn)
{
	// struct bdb_eraft_journal *s = handle;

	DB_TXN  *txnp = (DB_TXN *)txn;
	int     ret = txnp->abort(txnp);

	txnp = NULL;
	print_error(ret);
}

static int bdb_eraft_journal_get(void *handle, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	struct bdb_eraft_journal *s = handle;

	DBT key, val;

	init_DBT(&key, &val);
	key.data = &iid;
	key.size = sizeof(iid_t);
	/* 从数据库中查询关键字为iid的记录 */
	int ret = s->dbp->get(s->dbp, NULL, &key, &val, 0);
	print_error(ret);

	if (0 != ret) {
		printf("There is no record for iid: %d", iid);
		return 0;
	}

	eraft_entry_decode(eentry, val.data, val.size);
	assert(iid == eentry->iid);

	free(val.data);

	return 1;
}

static int bdb_eraft_journal_set(void *handle, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	struct bdb_eraft_journal *s = handle;

	size_t  len = eraft_entry_cubage(eentry);
	char    *buf = malloc(len);

	eraft_journal_encode(eentry, buf, len);

	DBT key, val;
	init_DBT(&key, &val);
	key.data = &iid;
	key.size = sizeof(iid_t);
	val.data = buf;
	val.size = len;
	/* 把记录写入数据库中，允许覆盖关键字相同的记录 */
	int ret = s->dbp->put(s->dbp, NULL, &key, &val, DB_OVERWRITE_DUP);
	print_error(ret);

	free(buf);

	if (ret != 0) {
		printf("There is no space for iid: %d?", iid);
		return 0;
	}

	return 1;
}

#if 0
static iid_t
bdb_eraft_journal_get_trim_instance(void *handle)
{
	struct bdb_eraft_journal        *s = handle;
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
bdb_eraft_journal_put_trim_instance(void *handle, iid_t iid)
{
	struct bdb_eraft_journal        *s = handle;
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
bdb_eraft_journal_trim(void *handle, iid_t iid)
{
	struct bdb_eraft_journal        *s = handle;
	int                             result;
	iid_t                           min = 0;
	MDB_cursor                      *cursor = NULL;
	MDB_val                         key, data;

	if (iid == 0) {
		return 0;
	}

	bdb_eraft_journal_put_trim_instance(handle, iid);

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

static void __pop_newest_log(struct bdb_eraft_journal *bdb)
{
#ifdef TEST_NETWORK_ONLY
	return;
#endif
	// MDB_val k, v;

	// mdb_pop(bdb->db_env, bdb->entries, &k, &v);
}

static void __pop_oldest_log(struct bdb_eraft_journal *bdb)
{
#ifdef TEST_NETWORK_ONLY
	return;
#endif
	// MDB_val k, v;

	// mdb_poll(bdb->db_env, bdb->entries, &k, &v);
}

typedef void (*ERAFT_DSTORE_LOAD_COMMIT_LOG_FCB)(struct eraft_journal *journal, raft_entry_t *entry, void *usr);
/** Load all log entries we have persisted to disk */
static int __load_foreach_append_log(struct bdb_eraft_journal *bdb, ERAFT_DSTORE_LOAD_COMMIT_LOG_FCB fcb, void *usr)
{
#if 0
	MDB_cursor      *curs;
	MDB_txn         *txn;
	MDB_val         k, v;
	int             e;
	int             n_entries = 0;

	e = mdb_txn_begin(bdb->db_env, NULL, MDB_RDONLY, &txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	e = mdb_cursor_open(txn, bdb->entries, &curs);

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

			fcb(bdb, &ety, usr);
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

static int bdb_eraft_journal_set_state(void *handle, char *key, size_t klen, char *val, size_t vlen)
{
#ifdef TEST_NETWORK_ONLY
	return 0;
#endif
	return 0;
	// struct bdb_eraft_journal       *s = handle;

	// return rdb_push(s->dbp, key, klen, val, vlen);
}

static int bdb_eraft_journal_get_state(void *handle, char *key, size_t klen, char *val, size_t vlen)
{
	return 0;
	// struct bdb_eraft_journal       *s = handle;

	// return rdb_pull(s->dbp, key, klen, val, vlen);
}

/************************************************************************************************/
static struct bdb_eraft_journal *bdb_eraft_journal_init(int acceptor_id, char *dbpath, uint64_t dbsize)
{
	struct bdb_eraft_journal *h = calloc(1, sizeof(struct bdb_eraft_journal));

	h->acceptor_id = acceptor_id;
	h->db_path = strdup(dbpath);
	h->db_size = dbsize;
	return h;
}

static void bdb_eraft_journal_free(struct bdb_eraft_journal *h)
{
	free(h->db_path);

	free(h);
}

void eraft_journal_init_bdb(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize)
{
	j->handle = bdb_eraft_journal_init(acceptor_id, dbpath, dbsize);

	j->api.open = bdb_eraft_journal_open;
	j->api.close = bdb_eraft_journal_close;
	j->api.tx_begin = bdb_eraft_journal_tx_begin;
	j->api.tx_commit = bdb_eraft_journal_tx_commit;
	j->api.tx_abort = bdb_eraft_journal_tx_abort;
	j->api.get = bdb_eraft_journal_get;
	j->api.set = bdb_eraft_journal_set;
	// j->api.trim = bdb_eraft_journal_trim;
	// j->api.get_trim_instance = bdb_eraft_journal_get_trim_instance;

	j->api.set_state = bdb_eraft_journal_set_state;
	j->api.get_state = bdb_eraft_journal_get_state;
}

void eraft_journal_free_bdb(struct eraft_journal *j)
{
	bdb_eraft_journal_free((struct bdb_eraft_journal *)j->handle);
	j->handle = NULL;
}

