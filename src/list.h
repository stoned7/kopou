#ifndef __LIST_H__
#define __LIST_H__

#include <stdio.h>
#include <stdlib.h>

#include "xalloc.h"

#define LIST_OK 0
#define LIST_ERR -1

typedef void (*rem_element_handler)(void *data);

typedef struct list_element {
        void *data;
        struct list_element *next;
        struct list_element *prev;
} list_element_t;

typedef struct list {
        unsigned long size;
        list_element_t *head;
        list_element_t *tail;
	rem_element_handler reh;
} list_t;

list_t* list_new(rem_element_handler reh);
void list_del(list_t *list);

int list_add_next(list_t *list, list_element_t *element, const void *data);
int list_add_prev(list_t *list, list_element_t *element, const void *data);
int list_rem(list_t *list, list_element_t *element, void **data);

static inline unsigned long list_size(list_t *list)
{
	return list->size;
}

static inline list_element_t *list_head(list_t *list)
{
	return list->head;
}

static inline list_element_t *list_tail(list_t *list)
{
	return list->tail;
}

static inline void* list_element_data(list_element_t *element)
{
	return element->data;
}

static inline list_element_t *list_element_next(list_element_t *element)
{
	return element->next;
}

static inline list_element_t *list_element_prev(list_element_t *element)
{
	return element->prev;
}

#endif
