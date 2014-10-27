#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include <sys/types.h>
#include "../src/kopou.h"


void write_db(void)
{
	FILE *fp;
	int r;

	fp = fopen("dbbucket.tmp", "w");
	if (!fp) {
		printf("fp fail\n");
		exit(1);
	}

	r = bgs_write(fp, "kopou", 5);
	if (r == 0)
		printf("error writing kopou\n");

	int8_t dbid = 1;
	r = bgs_write(fp, &dbid, sizeof(dbid));
	if (r == 0)
		printf("error writing kopou dbid\n");

	int8_t dbv = 1;
	r = bgs_write(fp, &dbv, sizeof(dbv));
	if (r == 0)
		printf("error writing kopou db version\n");

	time_t dbf = time(NULL);
	time_t be = htobe64(dbf);
	r = bgs_write(fp, &be, sizeof(be));
	if (r == 0)
		printf("error writing kopou db time\n");

	char buf[128];
	size_t len = 128;
	struct tm *tm = gmtime(&dbf);
	strftime(buf, len, "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", tm);
	printf("%s\n", buf);

	int64_t cn = 123456789;
	cn = htobe64(cn);
	r = bgs_write(fp, &cn, sizeof(cn));
	if (r == 0)
		printf("error writing kopou counts\n");

	fclose(fp);
}

void read_db(void)
{

	FILE *fp;
	int r;

	fp = fopen("dbbucket.tmp", "r");
	if (!fp) {
		printf("fp fail\n");
		exit(1);
	}

	char signature[5];
	r = bgs_read(fp, signature, 5);
	if (r = 0) {
		if (feof(fp))
			printf("eof reading\n");
		else
			printf("error reading %s\n", strerror(errno));
	}

	r = memcmp("kopou", signature, 5);
	if (r == 0)
		printf("it is kopou db file\n");

	int8_t dbid;
	r = bgs_read(fp, &dbid, sizeof(int8_t));
	if (dbid == 1) {
		printf("dbid is good %d\n", dbid);
	}

	int8_t dbv;
	r = bgs_read(fp, &dbv, sizeof(int8_t));
	if (dbv == 1) {
		printf("dbv is good %d\n", dbv);
	}

	time_t dbf;
	r = bgs_read(fp, &dbf, sizeof(dbf));
	dbf = be64toh(dbf);

	char buf[128];
	size_t len = 128;
	struct tm *tm = gmtime(&dbf);
	strftime(buf, len, "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", tm);
	printf("%s\n", buf);

	int64_t cn;
	r = bgs_read(fp, &cn, sizeof(cn));
	cn = be64toh(cn);
	printf("count %zu\n", cn);

	fclose(fp);
}


int main()
{
	write_db();
	read_db();
	return 0;
}
