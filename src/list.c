#include "list.h"

list_t* list_new(rem_element_handler reh)
{
        list_t *list = xmalloc(sizeof(*list));
        list->size = 0;
        list->head = NULL;
        list->tail = NULL;
        list->reh = reh;
        return list;
}

void list_del(list_t *list)
{
        void *data;
        while (list_size(list) > 0) {
		if (list_rem(list, list_tail(list), &data) == LIST_OK
					&& list->reh != NULL)
                        list->reh(data);
        }
        xfree(list);
}

int list_add_next(list_t *list, list_element_t *element, const void *data)
{
        if (element == NULL && list_size(list) != 0)
		return LIST_ERR;

        list_element_t *new_element = xmalloc(sizeof(*new_element));
        new_element->data = (void *)data;

        if (list_size(list) == 0) {
                list->head = list->tail = new_element;
                new_element->next = new_element->prev = NULL;
        } else {
                new_element->next = element->next;
                new_element->prev = element;
                if (element->next == NULL)
                        list->tail = new_element;
                else
                        element->next->prev = new_element;
                element->next = new_element;
        }
        list->size++;
        return LIST_OK;
}

int list_add_prev(list_t *list, list_element_t *element, const void *data)
{
        if (element == NULL && list_size(list) != 0)
		return LIST_ERR;

        list_element_t *new_element = xmalloc(sizeof(*new_element));
        new_element->data = (void *)data;

        if (list_size(list) == 0) {
                list->head = list->tail = new_element;
                new_element->next = new_element->prev = NULL;
        } else {
                new_element->next = element;
                new_element->prev = element->prev;
                if (element->prev == NULL)
                        list->head = new_element;
                else
                        element->prev->next = new_element;
                element->prev = new_element;
        }
        list->size++;
        return LIST_OK;
}

int list_rem(list_t *list, list_element_t *element, void **data)
{
        if (element == NULL || list_size(list) == 0)
		return LIST_ERR;

        *data = element->data;
        if (element == list->head) {
                list->head = element->next;
                if (list->head == NULL)
                        list->tail = NULL;
                else
                        element->next->prev = NULL;
        } else {
                element->prev->next = element->next;
                if (element->next == NULL)
                        list->tail = element->prev;
                else
                        element->next->prev = element->prev;
        }

        xfree(element);
        list->size--;
        return LIST_OK;
}
