/*
#include <stdlib.h>
#include <string.h>


int main1()
{

	char *req = "GET /package/detail/37?abc=xyz&123 HTTP/1.1\r\nHost: dev-packagedeployment.tavisca.com\r\nConnection:keep-alive\r\nCache-Control: max-age=0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,/*;q=0.8\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/37.0.2062.103 Safari/537.36\r\nAccept-Encoding: gzip,deflate,sdch\r\nAccept-Language: en-US,en;q=0.8\r\nCookie: ASP.NET_SessionId=t3lve43o5fzii2xwji53anuc\r\nContent-Length: 67890\r\nContent-Type: application/json, abcd\r\nKeep-Alive: 300\r\n\r\n";

	int reqlen = strlen(req) + 1;

	kconnection_t *c = create_http_connection(1);

	khttp_request_t *r = (khttp_request_t*)(c->req);
	r->buf = xmalloc(sizeof(kbuffer_t));
	r->buf->start = xmalloc(reqlen);
	memcpy(r->buf->start, req, reqlen);

	r->buf->end = r->buf->start + (reqlen - 1);
	r->buf->pos = r->buf->start;
	r->buf->last = r->buf->end;
	r->buf->next = NULL;

	int re = http_parse_requestline(c);
	if (re == HTTP_OK) {

		if (r->_parsing_state == parsing_reqline_done) {

			size_t rllen = (r->args_start == NULL)
				? ((r->uri_end) - (r->uri_start + 1))
				: ((r->args_start -1) - (r->uri_start + 1));
			kstr_t uri = _kstr_create(r->uri_start + 1, rllen);
			r->nsplitted_uri = kstr_tok(uri, "/", &r->splitted_uri);

			do {
				re = http_parse_header_line(c);
				printf("header result: %d, %d\n", re, r->_parsing_state);

				if (re == HTTP_ERR)
					break;

				if (r->_parsing_state == parsing_header_line_done) {
					size_t hl = r->header_name_end - r->header_name_start;
					if (hl > HTTP_HEADER_MAXLEN) {
						// TODO error 413 response
					}
					size_t vl = r->header_value_end - r->header_value_start;
					re = http_set_system_header(r, r->header_name_start, hl, r->header_value_start, vl);
				}
			} while (r->_parsing_state != parsing_header_done);

			if (r->_parsing_state == parsing_header_done) {
				if (r->content_length > 0) {
					r->body = r->header_end + 2;
					//r->body_start_index = 0;
				}
			}

		}
	}


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
		khttp_request_t *r = (khttp_request_t*)(c->req);
		r->buffer = xmalloc(sizeof(kbuffer_t));
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

	char *reqstr = "GET http://dev-packagedeployment.tavisca.com/package/detail/37 HTTP/1.1\r\nHost: dev-packagedeployment.tavisca.com\r\nKeep-Alive: 300\r\nConnection: keep-alive\r\nContent-Type: Application/Json\r\nContent-Length: 33\r\n\r\n{\"name\":sujan,\"phone\":9723750255}";

	char *reqline = "GET /a/b/hello%20world HTTP/1.0\r\n";
	//char *reqline = "GET / HTTP/1.0\r\n";
	//char *reqline = "GET /package/detail/37?abc=xyz&123 HTTP/1.1\r\n";
	//char *reqline = "GET /package/detail/37 HTTP/1.1\r\n";

	size_t len = strlen(reqline);

	kconnection_t *c = create_http_connection(1);
	khttp_request_t *r = (khttp_request_t*)(c->req);
	r->buffer = xmalloc(sizeof(kbuffer_t));
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
	return EXIT_SUCCESS;
}

*/
