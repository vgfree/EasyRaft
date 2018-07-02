#include "eraft_network_ext.h"

ERAFT_NETWORK_IMPL_INIT eraft_network_mapping_init(enum ERAFT_NETWORK_TYPE type)
{
	ERAFT_NETWORK_IMPL_INIT finit = NULL;

	switch (type)
	{
		case ERAFT_NETWORK_TYPE_LIBUV:
			finit = eraft_network_init_libuv;
			break;

		case ERAFT_NETWORK_TYPE_LIBCOMM:
			finit = eraft_network_init_libcomm;
			break;

		default:
			abort();
	}
	return finit;
}

ERAFT_NETWORK_IMPL_FREE eraft_network_mapping_free(enum ERAFT_NETWORK_TYPE type)
{
	ERAFT_NETWORK_IMPL_FREE ffree = NULL;

	switch (type)
	{
		case ERAFT_NETWORK_TYPE_LIBUV:
			ffree = eraft_network_free_libuv;
			break;

		case ERAFT_NETWORK_TYPE_LIBCOMM:
			ffree = eraft_network_free_libcomm;
			break;

		default:
			abort();
	}
	return ffree;
}

