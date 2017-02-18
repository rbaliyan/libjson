#ifndef __LIST_H__
#define __LIST_H__


typedef void(*list_free_t)(void* data);
typedef int(*list_cmp_t)(const void* data1, const void* data2);
typedef int(*list_print_t)(const void* stream, unsigned int index, const void* data);
struct list;
struct iter;

struct list* list_new(list_free_t f, list_cmp_t cmp, list_print_t print);
int list_add(struct list* list, const void* data);
int list_add_sorted(struct list* list, const void* data);
void* list_get(const struct list* list, unsigned int index);
int list_remove(struct list* list, unsigned int index);
int list_sort(struct list *list);
void list_del(struct list* list);
int list_size(const struct list *list);
struct iter* list_iter(struct list* list);
int list_find(const struct list *list, const void* data);
int list_print(const struct list *list, const void *stream);
#endif