#ifndef __JSON_H__
#define __JSON_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum json_err
{
    JSON_ERR_BEGIN = 0,
    JSON_ERR_SUCCESS = 0, 
    JSON_ERR_NO_MEM,
    JSON_ERR_KEY_REPEAT,
    JSON_ERR_KEY_NOT_FOUND,
    JSON_ERR_ARGS,
    JSON_ERR_PARSE,
    JSON_ERR_OVERFLOW,
    JSON_ERR_SYS,
    JSON_ERR_LAST,
};

enum json_type
{
    JSON_TYPE_INVALID,
    JSON_TYPE_NULL,
    JSON_TYPE_DICT,
    JSON_TYPE_STR ,
    JSON_TYPE_BOOL,
    JSON_TYPE_INT,
    JSON_TYPE_UINT,
    JSON_TYPE_DOUBLE,
    JSON_TYPE_HEX,
    JSON_TYPE_OCTAL,        
    JSON_TYPE_LIST,
    JSON_TYPE_OBJ,
    JSON_TYPE_ITER,
};
#ifndef inRange
#define inRange(a,x,y)  (((a)>=(x)) && ((a) <= (y)))
#endif
#define JsonIsSuccess(x)    (-(x) == JSON_ERR_SUCCESS)
#define JsonIsError(x) !JsonIsSuccess(x)

struct json;
struct json_iter;

struct json* json_new(void);
void json_del(struct json* json);
struct json* json_loads(char *start, char* end, int *err);
struct json* json_load(char* fname, int *err);
char* json_get_err(int err);
void* json_get(struct json *json, char *key);
int json_set(struct json *json, int type, char *key, void *val);
void* json_iter_next(struct json_iter *iter,  int *type);
int json_print(struct json *json, unsigned int indent);
int json_printf(struct json *json, char *fname, unsigned int indent);
int json_prints(struct json *json, char *buffer, unsigned int size, unsigned int indent);
char* json_str(struct json *json, int *len, unsigned int indent);
struct json* json_clone(struct json* json, int *err);
void json_iter_del(struct json_iter *iter);
char* json_sterror(int err);
int json_type(struct json* json);
size_t json_size(struct json* json);
int json_val(struct json* json, void* buffer, size_t size);
struct iter* json_iter(struct json* json);
#ifdef __cplusplus
}
#endif
#endif