#ifndef __DICT_H__
#define __DICT_H__  

struct dict;
struct iter;

typedef int (*dict_print_t)(void* stream, unsigned int index, const char *key, const void* val);
typedef void(*dict_free_t)(void* val);
typedef int(*dict_cmp_t)(void* data1, void* data2);
struct dict* dict_new(dict_free_t f, dict_cmp_t cmp, dict_print_t print);
int dict_set(struct dict *dict, char* key, void* val);
void* dict_get(struct dict* dict, char* key);
void dict_del(struct dict* dict);
int dict_print(struct dict* dict, void* stream);
struct iter* dict_iter(struct dict* dict);
#endif