#ifndef __JSON_H__
#define __JSON_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum json_err
{
    JSON_ERR_SUCCESS, 
    JSON_ERR_NO_MEM,
    JSON_ERR_KEY_NOT_FOUND,
    JSON_ERR_ARGS,
    JSON_ERR_PARSE,
    JSON_ERR_OVERFLOW,
    JSON_ERR_SYS,
};

enum json_type
{
    JSON_TYPE_INVALID,
    JSON_TYPE_NULL,
    JSON_TYPE_OBJ,
    JSON_TYPE_STR ,
    JSON_TYPE_BOOL,
    JSON_TYPE_INT,
    JSON_TYPE_UINT,
    JSON_TYPE_DOUBLE,
    JSON_TYPE_HEX,
    JSON_TYPE_OCTAL,        
    JSON_TYPE_LIST,
    /* Rest aren't for parsing*/
    JSON_TYPE_ITER,
    JSON_TYPE_DICT,
    JSON_TYPE_VAL,
};

struct json;
struct json_iter;

struct json* json_new(void);
void json_del(struct json* json);
struct json* json_loads(const char *start, const char* end, int *err);
struct json* json_load(const char* fname, int *err);
const char* json_get_err(int err);
const void* json_get(const struct json *json, const char *key, int *type);
int json_set(struct json *json, char *key, int type, void *val);
const void* json_iter_next(struct json_iter *iter,  int *type);
int json_print(const struct json *json, unsigned int indent);
int json_printf(const struct json *json, const char *fname, unsigned int indent);
int json_prints(const struct json *json, char *buffer, unsigned int size, unsigned int indent);
const char* json_str(const struct json *json, int *len, unsigned int indent);
struct json_iter *json_keys(const struct json* json);
struct json* json_clone(struct json* json);\
void json_iter_del(struct json_iter *iter);
#ifdef __cplusplus
}
#endif
#endif