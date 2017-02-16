#ifndef __LIST_H__
#define __LIST_H__


typedef void(*list_free_t)(void* data);
typedef int(*list_cmp_t)(void* data1, void* data2);
typedef int(*list_print_t)(void* stream, unsigned int index, void* data);
struct list;
struct iter;

struct list* list_new(list_free_t f, list_cmp_t cmp, list_print_t print);
int list_add(struct list* list, void* data);
int list_add_sorted(struct list* list, void* data);
void* list_get(struct list* list, unsigned int index);
int list_remove(struct list* list, unsigned int index);
int list_sort(struct list *list);
void list_del(struct list* list);
int list_size(struct list *list);
struct iter* list_iter(struct list* list);
int list_find(struct list *list, void* data);
int list_print(struct list *list, void *stream);
#endif