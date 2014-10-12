#ifndef JENKINS_HASH_H
#define JENKINS_HASH_H

#include <stdlib.h>
#include <stdint.h>

#include "common.h"

#ifdef    __cplusplus
extern "C" {
#endif

#if (BYTE_ORDER == BIG_ENDIAN)
#define ENDIAN_BIG 0
#elif (BYTE_ORDER == LITTLE_ENDIAN)
#define ENDIAN_LITTLE 1
#endif

uint32_t jenkins_hash(const void *key, size_t length);

#ifdef __cplusplus
}
#endif

#endif    /* JENKINS_HASH_H */
