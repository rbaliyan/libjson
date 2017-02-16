#ifndef __ITER_H__
#define __ITER_H__

typedef void*(*iter_get_t)(void* data, void* current);
typedef void*(*iter_next_t)(void* data, void* current);
typedef void(*iter_free_t)(void* data);
typedef int(*iter_size_t)(void* data);

struct iter;
struct iter* iter_new(void *data, iter_free_t f, iter_get_t get, iter_next_t next, iter_size_t size);
void* iter_next(struct iter* iter);
void* iter_get(struct iter* iter);
void iter_reset(struct iter*iter);
void iter_del(struct iter *iter);
int iter_size(struct iter* iter);

#endif