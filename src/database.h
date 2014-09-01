#ifndef __DATABASE_H__
#define __DATABASE_H__

#include "kopou.h"

struct _kopou_db {
	kopou_ht_t *primary;
	kopou_ht_t *secondary;
	long rehashpos;
	int locked;
} _kopou_db;


typedef struct kopou_db {
	struct _kopou_db *main;
} kopou_db_t;


#endif
