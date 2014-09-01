#include <stdio.h>
#include "../src/kopou.h"
#include "../src/xalloc.h"


typedef struct kpu_str {
	char *val;
	size_t len;
} kpu_str_t;

void kpu_str_init(kpu_str_t **kpu_str, size_t len)
{
	*kpu_str = xmalloc(sizeof(*kpu_str));
	(*kpu_str)->val = xmalloc(sizeof(len + 1));
	(*kpu_str)->len = len;
}

void kpu_str_free(kpu_str_t *kpu_str)
{
	xfree(kpu_str->val);
	xfree(kpu_str);
}

struct vnode {
	kpu_str_t *ip;
	int port;
	vnodes_state_t *state;
};


int main()
{
	int i;
	struct vnode *vnode1;

	vnode1 = xmalloc(sizeof(*vnode1));
	vnode1->state = xmalloc(_vnode_state_size_slots);
	kpu_str_init(&vnode1->ip, 16);
	snprintf(vnode1->ip->val, vnode1->ip->len,  "192.168.0.1");

	printf("%d\n", _vnode_state_size_slots);

	vnode_state_empty(*vnode1->state);

	vnode_state_add(*vnode1->state, 55);
	vnode_state_add(*vnode1->state, 127);
	vnode_state_add(*vnode1->state, 77);
	vnode_state_remove(*vnode1->state, 55);
	vnode_state_add(*vnode1->state, 78);
	vnode_state_add(*vnode1->state, 89);

	for (i = 0; i < VNODE_SIZE; i++){
		if (vnode_state_contain(*vnode1->state, i))
			printf("%d: yes\n", i);
	}

	printf("%s\n", vnode1->ip->val);

	xfree(vnode1->state);
	xfree(vnode1->ip);
	xfree(vnode1);
	return 0;
}
