#include <stdio.h>
#include <time.h>
#include <stdint.h>

#include "kstring.h"
#include "xalloc.h"

#define KOPOU_OK 0
#define KOPOU_ERR -1

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

#define LF '\n'
#define CR '\r'

#define CONNECTION_TYPE_HTTP 0
#define CONNECTION_TYPE_KOPOU 1


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
	parsing_reqline_start = 0,
	parsing_reqline_method,
	parsing_reqline_spaces_before_uri,
	parsing_reqline_schema,
	parsing_reqline_schema_slash,
	parsing_reqline_schema_slash_slash,
	parsing_reqline_host_start,
	parsing_reqline_host,
	parsing_reqline_host_end,
	parsing_reqline_host_ip_literal,
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
	parsing_header_done

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
	unsigned valid:1;
} namevalue_t;

typedef struct {
	int status;
	kstr_t statusstr;
	size_t content_length;
	kstr_t content_type;
	void *body;

	int nheaders;
	namevalue_t **headers;

	buffer_t *buffer;
} http_response_t;


typedef struct {
	kstr_t uri;
	int uric;
	kstr_t *uriv;

	unsigned method:4;
	unsigned connection_keepalive:1;
	unsigned connection_close:1;

	unsigned connection_keepalive_timeout:16;
	unsigned http_version:16;

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
	u_char *args_start;
	u_char *uri_end;
	u_char *uri_ext;
	u_char *http_start;
	unsigned major_version:4;
	unsigned minor_version:4;
	unsigned complex_uri:1;
	unsigned quoted_uri:1;
	unsigned plus_in_uri:1;
	unsigned space_in_uri:1;

	u_char *header_start;
	u_char *header_name_start;
	u_char *header_name_end;
	u_char *header_value_start;
	u_char *header_value_end;
	u_char *header_end;

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



int http_parse_requestline(kconnection_t *conn)
{
	u_char c, ch, *p, *m;
	http_request_t *r = (http_request_t*)(conn->req);
	printf("\n%s\n", r->buffer->start);

	parsing_state_t state = r->_parsing_state;
	for (p = r->buffer->pos; p <= r->buffer->last; p++) {
		ch = *p;
		switch (state) {

		case parsing_reqline_start:
			r->request_start = p;
			if (ch == CR || ch == LF)
				break;

			if (ch < 'A' || ch > 'Z') {
				/* TODO reply invalid method*/
				return HTTP_ERR;
			}
			state = parsing_reqline_method;
			break;

		case parsing_reqline_method:
			if (ch == ' ') {
				r->method_end = p;
				m = r->request_start;

				switch (p - m) {
				case 3:
					if (!strncmp(m, "GET", 3)) {
						r->method = HTTP_METHOD_GET;
						break;
					} else if (!strncmp(m, "PUT", 3)) {
						r->method = HTTP_METHOD_PUT;
						break;
					} else {
						/* TODO reply unsupport method*/
						return HTTP_ERR;
					}
				case 4:
					if (!strncmp(m, "HEAD", 4)) {
						r->method = HTTP_METHOD_HEAD;
						break;
					} else if (!strncmp(m, "POST", 4)) {
						r->method = HTTP_METHOD_POST;
						break;
					} else {
						/* TODO reply unsupport method*/
						return HTTP_ERR;
					}
				case 6:
					if (!strncmp(m, "DELETE", 6)) {
						r->method = HTTP_METHOD_DELETE;
						break;
					}
					else {
						/* TODO reply unsupport method*/
						return HTTP_ERR;
					}
				default:
					/* TODO reply unsupport method*/
					return HTTP_ERR;
				}
				state = parsing_reqline_spaces_before_uri;
				break;
			}

			if ((ch < 'A' || ch > 'Z') && ch != '_') {
				/* TODO reply invalid method*/
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_spaces_before_uri:

			if (ch == '/') {
				r->uri_start = p;
				state = parsing_reqline_after_slash_in_uri;
				break;
			}

			c = (u_char)(ch | 0x20);
			if (c >= 'a' && c <= 'z') {
				r->schema_start = p;
				state = parsing_reqline_schema;
				break;
			}

			if (ch == ' ')
				break;

			/* TODO reply protocol error*/
			return HTTP_ERR;

		case parsing_reqline_schema:

			c = (u_char)(ch | 0x20);
			if (c >= 'a' && c <= 'z')
				break;

			if (ch == ':') {
				r->schema_end = p;
				state = parsing_reqline_schema_slash;
				break;
			}

			/* TODO reply protocol error*/
			return HTTP_ERR;

		case parsing_reqline_schema_slash:
			if (ch == '/') {
				state = parsing_reqline_schema_slash_slash;
				break;
			}
			/* TODO reply protocol error*/
			return HTTP_ERR;

		case parsing_reqline_schema_slash_slash:
			if (ch == '/') {
				state = parsing_reqline_host_start;
				break;
			}
			/* TODO reply protocol error*/
			return HTTP_ERR;

		case parsing_reqline_host_start:
			r->host_start = p;
			state = parsing_reqline_host;

		case parsing_reqline_host:
			c = (u_char)(ch | 0x20);
			if (c >= 'a' && c <= 'z')
				break;

			if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')
				break;

		case parsing_reqline_host_end:
			r->host_end = p;
			switch (ch) {
			case ':':
				state = parsing_reqline_port;
				break;
			case '/':
				r->uri_start = p;
				state = parsing_reqline_after_slash_in_uri;
				break;
			case ' ':
				r->uri_start = r->schema_end + 1;
				r->uri_end = r->schema_end + 2;
				state = parsing_reqline_host_http_09;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;
		case parsing_reqline_port:
			if (ch >= '0' && ch <= '9')
				break;

			switch (ch) {
			case '/':
				r->port_end = p;
				r->uri_start = p;
				state = parsing_reqline_after_slash_in_uri;
				break;
			case ' ':
				r->port_end = p;
				r->uri_start = r->schema_end + 1;
				r->uri_end = r->schema_end + 2;
				state = parsing_reqline_host_http_09;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_host_http_09:
			switch (ch) {
			case ' ':
				break;
			case CR:
				r->minor_version = 9;
				state = parsing_reqline_almost_done;
				break;
			case LF:
				r->minor_version = 9;
				goto done;
			case 'H':
				r->http_start = p;
				state = parsing_reqline_http_H;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_after_slash_in_uri:

			if (usual[ch >> 5] & (1 << (ch & 0x1f))) {
				state = parsing_reqline_check_uri;
				break;
			}

			switch (ch) {
			case ' ':
				r->uri_end = p;
				state = parsing_reqline_check_uri_http_09;
				break;
			case CR:
				r->uri_end = p;
				r->minor_version = 9;
				state = parsing_reqline_almost_done;
				break;
			case LF:
				r->uri_end = p;
				r->minor_version = 9;
				goto done;
			case '.':
				r->complex_uri = 1;
				state = parsing_reqline_uri;
				break;
			case '%':
				r->quoted_uri = 1;
				state = parsing_reqline_uri;
				break;
			case '/':
				r->complex_uri = 1;
				state = parsing_reqline_uri;
				break;
			case '?':
				r->args_start = p + 1;
				state = parsing_reqline_uri;
				break;
			case '#':
				r->complex_uri = 1;
				state = parsing_reqline_uri;
				break;
			case '+':
				r->plus_in_uri = 1;
				break;
			case '\0':
				/* TODO reply protocol error*/
				return HTTP_ERR;
			default:
				state = parsing_reqline_check_uri;
				break;
			}
			break;

		case parsing_reqline_check_uri:

			if (usual[ch >> 5] & (1 << (ch & 0x1f)))
				break;

			switch (ch) {
			case '/':
				r->uri_ext = NULL;
				state = parsing_reqline_after_slash_in_uri;
				break;
			case '.':
				r->uri_ext = p + 1;
				break;
			case ' ':
				r->uri_end = p;
				state = parsing_reqline_check_uri_http_09;
				break;
			case CR:
				r->uri_end = p;
				r->minor_version = 9;
				state = parsing_reqline_almost_done;
				break;
			case LF:
				r->uri_end = p;
				r->minor_version = 9;
				goto done;
			case '%':
				r->quoted_uri = 1;
				state = parsing_reqline_uri;
				break;
			case '?':
				r->args_start = p + 1;
				state = parsing_reqline_uri;
				break;
			case '#':
				r->complex_uri = 1;
				state = parsing_reqline_uri;
				break;
			case '+':
				r->plus_in_uri = 1;
				break;
			case '\0':
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_check_uri_http_09:
			switch (ch) {
			case ' ':
				break;
			case CR:
				r->minor_version = 9;
				state = parsing_reqline_almost_done;
				break;
			case LF:
				r->minor_version = 9;
				goto done;
			case 'H':
				state = parsing_reqline_http_H;
				r->http_start = p;
				break;
			default:
				r->space_in_uri = 1;
				state = parsing_reqline_check_uri;
				p--;
			}
			break;

		case parsing_reqline_uri:
			if (usual[ch >> 5] & (1 << (ch & 0x1f)))
				break;

			switch (ch) {
			case ' ':
				r->uri_end = p;
				state = parsing_reqline_http_09;
				break;
			case CR:
				r->uri_end = p;
				r->minor_version = 9;
				state = parsing_reqline_almost_done;
				break;
			case LF:
				r->uri_end = p;
				r->minor_version = 9;
				state = parsing_reqline_almost_done;
				break;
			case '#':
				r->complex_uri = 1;
				break;
			case '\0':
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_http_09:
			switch (ch) {
			case ' ':
				break;
			case CR:
				r->minor_version = 9;
				state = parsing_reqline_almost_done;
				break;
			case LF:
				r->minor_version = 9;
				goto done;
			case 'H':
				r->http_start = p;
				state = parsing_reqline_http_H;
				break;
			default:
				r->space_in_uri = 1;
				state = parsing_reqline_uri;
				p--;
				break;
			}
			break;

		case parsing_reqline_http_H:
			switch (ch) {
			case 'T':
				state = parsing_reqline_http_HT;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;
		case parsing_reqline_http_HT:
			switch (ch) {
			case 'T':
				state = parsing_reqline_http_HTT;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;
		case parsing_reqline_http_HTT:
			switch (ch) {
			case 'P':
				state = parsing_reqline_http_HTTP;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_http_HTTP:
			switch (ch) {
			case '/':
				state = parsing_reqline_first_major_digit;
				break;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_first_major_digit:
			if (ch < '1' || ch > '9') {
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			r->major_version = ch - '0';
			state = parsing_reqline_major_digit;
			break;

		case parsing_reqline_major_digit:
			if (ch == '.') {
				state = parsing_reqline_first_minor_digit;
				break;
			}

			if (ch < '0' || ch > '9') {
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}

			r->major_version = r->major_version * 10 + ch - '0';
			break;
		case parsing_reqline_first_minor_digit:
			if (ch < '0' || ch > '9') {
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			r->minor_version = ch - '0';
			state = parsing_reqline_minor_digit;
			break;
		case parsing_reqline_minor_digit:

			if (ch == CR) {
				state = parsing_reqline_almost_done;
				break;
			}

			if (ch == LF)
				goto done;

			if (ch == ' ') {
				state = parsing_reqline_spaces_after_digit;
				break;
			}

			if (ch < '0' || ch > '9'){
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			r->minor_version = r->minor_version * 10 + ch - '0';
			break;

		case parsing_reqline_spaces_after_digit:
			switch (ch) {
			case ' ':
				break;
			case CR:
				state = parsing_reqline_almost_done;
				break;
			case LF:
				goto done;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}
			break;
		case parsing_reqline_almost_done:
			r->request_end = p - 1;
			switch (ch) {
			case LF:
				goto done;
			default:
				/* TODO reply protocol error*/
				return HTTP_ERR;
			}

		} /* end switch */

	}

	r->buffer->pos = p;
	r->_parsing_state = state;
	return HTTP_CONTINUE;

done:
	r->buffer->pos = p + 1;
	if (r->request_end == NULL)
		r->request_end = p;
	r->_parsing_state = parsing_reqline_done;

	r->http_version = r->major_version * 1000 + r->minor_version;

	if (r->http_version == 9 && r->method != HTTP_METHOD_GET) {
		/* TODO reply protocol error*/
		return HTTP_ERR;
	}

	return HTTP_OK;

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
	r->method = HTTP_METHOD_NOTSUPPORTED;
	r->http_version = 0;
	r->connection_keepalive = 0;
	r->connection_keepalive_timeout = 0;
	r->content_length = 0;
	r->content_type = NULL;
	r->body = NULL;
	r->nheaders = 0;
	r->headers = NULL;
	r->argc = 0;
	r->argv = NULL;
	r->_parsing_state = parsing_reqline_start;
	r->buffer = NULL;
	r->cmdid = NULL;
	r->cmd = NULL;

	r->res = xmalloc(sizeof(http_response_t));
	r->res->status = 0;
	r->res->statusstr = NULL;
	r->res->content_length = 0;
	r->res->content_type = NULL;
	r->res->body = NULL;
	r->res->nheaders = 0;
	r->res->headers = NULL;
	r->res->buffer = NULL;

	return c;
}

static u_char  lowcase[] =
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";


int http_parse_header_line(kconnection_t *conn)
{
	u_char c, ch, *p;

	http_request_t *r = (http_request_t*)(conn->req);
	//printf("\nparsing header start:\n%s\n", r->buffer->pos);

	if (r->_parsing_state == parsing_reqline_done) {
		r->header_start = r->buffer->pos;
		r->_parsing_state = parsing_header_start;
	} else if (r->_parsing_state == parsing_header_line_done) {
		r->_parsing_state = parsing_header_start;
	}

	parsing_state_t state = r->_parsing_state;

	for (p = r->buffer->pos; p <= r->buffer->last; p++) {
		ch = *p;

		switch (state) {

		case parsing_header_start:

			switch (ch) {
			case CR:
				r->header_end = p;
				state = parsing_header_almost_done;
				break;
			case LF:
				r->header_end = p;
				state = parsing_header_done;
				goto header_done;
			default:
				r->header_name_start = p;
				state = parsing_header_name;
				c = lowcase[ch];
				if (c) break;

				if (ch == '\0') {
					/* response err message */
					return HTTP_ERR;
				}
			}
			break;

		case parsing_header_name:

			c = lowcase[ch];
			if (c) break;

			if (ch == ':') {
				r->header_name_end = p;
				state = parsing_header_space_before_value;
				break;
			}

			if (ch == CR) {
				r->header_name_end = p;
				state = parsing_header_line_almost_done;
				break;
			}

			if (ch == LF) {
				r->header_name_end = p;
				state = parsing_header_line_done;
				goto header_done;
			}

			/* response err message */
			return HTTP_ERR;

		case parsing_header_space_before_value:

			switch (ch) {
			case ' ':
				break;
			case CR:
				r->header_value_start = p;
				r->header_value_end = p;
				state = parsing_header_line_almost_done;
				break;
			case LF:
				r->header_value_start = p;
				r->header_value_end = p;
				state = parsing_header_line_done;
				goto header_done;
			case '\0':
				/* response err message */
				return HTTP_ERR;
			default:
				r->header_value_start = p;
				state = parsing_header_value;
				break;
			}
			break;

		case parsing_header_value:
			switch (ch) {
			case ' ':
				state = parsing_header_space_after_value;
				break;
			case CR:
				state = parsing_header_line_almost_done;
				r->header_value_end = p;
				break;
			case LF:
				r->header_value_end = p;
				state = parsing_header_line_done;
				goto header_done;
			case '\0':
				/* response err message */
				return HTTP_ERR;
			default:
				break;
			}
			break;

		case parsing_header_space_after_value:
			switch (ch) {
			case ' ':
				break;
			case CR:
				state = parsing_header_almost_done;
				r->header_value_end = p;
				break;
			case LF:
				r->header_value_end = p;
				state = parsing_header_line_done;
				goto header_done;
			case '\0':
				/* response err message */
				return HTTP_ERR;
			default:
				state = parsing_header_value;
				break;
			}
			break;

		case parsing_header_line_almost_done:
			switch (ch) {
			case LF:
				state = parsing_header_line_done;
				goto header_done;
			case CR:
				break;
			default:
				/* response err message */
				return HTTP_ERR;
			}
			break;

		case parsing_header_almost_done:
			switch (ch) {
			case LF:
				state = parsing_header_done;
				goto header_done;
			default:
				/* response err message */
				return HTTP_ERR;
			}
			break;

		} /* state switch */
	} /* for end */

	r->buffer->pos = p;
	r->_parsing_state = state;
	return HTTP_CONTINUE;

header_done:
	r->buffer->pos = p + 1;
	r->_parsing_state = state;
	return HTTP_OK;
}

#include <stdlib.h>
#include <string.h>

int main()
{
	char *req = "GET /package/detail/37?abc=xyz&123 HTTP/1.1\r\nHost: dev-packagedeployment.tavisca.com\r\nConnection:keep-alive\r\nCache-Control: max-age=0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/37.0.2062.103 Safari/537.36\r\nAccept-Encoding: gzip,deflate,sdch\r\nAccept-Language: en-US,en;q=0.8\r\nCookie: ASP.NET_SessionId=t3lve43o5fzii2xwji53anuc\r\nContent-Length: 67890\r\nContent-Type: application/json, abcd\r\nKeep-Alive: 300\r\n\r\n";

	int reqlen = strlen(req) + 1;

	kconnection_t *c = create_http_connection(1);

	http_request_t *r = (http_request_t*)(c->req);
	r->buffer = xmalloc(sizeof(buffer_t));
	r->buffer->start = xmalloc(reqlen);
	memcpy(r->buffer->start, req, reqlen);

	r->buffer->end = r->buffer->start + (reqlen - 1);
	r->buffer->pos = r->buffer->start;
	r->buffer->last = r->buffer->end;
	r->buffer->size = reqlen;

	int re = http_parse_requestline(c);
	printf("result: %d, %d\n", re, r->space_in_uri);
	int i = 0;
	if (re == HTTP_OK) {

		if (r->_parsing_state == parsing_reqline_done) {
			do {
				i++;
				re = http_parse_header_line(c);
				printf("header result: %d, %d\n", re, r->_parsing_state);
			} while (r->_parsing_state != parsing_header_done);

		}
		/*
		size_t len1 = (r->args_start == NULL)
			? ((r->uri_end) - (r->uri_start + 1))
			: ((r->args_start -1) - (r->uri_start + 1));
		kstr_t uri = _kstr_create(r->uri_start + 1, len1);
		kstr_t *tokens;
		int uric = kstr_tok(uri, "/", &tokens);
		int i;
		for (i = 0; i < uric; i++)
			printf("%s\n", tokens[i]);
		*/
	}


	/*

	char *reqlines[] = { "GET / HTTP/1.0\r\n",
			   "GET /package/detail/37 HTTP/1.1\r\n",
			   "GET /package HTTP/1.1\r\n",
			   "GET /package/detail/37?abc=xyz&123 HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com/package/detail/37 HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com/package HTTP/1.1\r\n",
			   "GET http://www.google.com/package/detail/37 HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com:8888/package/detail/37 HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com:8888/ HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com/ HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com:8080 HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com/?aaa=bbb&ccc=ddd HTTP/1.1\r\n",
			   "GET http://dev-packagedeployment.tavisca.com/hello world/ram?aaa=bbb&ccc=ddd HTTP/1.1\r\n"
			};

	int i;
	for (i = 0; i < 14; i++) {
		int len = strlen(reqlines[i]) + 1;

		kconnection_t *c = create_http_connection(i);
		http_request_t *r = (http_request_t*)(c->req);
		r->buffer = xmalloc(sizeof(buffer_t));
		r->buffer->start = xmalloc(len);
		memcpy(r->buffer->start, reqlines[i], len);
		r->buffer->end = r->buffer->start + (len - 1);
		r->buffer->pos = r->buffer->start;
		r->buffer->last = r->buffer->end;
		r->buffer->size = len;

		int state = http_parse_requestline(c);
		printf("state %d: %d, %d\n", i, state, r->space_in_uri);
		if (state == 0) {

			size_t len1 = (r->args_start == NULL)
				? ((r->uri_end) - (r->uri_start + 1))
				: ((r->args_start -1) - (r->uri_start + 1));
			printf("%lu\n", len1);
			kstr_t uri = _kstr_create(r->uri_start + 1, len1);
			printf("%s\n", uri);
			kstr_t *tokens;
			int uric = kstr_tok(uri, "/", &tokens);
			int i;
			for (i = 0; i < uric; i++)
				printf("%s\n", tokens[i]);
		}

	}
	*/

	/*

	char *reqstr = "GET http://dev-packagedeployment.tavisca.com/package/detail/37 HTTP/1.1\r\nHost: dev-packagedeployment.tavisca.com\r\nKeep-Alive: 300\r\nConnection: keep-alive\r\nContent-Type: Application/Json\r\nContent-Length: 33\r\n\r\n{\"name\":sujan,\"phone\":9723750255}";

	char *reqline = "GET /a/b/hello%20world HTTP/1.0\r\n";
	//char *reqline = "GET / HTTP/1.0\r\n";
	//char *reqline = "GET /package/detail/37?abc=xyz&123 HTTP/1.1\r\n";
	//char *reqline = "GET /package/detail/37 HTTP/1.1\r\n";

	size_t len = strlen(reqline);

	kconnection_t *c = create_http_connection(1);
	http_request_t *r = (http_request_t*)(c->req);
	r->buffer = xmalloc(sizeof(buffer_t));
	r->buffer->start = xmalloc(len);
	memcpy(r->buffer->start, reqline, len);
	r->buffer->end = r->buffer->start + (len - 1);
	r->buffer->pos = r->buffer->start;
	r->buffer->last = r->buffer->end;
	r->buffer->size = len;

	int state = http_parse_requestline(c);


	if (state == 0) {

		size_t len = (r->args_start == NULL)
			? ((r->uri_end) - (r->uri_start + 1))
			: (r->args_start- (r->uri_start + 1));
		printf("%lu\n", len);
		kstr_t uri = _kstr_create(r->uri_start + 1, len);
		kstr_t *tokens;
		int uric = kstr_tok(uri, "/", &tokens);
		int i;
		for (i = 0; i < uric; i++)
			printf("%s\n", tokens[i]);
		printf("%s\n", uri);
	}
	printf("state: %d\n", state);
	*/
	return EXIT_SUCCESS;
}


