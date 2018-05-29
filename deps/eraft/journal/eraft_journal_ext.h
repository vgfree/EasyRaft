#pragma once

#include "eraft_journal.h"

typedef void (*ERAFT_JOURNAL_IMPL_INIT)(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize);

typedef void (*ERAFT_JOURNAL_IMPL_FREE)(struct eraft_journal *j);

enum ERAFT_JOURNAL_TYPE
{
        ERAFT_JOURNAL_TYPE_DEFAULT = 0,
        ERAFT_JOURNAL_TYPE_LMDB = 1,
        ERAFT_JOURNAL_TYPE_ROCKSDB = 2,
        ERAFT_JOURNAL_TYPE_BDB = 3
};


ERAFT_JOURNAL_IMPL_INIT eraft_journal_mapping_init(enum ERAFT_JOURNAL_TYPE type);

ERAFT_JOURNAL_IMPL_FREE eraft_journal_mapping_free(enum ERAFT_JOURNAL_TYPE type);


void eraft_journal_init_default(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize);

void eraft_journal_free_default(struct eraft_journal *j);

void eraft_journal_init_lmdb(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize);

void eraft_journal_free_lmdb(struct eraft_journal *j);

void eraft_journal_init_rocksdb(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize);

void eraft_journal_free_rocksdb(struct eraft_journal *j);

void eraft_journal_init_bdb(struct eraft_journal *j, int acceptor_id, char *dbpath, uint64_t dbsize);

void eraft_journal_free_bdb(struct eraft_journal *j);
