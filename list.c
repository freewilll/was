#include <stdlib.h>

#include "list.h"
#include "utils.h"

#define MIN_SIZE 0

List *new_list(int initial_allocation) {
    List *l = malloc(sizeof(List));

    l->length = 0;
    l->allocated = 0;
    l->elements = NULL;

    if (initial_allocation) {
        l->allocated = initial_allocation;
        l->elements = malloc(sizeof(void *) * initial_allocation);
    }

    return l;
}

void free_list(List *l) {
    free(l->elements);
    free(l);
}

static int round_up(int length) {
    if (length == 0) return 0;
    int result = 1;
    while (result < length) result *= 2;
    return result;
}

void resize_list(List* l, int new_length) {
    if (new_length < l->allocated) {
        l->length = new_length;
        return;
    }

    int new_allocated = round_up(new_length);
    if (new_allocated < MIN_SIZE) new_allocated = MIN_SIZE;
    if (new_length > l->length) {
        l->elements = realloc(l->elements, sizeof(void *) * new_allocated);
        if (!l->elements) panic("Realloc failed");
    }
    l->length = new_length;
    l->allocated = new_allocated;
}

void append_to_list(List *l, void *element) {
    resize_list(l, l->length + 1);
    l->elements[l->length - 1] = element;
}
