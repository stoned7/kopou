#include "kstring.h"
#include "xalloc.h"

#define HTTP_VERSION_10 0
#define HTTP_VERSION_11 1


#define CONTENT_TYPE_STRING 0
#define CONTENT_TYPE_LIST 1
#define CONTENT_TYPE_RAW 2

#define CONTENT_ENCODING_STRING 0
#define CONTENT_ENCODING_RAW 1



typedef struct {
	kstr_t name;
	kstr_t value;
} nameval_t;

typedef struct {
	int status;
	kstr_t statusstr;
	int keep_alive;
	size_t content_length;
	kstr_t conetnt_type;
	void *body;
	time_t datetime;

	int nheader;
	namevalue_t **headers;

	size_t _buflen;
	char *_buf;
	size_t _bufwrittenpos;
} http_response_t;


typedef struct {
	kstr_t uri;
	int nsplited_uri;
	kstr_t *splited_uri;

	kstr_t method;
	int version;
	int conn_keep_alive;
	int conn_keep_alive_to;


	size_t content_length;
	kstr_t content_type;
	void *body;

	int nheader;
	namevalue_t **headers;
	int argc;
	namevalue_t **argv;

	http_response_t *response;

	int _state;
	size_t _parsingpos;
	size_t _buflen;
	char *_buf;
	size_t _bufreadlen;

	kstr_t cmdid;
	void *cmd;
} http_request_t;

typedef struct {
	int fd;
	int connection_type;
	time_t creation_ts;
	time_t last_interaction_ts;
	union {
		void *internal;
		http_req_t *http;
	} req;
	int disconnect_after_write;
} kconnection_t;
