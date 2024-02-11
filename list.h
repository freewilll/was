#ifndef _LIST_H
#define _LIST_H

typedef struct list {
    int length;
    int allocated;
    void **elements;
} List;

List *new_list(int length);
void free_list(List *l);
void resize_list(List* l, int new_length);
void append_to_list(List *l, void *element);

#endif
