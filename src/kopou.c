
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

kopou_db_t *bucketdb;
kopou_db_t *versiondb;

static map_t *cmds_table;

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

	return !strcmp(key1, key2);
}

static void init_dbs(void)
{
	/* db size should be power of 2 always */
	bucketdb = kdb_new(1024, 5, generic_hf, generic_kc);
	versiondb = kdb_new(512, 5, generic_hf, generic_kc);
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

static void init_cmds_table(void)
{
	unsigned nt, p;

	cmds_table = map_new(NULL);

	/* bucket */
	kstr_t bucket_name = kstr_new("bucket");
	nt = 2;
	p = 1;
	kcommand_t *headbucket = xmalloc(sizeof(kcommand_t));
	*headbucket = (kcommand_t){ .method = HTTP_METHOD_HEAD,
		.execute = bucket_head_cmd, .nptemplate = nt, .nparams = p,
		.flag = KCMD_READ_ONLY | KCMD_SKIP_REQUEST_BODY |
			KCMD_SKIP_REPLICA | KCMD_SKIP_PERSIST };
	headbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	headbucket->ptemplate[0] = bucket_name;
	headbucket->ptemplate[1] = NULL;
	headbucket->params = xcalloc(p, sizeof(kstr_t));

	kcommand_t *getbucket = xmalloc(sizeof(kcommand_t));
	*getbucket = (kcommand_t){ .method = HTTP_METHOD_GET,
		.execute = bucket_get_cmd, .nptemplate = nt, .nparams = p,
		.flag = KCMD_READ_ONLY | KCMD_SKIP_REQUEST_BODY |
			KCMD_SKIP_REPLICA | KCMD_SKIP_PERSIST };
	getbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	getbucket->ptemplate[0] = bucket_name;
	getbucket->ptemplate[1] = NULL;
	getbucket->params = xcalloc(p, sizeof(kstr_t));
	headbucket->next = getbucket;

	kcommand_t *putbucket = xmalloc(sizeof(kcommand_t));
	*putbucket = (kcommand_t){ .method = HTTP_METHOD_PUT,
		.execute = bucket_put_cmd, .nptemplate = nt, .nparams = p,
		.flag = KCMD_WRITE_ONLY };
	putbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	putbucket->ptemplate[0] = bucket_name;
	putbucket->ptemplate[1] = NULL;
	putbucket->params = xcalloc(p, sizeof(kstr_t));
	getbucket->next = putbucket;

	kcommand_t *delbucket = xmalloc(sizeof(kcommand_t));
	*delbucket = (kcommand_t){ .method = HTTP_METHOD_DELETE, .execute =
		bucket_delete_cmd, .nptemplate = nt, .nparams = p,
		.flag = KCMD_WRITE_ONLY | KCMD_SKIP_REQUEST_BODY };
	delbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	delbucket->ptemplate[0] = bucket_name;
	delbucket->ptemplate[1] = NULL;
	delbucket->params = xcalloc(p, sizeof(kstr_t));
	putbucket->next = delbucket;

	delbucket->next = NULL;
	map_add(cmds_table, bucket_name, headbucket);

	/* version */
	kstr_t version_name = kstr_new("version");
	nt=2;
	p=1;
	kcommand_t *putversion = xmalloc(sizeof(kcommand_t));
	*putversion = (kcommand_t){.method = HTTP_METHOD_PUT, .execute = NULL,
		.nptemplate = nt, nparams = p, .flag = KCMD_WRITEONLY };
	putversion->ptemplate = xcalloc(nt, sizeof(kstr_t));
	putversion->ptemplate[0] = version_name;
	putversion->ptemplate[1] = NULL;
	putversion->params = xcalloc(p, sizeof(kstr_t));
	putversion->nparams = p;


	/* stats */
	kstr_t stats_name = kstr_new("stats");
	nt = 1;
	p = 0;
	kcommand_t *stats = xmalloc(sizeof(kcommand_t));
	*stats = (kcommand_t){.method = HTTP_METHOD_GET, .next = NULL,
		.execute = stats_get_cmd, .nptemplate = nt, .nparams = p,
		.flag = KCMD_READ_ONLY | KCMD_SKIP_REQUEST_BODY |
			KCMD_SKIP_REPLICA | KCMD_SKIP_PERSIST };
	stats->ptemplate = xcalloc(nt, sizeof(kstr_t));
	stats->ptemplate[0] = stats_name;
	map_add(cmds_table, stats_name, stats);

	/* favicon */
	kstr_t favicon_name = kstr_new("favicon.ico");
	nt = 1;
	p = 0;
	kcommand_t *favicon = xmalloc(sizeof(kcommand_t));
	*favicon = (kcommand_t){ .method = HTTP_METHOD_GET, .nparams = p,
		.execute = favicon_get_cmd, .next = NULL, .nptemplate = nt,
		.flag = KCMD_READ_ONLY | KCMD_SKIP_REQUEST_BODY |
		KCMD_SKIP_REPLICA | KCMD_SKIP_PERSIST | KCMD_RESPONSE_CACHABLE };
	favicon->ptemplate = xcalloc(nt, sizeof(kstr_t));
	favicon->ptemplate[0] = favicon_name;
	map_add(cmds_table, favicon_name, favicon);
}

int execute_command(kconnection_t *c)
{
	khttp_request_t *r = c->req;
	if (unlikely(settings.readonly_memory_threshold < xalloc_total_mem_used())) {
		if (r->cmd->flag & KCMD_WRITE_ONLY) {
			reply_403(c);
			return K_OK;
		}
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
	settings.http_keepalive = 1;
	settings.tcp_keepalive = 0;
	settings.http_close_connection_onerror = 1;
	settings.readonly_memory_threshold = 1048576;

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

static void loop_prepoll_handler(kevent_loop_t *ev)
{
	if (kopou.shutdown)
		kevent_loop_stop(ev);

	kopou.current_time = time(NULL);
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

	init_cmds_table();
	xalloc_set_oom_handler(kopou_oom_handler);
	kopou.conns = xcalloc(settings.http_max_ccur_conns + KOPOU_OWN_FDS,
							sizeof(kconnection_t*));
	init_dbs();
	klog(KOPOU_WARNING, "starting kopou, http %s:%d", settings.address,
			settings.port);
	if (init_kopou_listener() == K_ERR)
		_kdie("fail to start listener");

	kevent_loop_start(kopou.loop);

	stop();
	kevent_del(kopou.loop);
	klog(KOPOU_WARNING, "stoping kopou ...");
	return EXIT_SUCCESS;
}
