#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#include "kopou.h"
#include "kstring.h"
#include "xalloc.h"


#define CONTENT_TYPE_STRING 0
#define CONTENT_TYPE_LIST 1
#define CONTENT_TYPE_RAW 2

#define CONTENT_ENCODING_STRING 0
#define CONTENT_ENCODING_RAW 1

static unsigned char valid_header_field[] =
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static uint32_t valid_uri[] = {
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

int http_parse_request_line(kconnection_t *conn)
{
	unsigned char c, ch, *p, *m;
	khttp_request_t *r = conn->req;

	parsing_state_t state = r->_parsing_state;
	for (p = r->buf->pos; p <= r->buf->last; p++) {
		ch = *p;
		switch (state) {

		case parsing_reqline_start:
			r->request_start = p;
			if (ch == CR || ch == LF)
				break;

			if (ch < 'A' || ch > 'Z') {
				reply_400(conn);
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
						reply_405(conn);
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
						reply_405(conn);
						return HTTP_ERR;
					}
				case 6:
					if (!strncmp(m, "DELETE", 6)) {
						r->method = HTTP_METHOD_DELETE;
						break;
					}
					else {
						reply_405(conn);
						return HTTP_ERR;
					}
				default:
					reply_405(conn);
					return HTTP_ERR;
				}
				state = parsing_reqline_spaces_before_uri;
				break;
			}

			if (ch < 'A' || ch > 'Z') {
				reply_405(conn);
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_spaces_before_uri:

			if (ch == '/') {
				r->uri_start = p;
				state = parsing_reqline_after_slash_in_uri;
				break;
			}

			c = (unsigned char)(ch | 0x20);
			if (c >= 'a' && c <= 'z') {
				r->schema_start = p;
				state = parsing_reqline_schema;
				break;
			}

			if (ch == ' ')
				break;

			reply_400(conn);
			return HTTP_ERR;

		case parsing_reqline_schema:

			c = (unsigned char)(ch | 0x20);
			if (c >= 'a' && c <= 'z')
				break;

			if (ch == ':') {
				r->schema_end = p;
				state = parsing_reqline_schema_slash;
				break;
			}

			reply_400(conn);
			return HTTP_ERR;

		case parsing_reqline_schema_slash:
			if (ch == '/') {
				state = parsing_reqline_schema_slash_slash;
				break;
			}

			reply_400(conn);
			return HTTP_ERR;

		case parsing_reqline_schema_slash_slash:
			if (ch == '/') {
				state = parsing_reqline_host_start;
				break;
			}

			reply_400(conn);
			return HTTP_ERR;

		case parsing_reqline_host_start:
			r->host_start = p;
			state = parsing_reqline_host;

		case parsing_reqline_host:
			c = (unsigned char)(ch | 0x20);
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
				reply_400(conn);
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
				reply_400(conn);
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
				reply_400(conn);
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_after_slash_in_uri:

			if (valid_uri[ch >> 5] & (1 << (ch & 0x1f))) {
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
				reply_400(conn);
				return HTTP_ERR;
			default:
				state = parsing_reqline_check_uri;
				break;
			}
			break;

		case parsing_reqline_check_uri:

			if (valid_uri[ch >> 5] & (1 << (ch & 0x1f)))
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
				reply_400(conn);
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
			if (valid_uri[ch >> 5] & (1 << (ch & 0x1f)))
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
				reply_400(conn);
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
				reply_400(conn);
				return HTTP_ERR;
			}
			break;
		case parsing_reqline_http_HT:
			switch (ch) {
			case 'T':
				state = parsing_reqline_http_HTT;
				break;
			default:
				reply_400(conn);
				return HTTP_ERR;
			}
			break;
		case parsing_reqline_http_HTT:
			switch (ch) {
			case 'P':
				state = parsing_reqline_http_HTTP;
				break;
			default:
				reply_400(conn);
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_http_HTTP:
			switch (ch) {
			case '/':
				state = parsing_reqline_first_major_digit;
				break;
			default:
				reply_400(conn);
				return HTTP_ERR;
			}
			break;

		case parsing_reqline_first_major_digit:
			if (ch < '1' || ch > '9') {
				reply_400(conn);
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
				reply_400(conn);
				return HTTP_ERR;
			}

			r->major_version = r->major_version * 10 + ch - '0';
			break;
		case parsing_reqline_first_minor_digit:
			if (ch < '0' || ch > '9') {
				reply_400(conn);
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
				reply_400(conn);
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
				reply_400(conn);
				return HTTP_ERR;
			}
			break;
		case parsing_reqline_almost_done:
			r->request_end = p - 1;
			switch (ch) {
			case LF:
				goto done;
			default:
				reply_400(conn);
				return HTTP_ERR;
			}

		} /* end switch */

	}

	r->buf->pos = p;
	r->_parsing_state = state;
	return HTTP_CONTINUE;

done:
	r->buf->pos = p + 1;
	if (r->request_end == NULL)
		r->request_end = p;
	r->_parsing_state = parsing_reqline_done;
	r->http_version = r->major_version * 1000 + r->minor_version;
	if (r->http_version == HTTP_VERSION_9
			|| r->http_version == HTTP_VERSION_10) {
		reply_505(conn);
		return HTTP_ERR;
	}
	return HTTP_OK;
}

int http_parse_header_line(kconnection_t *conn)
{
	unsigned char c, ch, *p;

	khttp_request_t *r = conn->req;

	if (r->_parsing_state == parsing_reqline_done) {
		r->header_start = r->buf->pos;
		r->_parsing_state = parsing_header_start;
	} else if (r->_parsing_state == parsing_header_line_done) {
		r->_parsing_state = parsing_header_start;
	}

	parsing_state_t state = r->_parsing_state;

	for (p = r->buf->pos; p <= r->buf->last; p++) {
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
				goto done;
			default:
				r->header_name_start = p;
				state = parsing_header_name;
				c = valid_header_field[ch];
				if (c) break;

				if (ch == '\0') {
					reply_400(conn);
					return HTTP_ERR;
				}
			}
			break;

		case parsing_header_name:

			c = valid_header_field[ch];
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
				goto done;
			}

			reply_400(conn);
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
				goto done;
			case '\0':
				reply_400(conn);
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
				goto done;
			case '\0':
				reply_400(conn);
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
				goto done;
			case '\0':
				reply_400(conn);
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
				goto done;
			case CR:
				break;
			default:
				reply_400(conn);
				return HTTP_ERR;
			}
			break;

		case parsing_header_almost_done:
			switch (ch) {
			case LF:
				state = parsing_header_done;
				goto done;
			default:
				reply_400(conn);
				return HTTP_ERR;
			}
			break;

		}
	}

	r->buf->pos = p;
	r->_parsing_state = state;
	return HTTP_CONTINUE;

done:
	r->buf->pos = p + 1;
	r->_parsing_state = state;
	return HTTP_OK;
}

int http_parse_contentlength_body(kconnection_t *c)
{
	khttp_request_t *r = c->req;
	kbuffer_t *b = r->curbuf;
	size_t rs;

	if (r->_parsing_state == parsing_header_done)
		r->_parsing_state = parsing_body_start;

	parsing_state_t state = r->_parsing_state;
	switch (state) {
	case parsing_body_start:
		//r->buf->pos += 2;
		rs = (r->buf->last - r->buf->pos) +1;
		memcpy(b->pos, r->buf->pos, rs);
		r->buf->pos = r->buf->pos + rs;
		r->rcontent_length += rs;
		if (r->rcontent_length >= r->content_length)
			goto done;
		r->_parsing_state = parsing_body_continue;
		break;
	case parsing_body_continue:
		rs = (b->last - b->pos) +1;
		r->rcontent_length += rs;
		b->pos = b->pos + rs;
		if (r->rcontent_length >= r->content_length)
			goto done;
		break;
	case parsing_body_done:
		goto done;
	}

	return HTTP_CONTINUE;
done:
	if (r->rcontent_length != r->content_length) {
		reply_400(c);
		return HTTP_ERR;
	}

	if (r->rcontent_length > HTTP_REQ_CONTENT_LENGTH_MAX) {
		reply_413(c);
		return HTTP_ERR;
	}

	r->_parsing_state = parsing_done;
	return HTTP_OK;
}

int http_parse_chunked_body(kconnection_t *c)
{
	khttp_request_t *r;
	r = c->req;
	r->_parsing_state = parsing_body_start;

	reply_400(c);
	return HTTP_ERR;
}


