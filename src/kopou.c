#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include "kopou.h"

struct kopou_server kopou;
struct kopou_settings settings;
struct kopou_stats stats;

static void initialize_globals(int bg)
{
	kopou.pid = getpid();
	kopou.current_time = time(NULL);
	kopou.pidfile = kstr_new("/tmp/kopou.pid");
	kopou.shutdown = 0;
	kopou.listener = -1;
	kopou.clistener = -1;
	kopou.mlistener = -1;

	settings.cluster_name = kstr_new("kopou-dev");
	settings.address = kstr_new("0.0.0.0");
	settings.port = 7878;
	settings.cport = 7879;
	settings.mport = 7880;
	settings.background = bg;
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

static void usages(void)
{
	fprintf(stdout, "%s\n", "\nUsages:\n\tkopou <config-file> [--daemonize]\n\tkopou --help\n");
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

	fp = settings.background ? fopen(settings.logfile, "a") : stdout;
	if (!fp)
		return;

	char *levelstr[] = {"DEBUG", "INFO", "WARN", "ERR", "FATAL"};
	time_t rawtime;
        struct tm *timeinfo;
        char time_buf[64];
        rawtime = time(NULL);
        timeinfo = localtime(&rawtime);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", timeinfo);
	fprintf(fp,"[%s][%s][%d]%s\n", time_buf, levelstr[level], kopou.pid,  msg);
	fflush(fp);
	if (settings.background)
		fclose(fp);
}

static int become_background(void)
{
	int fd;
	FILE *fp;
	int r;

	r = fork();
	if (r == K_ERR)
		return K_ERR;

	if (r != 0)
		_exit(EXIT_SUCCESS);

	r = setsid();
	if (r == K_ERR)
		return K_ERR;

	r = fork();
	if (r == K_ERR)
		return K_ERR;

	if (r != 0)
		_exit(EXIT_SUCCESS);

	close (STDIN_FILENO);
	fd = open("/dev/null", O_RDWR);
	if (fd != STDIN_FILENO)
		return K_ERR;
	if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
		return K_ERR;
	if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
		return K_ERR;

	fp = fopen(kopou.pidfile,"w");
	if (!fp)
		return K_ERR;

	kopou.pid = getpid();
	fprintf(fp,"%d\n",(int)kopou.pid);
	fclose(fp);

	return K_OK;
}

static void kopou_signal_handler(int signal)
{
	if (signal == SIGINT || signal == SIGTERM)
		kopou.shutdown = 1;
}

static void stop(void)
{
	klog(KOPOU_WARNING, "shutting down kopou");
	if (settings.background)
		unlink(kopou.pidfile);
	exit(EXIT_SUCCESS);
}

static int setup_sighandler_asyncworkers(void)
{
	sigset_t mask;
        struct sigaction sigact;

	if (sigemptyset(&mask) == K_ERR)
		return K_ERR;

	if (sigaddset(&mask, SIGINT) == K_ERR)
		return K_ERR;
	if (sigaddset(&mask, SIGTERM) == K_ERR)
		return K_ERR;
	if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != K_OK)
		return K_ERR;

	/*TODO initiate all the pthreads here*/

	if (pthread_sigmask(SIG_UNBLOCK, &mask, NULL) != K_OK)
		return K_ERR;
	sigact.sa_handler = kopou_signal_handler;
	if (sigemptyset(&sigact.sa_mask) == K_ERR)
		return K_ERR;
	sigact.sa_flags = SA_NOCLDSTOP;
	if (sigaction(SIGINT, &sigact, NULL) == K_ERR)
		return K_ERR;
	if (sigaction(SIGTERM, &sigact, NULL) == K_ERR)
		return K_ERR;

	return K_OK;
}

int main(int argc, char **argv)
{
	if (argc != 2)
		if (argc != 3)
			usages();
	if (argc == 2)
		if (!strncmp(argv[1],"--help", 6))
			usages();
	if (argc == 3)
		if (strncmp(argv[2], "--daemonize", 11))
			usages();

	initialize_globals(argc == 3);
	if (set_config_from_file(argv[1]) == K_ERR)
		_kdie("fail reading config '%s'", argv[1]);
	settings.configfile = kstr_new(argv[1]);

	if (settings.background)
		if (become_background() == K_ERR)
			_kdie("fail to become background");
	if (setup_sighandler_asyncworkers() == K_ERR)
		_kdie("fail to setup signal handlers");

	klog(KOPOU_DEBUG, "starting kopou ...");
	while(1) {
		if (kopou.shutdown)
			stop();
	};
	return EXIT_SUCCESS;
}
