// list.c

typedef struct list {
    int length;
    int allocated;
    void **elements;
} List;

List *new_list(int length);
void free_list(List *l);
void append_to_list(List *l, void *element);

// utils.c
void panic(char *format, ...);
