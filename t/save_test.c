#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include <sys/types.h>

#include "../src/kopou.h"


typedef struct {
	kstr_t content_type;
	kstr_t key;
	void *data;
	size_t size;
} bucket_obj_t;


void writedata(FILE *fp)
{
	int i;
	bucket_obj_t **objs, *obj;

	objs = xmalloc(20 * sizeof(bucket_obj_t*));

	for (i = 0; i < 20; i++) {

		obj = xmalloc(sizeof(bucket_obj_t));
		obj->content_type = kstr_new("Application/Json");
		obj->key = kstr_new("realkey");
		obj->size = 10;
		obj->data = xmalloc(obj->size);
		memcpy(obj->data, "sujandutta", 10);
		objs[i] = obj;
	}

	for (i = 0; i < 20; i++) {
		obj = objs[i];

	}
}

void write_db(FILE *fp)
{
	int r;

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

	int64_t cn = 123456789;
	cn = htobe64(cn);
	r = bgs_write(fp, &cn, sizeof(cn));
	if (r == 0)
		printf("error writing kopou counts\n");

	writedata(fp);

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

}


int main()
{
	FILE *fp;
	int r;
	uint32_t read;

	fp = fopen("dbbucket.tmp", "w");
	if (!fp) {
		printf("fp fail write\n");
		exit(1);
	}


	bgs_write_length(fp, 0); //64 
	bgs_write_length(fp, 63); //64 
	bgs_write_length(fp, 244);
	bgs_write_length(fp, 16000); //16384
	bgs_write_length(fp, 4194000); //4194304
	bgs_write_length(fp, 4300000);

	fclose(fp);

	fp = fopen("dbbucket.tmp", "r");
	if (!fp) {
		printf("fp fail write\n");
		exit(1);
	}

	read = bgs_read_length(fp);
	printf("%u\n", read);
	read = bgs_read_length(fp);
	printf("%u\n", read);
	read = bgs_read_length(fp);
	printf("%u\n", read);
	read = bgs_read_length(fp);
	printf("%u\n", read);
	read = bgs_read_length(fp);
	printf("%u\n", read);
	read = bgs_read_length(fp);
	printf("%u\n", read);

	fclose(fp);

	return 0;
}
