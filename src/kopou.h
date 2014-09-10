#ifndef __KOPOU_H__
#define __KOPOU_H__

#include <limits.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "kstring.h"
#include "xalloc.h"
#include "aarray.h"
#include "list.h"
#include "kevent.h"
#include "tcp.h"

#define K_OK 0
#define K_ERR -1
#define K_FORCE_USE(v) ((void)v)

#define KOPOU_VERSION ".09"
#define KOPOU_ARCHITECTURE ((sizeof(long) == 8) ? 64 : 32)
#define KOPOU_TCP_NONBLOCK 1
#define KOPOU_MAX_ACCEPT_CONN 1024

#define ADDRESS_LENGTH 17
#define KOPOU_CRON_TIME 100

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
#define REQ_CONTENT_LENGTH_MAX (64 * (1024 * 1024))
#define REQ_BUFFER_SIZE_MAX (2048 + REQ_CONTENT_LENGTH_MAX)
#define RES_WRITTEN_SIZE_MAX (16 * (1024 * 1024))

#define REQ_CONTINUE_IDLE_TIMEOUT (60)
#define KOPOU_CLIENT_KEEPALIVE_TIMEOUT (10 * (60 * 60))

#define REQ_NAME_LENGTH 5

#define KOBJ_TYPE_STRING 0
#define KOBJ_TYPE_BINARY 1
#define KOBJ_TYPE_KSTR 2

#define CONFIG_LINE_LENGTH_MAX 1024

#define KOPOU_DEFAULT_MAX_CONCURRENT_CLIENTS 1024
#define KOPOU_OWN_FDS (32 + VNODE_SIZE)
#define KOPOU_TCP_KEEPALIVE 100

#define CR '\r'
#define NF '\n'
#define BEGIN_PARAM_LEN '#'
#define BEGIN_PARAM_VALUE '$'
#define END_DELIMITER "\r\n"
#define REQ_NORMAL '~'
#define REQ_REPLICA '^'
#define RES_SUCCESS '+'
#define RES_FAIL '-'
#define RES_REDIRECT '>'
#define PARSE_OK 0
#define PARSE_ERR -1


enum {
	KOPOU_CLIENT_TYPE_NORMAL = 0,
	KOPOU_CLIENT_TYPE_INTERNAL,
};

enum {
	KOPOU_REQ_TYPE_NONE = 0,
	KOPOU_REQ_TYPE_NORMAL,
	KOPOU_REQ_TYPE_REPLICA,
	KOPOU_REQ_TYPE_BIN,
	KOPOU_REQ_TYPE_HTTP,
};

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
	int client_tcpkeepalive;
	int client_keepalive;
	int client_keepalive_timeout;
	kstr_t configfile;
};

struct kclient; /* forward declaration */

struct kopou_server {
	pid_t pid;
	kstr_t pidfile;
	time_t current_time;
	int shutdown;
	int listener;
	kevent_loop_t *loop;
	int clistener;
	int mlistener;
	size_t bestmemory;
	int exceedbestmemory;
	int nclients;
	struct kclient **clients;
	struct kclient *curr_client;
};

struct kopou_stats {
	long long objects;
	long long missed;
	long long hits;
	long long deleted;
};


typedef struct kobj {
	void *val;
	size_t len;
	int type;
} kobj_t;

struct req_blueprint {
	kstr_t name;
	int readonly;
	int argc;
	int kindex;
	int cindex;
	void (*action)(struct kclient *client);
};

typedef struct kclient {
	int fd;
	int type;

	kstr_t remoteaddr;
	time_t created_ts;
	time_t last_access_ts;

	int expected_argc;
	int argc;
	kobj_t **argv;

	struct req_blueprint *blueprint;
	int req_type;
	int req_ready_to_process;

	size_t req_parsing_pos;
	size_t reqbuf_len;
	char *reqbuf;
	size_t reqbuf_read_len;

	size_t resbuf_len;
	void *resbuf;
	size_t resbuf_written_pos;

	int disconnect_after_write;
} kclient_t;

extern struct kopou_server kopou;
extern struct kopou_settings settings;
extern struct kopou_stats stats;

/* settings.c */
int set_config_from_file(const kstr_t filename);

/* networks.c */
void kopou_accept_new_connection(int fd, eventtype_t evtype);
void kopou_listener_error(int fd, eventtype_t evtype);

/* parsing.c */
int parse_req(kclient_t *c);

/* reply.c */
void reply_err(kclient_t *c);
void reply_err_protocol(kclient_t *c);
void reply_err_notfound(kclient_t *c);
void reply_err_unknownreq(kclient_t *c);
void reply_ok(kclient_t *c);
void reply_ok_redirect(kclient_t *c);
void reply_ok_write(kclient_t *c);
void reply_ok_write_redirect(kclient_t *c);

struct req_blueprint *get_req_blueprint(kobj_t *o);

#endif
