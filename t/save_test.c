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

int write_uint64(FILE *fp, uint64_t no)
{
	int r;
	r = bgs_write_len(fp, 8);
	no = htole64(no);
	r = bgs_write(fp, &no, 8);
	return r;
}


void writedata(FILE *fp)
{
	int i, r;
	bucket_obj_t **objs, *obj;

	objs = xmalloc(20 * sizeof(bucket_obj_t*));
	for (i = 0; i < 20; i++) {
		obj = xmalloc(sizeof(bucket_obj_t));
		obj->content_type = kstr_new("Application/Json");
		obj->key = kstr_new("realkey");
		obj->size = 12;
		obj->data = xmalloc(obj->size);
		memcpy(obj->data, "sujan\r\ndutta", obj->size);
		objs[i] = obj;
	}

	for (i = 0; i < 20; i++) {

		obj = objs[i];

		size_t klen = kstr_len(obj->key);
		//printf("klen %zu\n", klen);
		r = bgs_write_len(fp, klen);
		r = bgs_write(fp, obj->key, klen);

		size_t clen = kstr_len(obj->content_type);
		//printf("clen %zu\n", clen);
		r = bgs_write_len(fp, clen);
		r = bgs_write(fp, obj->content_type, clen);

		r = bgs_write_len(fp, obj->size);
		//printf("dlen %zu\n", obj->size);
		r = bgs_write(fp, obj->data, obj->size);
	}
}

void read_data(FILE *fp)
{
	int i, r;
	bucket_obj_t **objs, *obj;
	uint32_t len;
	char *data;

	objs = xmalloc(20 * sizeof(bucket_obj_t*));

	for (i = 0; i < 20; i++) {
		obj = xmalloc(sizeof(bucket_obj_t));

		len = bgs_read_len(fp);
		obj->key = _kstr_create("0", len);
		bgs_read(fp, obj->key, len);

		printf("\nklen:%zu:", kstr_len(obj->key));
		printf("k: %s", obj->key);

		len = bgs_read_len(fp);
		obj->content_type = _kstr_create("0", len);
		bgs_read(fp, obj->content_type, len);

		printf("\nclen:%zu:", kstr_len(obj->content_type));
		printf("c: %s", obj->content_type);

		len = bgs_read_len(fp);
		obj->size = len;
		obj->data = xmalloc(len);
		bgs_read(fp, obj->data, len);

		printf("\ndlen:%zu:", obj->size);
		printf("d: %s", (char*)obj->data);
	}
}

void write_db(FILE *fp)
{
	int r, i;
	int sign = 0x12345678;

	/*
	sign[0] = 0x6b; //107
	sign[1] = 0x6f; //111
	sign[2] = 0x70; //112
	sign[3] = 0x6f; //111
	sign[4] = 0x75; //117
	*/

	r = bgs_write_len(fp, sign);
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

	int64_t cn = 20;
	cn = htole64(cn);
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

	int sign;
	sign = bgs_read_len(fp);
	if (sign == 0x12345678) {
		printf("it is kopou db file\n");
	}

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

	int64_t cn;
	r = bgs_read(fp, &cn, sizeof(cn));
	cn = le64toh(cn);
	printf("count %zu\n", cn);

	read_data(fp);

	fclose(fp);
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
	write_db(fp);
	fclose(fp);

	read_db();
	/*
	bgs_write_len(fp, 0); //64 
	bgs_write_len(fp, 63); //64 
	bgs_write_len(fp, 244);
	bgs_write_len(fp, 16001); //16384
	bgs_write_len(fp, 4194001); //4194304
	bgs_write_len(fp, 4300001);

	fclose(fp);

	fp = fopen("dbbucket.tmp", "r");
	if (!fp) {
		printf("fp fail write\n");
		exit(1);
	}

	read = bgs_read_len(fp);
	printf("%u\n", read);
	read = bgs_read_len(fp);
	printf("%u\n", read);
	read = bgs_read_len(fp);
	printf("%u\n", read);
	read = bgs_read_len(fp);
	printf("%u\n", read);
	read = bgs_read_len(fp);
	printf("%u\n", read);
	read = bgs_read_len(fp);
	printf("%u\n", read);

	fclose(fp);
	*/
	return 0;
}
