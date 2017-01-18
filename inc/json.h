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
};

struct json;
struct json_iter;

int json_loads(const char *start, const char* end, struct json **json);
int json_load(const char* fname, struct json** json);
const char* json_get_err(int err);
const void* json_get(struct json *json, const char *key, int *type, struct json_iter *iter);
int json_set(struct json *json, const char *key, int type, void *val);
const void* json_iter_next(struct json_iter *iter,  int *type);
int json_print(struct json *json, unsigned int indent);
int json_printf(struct json *json, const char *fname, unsigned int indent);
int json_prints(struct json *json, const char *buffer, unsigned int size, unsigned int indent);

#ifdef __cplusplus
}
#endif
#endif