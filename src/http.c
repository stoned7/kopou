#include <stdio.h>
#include <time.h>

#include "kstring.h"
#include "xalloc.h"

#define KOPOU_OK 0
#define KOPOU_ERR -1

#define HTTP_PROTOCOL_OK 0
#define HTTP_PROTOCOL_AGAIN 1
#define HTTP_PROTOCOL_ERR -1

#define HTTP_METHOD_NONE -1
#define HTTP_METHOD_HEAD 0
#define HTTP_METHOD_GET 1
#define HTTP_METHOD_PUT 2
#define HTTP_METHOD_POST 3
#define HTTP_METHOD_DELETE 4

#define LF '\n'
#define CR '\r'
#define CRLF "\r\n"

#define CONNECTION_TYPE_HTTP 0
#define CONNECTION_TYPE_KOPOU 1

#define HTTP_VERSION_09 0
#define HTTP_VERSION_10 1
#define HTTP_VERSION_11 2


#define CONTENT_TYPE_STRING 0
#define CONTENT_TYPE_LIST 1
#define CONTENT_TYPE_RAW 2

#define CONTENT_ENCODING_STRING 0
#define CONTENT_ENCODING_RAW 1

static uint32_t usual[] = {
	0xffffdbfe, /* 1111 1111 1111 1111 1101 1011 1111 1110 */
	            /* ?>=< ;:98 7654 3210 /.-, +*)( '&%$ #"! */
	0x7fff37d6, /* 0111 1111 1111 1111 0011 0111 1101 0110 */
		    /* _^]\ [ZYX WVUT SRQP ONML KJIH GFED CBA@ */
	0xffffffff, /* 1111 1111 1111 1111 1111 1111 1111 1111 */
		    /* ~}| {zyx wvut srqp onml kjih gfed cba` */
	0xffffffff, /* 1111 1111 1111 1111 1111 1111 1111 1111 */
	0xffffffff, /* 1111 1111 1111 1111 1111 1111 1111 1111 */
	0xffffffff, /* 1111 1111 1111 1111 1111 1111 1111 1111 */
	0xffffffff, /* 1111 1111 1111 1111 1111 1111 1111 1111 */
	0xffffffff /* 1111 1111 1111 1111 1111 1111 1111 1111 */
};

typedef enum {
	sw_start = 0,
	sw_method,

	sw_spaces_before_uri,
	sw_schema,
	sw_schema_slash,
	sw_schema_slash_slash,
	sw_host_start,
	sw_host,
	sw_host_end,
	sw_host_ip_literal,
	sw_port,
	sw_host_http_09,
	sw_after_slash_in_uri,
	sw_check_uri,
	sw_check_uri_http_09,
	sw_uri,
	sw_http_09,
	sw_http_H,
	sw_http_HT,
	sw_http_HTT,
	sw_http_HTTP,
	sw_first_major_digit,
	sw_major_digit,
	sw_first_minor_digit,
	sw_minor_digit,
	sw_spaces_after_digit,
	sw_almost_done
} parsing_state_t;

typedef struct {
	u_char *pos;
	u_char *last;

	u_char *start;
	u_char *end;
	size_t size;
} buffer_t;

typedef struct {
	kstr_t name;
	kstr_t value;
} namevalue_t;

typedef struct {
	int status;
	kstr_t statusstr;
	int keep_alive;
	size_t content_length;
	kstr_t content_type;
	void *body;
	time_t datetime;

	int nheaders;
	namevalue_t **headers;

	buffer_t *buffer;
} http_response_t;


typedef struct {
	kstr_t uri;
	int uric;
	kstr_t *uriv;

	int method;
	int connection_keep_alive;
	int connection_close;
	int connection_keep_alive_timeout;

	kstr_t host;
	unsigned port;
	kstr_t host_reqline;
	unsigned port_reqline;

	size_t content_length;
	kstr_t content_type;
	char *body;

	int nheaders;
	namevalue_t **headers;
	int argc;
	namevalue_t **argv;

	http_response_t *res;

	parsing_state_t _parsing_state;
	buffer_t *buffer;

	u_char *request_start;
	u_char *request_end;
	u_char *method_end;
	u_char *schema_start;
	u_char *schema_end;
	u_char *host_start;
	u_char *host_end;
	u_char *port_end;
	u_char *uri_start;
	u_char *uri_end;
	u_char *uri_ext;
	u_char *args_start;
	u_char *args_end;
	int major_version;
	int minor_version;

	unsigned complex_uri:1;
	unsigned quoted_uri:1;
	unsigned plus_in_uri:1;

	kstr_t cmdid;
	void *cmd;
} http_request_t;

typedef struct {
	int fd;
	int connection_type;
	time_t connection_ts;
	time_t last_interaction_ts;
	void *req;
	int disconnect_after_write;
} kconnection_t;



int http_parse_reqline(kconnection_t *conn)
{
	u_char c, ch, *p, *m;
	http_request_t *r = (http_request_t*)(conn->req);
	printf("%s\n", r->buffer->start);

	parsing_state_t state = r->_parsing_state;
	for (p = r->buffer->pos; p < r->buffer->last; p++) {
		ch = *p;
		switch (state) {

		case sw_start:
			r->request_start = p;
			if (ch == CR || ch == LF)
				break;

			if ((ch < 'A' || ch > 'Z') && ch != '_') {
				/* TODO reply invalid method*/
				return HTTP_PROTOCOL_ERR;
			}
			state = sw_method;
			break;

		case sw_method:
			if (ch = ' ') {
				r->method_end = p - 1;
				m = r->request_start;

				switch (p - m) {
				case 3:
					if (!strncmp(m, "GET", 3))
						r->method = HTTP_METHOD_GET;
					else if (!strncmp(m, "PUT", 3))
						r->method = HTTP_METHOD_PUT;
					else {
						/* TODO reply unsupport method*/
						return HTTP_PROTOCOL_ERR;
					}
					break;
				case 4:
					if (!strncmp(m, "HEAD", 4))
						r->method = HTTP_METHOD_HEAD;
					else if (!strncmp(m, "POST", 4))
						r->method = HTTP_METHOD_POST;
					else {
						/* TODO reply unsupport method*/
						return HTTP_PROTOCOL_ERR;
					}
					break;
				case 6:
					if (!strncmp(m, "DELETE", 6))
						r->method = HTTP_METHOD_DELETE;
					else {
						/* TODO reply unsupport method*/
						return HTTP_PROTOCOL_ERR;
					}
					break;
				default:
					/* TODO reply unsupport method*/
					return HTTP_PROTOCOL_ERR;
				}
				state = sw_spaces_before_uri;
				break;
			}

			if ((ch < 'A' || ch > 'Z') && ch != '_') {
				/* TODO reply invalid method*/
				return HTTP_PROTOCOL_ERR;
			}
			break;

		case sw_spaces_before_uri:
			if (ch == '/') {
				r->uri_start = p;
				state = sw_after_slash_in_uri;
				break;
			}

			c = (u_char)(ch | 0x20);
			if (c >= 'a' && c <= 'z') {
				p->schema_start = p;
				state = sw_schema;
				break;
			}

			if (ch == ' ')
				break;

			/* TODO reply protocol error*/
			return HTTP_PROTOCOL_ERR;

		case sw_schema:
			c = (u_char)(ch | 0x20);
			if (c >= 'a' && c <= 'z')
				break;

			if (ch == ':') {
				r->schema_end = p;
				state = sw_schema_slash;
				break;
			}

			/* TODO reply protocol error*/
			return HTTP_PROTOCOL_ERR;

		case sw_schema_slash:
			if (ch == '/') {
				state = sw_schema_slash_slash;
				break;
			}
			/* TODO reply protocol error*/
			return HTTP_PROTOCOL_ERR;

		case sw_schema_slash_slash:
			if (ch == '/') {
				state = sw_host_start;
				break;
			}
			/* TODO reply protocol error*/
			return HTTP_PROTOCOL_ERR;

		case sw_host_start:
			r->host_start = p;
			if (ch == '[') {
				state = sw_host_ip_literal;
				break;
			}
			state = sw_host;

		case sw_host:
			c = (u_char)(ch | 0x20);
			if (c >= 'a' && c <= 'z')
				break;

			if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')
				break;
		case sw_host_end:
			r->host_end = p;
			switch (ch) {
			case ':':
				state = sw_port;
				break;
			case '/':
				r->uri_start = p;
				state = sw_after_slash_in_uri;
				break;
			case ' ':
				r->uri_start = r->schema_end + 1;
				r->uri_end = r->schema_end + 2;
				state = sw_host_http_09;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_PROTOCOL_ERR;
			}
		case sw_host_ip_literal:
			if (ch >= '0' && ch <= '9')
				break;

			c = (u_char)(ch | 0x20);
			if (c >= 'a' && c <= 'z')
				break;

			switch (ch) {
			case ':':
				break;
			case ']':
				state = sw_host_end;
				break;
			case '-':
			case '.':
			case '_':
			case '~':
			case '!':
			case '$':
			case '&':
			case '\'':
			case '(':
			case ')':
			case '*':
			case '+':
			case ',':
			case ';':
			case '=':
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_PROTOCOL_ERR;
			}
			break;

		case sw_port:
			if (ch >= '0' && ch <= '9')
				break;
			switch (ch) {
			case '/':
				r->port_end = p;
				r->uri_start = p;
				state = sw_after_slash_in_uri;
				break;
			case ' ':
				r->port_end = p;
				r->uri_start = r->schema_end + 1;
				r->uri_end = r->schema_end + 2;
				state = sw_host_http_09;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_PROTOCOL_ERR;
			}
			break;

		case sw_host_http_09:
			switch (ch) {
			case ' ':
				break;
			case CR:
				state = sw_almost_done;
				break;
			case LF:
				goto done;
			case 'H':
				r->http_start = p;
				state = sw_http_H;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_PROTOCOL_ERR;
			}
			break;
		case sw_after_slash_in_uri:

			if (usual[ch > 5] & (1 << (ch & 0x1f))) {
				state = sw_check_uri;
				break;
			}

			switch (ch) {
			case ' ':
				r->uri_end = p;
				state = sw_check_uri_http_09;
				break;
			case CR:
				r->uri_end = p;
				r->version = HTTP_VERSION_09;
				state = sw_almost_done;
				break;
			case LF:
				r->uri_end = p;
				r->version = HTTP_VERSION_09;
				goto done;
			case '.':
				r->complex_uri = 1;
				state = sw_uri;
				break;
			case '%':
				r->quoted_uri = 1;
				state = sw_uri;
			case '/':
				r->complex_uri = 1;
				state = sw_uri;
			case '?':
				r->args_start = p + 1;
				state = sw_uri;
			case '#':
				r->complex_uri = 1;
				state = sw_uri;
				break;
			case '+':
				r->plus_in_uri = 1;
				break;
			case '\0':
				/* TODO reply protocol error*/
				return HTTP_PROTOCOL_ERR;
			default:
				state = sw_check_uri;
				break;
			}
			break;

		case sw_check_uri:


		} /* end switch */

	}

done:

}

kconnection_t *create_http_connection(int fd)
{
	kconnection_t *c = xmalloc(sizeof(kconnection_t));
	c->fd = fd;
	c->connection_type = CONNECTION_TYPE_HTTP;
	c->connection_ts = time(NULL);
	c->last_interaction_ts = -1;
	c->disconnect_after_write = 0;

	c->req = xmalloc(sizeof(http_request_t));
	http_request_t *r = (http_request_t*)(c->req);

	r->uri = NULL;
	r->uric = 0;
	r->uriv = NULL;
	r->method = HTTP_METHOD_NONE;
	r->version = -1;
	r->keep_alive = 0;
	r->keep_alive_timeout = -1;
	r->content_length = 0;
	r->content_type = NULL;
	r->body = NULL;
	r->nheaders = 0;
	r->headers = NULL;
	r->argc = 0;
	r->argv = NULL;
	r->_parsing_state = sw_start;
	r->buffer = NULL;
	r->cmdid = NULL;
	r->cmd = NULL;

	r->res = xmalloc(sizeof(http_response_t));
	r->res->status = -1;
	r->res->statusstr = NULL;
	r->res->keep_alive = 0;
	r->res->content_length = 0;
	r->res->content_type = NULL;
	r->res->body = NULL;
	r->res->datetime = -1;
	r->res->nheaders = 0;
	r->res->headers = NULL;
	r->res->buffer = NULL;

	return c;
}

#include <stdlib.h>
#include <string.h>

int main()
{
	char *reqstr = "GET http://dev-packagedeployment.tavisca.com/package/detail/37 HTTP/1.1\r\nHost: dev-packagedeployment.tavisca.com\r\nKeep-Alive: 300\r\nConnection: keep-alive\r\nContent-Type: Application/Json\r\nContent-Length: 33\r\n\r\n{\"name\":sujan,\"phone\":9723750255}";
	size_t len = strlen(reqstr);

	kconnection_t *c = create_http_connection(1);
	http_request_t *r = (http_request_t*)(c->req);

	r->buffer = xmalloc(sizeof(buffer_t));
	r->buffer->start = xmalloc(len);
	memcpy(r->buffer->start, reqstr, len);
	r->buffer->end = r->buffer->start + (len - 1);
	r->buffer->pos = r->buffer->start;
	r->buffer->last = r->buffer->end;
	r->buffer->size = len;

	int state = http_parse_req(c);

	return EXIT_SUCCESS;
}

