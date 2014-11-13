#include "kopou.h"

#define BG_RW_SIZE_MAX 1024

static int _bgs_write(FILE *fp, const void *b, size_t len)
{
	int w;
	w = fwrite(b, len, 1, fp);
	return w;
}

int bgs_write(FILE *fp, const void *b, size_t len)
{
	size_t wlen;
	while (len) {
		wlen = len < BG_RW_SIZE_MAX
			? len : BG_RW_SIZE_MAX;
		if (_bgs_write(fp, b, wlen) == 0)
			return 0;
		len -= wlen;
		b += wlen;
	}
	return 1;
}

static int _bgs_read(FILE *fp, void *b, size_t len)
{
	int r;
	r = fread(b, len, 1, fp);
	return r;
}

int bgs_read(FILE *fp, void *b, size_t len)
{
	size_t rlen;
	while (len) {
		rlen = len < BG_RW_SIZE_MAX
			? len : BG_RW_SIZE_MAX;
		if (_bgs_read(fp, b, rlen) == 0)
			return 0;
		len -= rlen;
		b += rlen;
	}
	return 1;
}

int bgs_write_len(FILE *fp, uint32_t len)
{
	unsigned char buf[3];
	int r;

	if (len < (1 << 6)) {
		len = htole32(len);
		buf[0] = (len & 0xff) | (0 << 6);
		r = bgs_write(fp, buf, 1);
		if (!r)
			return K_ERR;
	} else if (len < (1 << 14)) {
		len = htole32(len);
		buf[0] = ((len >> 8) & 0xff) | (1 << 6);
		buf[1] = len & 0xff;
		r = bgs_write(fp, buf, 2);
		if (!r)
			return K_ERR;
	} else if (len < (1 << 22)) {
		len = htole32(len);
		buf[0] = ((len >> 16) & 0xff) | (2 << 6);
		buf[1] = (len >> 8) & 0xff;
		buf[2] = len & 0xff;
		r = bgs_write(fp, buf, 3);
		if (!r)
			return  K_ERR;
	} else {
		buf[0] = 3 << 6;
		r = bgs_write(fp, buf, 1);
		if (!r)
			return K_ERR;
		len = htole32(len);
		r = bgs_write(fp, &len, 4);
		if (!r)
			return K_ERR;
	}
	return K_OK;
}

uint32_t bgs_read_len(FILE *fp)
{
	unsigned char buf[3];
	uint32_t len;
	int size, r;

	r = bgs_read(fp, buf, 1);
	if (!r)
		return K_LEN_ERR;

	size = (buf[0] & 0xc0) >> 6;
	if (size == 0) {
		len = buf[0] & 0x3F;
		return le32toh(len);
	} else if (size == 1) {
		r = bgs_read(fp, buf+1, 1);
		if (!r)
			return K_LEN_ERR;
		len = ((buf[0] & 0x3F) << 8)  | buf[1];
		return le32toh(len);
	} else if (size == 2) {
		r = bgs_read(fp, buf+1, 2);
		if (!r)
			return K_LEN_ERR;
		len = (((buf[0] & 0x3F) << 16) | (buf[1] << 8)) | buf[2];
		return le32toh(len);
	} else if (size == 3) {
		r = bgs_read(fp, &len, 4);
		if (!r)
			return K_LEN_ERR;
		return le32toh(len);
	}

	return K_LEN_ERR;
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

