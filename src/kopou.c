#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "kopou.h"
#include "xalloc.h"

#define KOPOU_CLUSTER_SIZE 128

struct node {
	char *ip;
	int port;
};

struct node cluster[KOPOU_CLUSTER_SIZE];


void klog(int level, const char *fmt, ...)
{
	va_list params;
	char msg[KOPOU_MAX_LOGMSG_LEN];
	FILE *fp;

	if (level < KOPOU_LOG_VERVOSITY)
		return;

	va_start(params, fmt);
	vsnprintf(msg, sizeof(msg), fmt, params);
	va_end(params);

	fp = KOPOU_LOG_STDOUT ? stdout : fopen(KOPOU_LOG_FILE, "a");
	if (!fp)
		return;

	pid_t pid;
	pid = getpid();

	char *levelstr[] = {"DEBUG", "INFO", "WARN", "ERR", "FATAL"};
	time_t rawtime;
        struct tm *timeinfo;
        char time_buf[64];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", timeinfo);
	fprintf(fp,"[%s][%s][%d]%s\n", time_buf, levelstr[level], pid,  msg);
	fflush(fp);
	if (!KOPOU_LOG_STDOUT)
		fclose(fp);
}

int main1(int argc, char **argv)
{
	klog(KOPOU_DEBUG, "starting server ...");
	//_kdie("i am dieing %d", 2);
	return 0;

	vnodes_state_t bitarray;
	int i;
	printf("%d\n", _vnode_state_size_slots);


	vnode_state_add(bitarray, 55);
	vnode_state_add(bitarray, 127);
	vnode_state_add(bitarray, 77);
	vnode_state_remove(bitarray, 55);
	vnode_state_add(bitarray, 78);
	vnode_state_add(bitarray, 89);

	vnode_state_empty(bitarray);
	for (i = 0; i < VNODE_SIZE; i++){
		if (vnode_state_contain(bitarray, i))
			printf("%d: test yes\n", i);
	}

	vnode_state_fill(bitarray);
	for (i = 0; i < VNODE_SIZE; i++){
		if (vnode_state_contain(bitarray, i))
			printf("%d: test yes\n", i);
	}
	return 0;
}
