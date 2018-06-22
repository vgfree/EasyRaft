#pragma once

#include <string.h>
#include "list.h"

struct eraft_dotask
{
	struct list_node        node;
	int    type;
	bool			merge;
	char                    *identity;

	char                    object[0];
};

void eraft_dotask_init(struct eraft_dotask *task, int type, bool merge, char *identity);

void eraft_dotask_free(struct eraft_dotask *task);
