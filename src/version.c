#include "kopou.h"

typedef struct _version_obj {
	struct _version_obj *next;
	struct _version_obj *prev;
	void *data;
	kstr_t content_type;
	kstr_t *tags;
	int ntags;
	size_t size;
	time_t lastupdated_ts;
	unsigned long long version;
} version_obj_t;


