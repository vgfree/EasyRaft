#include "eraft_journal_ext.h"

ERAFT_JOURNAL_IMPL_INIT eraft_journal_mapping_init(enum ERAFT_JOURNAL_TYPE type)
{
	ERAFT_JOURNAL_IMPL_INIT finit = NULL;

	switch (type)
	{
		case ERAFT_JOURNAL_TYPE_DEFAULT:
			finit = eraft_journal_init_default;
			break;

		case ERAFT_JOURNAL_TYPE_LMDB:
			finit = eraft_journal_init_lmdb;
			break;

		case ERAFT_JOURNAL_TYPE_ROCKSDB:
			finit = eraft_journal_init_rocksdb;
			break;

		case ERAFT_JOURNAL_TYPE_BDB:
			finit = eraft_journal_init_bdb;
			break;

		default:
			abort();
	}
	return finit;
}

ERAFT_JOURNAL_IMPL_FREE eraft_journal_mapping_free(enum ERAFT_JOURNAL_TYPE type)
{
	ERAFT_JOURNAL_IMPL_FREE ffree = NULL;

	switch (type)
	{
		case ERAFT_JOURNAL_TYPE_DEFAULT:
			ffree = eraft_journal_free_default;
			break;

		case ERAFT_JOURNAL_TYPE_LMDB:
			ffree = eraft_journal_free_lmdb;
			break;

		case ERAFT_JOURNAL_TYPE_ROCKSDB:
			ffree = eraft_journal_free_rocksdb;
			break;

		case ERAFT_JOURNAL_TYPE_BDB:
			ffree = eraft_journal_free_bdb;
			break;

		default:
			abort();
	}
	return ffree;
}

