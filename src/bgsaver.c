#include "kopou.h"

static size_t _bgs_write(FILE *f, const void *b, size_t len)
{
	size_t r;
	r = fwrite(b, len, 1, f);
	return r;
}

size_t bgs_write(FILE *f, const void *b, size_t len)
{
	size_t nw;

	while (len) {
		if ((nw = _bgs_write(f, b, len)) == 0)
			return K_ERR;
		len -= nw;
		b = (char*)b + nw;
	}

	return nw;
}

static size_t _bgs_read(FILE *f, void *b, size_t len)
{
	size_t r;
	r = fread(b, len, 1, f);
	return r;
}

size_t bgs_read(FILE *f, void *b, size_t len)
{
	size_t nr;
	while (len) {
		if ((nr = _bgs_read(f, b, len)) == 0)
			if (ferror(f)) return K_ERR;
		len -= nr;
		b = (char*)b + nr;
	}
	return nr;
}

void bgs_save_async(void)
{
	pid_t saverbaby;

	saverbaby = fork();
	if (saverbaby == 0) {
		kopou_db_t *db;
		int j;
		for (j = 0; j < kopou.ndbs; j++) {
			db = get_db(j);
			if (db->backup_hdd && db->dirty_considered > 0) {
				if (db->backup_hdd(db) == K_ERR)
					exit(EXIT_FAILURE);
			}
		}
		exit(EXIT_SUCCESS);
	} else {
		kopou.saver = saverbaby;
		if (saverbaby == -1) {
			klog(KOPOU_WARNING, "fail to fork db saver");
			return;
		}
		klog(KOPOU_DEBUG, "initiated db saver");
		kopou.can_db_resize = 0;
	}
}

void bgs_save_status(void)
{
	pid_t pid;
	kopou_db_t *db;
	int status, exit_status, exit_signal, i;

	pid = waitpid(kopou.saver, &status, WNOHANG);
	if (pid == K_ERR) {
		kopou.saver_status = K_ERR;
		klog(KOPOU_ERR, "wait pid fail for saver, %s", strerror(errno));
		goto done;
	}

	if (!pid) return;

	exit_status = WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		exit_signal = WTERMSIG(status);
	else
		exit_signal = 0;


	if (!exit_signal && exit_status == EXIT_SUCCESS) {
		for (i = 0; i < kopou.ndbs; i++) {
			db = get_db(i);
			if (db->backup_hdd)
				db->dirty = db->dirty - db->dirty_considered;
		}
		kopou.saver_status = K_OK;
	} else if (!exit_signal && exit_status == EXIT_FAILURE) {
		klog(KOPOU_ERR, "saver return errror, pid %d", pid);
		kopou.saver_status = K_ERR;
	} else {
		/*TODO: remove the temp file */
		klog(KOPOU_ERR, "saver stoped by signal, pid %d, signal %d",
				pid, exit_signal);
		if (exit_signal != SIGUSR1)
			kopou.saver_status = K_ERR;
	}

done:
	kopou.saver_complete_ts = kopou.current_time;
	kopou.saver = -1;
	kopou.can_db_resize = 1;
	klog(KOPOU_DEBUG, "saver completed, pid %d", pid);
}
