/* ================ sha1.h ================ */
/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain
*/

/* Continue 100% Public Domain
   Added new API
   "sujan dutta" <sujandutta@gmail.com>
*/

#ifndef __SHA1_H__
#define __SHA1_H__

#include "config.h"

typedef struct {
    u_int32_t state[5];
    u_int32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(u_int32_t state[5], const unsigned char buffer[64]);
void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context, const unsigned char* data, u_int32_t len);
void SHA1Final(unsigned char digest[20], SHA1_CTX* context);

void sha1hash(const char *buf, size_t len, char *hashkey);

#endif
