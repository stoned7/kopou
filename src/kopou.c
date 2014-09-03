#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "kopou.h"

struct kopou_server kopou;
struct kopou_settings settings;
struct kopou_stats stats;

void initialize_globals(void)
{
	kopou.pid = getpid();
	time(&kopou.current_time);

	settings.cluster_name = kstr_new("kopou-dev");
	settings.address = kstr_new("0.0.0.0");
	settings.port = 7878;
	settings.cport = 7879;
	settings.mport = 7880;
	settings.demonize = 0;
	settings.verbosity = KOPOU_DEFAULT_VERBOSITY;
	settings.logfile = kstr_new("./kopou-dev.log");
	settings.dbdir = kstr_new(".");
	settings.dbfile = kstr_new("./kopou-dev.kpu");
	settings.max_ccur_clients = KOPOU_DEFAULT_MAX_CONCURRENT_CLIENTS;
	settings.client_idle_timeout = KOPOU_DEFAULT_CLIENT_IDLE_TIMEOUT;
	settings.client_keepalive = 0;

	stats.objects = 0;
	stats.hits = 0;
	stats.missed = 0;
	stats.deleted = 0;
}

void usages(void)
{
	fprintf(stdout, "%s\n", "\nUsages:\n\t./kopou <config-file>\n");
	exit(EXIT_FAILURE);

}

void klog(int level, const char *fmt, ...)
{
	va_list params;
	char msg[KOPOU_MAX_LOGMSG_LEN];
	FILE *fp;

	if (level < settings.verbosity)
		return;

	va_start(params, fmt);
	vsnprintf(msg, sizeof(msg), fmt, params);
	va_end(params);

	fp = !settings.demonize ? stdout : fopen(settings.logfile, "a");
	if (!fp)
		return;

	char *levelstr[] = {"DEBUG", "INFO", "WARN", "ERR", "FATAL"};
	time_t rawtime;
        struct tm *timeinfo;
        char time_buf[64];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", timeinfo);
	fprintf(fp,"[%s][%s][%d]%s\n", time_buf, levelstr[level], kopou.pid,  msg);
	fflush(fp);
	if (!settings.demonize)
		fclose(fp);
}

int main(int argc, char **argv)
{
	if (argc != 2)
		usages();

	if (!strncmp(argv[1],"--help", 6) || !strncmp(argv[1], "-h", 2))
		usages();

	initialize_globals();
	if (set_config_from_file(argv[1]) == K_ERR)
		_kdie("fail reading config '%s'", argv[1]);





	klog(KOPOU_DEBUG, "starting kopou ...");
	return EXIT_SUCCESS;
}
