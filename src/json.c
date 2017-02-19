#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "json.h"
#include "fsutils.h"
#include "utils.h"
#include "list.h"
#include "dict.h"
#include "iter.h"
#define MODULE "JSON"
#include "trace.h"

#define isJsonErr(x)        (inRange((x), JSON_ERR_BEGIN, JSON_ERR_LAST))
#define JsonErr(x)          (-(JSON_ERR_BEGIN + (x)))
#define JSON_MAX_VAL_SIZE   (sizeof(double))
#define MIN2(x,y)           ((x)<(y)?(x):(y))

/*  Json Value */
struct json
{
    int type;
    union
    {
        bool boolean;
        unsigned int uint_number;
        long long_number;
        double double_number;
        char *str;
        struct dict* dict;
        struct list* list;
        struct json* json;
        void *data;
    };
};

struct io_stream
{
    FILE *stream;
    unsigned int depth;
    unsigned int indent;
};

/* Function declarations */
static void free_obj(int type, void* data);

static struct json* parse(char *start, char *end, int *err);
static struct json* parse_val(char *start, char *end, char **raw, int *err);
static struct list* parse_list(char *start, char *end, char **raw, int *err);
static struct dict* parse_dict(char *start, char *end, char **raw, int *err);


static int print(FILE *stream, struct json* json, unsigned int indent, unsigned int depth);
static int print_val(FILE *stream, struct json* json, unsigned int indent, unsigned int depth);
static int print_dict(FILE *stream, struct dict* dict, unsigned int indent, unsigned int depth);
static int print_list(FILE *stream, struct list* list, unsigned int indent, unsigned int depth);
static int print_list_cb(void* stream, unsigned int index,void *data);
static int print_dict_cb(void* stream, unsigned int index, char *key, void* val);
static int print_indent(FILE *stream, int indent, int depth);

static struct json* clone(struct json* json, int *err);
static struct list* clone_list(struct list *list, int *err);
static struct dict* clone_dict(struct dict* dict, int *err);
static void* clone_obj(int type, void *data, int *err);

static void init_val(struct json *json, int type, void* data);

#if 0
static const char *type_str(unsigned int type)
{
    const char* _table[] = {
    [JSON_TYPE_INVALID] = "invalid",
    [JSON_TYPE_NULL]    = "null",
    [JSON_TYPE_DICT]    = "dict",
    [JSON_TYPE_STR]     = "str",
    [JSON_TYPE_BOOL]    = "bool",
    [JSON_TYPE_INT]     = "int",
    [JSON_TYPE_UINT]    = "uint",
    [JSON_TYPE_DOUBLE]  = "double",
    [JSON_TYPE_HEX]     = "hex",
    [JSON_TYPE_OCTAL]   = "octal",        
    [JSON_TYPE_LIST]    = "list",
    [JSON_TYPE_OBJ]     = "obj",
    };

    if(type < sizeof(_table)/sizeof(char*)){
        return _table[type];
    } else {
        return "Invalid Type";
    }
}
#endif

static int json_cmp(struct json *j1, struct json *j2)
{
    int ret = -1;
    if(j1->type == j2->type){
        switch(j1->type){
            case JSON_TYPE_BOOL:
                ret = (j1->boolean == j2->boolean) ? 0 : ((j1->boolean) ? -1 : 1);
                break;

            case JSON_TYPE_UINT:
            case JSON_TYPE_INT:
                ret = j2->long_number - j1->long_number;
                break;

            case JSON_TYPE_HEX:
            case JSON_TYPE_OCTAL:
                ret = j2->uint_number - j1->uint_number;
                break;

            case JSON_TYPE_DOUBLE:
                ret = j2->double_number - j1->double_number;
                break;

            case JSON_TYPE_LIST:
                ret = list_size(j2->list) - list_size(j1->list);
                break;

            case JSON_TYPE_STR:
                ret = strcmp(j2->str, j1->str);
                break;

            case JSON_TYPE_DICT:
                ret = dict_size(j2->dict) - dict_size(j1->dict);
                break;

            case JSON_TYPE_OBJ:
                ret = json_cmp(j1->json, j2->json);
                break;

            default:
                    break;
        }
    } else {
        TRACE(ERROR,"Can not compare different JSON objects");
    }
    return ret;
}


static void init_val(struct json *json, int type, void* data)
{
    if(json){
        json->type = type;
        if(data){
            /* Set Value according to type*/
            switch(type){
                case JSON_TYPE_BOOL:
                    json->boolean = *(bool*)data;
                    break;

                case JSON_TYPE_UINT:
                case JSON_TYPE_INT:
                    memcpy(&json->long_number, data, sizeof(json->long_number));
                    break;

                case JSON_TYPE_HEX:
                case JSON_TYPE_OCTAL:
                    memcpy(&json->uint_number, data, sizeof(json->uint_number));
                    break;

                case JSON_TYPE_DOUBLE:
                    memcpy(&json->double_number, data, sizeof(json->double_number));
                    break;

                case JSON_TYPE_LIST:
                    json->list = (struct list *)data;
                    break;

                case JSON_TYPE_STR:
                    json->str = (char*)data;
                    break;

                case JSON_TYPE_DICT:
                    json->dict = (struct dict*)data;
                    break;

                case JSON_TYPE_OBJ:
                    json->json = (struct json*)data;
                    break;

                default:
                    json->data = NULL;
                    break;
            }
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
    }
}

static void free_obj(int type, void* data)
{
    switch(type){
        case JSON_TYPE_DICT:
            /* Free JSON Object*/
            dict_del(data);
        break;
        case JSON_TYPE_LIST:
            /* Free List*/
            list_del(data);
        break;
        case JSON_TYPE_STR:
            /* Free String */
            free(data);
        break;
        case JSON_TYPE_OBJ:
            json_del(data);
        break;
        default:
        break;
    }
}

/*
* @brief Parse a json list in buffer
* @param start Pointer to start of buffer
* @param end Pointer to end of buffer
* @param raw Pointer to plcae holder for data remaining after parsing
* @param err Pointer for error status
* @return Json list
*/
static struct list* parse_list(char *start, char *end,char **raw, int *err)
{
    struct list *list = NULL;
    struct json *json = NULL;
    char *begin = start;
    char *temp = NULL;

    /* Check data */
    if(start && end && raw && err && (start < end)){

        /* Check beginning, List should start with [*/
        if(*start != '['){
            TRACE(ERROR, "List must begin with [");
            *raw = begin;
            *err = JsonErr(JSON_ERR_PARSE);
            return NULL;
        }

        /* Skip all white space after [ */
        start = trim(start + 1, end);
        if(start == end){
            TRACE(ERROR, "Missing ]");
            *raw = begin;
            *err = JsonErr(JSON_ERR_PARSE);
            return NULL;
        }

        /* Allocate a new List of Json Object */
        if(!(list = list_new((list_free_t)json_del, (list_cmp_t)json_cmp, (list_print_t)print_list_cb))){
            TRACE(ERROR, "Failed to init List");
            *err = JsonErr(JSON_ERR_NO_MEM);
            *raw = begin;
            return NULL;
        }

        /* List is terminated with ], Check for empty  list */
        if(*start == ']'){
            /* Empty List */
            *raw = start + 1;
            *err = JsonErr(JSON_ERR_SUCCESS);
            return list;
        } 

        /* Parse all data */
        for(*raw = begin; *start && (start < end); *raw = start ){

            /* Parse Value field for List object */
            if(!(json = parse_val(start, end, &temp, err))){
                TRACE(ERROR, "Failed to parse value");
                list_del(list);
                *raw = begin;
                *err = JsonErr(JSON_ERR_PARSE);
                return NULL;
            }

            /* Add Value in List */
            if((list_add(list, json)) < 0 ){
                TRACE(ERROR, "Failed to add value in List");
                *raw = begin;
                *err = JSON_ERR_NO_MEM;
                list_del(list);
                return NULL;
            }

            /* Trim data */
            start = trim(temp, end);
            if(start < end ){
                /* Check if end of list reached */
                if(*start == ']'){
                    *raw = start + 1;
                    *err = JsonErr(JSON_ERR_SUCCESS);
                    return list;
                } else if(*start != ','){
                    /* List Must have comma */
                    TRACE(ERROR, "Missing ,");
                    *raw = begin;
                    list_del(list);
                    *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }
                /* Trim after comma */
                start = trim(start+1, end);
            }
        }
        /* Failed to find the end of list */
        if(list)
            list_del(list);

        *err = JsonErr(JSON_ERR_PARSE);
        TRACE(ERROR, "Parsing error!! Missing ]");
    } else {
        TRACE(ERROR, "Invalid arguments");
        if(err)
         *err = JsonErr(JSON_ERR_PARSE);
    }

    return NULL;
}

/*
* @brief Parse a json value in buffer
* @param start Pointer to start of buffer
* @param end Pointer to end of buffer
* @param raw Pointer to plcae holder for data remaining after parsing
* @param err Pointer for error status
* @return Json value
*/
static struct json* parse_val(char *start, char *end, char **raw, int *err)
{
    struct json *json = NULL;
    char *begin = start;
    char *temp = NULL;
    double double_val = 0.0f;
    unsigned int uint_number;
    bool bool_val = false;
    long long_val = 0;
    int type = JSON_TYPE_INVALID;
    bool overflow = false;
    bool unsigned_flag = false; 
    int len = 0;
    int sign = 1;
    void *val = NULL;

    /* Check for valid dat */
    if(start && end && raw && err && (start < end)){
        /* Parse Value */
        switch(*start){
            case '{':
                type = JSON_TYPE_DICT;

                /* Parse Json Object */
                if(!(val = (void*)parse_dict(start, end, &temp, err))){
                    TRACE(ERROR,"Failed to parse dict");
                    *raw = begin;
                    *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }
                *raw = temp;
                
                break;

            case '[':
                type = JSON_TYPE_LIST;

                /* Parse List */
                if(!(val = (void*)parse_list(start, end, &temp, err))){
                    TRACE(ERROR,"Failed to parse list");
                    *err = JsonErr(JSON_ERR_PARSE);
                    *raw = begin;
                    return NULL;
                }
                *raw = temp;
                break;

            case 't' :case 'f':
                type = JSON_TYPE_BOOL;
                /* Parse boolean */
                bool_val = parse_boolean(start, end, &temp);

                /* Check for failure */
                if(start == temp){
                    TRACE(ERROR,"Failed to parse object");
                    *err = JsonErr(JSON_ERR_PARSE);
                    *raw = begin;
                    return NULL;
                }
                *raw = temp;
                val = (void*)&bool_val;
                break;

            case '"':
                /* Parse quoted string */
                type = JSON_TYPE_STR;
                len = 0;

                /* Check for failure */
                if(!(val = (void*)parse_str(start, end, &temp, &len))){
                    TRACE(ERROR, "Failed to parse object");
                    *err = JsonErr(JSON_ERR_PARSE);
                    *raw = begin;
                    return NULL;
                }
                *raw = temp;
                break;
            case 'n': case 'N':
                type = JSON_TYPE_NULL;
                if(((end - start) < 4) || ((strncasecmp(start, "null", 4)) != 0)){
                    TRACE(ERROR, "Failed to parse null");
                    *raw = begin;
                    *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }
                *raw = start + 4;
            break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            case '.': case '+':case '-':

                type = JSON_TYPE_INT;
                overflow = false;
                unsigned_flag = false;
                sign = 1;
                if(*start == '-'){
                    sign = -1;
                    start++;
                } else if(*start == '+'){
                    start++;
                }
                /* Check if we are at the end of data */
                if(start >= end){
                    TRACE(ERROR,"Failed to parse number, only sign found");
                    *err = JsonErr(JSON_ERR_PARSE);
                    *raw = begin;
                    return NULL;
                }
                /* Parse Number */
                if(*start == '.'){
                    start++;
                    /* Parse double value*/
                    double_val = parse_float(start, end, &temp);
                    if(start == temp){
                        TRACE(ERROR,"Failed to parse double value");
                        *err = JsonErr(JSON_ERR_PARSE);
                        *raw = begin;
                        return NULL;
                    }
                    /* Parsing successful*/
                    type = JSON_TYPE_DOUBLE;
                    double_val *= sign;
                    val = (void*)&double_val;

                } else if(is_hex(start, end)){
                    start +=2;
                    /* Parse hex value */
                    uint_number = parse_hex(start, end, &temp, &overflow);
                    if(start == temp){
                        TRACE(ERROR,"Failed to parse hex value");
                        *err = JsonErr(JSON_ERR_PARSE);
                        *raw = begin;
                        return NULL;
                    } else if(sign == -1 ){
                        TRACE(ERROR,"Hex value can not be negative");
                        *err = JsonErr(JSON_ERR_PARSE);
                        *raw = begin;
                        return NULL;
                    }
                    /* Parsing successful*/
                    val = (void*)&uint_number;
                    type = JSON_TYPE_HEX;
                } else if(is_octal(start, end)){
                    start++;
                    /* Parse octal values */
                    uint_number = parse_octal(start, end, &temp, &overflow);
                    if(start == temp){
                        TRACE(ERROR,"Failed to parse octal value");
                        *err = JsonErr(JSON_ERR_PARSE);
                        *raw = begin;
                        return NULL;
                    } else if(sign == -1 ){
                        TRACE(ERROR,"Octal value can not be negative");
                        *err = JsonErr(JSON_ERR_PARSE);
                        *raw = begin;
                        return NULL;
                    }
                    /* Parsing successful*/
                    val = (void*)&uint_number;
                    type = JSON_TYPE_OCTAL;
                } else {
                    long_val = parse_int(start, end, &temp, &overflow, &unsigned_flag);
                    if(start == temp){
                        TRACE(ERROR,"Failed to parse octal value");
                        *err = JsonErr(JSON_ERR_PARSE);
                        *raw = begin;
                        return NULL;
                    }
                    
                    /* Parsing successful*/
                    
                    long_val *= sign;
                    val = (void*)&long_val;
                    if(unsigned_flag){
                        type = JSON_TYPE_UINT;
                    }
                    /* Check if there is also a fractional part */
                    start = temp;
                    if((start < end) &&( *start == '.')){
                        start++;
                        double_val = parse_float(start, end, &temp);
                        if(start == temp){
                            TRACE(ERROR,"Failed to parse decimal value");
                            *err = JsonErr(JSON_ERR_PARSE);
                            *raw = begin;
                            return NULL;
                        }
                        /* Check sign */
                        if(long_val > 0){
                            double_val += long_val;
                        } else {
                            double_val = long_val - double_val;
                        }
                        type = JSON_TYPE_DOUBLE;
                        val = (void*)&double_val;
                    }
                }
                *raw = trim(temp, end);
                *err = JsonErr(JSON_ERR_SUCCESS);
                break;

            default:
                TRACE(ERROR,"Missing Value");
                *err = JsonErr(JSON_ERR_PARSE);
                *raw = begin;
                return NULL;
        }
        /* Allocate json vlue*/
        if(!(json = json_new())){
            TRACE(ERROR," Failed to alocate memeory");
            *err = JsonErr(JSON_ERR_NO_MEM);
            *raw = begin;
            /* Make Sure to free what we allocated*/
            free_obj(type, val);
        } else {
            init_val(json, type, val);
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
        if(err)
         *err = JsonErr(JSON_ERR_PARSE);
    }
    return json;
}

static struct dict* parse_dict(char *start,  char *end,  char **raw, int *err)
{
    char *begin = start;
    char *key = NULL;
    char *temp = NULL;
    struct json* json = NULL;
    struct dict* dict = NULL;
    int len = 0;

    /* Check for args */
    if(start && end && raw && err){
        /* Nothing Parsed */
        *raw = start;
         if(*start == '{'){
             /* Trim after { */
            start = trim(start + 1, end);

            /* Check if we are at the end */
            if(start >= end){
                TRACE(ERROR,"Missing }");
                *err = JsonErr(JSON_ERR_PARSE);
                *raw = begin;
                return NULL;
            }

            if(!(dict = dict_new((dict_free_t)json_del, (dict_cmp_t)json_cmp, (dict_print_t)print_dict_cb))){
                TRACE(ERROR,"Failed to allocate dict object");
                *err = JsonErr(JSON_ERR_NO_MEM);
                *raw = begin;
                return NULL;
            }

            /* Empty Object*/
            if(*start == '}'){
                temp = trim(start + 1, end);
                *err = JsonErr(JSON_ERR_SUCCESS);
                *raw = temp;
                return dict;
            }
            
            /* Traverse all rest of data  */
            for(*raw = begin; *start && (start < end); *raw = start){

                /* Rest Should bey Key:Val pair */
                key = parse_str(start, end, &temp, &len);
                if(!key || (temp == start)){
                    TRACE(ERROR,"Missing Key, should start with \"");
                    dict_del(dict);
                    *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }
                
                /* Trim whitespace */
                start = trim(temp, end);
                if(( start >= end ) || *start != ':'){
                    TRACE(ERROR,"Missing :");
                    dict_del(dict);
                    free(key);
                    *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }

                /* Trim white space after collon */
                start = trim(start + 1, end);
                if( start >= end ){
                    TRACE(ERROR,"Missing Value after :");
                    dict_del(dict);
                    free(key);
                    *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }

                /* Parse current Value */
                if(!(json = parse_val(start, end, &temp, err))){
                    TRACE(ERROR,"Failed to parse value for %s", key);
                    dict_del(dict);
                    free(key);
                    *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }

                /* Check duplicate entry */
                if(dict_get(dict, key)){
                    TRACE(ERROR,"Duplicate Key %s in json object", key);
                    dict_del(dict);
                    free(key);
                    *err = JsonErr(JSON_ERR_KEY_REPEAT);
                    return NULL;
                }

                /* Add Key value pair in json object */
                if((dict_set(dict, key, json)) < 0){
                    TRACE(ERROR,"Failed to add value for %s in json object", key);
                    dict_del(dict);
                    free(key);
                    *err = JsonErr(JSON_ERR_NO_MEM);
                    return NULL;
                }

                /* Trim */
                start = trim(temp, end);
                if(start >= end ){
                    TRACE(ERROR,"Missing }");
                    dict_del(dict);
                    *err = JsonErr(JSON_ERR_NO_MEM);
                    return NULL;
                }
                
                /* Only valid characters are comma and } */
                switch(*start){
                    case ',':
                        /* Process next field */
                        start = trim(start + 1, end);
                        continue;
                        break;

                    case '}':
                        /* Object closure */
                        start = trim(start + 1, end);
                        /* Whats remaining */
                        *raw = start;
                        return dict;

                    default:
                        TRACE(ERROR,"Missing ,");
                        *raw = begin;
                        dict_del(dict);
                        if(err)
                            *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }
            }
         }
         *err = JsonErr(JSON_ERR_SUCCESS);
    } else {
        TRACE(ERROR, "Invalid arguments");
        if(err)
         *err = JsonErr(JSON_ERR_PARSE);
    }

    return dict;
}
/*
* @brief Parse a json object in buffer
* @param start Pointer to start of buffer
* @param end Pointer to end of buffer
* @param err Pointer for error status
* @return Json object
*/
static struct json* parse(char *start,  char *end, int *err)
{
    char *temp = NULL;
    struct json* json = NULL;
    struct list *list = NULL;
    struct dict* dict = NULL;

    /* Check for args */
    if(start && end && err && (start < end)){
        /* Trim Extra Space */
        start = trim(start, end);

        /* Object should start with { */
        if(*start == '{'){
            if(!(dict = parse_dict(start, end, &temp, err))){
                TRACE(ERROR,"Failed to parse dict");
                dict_del(dict);
            } else {
                /* Alocate JSON object */
                if(!(json = json_new())){
                    TRACE(ERROR,"Failed to allocate json object");
                    *err = JsonErr(JSON_ERR_NO_MEM);
                    dict_del(dict);
                } else {
                    init_val(json, JSON_TYPE_DICT, dict);
                    *err = JsonErr(JSON_ERR_SUCCESS);
                }
            }
        } else if(*start == '['){
            if(!(list = parse_list(start, end, &temp, err))){
                TRACE(ERROR,"Failed to parse list");
                list_del(list);
            } else {
                /* Alocate JSON object */
                if(!(json=json_new())){
                    TRACE(ERROR,"Failed to allocate json object");
                    *err = JsonErr(JSON_ERR_NO_MEM);
                    list_del(list);
                } else {
                    init_val(json, JSON_TYPE_LIST, list);
                    *err = JsonErr(JSON_ERR_SUCCESS);
                }
            }
        } else {
             TRACE(ERROR,"Failed to parse JSON Object Invalid character %c", *start);
             *err = JsonErr(JSON_ERR_PARSE);
        }
        if(json){
            start = trim(temp, end);
            if(start < end){
                TRACE(ERROR,"Invalid character %c after json object", *start);
                *err = JsonErr(JSON_ERR_PARSE);
                json_del(json);
                json = NULL;
            }
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
        if(err)
         *err = JsonErr(JSON_ERR_PARSE);
    }
    return json;
}

/*
* @brief Print Json Value to stream
* @param stream Stream for output
* @param val Json Value
* @param indent Indentation to be used for pertty printing
* @param depth Depth depth inside Json Obect
* @return number of bytes printed
*/
static int print_val(FILE *stream, struct json* json, unsigned int indent, unsigned int depth)
{
    int ret = 0;
    switch(json->type){
        case JSON_TYPE_NULL:
        ret = fprintf(stream, "null");
        break;
        case JSON_TYPE_DICT:
            ret = print_dict(stream, json->dict, indent, depth + 1);
        break;
        case JSON_TYPE_STR:
            ret = fprintf(stream, "\"%s\"", json->str);
        break;
        case JSON_TYPE_BOOL:
            ret = fprintf(stream, "%s", json->boolean == true ? "true" : "false");
        break;
        case JSON_TYPE_DOUBLE:
            ret = fprintf(stream, "%lf", json->double_number);
        break;
        case JSON_TYPE_INT:
            ret = fprintf(stream, "%ld", json->long_number);
        break;
        /*
        case JSON_TYPE_LONG:
            ret = fprintf(stream, "%llu", json->long_number);
        break;*/
        case JSON_TYPE_UINT:
            ret = fprintf(stream, "%lu", (unsigned long)json->long_number);
        break;
        case JSON_TYPE_HEX:
            ret = fprintf(stream, "0x%0x", json->uint_number);
        break;
        case JSON_TYPE_OCTAL:
            ret = fprintf(stream, "0%o", json->uint_number);
        break;
        case JSON_TYPE_LIST:
            ret = print_list(stream, json->list, indent, depth);
        break;
        case JSON_TYPE_OBJ:
            ret = print(stream, json->json, indent, depth + 1);
        break;
        default:
        TRACE(WARN,"Unknown type:%d", json->type);
        break;
    }

    return ret;
}

/*
* @brief Print Indentation to stream
* @param stream Stream for output
* @param indent Indentation to be used for pertty printing
* @param depth Depth depth inside Json Obect
* @return number of bytes printed
*/
static int print_indent(FILE *stream, int indent, int depth)
{
    int indentation;
    if((depth <= 0) ||( indent <= 0)){
        if(indent)
            return fprintf(stream, "\n");
        else
            return 0;
    }

    indentation = indent * depth;
    fprintf(stream, "\n%*c",indentation, ' ');
    return indentation;
}


/*
* @brief Print Json List to stream
* @param stream Stream for output
* @param list Json List
* @param indent Indentation to be used for pertty printing
* @param depth Depth depth inside Json Obect
* @return number of bytes printed
*/
static int print_list(FILE *stream, struct list *list, unsigned int indent, unsigned int depth)
{
    int ret = 0;
    struct io_stream io_stream = {.stream = stream, .indent=indent, .depth=depth};
    if(list){
        ret += fprintf(stream, "[");
        ret += list_print(list, &io_stream);
        ret += fprintf(stream, "]");
    }
    return ret;
}

static int print_list_cb(void* stream, unsigned int index, void *data)
{
    int ret = 0;
    struct io_stream* io_stream = (struct io_stream*)stream;
    if(index){
        ret += fprintf(io_stream->stream, ",");
    }
    ret += print_val(io_stream->stream, data, io_stream->indent, io_stream->depth);
    return ret;
}

static int print_dict_cb(void* stream, unsigned int index, char *key, void* data)
{
    int ret = 0;
    struct io_stream* io_stream = (struct io_stream*)stream;
    if(index){
        ret += fprintf(io_stream->stream, ",");
    }
    ret += print_indent(io_stream->stream, io_stream->indent, io_stream->depth + 1);
    ret += fprintf(io_stream->stream, "\"%s\":", key);
    ret += print_val(io_stream->stream, data, io_stream->indent, io_stream->depth + 1);
    return ret;
}
/*
* @brief Print Dict to stream
* @param stream Stream for output
* @param json Json object
* @param indent Indentation to be used for pertty printing
* @param depth Depth depth inside Json Obect
* @return number of bytes printed
*/
static int print_dict(FILE *stream, struct dict *dict, unsigned int indent, unsigned int depth)
{
    int ret = 0;
    struct io_stream io_stream = {.stream = stream, .indent=indent, .depth=depth};
    
    /* Check data */
    if(dict){
        //ret += print_indent(stream, indent, depth);
        ret += fprintf(stream, "{");
        ret += dict_print(dict, &io_stream);
        ret += print_indent(stream, indent, depth);
        ret += fprintf(stream, "}");
        //ret += print_indent(stream, indent, depth);
    }

    return ret;
}

static int print(FILE *stream, struct json *json, unsigned int indent, unsigned int depth)
{
    int ret = -1;
    if(json){
        if(json->type == JSON_TYPE_OBJ){
            ret = print(stream, json->json, indent, depth);
        } else if(json->type == JSON_TYPE_LIST){
            ret = print_list(stream, json->list, indent, depth);
        } else if(json->type == JSON_TYPE_DICT){
            ret = print_dict(stream, json->dict, indent, depth);
        } else if(json->type == JSON_TYPE_NULL){
            ret = print_val(stream, json, indent, depth);
        } else {
            TRACE(ERROR,"Invalid Json Object : %d", json->type);
            ret = JsonErr(JSON_ERR_ARGS);
        }
    } else {
        TRACE(ERROR,"Null Object");
        ret = JsonErr(JSON_ERR_ARGS);
    }
    return ret;
}

static void* clone_obj(int type, void* src, int *err)
{
    void *data = NULL;
    if(src && err){
        switch(type){
            case JSON_TYPE_STR:
                if(!(data = strdup(src))){
                    *err = JsonErr(JSON_ERR_NO_MEM);
                }
            break;
            case JSON_TYPE_LIST:
                data = clone_list(src, err);
            break;
            case JSON_TYPE_DICT:
                data = clone_dict(src, err);
            break;
            case JSON_TYPE_OBJ:
                data = clone(src, err);
            break;
            default:
                data = src;
                *err = JsonErr(JSON_ERR_SUCCESS);
            break;  
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
        if(err)
            *err = JsonErr(JSON_ERR_ARGS);
    }
    return data;
}

/*
* @brief Clone json value
* @param src_val Json value to be cloned
* @param err plcaeholder for error
* @return Pointer to new generated json value
*/
struct json* clone(struct json* src, int *err)
{
    struct json* json = NULL;
    void* data = NULL;
    if(src){
        /* Duplicate Json Data */
        if((data = clone_obj(src->type, src->data, err))){
            if((src->type != JSON_TYPE_LIST)&&
               (src->type != JSON_TYPE_DICT)&&
               (src->type != JSON_TYPE_OBJ)&&
               (src->type != JSON_TYPE_STR)){
                /* For Non Pointer data set pointer to buffer from where it will be copied */
                data = &src->data;
            }
            if((json = json_new())){
                *err = JsonErr(JSON_ERR_SUCCESS);
                init_val(json, src->type, data);
            } else {
                TRACE(ERROR,"Failed to allocate json val");
                *err = JsonErr(JSON_ERR_NO_MEM);
                /* Free memory which was allocated for clone */
                free_obj(src->type, data);
            }
        } else {
            TRACE(ERROR,"Failed to allocate data");
        }
    }

    return json;
}

/*
* @brief Clone json list
* @param src_list Json list to be cloned
* @param err plcaeholder for error
* @return Pointer to new generated json list
*/
static struct list* clone_list(struct list *src_list, int *err)
{
    struct list* list = NULL;
    struct json* src_json = NULL;
    struct json* json = NULL;
    struct iter* iter = NULL;

    if(src_list){
        if((iter = list_iter(src_list))){
            if((list = list_new((list_free_t)json_del, (list_cmp_t)json_cmp, (list_print_t)print_list_cb))){
                for(src_json = iter_next(iter); src_json; src_json = iter_next(iter)){
                    if((json = json_clone(src_json, err))){
                        if((list_add(list, json)) < 0){
                            TRACE(ERROR,"Failed to add in list");
                            list_del(list);
                            list = NULL;
                            *err = JsonErr(JSON_ERR_NO_MEM);
                            break;
                        } else {
                            *err = JsonErr(JSON_ERR_SUCCESS);
                        }
                    }
                }
            } else {
                TRACE(ERROR,"Memory allocation failure");
                *err = JsonErr(JSON_ERR_NO_MEM);
            }
            iter_del(iter);
        } else {
            TRACE(ERROR,"Memory allocation failure");
            *err = JsonErr(JSON_ERR_NO_MEM);
        }
    }
    return list;
}

static struct dict* clone_dict(struct dict *src_dict, int *err)
{
    struct dict *dict = NULL;
    struct json *json = NULL;
    struct iter *iter = NULL;
    char *key;
    if(src_dict && err){
        /* Iterator for original dict */
        if((iter = dict_iter(src_dict))){
            if((dict = dict_new((dict_free_t)json_del, (dict_cmp_t)json_cmp, (dict_print_t)print_dict_cb))){
                /* Traverse entire dict and duplicate*/
                for(key = iter_next(iter); key; key = iter_next(iter)){
                    /* duplicate key */
                    /* Value should be here, if not found some internal error occurred*/
                    if((json = dict_get(src_dict, key))){
                        if((json = json_clone(json, err))){
                            if((dict_set(dict, key, json))>=0){
                                *err = JsonErr(JSON_ERR_SUCCESS);
                            } else {
                                TRACE(ERROR,"Failed to set dict entry");
                                *err = JsonErr(JSON_ERR_NO_MEM);
                                json_del(json);
                            }
                        } else {
                            TRACE(ERROR,"Failed to allocate json val");
                            *err = JsonErr(JSON_ERR_NO_MEM);
                        }
                    } else {
                        TRACE(ERROR,"Internal error null value");
                        *err = JsonErr(JSON_ERR_NO_MEM);
                    }
                }
            } else {
                TRACE(ERROR,"Memory allocation failure");
                *err = JsonErr(JSON_ERR_NO_MEM);
            }
            
            iter_del(iter);
        } else {
            TRACE(ERROR,"Failed to get iterator");
            *err = JsonErr(JSON_ERR_NO_MEM);
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
        if(err)
            *err = JsonErr(JSON_ERR_ARGS);
    }
    return dict;
}

/*
* @brief Load a json oject from buffer
* @param start Pointer to start of buffer
* @param end Pointer to end of buffer
* @param err Pointer for error status
* @return Json object
*/
struct json* json_loads(char *start, char* end, int *err)
{
    struct json *json = NULL;

    /* Check data */
    if( start && end ){
        /* Process Buffer */
        json = parse(start, end, err);
    } else {
        /* Invalid input */
        TRACE(ERROR,"Invalid params");
        if(err) 
            *err = JsonErr(JSON_ERR_ARGS);
    }

    return json;
}

/*
* @brief Load a json oject from file
* @param fname filename
* @param err Pointer for error status
* @return Json object
*/
struct json* json_load(char* fname, int *err)
{
    struct json* json = NULL;
    unsigned int len = 0;
    char *buffer = NULL;

    /* Check for data*/
    if(fname){
        /* Read all data */
        buffer = readall(fname, &len);

        /* Check if data was read correctly */            
        if(( len > 0 ) && buffer){

            /* Start Parsing */
            json = json_loads(buffer, buffer + len, err);

            /* Free Buffer */ 
            free(buffer); 
        } else {
            /* Failed to read file in buffer*/
            TRACE(ERROR,"Failed to read file : %s", fname);

            if(err)
                *err = JsonErr(JSON_ERR_SYS);
            json = NULL;
        }
    } else {
        /* Invalid arguments*/
        TRACE(ERROR,"Invalid args");
        if(err)
            *err = JsonErr(JSON_ERR_ARGS);
    }
    return json;
}

/*
* @brief Print json object to stdout
* @param json Json object
* @param indent Indetation for pretty printing
* @return Number of bytes printed
*/
int json_print(struct json *json, unsigned int indent)
{
    return print(stdout, json, indent, 0);
}

/*
* @brief Print json object to file
* @param json Json object
* @param fname Filename for output
* @param indent Indetation for pretty printing
* @return Number of bytes printed
*/
int json_printf(struct json *json, char *fname, unsigned int indent)
{
    FILE *fp = fopen(fname, "w");
    int len = 0;
    if(fp){
        len = print(stdout, json, indent, 0);
        fclose(fp);
    } else {
        TRACE(ERROR,"Failed to open file : %s", fname);
        len = JsonErr(JSON_ERR_SYS);
    }
   return len;
}


/*
* @brief Print json object to a buffer
* @param json Json object
* @param buffer Buffer where json needs to be printed
* @param size Size of buffer
* @param indent Indetation for pretty printing
* @return Number of bytes printed
*/
int json_prints(struct json *json, char *buffer, unsigned int size, unsigned int indent)
{
    int len = 0;
    FILE *fp = fmemopen((void*)buffer, size,  "w");
    if(fp){
        len = print(stdout, json, indent, 0);
        buffer[len] = 0;
        fclose(fp);
    } else {
        TRACE(ERROR,"Failed to initialized buffer as file");
        len = JsonErr(JSON_ERR_SYS);
    }
   return len;
}


/*
* @brief Convert json to string representation (Dynamically generated buffer)
* @param json Json object
* @param len Pointer to length where length of string will be saved
* @param indent Indetation for pretty printing
* @return Pointer to json string representation
*/
char* json_str(struct json *json, int *len, unsigned int indent)
{
    char **buf_ptr;
    char *buffer;
    size_t *size_ptr = NULL;
    int print_length = 0;
    FILE *fp = fdmemopen(&buf_ptr, &size_ptr);
    if(fp){
         print_length = print(stdout, json, indent, 0);
         buffer = *buf_ptr;
         fclose(fp);
         if(len)
            *len = print_length;
         if(print_length > 0)
            buffer[print_length] = 0;

         return buffer;
    } else {
        TRACE(ERROR,"Failed to initialize buffer as file");
        if(len)
            *len = JsonErr(JSON_ERR_SYS);
    }
   return NULL;

}

int set_dict(struct dict *dict, int type, char *key, void* val)
{
    int err = 0;
    struct json *orig = NULL;
    struct json *json = NULL;
    if(dict && key){
        if(!val){
            /* Remove from dict*/
            dict_set(dict, key, NULL);
        } else if((val = clone_obj(type, val, &err))){
            if((json = json_new())){
                init_val(json, type, val);
                /* Check if there is original value and its a list*/
                if((orig = dict_get(dict, key)) && (orig->type == JSON_TYPE_LIST)){
                    TRACE(DEBUG, "Duplicate Found");
                    /* Append in List*/
                    if((list_add(orig->list, json)) < 0){
                        TRACE(ERROR, "Failed to add in List");
                        err = JsonErr(JSON_ERR_NO_MEM);
                        json_del(json);
                    } else {
                        /*Success*/
                        err = JsonErr(JSON_ERR_SUCCESS);
                    }
                } else if((dict_set(dict, key, json))< 0){
                    TRACE(ERROR, "Failed to add in dict");
                    err = JsonErr(JSON_ERR_NO_MEM);
                    json_del(json);
                } else {
                    /*Success*/
                    err = JsonErr(JSON_ERR_SUCCESS);
                }
            } else {
                TRACE(ERROR,"Failed to allocate Json object");
                err = JsonErr(JSON_ERR_NO_MEM);
                /* Free the duplicate value */
                free_obj(type, val);
            }
        } else {
            TRACE(ERROR,"Failed to duplicate value");
            err = JsonErr(JSON_ERR_NO_MEM);
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
        err = JsonErr(JSON_ERR_ARGS);
    }
    return err;
}

int set_list(struct list *list, int type, void* val)
{
    int err = 0;
    struct json* json = NULL;
    if(list && val){
        if((val = clone_obj(type, val, &err))){
            if((json = json_new())){
                init_val(json, type, val);
                if((list_add(list, json)) < 0){
                    TRACE(ERROR,"Failed to add in list");
                    err = JsonErr(JSON_ERR_NO_MEM);
                    list_del(list);
                    json_del(json);
                } else {
                    /* We have changed the current Json object to its List version*/ 
                    err = JsonErr(JSON_ERR_SUCCESS);
                }
            }  else {
                TRACE(ERROR,"Failed to allocate Json object");
                err = JsonErr(JSON_ERR_NO_MEM);
                /* Free the duplicate value */
                free_obj(type, val);
            }
        } else {
            TRACE(ERROR,"Failed to duplicate value");
            err = JsonErr(JSON_ERR_NO_MEM);
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
        err = JsonErr(JSON_ERR_ARGS);
    }

    return err;
}

/*
* @brief Set Key in JSON object
* If Provided value is NULL key will be removed
* For List the key will be added
* For others values will be replcaed, if key exists or it will be added
* @param json Json object
* @param key Key for json
* @param type Type of value
* @param val Value that needs to be saved
* @return JSON_ERR Value
*/
int json_set(struct json *json, int type, char *key, void *val)
{
    int err = JSON_ERR_ARGS;
    struct dict* dict = NULL;
    struct list* list = NULL;

    if(json){
       switch(json->type){
            case JSON_TYPE_DICT:
                err = set_dict(json->dict, type, key, val);
            break;
            case JSON_TYPE_LIST:
                err = set_list(json->list, type, val);
            break;
            case JSON_TYPE_OBJ:
                err = json_set(json->json, type, key, val);
            break;
            case JSON_TYPE_NULL:
                if(val){
                    if(key){
                        if((dict = dict_new((dict_free_t)json_del, (dict_cmp_t)json_cmp, (dict_print_t)print_dict_cb))){
                            err = set_dict(dict, type, key, val);
                            if(JsonIsError(err)){
                                dict_del(dict);
                            } else {
                                init_val(json, JSON_TYPE_DICT, dict);
                            }
                        } else {
                            TRACE(ERROR, "Failed to allocate dict object");
                            err = JsonErr(JSON_ERR_NO_MEM);
                        }
                    } else {
                        if((list = list_new((list_free_t)json_del, (list_cmp_t)json_cmp, (list_print_t)print_list_cb))){
                            err = set_list(list, type, val);
                            if(JsonIsError(err)){
                                list_del(list);
                            } else {
                                init_val(json, JSON_TYPE_LIST, list);
                            }
                        } else {
                            TRACE(ERROR, "Failed to allocate dict object");
                            err = JsonErr(JSON_ERR_NO_MEM);
                        }
                    }
                } else {
                    TRACE(ERROR, "Value missing");
                    err = JsonErr(JSON_ERR_ARGS);
                }
            break;
            default:
                TRACE(ERROR, "Invalid operation on json object");
            break;
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
        err = JsonErr(JSON_ERR_ARGS);
    } 
    return err;
}

/*
* @brief Get a new empty json object
* @return json object
*/
struct json* json_new(void)
{
    struct json  *json = NULL;
    if((json = malloc(sizeof(struct json)))){
        json->data = NULL;
        json->type = JSON_TYPE_NULL;
    } else {
        TRACE(ERROR, "Failed to allocate value");
    }
    return json;
}

/*
* @brief Delete json object and free all data
* @param json Json data
*/
void json_del(struct json* json)
{
    if(json){
        /* Free Value based on the its type*/
        free_obj(json->type, json->data);
        free(json);
    } else {
        TRACE(ERROR, "Invalid arguments");
    }
}

/*
* @brief Get value for key from json
* List and json object value is not returned, instead iterator is returned 
* @param json Json object
* @param key Key to fetch
* @param type type of value
* @return value stored in json
*/
void* json_get(struct json *json, char *key)
{
    if(json){
        if(json->type == JSON_TYPE_DICT){
            return dict_get(json->dict, key);
        } else if(json->type == JSON_TYPE_OBJ){
            return json_get(json->json, key);
        } else {
            TRACE(ERROR,"Get operation not supported on this json object");
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
    }
    return NULL;
}

struct iter* json_iter(struct json* json)
{
    if(json){
        if(json->type == JSON_TYPE_LIST){
            return list_iter(json->list);
        } else if(json->type == JSON_TYPE_DICT){
            return dict_iter(json->dict);
        } else {
            TRACE(ERROR, "Method not supported for json object");
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
    }
    return NULL;
}

/*
* @brief Get iterator for json keys
* @param json Json object
* @return Iterator for json  keys
*/
int json_type(struct json* json)
{
    if(json){
        return json->type;
    }  else {
        TRACE(ERROR,"NULL object");
    }
    return JSON_TYPE_INVALID;
}

size_t json_size(struct json* json)
{
    size_t size = 0;
    if(json){
        switch(json->type){
            case JSON_TYPE_BOOL:
                size = sizeof(bool);
            break;
            case JSON_TYPE_NULL:
                size = 0;
            break;
            case JSON_TYPE_DICT:
                size = dict_size(json->dict);
            break;
            case JSON_TYPE_STR:
                size = strlen(json->str) + 1;
            break;
            case JSON_TYPE_LIST:
                size = list_size(json->list);
            break;
            case JSON_TYPE_INT:
            case JSON_TYPE_UINT:
                size = sizeof(json->long_number);
            break;
            case JSON_TYPE_HEX:
            case JSON_TYPE_OCTAL: 
                size = sizeof(json->uint_number);
            break;
            case JSON_TYPE_DOUBLE:       
                size = sizeof(json->double_number);
            break;
            case JSON_TYPE_OBJ:
                size = json_size(json->json);
            default:
                size = 0;
            break;

        }
    } else {
        TRACE(ERROR,"NULL object");
    }
    return size;
}

int json_val(struct json* json, void* buffer, size_t size)
{
    int len = -1;
    if(json){
        if(json->type == JSON_TYPE_OBJ){
            return json_val(json->json, buffer, size);
        } else if(json->type == JSON_TYPE_STR){
            len = MIN2(size, json_size(json));
            memcpy(buffer, json->str, len);
        } else if((json->type != JSON_TYPE_DICT) &&
                (json->type != JSON_TYPE_LIST)){
            len = MIN2(size, json_size(json));
            memcpy(buffer, &json->data, len);
        } else {
            TRACE(ERROR, "Method not supported for json object");
            len = JsonErr(JSON_ERR_ARGS);
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
        len = JsonErr(JSON_ERR_ARGS);
    }

    return len;
}

/*
* @brief Clone json object
* @param src_val Json object to be cloned
* @param err plcaeholder for error
* @return Pointer to new generated json object
*/
struct json* json_clone(struct json* json, int *err)
{
    if(json && err){
        json = clone(json, err);
    } else {
        TRACE(ERROR, "Null object");
        if(err)
            *err = JsonErr(JSON_ERR_ARGS);
    }
    
    return json;
}

/*
* @brief Get string representation of json error
* @param err error code
* @return string representation of error
*/
char* json_sterror(int err)
{
    static char* _err_table[] ={
        [JSON_ERR_SUCCESS]      = "Success", 
        [JSON_ERR_NO_MEM]       = "Not enough memory",
        [JSON_ERR_KEY_NOT_FOUND]= "Key not found",
        [JSON_ERR_ARGS]         = "Invalid arguments",
        [JSON_ERR_PARSE]        = "Parsing ERROR",
        [JSON_ERR_OVERFLOW]     = "Integer Overflow detected",
        [JSON_ERR_SYS]          = "System ERROR",
        [JSON_ERR_LAST]         = "Invalid ERROR",
    };

    /* Error is negative value of error code*/
    err = -err;

    /* Check for range of error*/
    if((err >= 0) && (err >= JSON_ERR_SUCCESS) && (err < JSON_ERR_LAST)){
        return _err_table[err];
    } else {
        return "Invalid error code";
    }
}
