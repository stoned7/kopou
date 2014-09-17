#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdarg.h>

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
	kopou.hlistener = -1;
	kopou.loop = NULL;
	kopou.ilistener = -1;
	kopou.nconns = 0;
	kopou.curr_conn = NULL;
	kopou.bestmemory = 0;
	kopou.exceedbestmemory = 0;
	kopou.conns = NULL;

	settings.cluster_name = kstr_new("kopou-dev");
	settings.address = kstr_new("0.0.0.0");
	settings.port = 7878;
	settings.kport = 7879;
	settings.background = bg;
	settings.verbosity = KOPOU_DEFAULT_VERBOSITY;
	settings.logfile = kstr_new("./kopou-dev.log");
	settings.dbdir = kstr_new(".");
	settings.dbfile = kstr_new("./kopou-dev.kpu");
	settings.http_max_ccur_conns = HTTP_DEFAULT_MAX_CONCURRENT_CONNS;
	settings.http_keepalive_timeout = HTTP_DEFAULT_KEEPALIVE_TIMEOUT;
	settings.http_keepalive = 0;
	settings.tcp_keepalive = 0;

	stats.objects = 0;
	stats.hits = 0;
	stats.missed = 0;
	stats.deleted = 0;
}

static void kopou_oom_handler(size_t size)
{
	_kdie("out of memory, total memory used: %lu bytes"
		"and fail to alloc:%lu bytes", xalloc_total_mem_used(), size);
}


static void usages(char *name)
{
	fprintf(stdout, "\nUsages:\n\t%s <config-file> [--background[-b]]\n", name);
	fprintf(stdout, "\t%s --help[-h]\n", name);
	fprintf(stdout, "\t%s --version[-v]\n\n", name);
	exit(EXIT_SUCCESS);
}

static void version(void)
{
	fprintf(stdout, "\nkopou-server v%s %d bits, +cluster[%d vnodes]\n\n",
			KOPOU_VERSION, KOPOU_ARCHITECTURE, VNODE_SIZE);
	exit(EXIT_SUCCESS);
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
	int i;
	for (i = 0; i < kopou.nconns; i++) {
		if (kopou.conns[i] && kopou.conns[i]->fd > 0)
			tcp_close(kopou.conns[i]->fd);
	}
	tcp_close(kopou.hlistener);
	if (settings.background)
		unlink(kopou.pidfile);
	/* signal db worker to finish job */
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

static void loop_prepoll_handler(kevent_loop_t *ev)
{
	if (kopou.shutdown)
		kevent_loop_stop(ev);
	/* check cli req continue timeout/idle time
	 * clean them if required
	 */
}

static void loop_error_handler(kevent_loop_t *ev, int eerrno)
{
	K_FORCE_USE(ev);
	if (eerrno != EINTR)
		klog(KOPOU_ERR, "loop err: %s", strerror(eerrno));
	kopou.shutdown = 1;
}

static int initialize_kopou_listener(void)
{
	kopou.hlistener = tcp_create_listener(settings.address, settings.port,
			KOPOU_TCP_NONBLOCK);
	if (kopou.hlistener == TCP_ERR) {
		klog(KOPOU_ERR, "fail creating http listener %s:%d",
				settings.address, settings.port);
		return K_ERR;
	}

	kopou.loop = kevent_new(settings.http_max_ccur_conns + KOPOU_OWN_FDS,
				loop_prepoll_handler, loop_error_handler);
	if (!kopou.loop) {
		klog(KOPOU_ERR, "fail creating main loop");
		return K_ERR;
	}

	if (kevent_add_event(kopou.loop, kopou.hlistener, KEVENT_READABLE,
			http_accept_new_connection, NULL,
			http_listener_error) == KEVENT_ERR) {
		klog(KOPOU_ERR, "fail registering http listener to loop");
		return K_ERR;
	}
	return K_OK;
}


int main(int argc, char **argv)
{
	if (argc != 2)
		if (argc != 3)
			usages(argv[0]);

	if (argc == 2) {
		if (!strcmp(argv[1],"--help")
				|| !strcmp(argv[1], "-h"))
			usages(argv[0]);
		else if (!strcmp(argv[1], "--version")
				|| !strcmp(argv[1], "-v"))
			version();
	}

	if (argc == 3)
		if (strcmp(argv[2], "--background"))
			if (strcmp(argv[2], "-b"))
				usages(argv[0]);

	initialize_globals(argc == 3);
	if (settings_from_file(argv[1]) == K_ERR)
		_kdie("fail reading config '%s'", argv[1]);
	settings.configfile = kstr_new(argv[1]);

	if (settings.background)
		if (become_background() == K_ERR)
			_kdie("fail to become background");
	if (setup_sighandler_asyncworkers() == K_ERR)
		_kdie("fail to setup signal handlers");


	xalloc_set_oom_handler(kopou_oom_handler);
	kopou.conns = xcalloc(settings.http_max_ccur_conns + KOPOU_OWN_FDS,
							sizeof(kconnection_t*));
	klog(KOPOU_WARNING, "starting kopou ...");
	if (initialize_kopou_listener() == K_ERR)
		_kdie("fail to start listener");

	kevent_loop_start(kopou.loop);

	stop();
	kevent_del(kopou.loop);
	klog(KOPOU_WARNING, "stoping kopou ...");
	return EXIT_SUCCESS;
}
