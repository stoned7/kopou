#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "kopou.h"
#include "xalloc.h"

static int config_tok_line(const char *line, kstr_t **argv);

static inline int _yesnotoi(kstr_t str)
{
	if (!strcasecmp(str,"yes"))
		return 1;
	else if (!strcasecmp(str,"no"))
		return 0;
	else
		return -1;
}

static void _config_from_line(kstr_t config)
{
	kstr_t *lines, *argv;
	int nlines, i, argc, j;
	char *err_msg = NULL;

	nlines = kstr_tok(config, "\n", &lines);
	for (i = 0; i < nlines; i++) {

		if (lines[i][0] == '#' || lines[i][0] == '\0') {
			kstr_del(lines[i]);
			continue;
		}

		argc = config_tok_line(lines[i], &argv);
		if (argc == 0) {
			kstr_del(lines[i]);
			continue;
		}

		if (argv == NULL) {
			err_msg = "invalid config file";
			goto err;
		}

		kstr_tolower(argv[0]);

		if (!strcasecmp(argv[0], "cluster") && argc == 2) {
                        settings.cluster_name = kstr_dup(argv[1]);
                } else if (!strcasecmp(argv[0], "address") && argc == 2) {
			settings.address = kstr_dup(argv[1]);
		} else if (!strcasecmp(argv[0], "port") && argc == 2) {
			settings.port = atoi(argv[1]);
			if (settings.port < 1024 || settings.port > 65535) {
				err_msg = "port out of range";
				goto err;
			}
		} else if (!strcasecmp(argv[0], "verbosity") && argc == 2) {
			kstr_tolower(argv[1]);
			if (!strcasecmp(argv[1], "debug"))
				settings.verbosity = KOPOU_DEBUG;
			else if (!strcasecmp(argv[1], "info"))
				settings.verbosity = KOPOU_INFO;
			else if (!strcasecmp(argv[1], "warning"))
				settings.verbosity = KOPOU_WARNING;
			else if (!strcasecmp(argv[1], "error"))
				settings.verbosity = KOPOU_ERR;
			else {
				err_msg = "invalid log verbosity";
				goto err;
			}
		} else if (!strcasecmp(argv[0], "log_dir") && argc == 2) {
			settings.logfile = kstr_dup(argv[1]);
		} else if (!strcasecmp(argv[0], "db_dir") && argc == 2) {
			settings.dbdir = kstr_dup(argv[1]);
		} else if (!strcasecmp(argv[0], "working_dir") && argc == 2) {
			settings.workingdir = kstr_dup(argv[1]);
		} else if (!strcasecmp(argv[0], "http_max_concurrent_connections")
								&& argc == 2) {
			settings.http_max_ccur_conns = atoi(argv[1]);
		} else if (!strcasecmp(argv[0], "http_keepalive_timeout")
								&& argc == 2) {
			int to = atoi(argv[1]);
			settings.http_keepalive_timeout = (to * (60 * 60));
		} else if (!strcasecmp(argv[0], "http_keepalive")
								 && argc == 2) {
			settings.http_keepalive = _yesnotoi(argv[1]);
			if (settings.http_keepalive == -1) {
				err_msg = "invalid http keepalive, yes or no only";
				goto err;
			}
		} else if (!strcasecmp(argv[0], "tcp_keepalive")
								&& argc == 2) {
			settings.tcp_keepalive = _yesnotoi(argv[1]);
			if (settings.tcp_keepalive == -1) {
				err_msg = "invalid tcp keepalive, yes or no only";
				goto err;
			}
		}

		for (j = 0; j < argc; j++) {
                        kstr_del(argv[j]);
                }
                xfree(argv);
                argv = NULL;
                kstr_del(lines[i]);
	}

	xfree(lines);
	return;
err:
	fprintf(stderr, "config-file,error: %s\n", err_msg);
	exit(EXIT_FAILURE);
}

int settings_from_file(const kstr_t filename)
{
	char buf[CONFIG_LINE_LENGTH_MAX];
	FILE *configfile;

	configfile = fopen(filename, "r");
	if (!configfile)
		return K_ERR;

	kstr_t config = kstr_empty_new();
	while (fgets(buf, sizeof(buf), configfile) != NULL)
		config = kstr_ncat_str(config, buf, strlen(buf));

	fclose(configfile);
	_config_from_line(config);
	if (settings.background) {
		kstr_ncat_str(settings.logfile, "/",1);
		kstr_ncat_str(settings.logfile, settings.cluster_name,
				kstr_len(settings.cluster_name));
		kstr_ncat_str(settings.logfile, ".log", 4);
	}
	settings.dbfile = kstr_new(settings.dbdir);
	kstr_ncat_str(settings.dbfile, "/", 1);
	kstr_ncat_str(settings.dbfile, settings.cluster_name,
				kstr_len(settings.cluster_name));
	kstr_ncat_str(settings.dbfile, ".kpu", 4);
	kstr_del(config);
	return K_OK;
}


static int config_tok_line(const char *line, kstr_t **argv)
{
        const char *pline = line;
        kstr_t current = NULL;
        kstr_t *vector = NULL;

        int argc = 0, inq, insq, done;

	while (1) {
		while (*pline && isspace(*pline))
			pline++;

		if (*pline) {
			inq = 0;
			insq = 0;
			done = 0;
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
					} else if (!*pline)
						goto err;
					else
						current = kstr_ncat_str(current, pline, 1);
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
					else
						current = kstr_ncat_str(current, pline, 1);
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
