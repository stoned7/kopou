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
#define K_AGAIN 1
#define K_FORCE_USE(v) ((void)v)

#define KOPOU_VERSION ".09"
#define KOPOU_ARCHITECTURE ((sizeof(long) == 8) ? 64 : 32)
#define KOPOU_TCP_NONBLOCK 1
#define KOPOU_HTTP_ACCEPT_MAX_CONN 1024

#define ADDRESS_LENGTH 17
#define KOPOU_CRON_INTERVAL 100

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

#define HTTP_REQ_BUFFER_SIZE (1024 << 4) /* 16K */
#define HTTP_REQ_CONTENT_LENGTH_MAX ((1024 << 10) << 6) /* 64 MB */
#define HTTP_REQ_BUFFER_SIZE_MAX (2048 + HTTP_REQ_CONTENT_LENGTH_MAX)
#define HTTP_RES_WRITTEN_SIZE_MAX ((1024 << 10) << 4) /* 16 MB */

#define HTTP_REQ_CONTINUE_IDLE_TIMEOUT (60)
#define HTTP_DEFAULT_KEEPALIVE_TIMEOUT (36000)

#define CONFIG_LINE_LENGTH_MAX 1024

#define HTTP_DEFAULT_MAX_CONCURRENT_CONNS 1024
#define KOPOU_OWN_FDS (32 + (2 * VNODE_SIZE))
#define KOPOU_DEFAULT_TCP_KEEPALIVE_INTERVAL 100

#define HTTP_OK 0
#define HTTP_CONTINUE 1
#define HTTP_ERR -1

#define HTTP_HEADER_MAXLEN 32

#define HTTP_VERSION_9 9
#define HTTP_VERSION_10 1000
#define HTTP_VERSION_11 1001

#define HTTP_METHOD_NOTSUPPORTED 0
#define HTTP_METHOD_HEAD 1
#define HTTP_METHOD_GET 2
#define HTTP_METHOD_PUT 3
#define HTTP_METHOD_POST 4
#define HTTP_METHOD_DELETE 5

#define HTTP_PROXY_CONNECTION "proxy-connection"
#define HTTP_CONNECTION "connection"
#define HTTP_CONTENT_LENGTH "content-length"
#define HTTP_CONTENT_TYPE "content-type"
#define HTTP_TRANSFER_ENCODING "transfer-encoding"
#define HTTP_TE_CHUNKED "chunked"
#define HTTP_CONN_KEEP_ALIVE "keep-alive"
#define HTTP_CONN_CLOSE "close"

#define LF '\n'
#define CR '\r'

#define CONNECTION_TYPE_HTTP 0
#define CONNECTION_TYPE_KOPOU 1

typedef enum {
	parsing_reqline_start = 0,
	parsing_reqline_method,
	parsing_reqline_spaces_before_uri,
	parsing_reqline_schema,
	parsing_reqline_schema_slash,
	parsing_reqline_schema_slash_slash,
	parsing_reqline_host_start,
	parsing_reqline_host,
	parsing_reqline_host_end,
	parsing_reqline_port,
	parsing_reqline_host_http_09,
	parsing_reqline_after_slash_in_uri,
	parsing_reqline_check_uri,
	parsing_reqline_check_uri_http_09,
	parsing_reqline_uri,
	parsing_reqline_http_09,
	parsing_reqline_http_H,
	parsing_reqline_http_HT,
	parsing_reqline_http_HTT,
	parsing_reqline_http_HTTP,
	parsing_reqline_first_major_digit,
	parsing_reqline_major_digit,
	parsing_reqline_first_minor_digit,
	parsing_reqline_minor_digit,
	parsing_reqline_spaces_after_digit,
	parsing_reqline_almost_done,
	parsing_reqline_done,
	parsing_header_start,
        parsing_header_name,
        parsing_header_space_before_value,
        parsing_header_value,
        parsing_header_space_after_value,
	parsing_header_line_almost_done,
	parsing_header_line_done,
        parsing_header_almost_done,
	parsing_header_done,
	parsing_body_start,
	parsing_body_done
} parsing_state_t;


typedef struct {
	void *val;
	kstr_t content_type;
	size_t size;
	unsigned long long version;
	unsigned type:4;
	unsigned encoding:4;
} kobj_t;

typedef struct knamevalue {
	kstr_t name;
	kstr_t value;
	struct knamevalue *next;
} knamevalue_t;

typedef struct kbuffer {
	unsigned char *pos;
	unsigned char *last;
	unsigned char *start;
	unsigned char *end;
	struct kbuffer *next;
} kbuffer_t;

typedef struct {
	void *req;
	time_t connection_ts;
	time_t last_interaction_ts;
	int fd;
	unsigned connection_type:1;
	unsigned disconnect_after_reply:1;
} kconnection_t;

typedef struct {
	kstr_t name;
	void (*action)(kconnection_t *client);
	int readonly;
	int argc;
	int kindex;
	int cindex;
} kcommand_t;

typedef struct {
	kbuffer_t *buf;
	kbuffer_t *curbuf;
	kstr_t *headers;
	int nheaders;
	unsigned char flag;
} khttp_response_t;

typedef struct {
	kcommand_t *cmd;
	khttp_response_t *res;
	kbuffer_t *buf;
	kbuffer_t *curbuf;

	unsigned char *request_start;
	unsigned char *request_end;
	unsigned char *method_end;
	unsigned char *schema_start;
	unsigned char *schema_end;
	unsigned char *host_start;
	unsigned char *host_end;
	unsigned char *port_end;
	unsigned char *uri_start;
	unsigned char *args_start;
	unsigned char *uri_end;
	unsigned char *uri_ext;
	unsigned char *http_start;

	unsigned char *header_start;
	unsigned char *header_name_start;
	unsigned char *header_name_end;
	unsigned char *header_value_start;
	unsigned char *header_value_end;
	unsigned char *header_end;

	unsigned char *body;

	kstr_t *splitted_uri;
	knamevalue_t *headers;
	int nsplitted_uri;

	size_t content_length;
	kstr_t content_type;
	parsing_state_t _parsing_state;

	time_t timestamp;
	unsigned body_end_index;

	unsigned connection_keepalive_timeout:16;
	unsigned http_version:16;
	unsigned major_version:4;
	unsigned minor_version:4;
	unsigned method:4;
	unsigned connection_keepalive:1;
	unsigned connection_close:1;
	unsigned transfer_encoding_chunked:1;
	unsigned complex_uri:1;
	unsigned quoted_uri:1;
	unsigned plus_in_uri:1;
	unsigned space_in_uri:1;
} khttp_request_t;

struct kopou_settings {
	kstr_t cluster_name;
	kstr_t address;
	kstr_t logfile;
	kstr_t configfile;
	kstr_t dbdir;
	kstr_t dbfile;
	kstr_t workingdir;
	int http_max_ccur_conns;
	int tcp_keepalive;
	int http_keepalive;
	int port;
	int kport;
	int background;
	int verbosity;
	int http_keepalive_timeout;
	int http_close_connection_onerror;
};

struct kopou_stats {
	long long objects;
	long long missed;
	long long hits;
	long long deleted;
};

struct kopou_server {
	kstr_t pidfile;
	kevent_loop_t *loop;
	kconnection_t *curr_conn;
	kconnection_t **conns;
	int nconns;

	pid_t pid;
	size_t bestmemory;
	int exceedbestmemory;

	time_t current_time;
	int shutdown;
	int hlistener;
	int ilistener;
};

extern struct kopou_server kopou;
extern struct kopou_settings settings;
extern struct kopou_stats stats;

/* settings.c */
int settings_from_file(const kstr_t filename);

/* networks.c */
void http_accept_new_connection(int fd, eventtype_t evtype);
void http_listener_error(int fd, eventtype_t evtype);

/* http.c */
int http_parse_request_line(kconnection_t *conn);
int http_parse_header_line(kconnection_t *conn);
int http_parse_contentlength_body(kconnection_t *c);
int http_parse_chunked_body(kconnection_t *c);

/* reply.c */
void reply_400(kconnection_t *c); //bad request
void reply_413(kconnection_t *c); //too large
void reply_404(kconnection_t *c); //not found
void reply_411(kconnection_t *c); //length required

void reply_500(kconnection_t *c); //internal server err
void reply_501(kconnection_t *c); //not implemented
void reply_503_now(kbuffer_t *b); //service unavailable
void reply_505(kconnection_t *c); //HTTP Version Not Supported

void reply_200(kconnection_t *c); //ok
void reply_201(kconnection_t *c); //created
void reply_301(kconnection_t *c); //Move Permanently
void reply_302(kconnection_t *c); //Found

struct req_blueprint *get_req_blueprint(kobj_t *o);

static inline void get_http_date(char *buf, size_t len)
{
	struct tm *tm = gmtime(&kopou.current_time);
	strftime(buf, len, "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", tm);
}

static inline void get_http_server_str(char *buf, size_t len)
{
	snprintf(buf, len, "Server: kopou v%s %d bits, -cluster[%d]\r\n",
			KOPOU_VERSION, KOPOU_ARCHITECTURE, VNODE_SIZE);
}

#endif
