#ifndef _STRMAP_H
#define _STRMAP_H

typedef struct strmap {
    char **keys;
    void **values;
    int size;
    int used_count;
    int element_count;
} StrMap;

typedef struct strmap_iterator {
    StrMap *map;
    int pos;
    int original_size;
} StrMapIterator;

StrMap *new_strmap(void);
void free_strmap(StrMap *map);
void *strmap_get(StrMap *strmap, char *key);
void strmap_put(StrMap *strmap, char *key, void *value);
void strmap_delete(StrMap *strmap, char *key);
StrMapIterator strmap_iterator(StrMap *map);
int strmap_iterator_finished(StrMapIterator *iterator);
void strmap_iterator_next(StrMapIterator *iterator);
char *strmap_iterator_key(StrMapIterator *iterator);
#define strmap_foreach(ls, it) for (StrMapIterator it = strmap_iterator(ls); !strmap_iterator_finished(&it); strmap_iterator_next(&it))

#endif
