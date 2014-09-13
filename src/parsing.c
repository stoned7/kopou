#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kopou.h"
#include "xalloc.h"

void taction(kclient_t *c)
{
	K_FORCE_USE(c);
}
struct req_blueprint *get_req_blueprint(kobj_t *o)
{
	/* only for testing */
	K_FORCE_USE(o);

	if (strcasecmp((char*)o->val, "rins") != 0)
		return NULL;

	struct req_blueprint *bp;
	bp = xmalloc(sizeof(*bp));

	bp->name = kstr_new("rins");
	bp->readonly = 0;
	bp->argc = 3;
	bp->kindex = 1;
	bp->cindex = 2;
	bp->action = taction;
	return bp;
}

int parse_req(kclient_t *c)
{
	int i, argvlen, actual_argvlen;
	char *dup;
	char *end;
	size_t rollback;

	dup = c->reqbuf;
	dup += c->req_parsing_pos;

	if (c->req_type == KOPOU_REQ_TYPE_NONE) {
		if (dup[0] == REQ_NORMAL)
			c->req_type = KOPOU_REQ_TYPE_NORMAL;
		else if (dup[0] == REQ_REPLICA)
			c->req_type = KOPOU_REQ_TYPE_REPLICA;
		c->req_parsing_pos = 1;
	}

	if (c->expected_argc == 0) {
		end = strstr(dup, END_DELIMITER);
		if (!end) {
			if (c->reqbuf_read_len > 2) {
				reply_err_protocol(c);
				return PARSE_ERR;
			}
			return PARSE_OK;
		}

		dup++;
		c->req_parsing_pos++;
		while (*dup != *end) {
			c->expected_argc = (c->expected_argc * 10)
							+ (*dup - '0');
			dup++;
			c->req_parsing_pos++;
		}
		dup += 2;
		c->req_parsing_pos += 2;
		c->argv = xcalloc(c->expected_argc, sizeof(kobj_t*));
	}

	for (i = c->argc; i < c->expected_argc; i++) {

		argvlen = 0;
		rollback = c->req_parsing_pos;

		if (dup[0] == BEGIN_PARAM_LEN) {
			dup++;
			c->req_parsing_pos++;
		}
		end = strchr(dup, BEGIN_PARAM_VALUE);
		if (!end) {
			if (c->req_parsing_pos + 12 < c->reqbuf_read_len)
				return PARSE_OK;
			reply_err_protocol(c);
			return PARSE_ERR;
		}

		while (*dup != *end) {
			argvlen = (argvlen * 10) + (*dup - '0');
			c->req_parsing_pos++;
			dup++;
		}
		dup++;
		c->req_parsing_pos++;

		end = strstr(dup, END_DELIMITER);
		if (!end) {
			if (c->req_parsing_pos + argvlen < c->reqbuf_read_len) {
				c->req_parsing_pos = rollback;
				return PARSE_OK;
			}
			reply_err_protocol(c);
			return PARSE_ERR;
		}

		actual_argvlen = end - dup;
		if (argvlen > actual_argvlen) {
			reply_err_protocol(c);
			return PARSE_ERR;
		}

		kobj_t *o = xmalloc(sizeof(kobj_t));
		if (c->blueprint && (c->blueprint->cindex == c->argc)) {
			o->val = xmalloc(argvlen);
			o->type = KOBJ_TYPE_BINARY;
			o->len = argvlen;
			memcpy(o->val, dup, o->len);
		}
		else {
			o->val = xmalloc(argvlen + 1);
			o->type = KOBJ_TYPE_STRING;
			o->len = argvlen + 1;
			memcpy(o->val, dup, o->len);
			((char*)o->val)[argvlen] = '\0';
		}

		if (c->argc == 0) {
			if ((c->blueprint = get_req_blueprint(o)) == NULL) {
				reply_err_unknownreq(c);
				return PARSE_ERR;
			}
		}

		dup += argvlen + 2;
		c->req_parsing_pos += argvlen + 2;
		c->argv[c->argc] = o;
		c->argc++;
	}

	if (c->expected_argc == c->argc)
		c->req_ready_to_process = 1;
	return PARSE_OK;
}
