#include <ctype.h>
#include "kstring.h"
#include "xalloc.h"

#define KSTR_EOS '\0'

static kstr_t _kstr_create(char *str, size_t len)
{
	kstr_t kstr = xmalloc(lsize + len + 1);
	memcpy(kstr, &len, lsize);
	memcpy(kstr + lsize, str, len);
	kstr[lsize + len] = KSTR_EOS;
	return kstr + lsize;
}

kstr_t kstr_new(char *str)
{
	return _kstr_create(str, strlen(str));
}

kstr_t kstr_empty_new()
{
        return _kstr_create("", 0);
}

void kstr_del(kstr_t str)
{
	xfree(str - lsize);
}

kstr_t kstr_dup(kstr_t str)
{
	return _kstr_create(str, kstr_len(str));
}

int kstr_tok(kstr_t str, const char *deli, kstr_t **tokens)
{
	size_t len, delilen;
	size_t i, j = 0;
	int ntoken = 0, ii;
        kstr_t *vector = NULL;

	len = kstr_len(str);
	delilen = strlen(deli);
	if (len <= 0 || delilen <= 0)
		goto err;

	for (i = 0; i < len; i++) {
		if ((delilen == 1 && *(str + i) == deli[0]) ||
				(memcmp(str + i, deli, delilen) == 0)) {
			if (i > j) {
				vector = xrealloc(vector, sizeof(kstr_t) * (ntoken + 1));
				vector[ntoken] = _kstr_create(str + j, i - j);
				ntoken++;
			}
			j = i + delilen;
			i = i + delilen - 1;
		}
	}

	if (len > j) {
		vector = xrealloc(vector, sizeof(kstr_t) * (ntoken + 1));
		vector[ntoken] = _kstr_create(str + j, len - j);
		ntoken++;
	}

	*tokens = vector;
	return ntoken;
err:
	for (ii = 0; ii < ntoken; ii++)
		kstr_del(vector[ii]);
	xfree(vector);
        return 0;
}

kstr_t kstr_ncat_str(kstr_t kstr, const char *str, size_t len)
{
	size_t clen = kstr_len(kstr);
	size_t nlen = clen + len;
	kstr = kstr - lsize;
	kstr = xrealloc(kstr, lsize + nlen + 1);
	memcpy(kstr, &nlen, lsize);
	memcpy(kstr + lsize + clen, str, len);
	kstr[lsize + nlen] = KSTR_EOS;
	return kstr + lsize;
}


int kstr_cmp(const kstr_t str1, const kstr_t str2)
{
	size_t len1, len2;
	size_t minlen;
	int result;

	len1 = kstr_len(str1);
	len2 = kstr_len(str2);
	minlen = len1 < len2 ? len1 : len2;
	result = memcmp(str1, str2, minlen);
	if (result == 0)
		return (int)(len1 - len2);
	return result;
}

int kstr_tolower(kstr_t str)
{
        size_t len, i;

	len = kstr_len(str);
	for (i = 0; i < len; i++)
		str[i] = tolower(str[i]);
	return 0;
}

int kstr_toupper(kstr_t str)
{
	size_t len, i;

	len = kstr_len(str);
	for (i = 0; i < len; i++)
		str[i] = toupper(str[i]);
	return 0;
}
