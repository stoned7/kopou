#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "xalloc.h"
#include "kopou.h"

static int config_tok_line(const char *line, kstr_t **argv);

static inline int config_yesnotoi(kstr_t str)
{
	if (!strcasecmp(str,"yes")) return 1;
	else if (!strcasecmp(str,"no")) return 0;
	else return -1;
}

static void set_config_from_str(kstr_t config)
{

}

void set_config_from_file(const kstr_t filename)
{
        char buf[CONFIG_LINE_LENGTH_MAX];
        kstr_t config = kstr_empty_new();

        FILE *configfile;
        configfile = fopen(filename, "r");
        if (configfile) {
                while (fgets(buf, sizeof(buf), configfile) != NULL) {
			config = kstr_ncat_str(config, buf, strlen(buf));
                }
                fclose(configfile);
		set_config_from_str(config);
        }
        kstr_del(config);
}


static int config_tok_line(const char *line, kstr_t **argv)
{
        const char *pline = line;
        kstr_t current = NULL;
        kstr_t *vector = NULL;

        int argc = 0;

        while (1) {
                while (*pline && isspace(*pline))
			pline++;

                if (*pline) {
                        int inq = 0, insq = 0, done = 0;
                        if (current == NULL)
                                current = kstr_empty_new();

                        while (!done) {
                                if (inq) {
                                        if (*pline == '\\' && *(pline + 1)) {
                                                char c;
                                                pline++;
                                                switch (*pline) {
                                                        case 'n':
                                                                c = '\n';
                                                                break;
                                                        case 'r':
                                                                c = '\r';
                                                                break;
                                                        case 't':
                                                                c = '\t';
                                                                break;
                                                        case 'b':
                                                                c = '\b';
                                                                break;
                                                        case 'a':
                                                                c = '\a';
                                                                break;
                                                        default:
                                                                c = *pline;
                                                                break;
                                                }
                                                current = kstr_ncat_str(current, &c, 1);
                                        } else if (*pline == '"') {
                                                if (*(pline + 1) && !isspace(*(pline + 1)))
                                                        goto err;
                                                done = 1;
                                        } else if (!*pline) {
                                                goto err;
                                        } else {
                                                current = kstr_ncat_str(current, pline, 1);
                                        }
                                } else if (insq) {
                                        if (*pline == '\\' && *(pline + 1) == '\'') {
                                                pline++;
                                                current = kstr_ncat_str(current, "'", 1);
                                        } else if (*pline == '\'') {
                                                if (*(pline + 1) && !isspace(*(pline + 1)))
                                                        goto err;
                                                done = 1;
                                        } else if (!*pline)
                                                goto err;
                                        else {
                                                current = kstr_ncat_str(current, pline, 1);
                                        }
                                } else {
                                        switch (*pline) {
                                                case ' ':
                                                case '\n':
                                                case '\r':
                                                case '\t':
                                                case '\0':
                                                        done = 1;
                                                        break;
                                                case '"':
                                                        inq = 1;
                                                        break;
                                                case '\'':
                                                        insq = 1;
                                                        break;
                                                default :
                                                        current = kstr_ncat_str(current, pline, 1);
                                                        break;
                                        }
                                }
                                if (*pline)
					pline++;
                        }
                        vector = xrealloc(vector, ((argc) + 1) * sizeof(kstr_t));
                        vector[argc] = current;
                        argc++;
                        current = NULL;
                } else {
			if (vector == NULL)
                                vector = xmalloc(sizeof(void*));
                        *argv = vector;
                        return argc;
		}
        }

err:
        while ((argc)--)
                kstr_del(vector[argc]);
        xfree(vector);
        if (current)
                kstr_del(current);
        return 0;
}

#include <stdio.h>
int main()
{
	kstr_t *argv;
	int count = config_tok_line("\n", &argv); //"hello 0.0.0.0 7878\n -abc +123 +abc I \"am\" there\n", &argv);
	printf("%d\n", count);
	int i;

	for (i = 0; i < count; i++) {
		printf("%s:%d\n", argv[i], kstr_len(argv[i]));
	}

	return 0;
}
