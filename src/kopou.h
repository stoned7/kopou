#ifndef __KOPOU_H__
#define __KOPOU_H__

#include <limits.h>
#include <string.h>
#include <time.h>
#include "kstring.h"
#include "xalloc.h"
#include "aarray.h"
#include "database.h"
#include "list.h"
#include "kevent.h"
#include "tcp.h"

#define K_OK 0
#define K_ERR -1

#define KOPOU_CRON_TIME 10

#define VNODE_SIZE 0x080
#define _vnode_state_size_slots (VNODE_SIZE >> 0x003)
#define _vnode_state_slots(y) (y >> 0x003)
#define _vnode_state_mask(y) (0x001 << (y & 0x007))
#define vnode_state_add(x, y) \
		((x)[_vnode_state_slots(y)] |= _vnode_state_mask(y))
#define vnode_state_remove(x, y) \
		((x)[_vnode_state_slots(y)] &= ~_vnode_state_mask(y))
#define vnode_state_contain(x, y) \
		((x)[_vnode_state_slots(y)] & _vnode_state_mask(y))
#define vnode_state_empty(x) (memset(x, 0x000, _vnode_state_size_slots))
#define vnode_state_fill(x) (memset(x, 0x0ff, _vnode_state_size_slots))
typedef unsigned char vnodes_state_t[_vnode_state_size_slots];

#define KOPOU_MAX_LOGMSG_LEN 256
#define KOPOU_DEBUG 0
#define KOPOU_INFO 1
#define KOPOU_WARNING 2
#define KOPOU_ERR 3
#define _KOPOU_FATAL 4
#define KOPOU_DEFAULT_VERBOSITY KOPOU_WARNING
void klog(int level, const char *fmt, ...);
#define _kdie(m, ...)\
	do { klog(_KOPOU_FATAL,"[%s:%d:%s()]"m,__FILE__,__LINE__,__func__,##__VA_ARGS__);\
		exit(EXIT_FAILURE);\
	} while (0)

#define KOPOU_KEY_MAX_SIZE (256)
#define REQ_BUFFER_SIZE (16 * 1024)
#define REQ_CONTENT_LENGTH_MAX (32 * (1024 * 1024))
#define REQ_BUFFER_SIZE_MAX (2 * REQ_CONTENT_LENGTH_MAX)
#define REQ_CONTINUE_IDLE_TIMEOUT (60 * 1000)

#define REQ_NAME_LENGTH 5
#define REMOTE_ADDR_LENGTH 16

#define KOBJ_TYPE_STRING 0
#define KOBJ_TYPE_BINARY 1
#define KOBJ_ENCODING_KSTR 2
#define KOBJ_ENCODING_RAW 3

#define CONFIG_LINE_LENGTH_MAX 1024

#define KOPOU_DEFAULT_MAX_CONCURRENT_CLIENTS 1024
#define KOPOU_DEFAULT_CLIENT_IDLE_TIMEOUT (2 * 60)

struct kopou_settings {
	kstr_t cluster_name;
	kstr_t address;
	int port;
	int cport;
	int mport;
	int background;
	int verbosity;
	kstr_t logfile;
	kstr_t dbdir;
	kstr_t dbfile;
	kstr_t workingdir;
	int max_ccur_clients;
	int client_idle_timeout;
	int client_keepalive;
	kstr_t configfile;
};


struct kopou_server {
	pid_t pid;
	kstr_t pidfile;
	time_t current_time;
	int shutdown;
	int listener;
	int clistener;
	int mlistener;
};

struct kopou_stats {
	long long objects;
	long long missed;
	long long hits;
	long long deleted;
};

extern struct kopou_server kopou;
extern struct kopou_settings settings;
extern struct kopou_stats stats;

struct kclient;

typedef struct kobj {
	void *val;
	size_t len;
	int type;
	int encoding;
} kobj_t;

struct req_blueprint {
	char name[REQ_NAME_LENGTH];
	int readonly;
	int argc;
	int name_index;
	int key_index;
	int content_index;
	void (*action)(struct kclient *client);
};

struct kclient {
	int fd;
	char remoteaddr[REMOTE_ADDR_LENGTH];
	int remoteport;
	time_t created_ts;
	time_t last_access_ts;
	int expected_argc;
	int argc;
	kobj_t **argv;
	struct req_blueprint *blueprint;
	int req_type;
	int req_ready_to_process;
	size_t req_parsing_pos;
	time_t req_parsing_start_ts;
	size_t reqbuff_len;
	char *reqbuff;
	size_t reqbuff_read_pos;
	size_t resbuff_len;
	char *resbuff;
	size_t resbuff_write_pos;
};

int set_config_from_file(const kstr_t filename);

#endif
