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
kopou_db_t **dbs;

map_t *cmds_table;

kbuffer_t *create_kbuffer(size_t size)
{
	kbuffer_t *b;
	b = xmalloc(sizeof(*b));
	b->start = xmalloc(size);
	b->end = b->start + (size -1);
	b->pos = b->last = b->start;
	b->next = NULL;
	return b;
}

uint32_t generic_hf(const kstr_t key)
{
	return jenkins_hash(key, kstr_len(key));
}

int generic_kc(const kstr_t key1, const kstr_t key2)
{
	size_t l1 = kstr_len(key1);
	size_t l2 = kstr_len(key2);
	if (l1 != l2)
		return 0;

	return !memcmp(key1, key2, l1);
}

static void init_dbs(void)
{
	cmds_table = map_new(NULL);

	dbs = xcalloc(kopou.ndbs, sizeof(kopou_db_t*));
	dbs[kopou.ndbs] = bucket_init(kopou.ndbs);
	bucket_init_cmds_table(kopou.ndbs);
	kopou.ndbs++;

	dbs[kopou.ndbs] = version_init(kopou.ndbs);
	version_init_cmds_table(kopou.ndbs);

	kopou.ndbs++;
}

static int match_uri(kcommand_t *cmd, khttp_request_t *r)
{
	int i, pi = 0;
	char *tv, *sv;

	if (r->nsplitted_uri != cmd->nptemplate)
		return 0;

	for (i = 0; i < cmd->nptemplate; i++) {
		tv = cmd->ptemplate[i];
		sv = r->splitted_uri[i];
		if (!tv) {
			if (pi > cmd->nparams)
				return 0;
			cmd->params[pi] = sv;
			pi++;
			continue;
		}
		if (strcasecmp(sv, tv))
			return 0;
	}
	return 1;
}

kcommand_t *get_matched_cmd(kconnection_t *c)
{
	kcommand_t *cmd;
	khttp_request_t *r;

	r = c->req;
	if (r->nsplitted_uri > 0) {
		cmd = map_lookup(cmds_table, r->splitted_uri[0]);
		while (cmd) {
			if (r->method == cmd->method)
				if (match_uri(cmd, r))
					return cmd;
			cmd = cmd->next;
		}
	}
	return NULL;
}

int execute_command(kconnection_t *c)
{
	khttp_request_t *r = c->req;
	if (kopou.space > settings.readonly_memory_threshold
		&& r->cmd->flag & KCMD_WRITE_ONLY) {
			reply_403(c);
			return K_OK;
	}
	return r->cmd->execute(c);
}

static void init_globals(int bg)
{
	kopou.pid = getpid();
	kopou.current_time = time(NULL);
	kopou.pidfile = kstr_new("/tmp/kopou.pid");
	kopou.shutdown = 0;
	kopou.hlistener = -1;
	kopou.loop = NULL;
	kopou.klistener = -1;
	kopou.nconns = 0;
	kopou.cronfd = -1;
	kopou.conns = NULL;
	kopou.tconns = 0;
	kopou.conns_last_cron_ts = kopou.current_time;
	kopou.ndbs = 0;
	kopou.can_db_resize = 1;
	kopou.saver = -1;
	kopou.saver_status = K_AGAIN;
	kopou.saver_complete_ts = kopou.current_time;
	kopou.space = 0;

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
	settings.http_keepalive = 1;
	settings.tcp_keepalive = 0;
	settings.http_close_connection_onerror = 1;
	settings.readonly_memory_threshold = 1048576;
	settings.cron_interval = 500;
	settings.http_req_continue_timeout = 5.0;
	settings.conns_cron_interval = 2.0;
	settings.db_resize_interval = 10.0;
	settings.db_backup_interval = 2.0;
}

static void kopou_oom_handler(size_t size)
{
	_kdie("out of memory, total memory used: %lu bytes"
		"and fail to alloc:%lu bytes", kopou.space, size);
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
	fprintf(stdout, "\nkopou v%s %d bits, -cluster[%d vnodes]\n\n",
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
        struct tm *timeinfo;
        char time_buf[64];
        timeinfo = gmtime(&kopou.current_time);
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

static void db_expand_cron(void)
{
	int i;
	if (kopou.can_db_resize) {
		for (i = 0; i < kopou.ndbs; i++)
			kdb_try_expand(get_db(i));
	}
}


static void db_backup_cron(void)
{
	kopou_db_t *db;
	int i, rs;


	if (kopou.saver != -1) {
		bgs_save_status();
		return;
	}

	if (difftime(kopou.current_time, kopou.saver_complete_ts)
		< settings.db_backup_interval)
		return;

	rs = 0;
	for (i = 0; i < kopou.ndbs; i++) {
		db = get_db(i);
		if (db->backup_hdd && db->dirty > 0) {
			db->dirty_considered = db->dirty;
			if (!rs)
				rs = 1;
		}
	}

	if (rs)
		bgs_save_async();
}

static void loop_prepoll_handler(kevent_loop_t *ev)
{
	if (kopou.shutdown)
		kevent_loop_stop(ev);
	kopou.current_time = time(NULL);
}

static void loop_error_handler(kevent_loop_t *ev, int eerrno)
{
	K_FORCE_USE(ev);
	if (eerrno != EINTR)
		klog(KOPOU_ERR, "loop err: %s", strerror(eerrno));
	kopou.shutdown = 1;
}

static int init_kopou_listener(void)
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

static void kopou_cron_error(int fd, eventtype_t evtype)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(evtype);
	klog(KOPOU_ERR, "kopou cron job err: %s", strerror(errno));
	kopou.shutdown = 1;
}

static void connection_cron(void)
{
	kconnection_t *c;
	khttp_request_t *r;
	int i;

	if (difftime(kopou.current_time, kopou.conns_last_cron_ts)
		< settings.conns_cron_interval)
		return;

	for (i = 0; i < kopou.tconns; i++) {

		if ((c = kopou.conns[i]) == NULL) continue;
		if (c->req == NULL) continue;

		r = c->req;
		if (r->buf == NULL) continue;

		if (c->connection_type == CONNECTION_TYPE_HTTP) {
			if (difftime(kopou.current_time, c->last_interaction_ts)
				>= settings.http_req_continue_timeout) {
				http_delete_connection(c);
				klog(KOPOU_WARNING, "deleted hanged http connection %d", c->fd);
			}
		}
	}

	kopou.conns_last_cron_ts = kopou.current_time;
}

static void kopou_cron(int fd, eventtype_t evtype)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(evtype);

	uint64_t num_exp;
        ssize_t tsize;
        tsize = read(fd, &num_exp, sizeof(uint64_t));
        if (tsize != sizeof(uint64_t)) {
		klog(KOPOU_WARNING, "reading err from cron %d", fd);
                return;
        }

	kopou.current_time = time(NULL);

	//db_backup_cron();
	db_expand_cron();
	connection_cron();

}

static int init_kopou_cron(void)
{
	kopou.cronfd = timerfd_create(CLOCK_REALTIME, 0);
	if (kopou.cronfd == K_ERR) {
		klog(KOPOU_ERR, "fail creating kopou cron job");
		return K_ERR;
	}

	struct itimerspec cron_inv;
        cron_inv.it_interval.tv_sec = 0;
        cron_inv.it_interval.tv_nsec = settings.cron_interval * 1000000;
        cron_inv.it_value.tv_sec = 0;
        cron_inv.it_value.tv_nsec = settings.cron_interval * 1000000;

	if (timerfd_settime(kopou.cronfd, 0, &cron_inv, NULL) == K_ERR) {
		klog(KOPOU_ERR, "fail setting kopou cron interval");
		return K_ERR;
	}

	if (kevent_add_event(kopou.loop, kopou.cronfd, KEVENT_READABLE,
		kopou_cron, NULL, kopou_cron_error) == KEVENT_ERR) {
		klog(KOPOU_ERR, "fail registering kopou cron job");
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

	init_globals(argc == 3);

	if (settings_from_file(argv[1]) == K_ERR)
		_kdie("fail reading config '%s'", argv[1]);
	settings.configfile = kstr_new(argv[1]);

	if (settings.background)
		if (become_background() == K_ERR)
			_kdie("fail to become background");

	if (setup_sighandler_asyncworkers() == K_ERR)
		_kdie("fail to setup signal handlers");

	xalloc_set_oom_handler(kopou_oom_handler);

	kopou.tconns = settings.http_max_ccur_conns + KOPOU_OWN_FDS;
	kopou.conns = xcalloc(kopou.tconns, sizeof(kconnection_t*));

	init_dbs();
	klog(KOPOU_WARNING, "starting kopou, http %s:%d", settings.address,
			settings.port);

	if (init_kopou_listener() == K_ERR)
		_kdie("fail to start listener");
	if (init_kopou_cron() == K_ERR)
		_kdie("fail to start kopou cron");

	size_t beforereq = xalloc_total_mem_used();
	klog(KOPOU_DEBUG, "Total Memory Used Before any request %zu", beforereq);

	kevent_loop_start(kopou.loop);

	stop();
	kevent_del(kopou.loop);
	klog(KOPOU_WARNING, "stoping kopou ...");
	return EXIT_SUCCESS;
}
