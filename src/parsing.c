#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define PARSE_CONT 1

enum {
	KOPOU_REQ_TYPE_NONE = 0,
	KOPOU_REQ_TYPE_NORMAL,
	KOPOU_REQ_TYPE_REPLICA,
};

int parse_req(char *data, int len)
{
	int req_type = KOPOU_REQ_TYPE_NONE;
	int expected_argc = 0;
	int i, argvlen, actual_argvlen;
	char *end, *rollback;

	if (data[0] == REQ_NORMAL)
		req_type = KOPOU_REQ_TYPE_NORMAL;
	else if (data[0] == REQ_REPLICA)
		req_type = KOPOU_REQ_TYPE_REPLICA;

	end = strstr(data, END_DELIMITER);
	if (!end)
		return PARSE_ERR;

	data++;
	while (*data != *end) {
		expected_argc = (expected_argc * 10) + (*data - '0');
		data++;
	}
	data += 2;

	for (i = 0; i < expected_argc; i++) {
		argvlen = 0;
		rollback = data;
		if (data[0] == BEGIN_PARAM_LEN)
			data++;
		end = strchr(data, BEGIN_PARAM_VALUE);
		if (!end)
			return PARSE_ERR;

		while (*data != *end) {
			argvlen = (argvlen * 10) + (*data - '0');
			data++;
		}
		data++;

		end = strstr(data, END_DELIMITER);
		if (!end) {
			data = rollback;
			return PARSE_ERR;
		}

		actual_argvlen = end - data;
		if (argvlen > actual_argvlen)
			return PARSE_ERR;

		char *argv = malloc((argvlen + 1) * sizeof(char));
		memcpy(argv, data, argvlen);
		argv[argvlen + 1] = '\0';
		free(argv);
		data += (argvlen + 2);
	}
	return PARSE_OK;
}
