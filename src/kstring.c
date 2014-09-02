#include "kstring.h"
#include "xalloc.h"

#define KSTR_EOS '\0'

static kstr_t kstr_len_new(char *str, size_t len)
{
	kstr_t kstr = xmalloc(lsize + len + 1);
	memcpy(kstr, &len, lsize);
	memcpy(kstr + lsize, str, len);
	kstr[lsize + len] = KSTR_EOS;
	return kstr + lsize;
}

kstr_t kstr_new(char *str)
{
	return kstr_len_new(str, strlen(str));
}

kstr_t kstr_empty_new()
{
        return kstr_len_new("", 0);
}

kstr_t kstr_cat_strn(kstr_t kstr, char *str, size_t len)
{
	size_t clen = kstr_len(kstr);
	size_t nlen = clen + 1 + len;
	kstr_t *ori = kstr - lsize;
	ori = xrealloc(ori, lsize + nlen);
	memcpy(ori, &nlen, lsize);
	memcpy(ori + lsize + clen, str, len);
	ori[lsize + nlen] = kSTR_EOS;
	return ori + lsize;
}

void kstr_del(kstr_t str)
{
	xfree(str - lsize);
}
