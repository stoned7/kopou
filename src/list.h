#ifndef __LIST_H__
#define __LIST_H__

#include <stdio.h>
#include <stdlib.h>

#define LIST_OK 0
#define LIST_ERR -1

typedef struct list_element {
        void *data;
        struct list_element *next;
        struct list_element *prev;
} list_element_t;

typedef struct list {
        unsigned long size;
        list_element_t *head;
        list_element_t *tail;
        void (*del_element_handler)(void *data);
} list_t;

list_t *list_new(void (*del_element_handler)(void *data));
void list_del(list_t *list);
int list_ele_add_next(list_t *list, list_element_t *element, const void *data);
int list_ele_add_prev(list_t *list, list_element_t *element, const void *data);
int list_ele_del(list_t *list, list_element_t *element, void **data);

#define list_size(list) ((list)->size)
#define list_head(list) ((list)->head)
#define list_tail(list) ((list)->tail)
#define list_ele_data(element) ((element)->data)
#define list_ele_next(element) ((element)->next)
#define list_ele_prev(element) ((element)->prev)

#endif
