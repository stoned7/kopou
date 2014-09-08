#include "kopou.h"
#include "aarray.h"

struct _kopou_db {
	aarray_t *primary;
	aarray_t *secondary;
	long rehashpos;
	int locked;
} _kopou_db;


typedef struct kopou_db {
	struct _kopou_db *main;
} kopou_db_t;
