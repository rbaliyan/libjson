#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "json.h"
#include "fsutils.h"
#include "utils.h"
#define MODULE "JSON"
#include "trace.h"

/*  Json Value */
struct json_val
{
    int type;
    union
    {
        unsigned int uint_number;
        const void* data;
        long long_number;
        double double_number;
        const char *string;
        bool boolean;
        struct json* json_obj;
        struct json_list* list;
    };
    struct json_val *next;
    struct json_val *prev;
};

/* Json List of values*/
struct json_list
{
    size_t count;
    struct json_val * start;
    struct json_val * end; 
};

/* A simple list based dictionary for json */
struct json_dict
{
    const char *key;
    struct json_val  *val;
    struct json_dict *next;
    struct json_dict *prev;
};

/* Json Object*/
struct json
{
    size_t count;
    struct json_list *list;
    struct json_dict *dict_start;
    struct json_dict *dict_end;
};

struct json_iter
{
    union{
        struct{
            struct json_list  *list;
            struct json_val  *current_val;
        };
        struct{
            struct json *json;
            struct json_dict *current_dict;
        };
        struct{
            const char *str;
            const char *current_ptr;
        };
    };    
    enum json_type type;
};


static int json_dict_add(struct json *json, const char *key, const struct json_val * val);
static int json_list_add(struct json_list * list, struct json_val * val);
static struct json * json_parse(const char *start, const char *end, int *err);
static struct json_list * json_parse_list(const char *start, const char *end, const char **raw, int *err);
static struct json_val * json_parse_val(const char *start, const char *end, const char **raw, int *err);
static struct json * json_parse_obj(const char *start, const  char *end, const  char **raw, int *err);

static struct json * json_alloc_obj();
static struct json_val * json_alloc_val(int json_type, const void* data);
static struct json_list * json_alloc_list();
static struct json_dict * json_alloc_dict(const char* key, const struct json_val  * val);
static void json_free(const struct json * json);
static void json_free_list(const struct json_list * list);
static void json_free_dict(const struct json_dict *dict);
static void json_free_val(const struct json_val * va);
static void json_free_type(int type, const void *data);
static char *buffer_alloc(size_t size);
static void buffer_free(const char * buffer);
static int json_print_val(FILE *stream, const struct json_val  *val, unsigned int indent, unsigned int level);
static int json_print_obj(FILE *stream, const struct json *json, unsigned int indent, unsigned int level);
static int json_indent(FILE *stream, int indent, int level);
static void* json_clone_type(int type, const void* data, int *err);
struct json_val* json_clone_val(const struct json_val* src_val, int *err);
static char* json_clone_str(const char* src_str, int *err);
static struct json_list* json_clone_list(const struct json_list *src_list, int *err);
static struct json* json_clone_obj(const struct json* src_json, int *err);
/*
* Json Object allocate 
*/
static struct json* json_alloc_obj()
{
    struct json *json = calloc(sizeof(struct json), 1);
    if(json){
        /* Set Params*/
        json->count = 0;
        json->dict_start = NULL;
        json->dict_end = NULL;
    } else {
        TRACE(ERROR, "Failed to allocate object");
    }
    return json;
}

/*
* Json Value allocate 
*/
static struct json_val * json_alloc_val(int json_type, const void* data)
{
    struct json_val  *val = calloc(sizeof(struct json_val ), 1);
    if(val){
        val->type = json_type;
        /* Set Value according to type*/
        switch(json_type){
            case JSON_TYPE_BOOL:
                val->boolean = *(bool*)data;
                break;

            case JSON_TYPE_UINT:
            case JSON_TYPE_INT:
                memcpy(&val->long_number, data, sizeof(val->long_number));
                break;
            case JSON_TYPE_HEX:
            case JSON_TYPE_OCTAL:
                memcpy(&val->uint_number, data, sizeof(val->uint_number));
                break;

            case JSON_TYPE_DOUBLE:
                memcpy(&val->double_number, data, sizeof(val->double_number));
                break;

            case JSON_TYPE_LIST:
                val->list = (struct json_list *)data;
                break;
            case JSON_TYPE_STR:
                val->string = (const char*)data;
                break;
            case JSON_TYPE_OBJ:
                val->json_obj = (struct json *)data;
                break;
            default:
                val->data = data;
        }
        val->next = val->prev = NULL;
    } else {
        TRACE(ERROR, "Failed to allocate value");
    }
    return val;
}

/*
* Json List allocate
*/
static struct json_list * json_alloc_list()
{
    struct json_list  *list = calloc(sizeof(struct json_list ), 1);
    if(list){
        /* Set Params*/
        list->start = list->end = NULL;
        list->count = 0;
    }else {
        TRACE(ERROR, "Failed to allocate list");
    }
    return list;
}
/*
* Json dict allocate 
*/
static struct json_dict * json_alloc_dict(const char* key, const struct json_val  * val)
{
    struct json_dict *dict = calloc(sizeof(struct json_dict), 1);
    if(dict){
        /* Set params*/
        dict->key = key;
        dict->val = (struct json_val *)val;
    }else {
        TRACE(ERROR, "Failed to allocate dict");
    }
    return dict;
}

/*
* Json Object free 
*/
static void json_free(const struct json* json)
{
    struct json_dict *dict  = NULL;
    struct json_dict *next_dict  = NULL;

    if(json->list){
        json_free_list(json->list);
    } else {
        /* Traverse all dicts*/
        for(dict = json->dict_start; dict; dict=next_dict){
            /* Save next dict and free current*/
            next_dict = dict->next;
            json_free_dict(dict);
        }
    }
}

/*
* Json dict free 
*/
static void json_free_dict(const struct json_dict *dict)
{
    /* Dict Key is also allcated at the time of parsing*/
    buffer_free(dict->key);

    /* Free Value */
    json_free_val(dict->val);
}

/*
* Json list free 
*/
static void json_free_list(const struct json_list * list)
{
    struct json_val * val = NULL;
    struct json_val * temp = NULL;
    /* Traverse all list*/
    for(val = list->start; val; val = temp){
        /* Save next and free current pointer*/
        temp = val->next;
        json_free_val(val);
    }
}

/*
* Json value free 
*/
static void json_free_val(const struct json_val * val)
{
    /* Free Value based on the its type*/
    switch(val->type){
        case JSON_TYPE_OBJ:
        /* Free JSON Object*/
        json_free(val->json_obj);
        break;
        case JSON_TYPE_LIST:
        /* Free List*/
        json_free_list(val->list);
        break;
        case JSON_TYPE_STR:
        /* Free String */
        buffer_free(val->string);
        break;
        default:
        /* Rest types do not need free*/
        break;
    }
}
/*
* Json Free based on type
*/
static void json_free_type(int type, const void *data)
{
    switch(type){
        case JSON_TYPE_STR:
            buffer_free(data);
        break;
        case JSON_TYPE_LIST:
            json_free_list(data);
        break;
        case JSON_TYPE_OBJ:
            json_free(data);
        break;
        case JSON_TYPE_VAL:
            json_free_val(data);
        break;
        default:
        break;
    }
}

/*
* Buffer allocate 
*/
static char *buffer_alloc(size_t size)
{
    return (char*)malloc(size);
}

/*
* Buffer free
*/
static void buffer_free(const char *buffer)
{
    free((void*)buffer);
}

/*
* A Simple dict based on linked list
*/
static int json_dict_add(struct json *json, const char *key, const struct json_val * val)
{
    int err = 0;
    struct json_dict* dict = NULL;
    struct json_val  *temp;
    /* Check data */
    if( json && key && val ){
        /* Travers dict */
        for(dict = json->dict_start; dict; dict = dict->next){
            if(strcmp(dict->key, key)== 0){
                fprintf(stderr, "Key %s repeated", key);
                TRACE(WARN,"Key %s repeated", key);
                return 0;
            }
        }
        /* Allocate dict */
        if((dict = json_alloc_dict(key, val))){
            /* Add in the end */
            dict->prev = json->dict_end;

             /* If end is not set it is first */
            if(json->dict_end){
                json->dict_end->next = dict;
            } else {
                json->dict_start = dict;
            }
            json->dict_end = dict;
            json->count++;
            err = 1;
        } else {
             TRACE(ERROR, "Dict Error");
        }
    } else {
       TRACE(WARN,"Arguments Error");
    }
    return err;
}

static int json_list_add(struct json_list *list, struct json_val * val)
{
    int err = 0;
    /* Check data */
    if(list && val){
        /* Add in the end */
        val->prev = list->end;
        /* If end is not set it is first */
        if(list->end){
            list->end->next = val;
        } else {
            list->start = val;
        }
        list->end = val;
        list->count++;
        err = 1;
    } else {
       TRACE(WARN,"Arguments Error");
    }
    return err;
}

/*
* Parse List of values
*/
static struct json_list * json_parse_list(const char *start, const char *end,const char **raw, int *err)
{
    struct json_list * list = NULL;
    struct json_val  *val = NULL;
    const char *begin = start;
    const char *temp = NULL;

    /* Check data */
    if(start && end && raw && (start < end)){

        /* Check beginning */
        if(*start != '['){
            fprintf(stderr, "List must begin with [\n");
            TRACE(ERROR, "PARSE ERROR");
            *raw = begin;
            return NULL;
        }

        /* Trim Data */
        start = trim(start + 1, end);
        if(start == end){
            fprintf(stderr, "Missing ]\n");
            TRACE(ERROR, "PARSE ERROR");
            *raw = begin;
            return NULL;
        }

        /* Allocate List */
        list = json_alloc_list();
        if(!list){
            TRACE(ERROR, "PARSE ERROR");
            fprintf(stderr, "Failed to init List\n");
            if(err)
                *err = JSON_ERR_NO_MEM;
            *raw = begin;
            return NULL;
        }

        /* Check for empty  list */
        if(*start == ']'){
            /* Empty List */
            *raw = start + 1;
            return list;
        } 

        /* Parse all data */
        for(*raw = begin; *start && (start < end); *raw = start ){

            /* Parse Value */
            val = json_parse_val(start, end, &temp, err);

            /* Check for failure */
            if(!val || (temp == start)){
                fprintf(stderr, "Failed to parse value\n");
                TRACE(ERROR, "PARSE ERROR");
                json_free_list(list);
                *raw = begin;
                if(err)
                    *err = JSON_ERR_PARSE;
                return NULL;
            }

            /* Add Value in List */
            if(!json_list_add(list, val)){
                fprintf(stderr, "Failed to add value in List\n");
                TRACE(ERROR, "PARSE ERROR");
                *raw = begin;
                if(err)
                    *err = JSON_ERR_NO_MEM;
                json_free_list(list);
                return NULL;
            }

            /* Trim data */
            start = trim(temp, end);
            if(start < end ){
                /* Check if end of list reached */
                if(*start == ']'){
                    *raw = start + 1;
                    return list;
                } else if(*start != ',') {
                    /* List Must have comma */
                    fprintf(stderr, "Missing ,");
                    TRACE(ERROR, "PARSE ERROR");
                    *raw = begin;
                    json_free_list(list);
                    if(err)
                        *err = JSON_ERR_PARSE;
                    return NULL;
                }
                /* Trim after comma */
                start = trim(start+1, end);
            }
        }
        
    }
    /* Failed to find the end of list */
    if(list){
        json_free_list(list);
    }
    if(err)
        *err = JSON_ERR_PARSE;

    return NULL;
}

/*
* Json Parse Value
*
*/
static struct json_val * json_parse_val(const char *start, const char *end,const  char **raw, int *err)
{
    struct json_val  *j_val = NULL;
    const char *begin = start;
    const char *temp = NULL;
    double double_val = 0.0f;
    unsigned int uint_number;
    bool bool_val = false;
    long long_val = 0;
    int json_type;
    bool overflow = false;
    bool unsigned_flag = false; 
    int len = 0;
    int sign = 1;
    void *val;

    /* Check for valid dat */
    if(start && end && raw && ( start < end )){
        /* Parse Value */
        switch(*start){
            case '{':
                json_type = JSON_TYPE_OBJ;

                /* Parse Json Object */
                val = (void*)json_parse_obj(start, end, &temp, err);

                /* Check for failure */
                if((start == temp) ||(val == NULL)){
                    fprintf(stderr, "Failed to parse object\n");
                    TRACE(ERROR, "PARSE ERROR");
                    start = begin;
                    return NULL;
                }
                *raw = temp;
                
                break;

            case '[':
                json_type = JSON_TYPE_LIST;

                /* Parse List */
                val = (void*)json_parse_list(start, end, &temp, err);

                /* Check for failure */
                if((start == temp) ||(val == NULL)){
                    fprintf(stderr, "Failed to parse object\n");
                    TRACE(ERROR, "PARSE ERROR");
                    return NULL;
                }
                *raw = temp;
                break;

            case 't' :case 'f':
                json_type = JSON_TYPE_BOOL;
                /* Parse boolean */
                bool_val = get_boolean(start, end, &temp);

                /* Check for failure */
                if(start == temp){
                    fprintf(stderr, "Failed to parse object\n");
                    TRACE(ERROR, "PARSE ERROR");
                    return NULL;
                }
                *raw = temp;
                val = (void*)&bool_val;
                break;

            case '"':
                /* Parse quoted string */
                json_type = JSON_TYPE_STR;
                len = 0;
                val = (void*)get_string(start, end, &temp, &len);

                /* Check for failure */
                if((start == temp) || !val){
                    fprintf(stderr, "Failed to parse object\n");
                    TRACE(ERROR, "PARSE ERROR");
                    *err = JSON_ERR_PARSE;
                    return NULL;
                }
                *raw = temp;
                break;
            case 'n': case 'N':
                json_type = JSON_TYPE_NULL;
                if(((end - start) < 4) || ((strncasecmp(start, "null", 4)) != 0)){
                    fprintf(stderr, "Failed to parse null\n");
                    TRACE(ERROR, "PARSE ERROR");
                    *err = JSON_ERR_PARSE;
                    return NULL;
                }
                *raw = start + 4;
            break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            case '.': case '+':case '-':

                json_type = JSON_TYPE_INT;
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
                    fprintf(stderr, "Failed to parse number, only sign found\n");
                    TRACE(ERROR, "PARSE ERROR");
                    *err = JSON_ERR_PARSE;
                    return NULL;
                }
                /* Parse Number */
                if(*start == '.'){
                    start++;
                    /* Parse double value*/
                    double_val = get_fraction(start, end, &temp);
                    if(start == temp){
                        fprintf(stderr, "Failed to parse double value\n");
                        TRACE(ERROR, "PARSE ERROR");
                        *err = JSON_ERR_PARSE;
                        return NULL;
                    }
                    /* Parsing successful*/
                    json_type = JSON_TYPE_DOUBLE;
                    double_val *= sign;
                    val = (void*)&double_val;

                } else if(is_hex(start, end)){
                    start +=2;
                    /* Parse hex value */
                    uint_number = get_hex(start, end, &temp, &overflow);
                    if(start == temp){
                        fprintf(stderr, "Failed to parse hex value\n");
                        TRACE(ERROR, "PARSE ERROR");
                        *err = JSON_ERR_PARSE;
                        return NULL;
                    } else if(sign == -1 ){
                        fprintf(stderr, "Hex value can not be negative\n");
                        TRACE(ERROR, "PARSE ERROR");
                        *err = JSON_ERR_PARSE;
                        return NULL;
                    }
                    /* Parsing successful*/
                    val = (void*)&uint_number;
                    json_type = JSON_TYPE_HEX;
                } else if(is_octal(start, end)){
                    start++;
                    /* Parse octal values */
                    uint_number = get_octal(start, end, &temp, &overflow);
                    if(start == temp){
                        fprintf(stderr, "Failed to parse octal value\n");
                        TRACE(ERROR, "PARSE ERROR");
                        *err = JSON_ERR_PARSE;
                        return NULL;
                    } else if(sign == -1 ){
                        fprintf(stderr, "Octal value can not be negative\n");
                        TRACE(ERROR, "PARSE ERROR");
                        *err = JSON_ERR_PARSE;
                        return NULL;
                    }
                    /* Parsing successful*/
                    val = (void*)&uint_number;
                    json_type = JSON_TYPE_OCTAL;
                } else {
                    long_val = get_integer(start, end, &temp, &overflow, &unsigned_flag);
                    if(start == temp){
                        fprintf(stderr, "Failed to parse octal value\n");
                        TRACE(ERROR, "PARSE ERROR");
                        *err = JSON_ERR_PARSE;
                        return NULL;
                    }
                    
                    /* Parsing successful*/
                    
                    long_val *= sign;
                    val = (void*)&long_val;
                    if(unsigned_flag){
                        json_type = JSON_TYPE_UINT;
                    }
                    /* Check if there is also a fractional part */
                    start = temp;
                    if((start < end) &&( *start == '.')){
                        start++;
                        double_val = get_fraction(start, end, &temp);
                        if(start == temp){
                            fprintf(stderr, "Failed to parse decimal value\n");
                            TRACE(ERROR, "PARSE ERROR");
                            *err = JSON_ERR_PARSE;
                            return NULL;
                        }
                        /* Check sign */
                        if(long_val > 0){
                            double_val += long_val;
                        } else {
                            double_val = long_val - double_val;
                        }
                        json_type = JSON_TYPE_DOUBLE;
                        val = (void*)&double_val;
                    }
                }
                *raw = trim(temp, end);
                if(err)
                    *err = JSON_ERR_SUCCESS;

                break;

            default:
                fprintf(stderr, "Missing Value\n");
                if(err)
                    *err = JSON_ERR_PARSE;
                return NULL;
        }
    }

    /* Allocate json vlue*/
    if((j_val = json_alloc_val(json_type, val)) == NULL ){
        fprintf(stderr, " Failed to alocate memeory");
        *err = JSON_ERR_NO_MEM;
        *raw = begin;
    }
    
    return j_val;
}


/*
* Parse single Json object as dcitionary
*/
static struct json * json_parse_obj(const char *start, const  char *end, const  char **raw, int *err)
{
    const char *begin = start;
    const char *key = NULL;
    const char *temp = NULL;
    struct json * json = NULL;
    struct json_val  *val = NULL;
    struct json_list *list = NULL;
    int len = 0;

    /* Check for args */
    if(start && end && raw){
        /* Nothing Parsed */
        *raw = start;

        /* Trim Extra Space */
        start = trim(start, end);

        /* Object should start with { */
        if(*start != '{'){
            if(*start == '['){
                list = json_parse_list(start, end, &temp, err);
                start = trim(temp, end);
                if(start < end){
                    fprintf(stderr, "Invalid json data\n");
                    TRACE(ERROR, "PARSE ERROR");
                    json_free_list(list);
                    list = NULL;
                    return NULL;
                } else {
                    /* Alocate JSON object */
                    if(!(json=json_alloc_obj())){
                        TRACE(ERROR, "PARSE ERROR");
                        fprintf(stderr, "Failed to allocate json object\n");
                        if(err)
                            *err = JSON_ERR_NO_MEM;
                        *raw = begin;
                        json_free_list(list);
                        return NULL;
                    }
                    *raw = temp;
                    json->list = list;
                    return json;
                }
            }

            if(list == NULL){
                TRACE(ERROR, "PARSE ERROR");
                fprintf(stderr, "Missing {");
                if(err)
                    *err = JSON_ERR_PARSE;
                *raw = begin;
                return NULL;
            }
        }
        /* Trim after { */
        start = trim(start + 1, end);

        /* Check if we are at the end */
        if(start >= end){
            TRACE(ERROR, "PARSE ERROR");
            fprintf(stderr, "Missing }\n");
            if(err)
                *err = JSON_ERR_PARSE;
            *raw = begin;
            return NULL;
        }

        /* Alocate JSON object */
        if(!(json=json_alloc_obj())){
            TRACE(ERROR, "PARSE ERROR");
            fprintf(stderr, "Failed to allocate json object\n");
            if(err)
                *err = JSON_ERR_NO_MEM;
            *raw = begin;
            return NULL;
        }

        if(*start == '}'){
            temp = trim(start + 1, end);
            if(err)
                *err = JSON_ERR_SUCCESS;
            *raw = temp;
            return json;
        }
        
        /* Traverse all string */
        for(*raw = begin; *start && (start < end); *raw = start){

            /* Rest Should bey Key:Val pair */
            key = get_string(start, end, &temp, &len);
            if(!key || (temp == start)){
                fprintf(stderr, "Missing Key, should start with \"\n");
                TRACE(ERROR, "PARSE ERROR");
                json_free(json);
                if(err)
                    *err = JSON_ERR_PARSE;
                return NULL;
            }
            
            /* Trim whitespace */
            start = trim(temp, end);
            if(( start == end ) || *start != ':'){
                fprintf(stderr, "Missing :\n");
                TRACE(ERROR, "PARSE ERROR");
                json_free(json);
                if(err)
                    *err = JSON_ERR_PARSE;
                return NULL;
            }

            /* Trim white space after collon */
            start = trim(start + 1, end);
            if( start >= end ){
                fprintf(stderr, "Missing Value after :\n");
                TRACE(ERROR, "PARSE ERROR");
                json_free(json);
                if(err)
                    *err = JSON_ERR_PARSE;
                return NULL;
            }

            /* Parse current Value */
            val = json_parse_val(start, end, &temp, err);

            /* Check for success */
            if(!val || (start == temp)){
                fprintf(stderr, "Failed to parse value for %s\n", key);
                TRACE(ERROR, "PARSE ERROR");
                json_free(json);
                if(err)
                    *err = JSON_ERR_PARSE;
                return NULL;
            }

            /* Add Key value pair in json object */
            if(!json_dict_add(json, key, val)){
                fprintf(stderr, "Failed to add value for %s in json object\n", key);
                json_free(json);
                if(err)
                    *err = JSON_ERR_NO_MEM;
                return NULL;
            }

            /* Trim */
            start = trim(temp, end);
            if(start == end ){
                fprintf(stderr, "Missing }\n");
                TRACE(ERROR, "PARSE ERROR");
                json_free(json);
                if(err)
                    *err = JSON_ERR_NO_MEM;
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
                    return json;

                default:
                    fprintf(stderr, "Missing ,\n");
                    *raw = begin;
                     json_free(json);
                    if(err)
                        *err = JSON_ERR_PARSE;
                return NULL;
            }
        }
        
    }

    if(err)
        *err = JSON_ERR_SUCCESS;

    return json;
}

/*
* Json Parse buffer
*/
static struct json * json_parse(const char *start, const char *end, int *err)
{
    const char *temp = NULL;
    struct json *json = NULL;
    if(start && end){
        start = trim(start, end);
        json = json_parse_obj(start, end, &temp, err);
        if(json && temp){
            start = trim(temp, end);
            if(start < end){
                fprintf(stderr, "Invalid characters after json object");
                TRACE(ERROR, "PARSE ERROR");
            }
        }
    } else {
        TRACE(ERROR, "Invalid Params");
        fprintf(stderr, "Invalid params");
        if(err)
            *err = JSON_ERR_ARGS;
    }

    return json;
}
static int json_print_list(FILE *stream, const struct json_list *list, unsigned int indent, unsigned int level)
{
    int ret = 0;
    ret+=fprintf(stream, "[");
    int count = 0;
    struct json_val *val;
    for(val=list->start;val; val=val->next){
        if(count){
            fprintf(stream, ",");
            ret++;
        }
        count++;
        ret += json_print_val(stream, val, indent, level);
    }  
    ret+=fprintf(stream, "]");

    return ret;
}
/*
* JSON print json value
*/
static int json_print_val(FILE *stream, const struct json_val  *val, unsigned int indent, unsigned int level)
{
    int ret = 0;
    int count = 0;
    switch(val->type){
        case JSON_TYPE_NULL:
        ret = fprintf(stream, "null");
        break;
        case JSON_TYPE_OBJ:
            ret = json_print_obj(stream, val->json_obj, indent, level + 1);
        break;
        case JSON_TYPE_STR:
            ret = fprintf(stream, "\"%s\"",val->string);
        break;
        case JSON_TYPE_BOOL:
            ret = fprintf(stream, "%s",val->boolean == true ? "true" : "false");
        break;
        case JSON_TYPE_DOUBLE:
            ret = fprintf(stream, "%lf",val->double_number);
        break;
        case JSON_TYPE_INT:
            ret = fprintf(stream, "%ld",val->long_number);
        break;
        /*
        case JSON_TYPE_LONG:
            ret = fprintf(stream, "%llu",val->long_number);
        break;*/
        case JSON_TYPE_UINT:
            ret = fprintf(stream, "%lu",val->long_number);
        break;
        case JSON_TYPE_HEX:
            ret = fprintf(stream, "0x%0x",val->uint_number);
        break;
        case JSON_TYPE_OCTAL:
            ret = fprintf(stream, "0%o",val->uint_number);
        break;
        case JSON_TYPE_LIST:
            ret = json_print_list(stream, val->list, indent, level);
        break;
        default:
        break;
    }

    return ret;
}
/*
* Print indentation for JSON
*/
static int json_indent(FILE *stream, int indent, int level)
{
    int i;
    int indentation;
    if((level <= 0) ||( indent <= 0))
        return 0;

    indentation = indent * level;
    fprintf(stream, "%*c",indentation, ' ');
    //for(i = 0; i < indentation; i++)
    //    fprintf(stream, " ");
    return indentation;
}

/*
* JSON print single object
*/
static int json_print_obj(FILE *stream, const struct json *json, unsigned int indent, unsigned int level)
{
    int i = 0;
    int ret = 0;
    int count = 0;
    struct json_val  *val = NULL;
    struct json_dict *dict = NULL;
    
    /* Check data */
    if(json){
        fprintf(stream, "{");
        ret++;
        /* Traverse all dict keys */
        for(dict = json->dict_start; dict; dict = dict->next){
            if(count > 0){
                ret += fprintf(stream, ",");
            }
            if(indent){
                ret += fprintf(stream, "\n");
            }

            count++;
            ret += json_indent(stream, indent, level + 1);
            ret += fprintf(stream, "\"%s\":", dict->key);
            ret += json_print_val(stream, dict->val, indent, level);
        }
        if(indent){
            ret += fprintf(stream, "\n");
        }
        ret += json_indent(stream, indent, level);
        ret += fprintf(stream, "}");
    }

    return ret;
}


static struct json_iter *json_get_iter(int type, const void* data)
{
    struct json_iter *iter = NULL;

    iter = calloc(sizeof(struct json_iter), 1);
    if(iter){
        /* Set List for Iterator */            
        if(type == JSON_TYPE_LIST){
            iter->type = JSON_TYPE_LIST;
            iter->list = (struct json_list *)data;
            iter->current_val = NULL;
        } else if(type == JSON_TYPE_OBJ){
            iter->type = JSON_TYPE_OBJ;
            iter->json = (struct json* )data;
            iter->current_dict = NULL;
        } else if(type == JSON_TYPE_STR){
            iter->type = JSON_TYPE_STR;
            iter->str = (char* )data;
            iter->current_ptr = NULL;
        } else {
            fprintf(stderr, "Invalid type for iterator\n");
            free(iter);
            iter = NULL;
        }
    } else {
        fprintf(stderr, "Failed to allocate memory for iterator\n");
    }

    return iter;
}



static char* json_clone_str(const char* src_str, int *err)
{
    char *str = NULL;
    int len = 0;
    len = strlen(src_str);
    if( len > 0 ){
        if((str = buffer_alloc(len))){
            strcpy(str, src_str);
        } else {
            fprintf(stderr, "Memory allocation failure\n");
            if(err)
                *err = JSON_ERR_NO_MEM;
        }
    } else {
        fprintf(stderr, " Null String\n");
        if(err)
            *err = JSON_ERR_ARGS;
    }

    return str;
}

struct json_val* json_clone_val(const struct json_val* src_val, int *err)
{
    struct json_val* val = NULL;
    void* data = NULL;
    if(src_val){
        if((data = json_clone_type(src_val->type, src_val->data, err))){

            if((src_val->type != JSON_TYPE_LIST)&&
               (src_val->type != JSON_TYPE_STR)&&
               (src_val->type != JSON_TYPE_OBJ)){
                   data = (void*)&src_val->data;
            }

            if((val = json_alloc_val(src_val->type, data))){
                if(err) 
                    *err = JSON_ERR_SUCCESS;
            } else {
                fprintf(stderr, "Failed to allocate json val\n");
                if(err)
                    *err = JSON_ERR_NO_MEM;
                json_free_type(src_val->type, data);
            }
        } else {
            fprintf(stderr, "Failed to allocate data\n");
            if(err)
                *err = JSON_ERR_NO_MEM;
        }
    }

    return val;
}

static struct json_list* json_clone_list(const struct json_list *src_list, int *err)
{
    struct json_list* list = NULL;
    struct json_val* src_val = NULL;
    struct json_val* val = NULL;

    if(src_list){
        if((list = json_alloc_list())){
            for(src_val = src_list->start; src_val; src_val = src_val->next){
                if((val = json_clone_val(src_val, err))){
                    if(!json_list_add(list, val)){
                        fprintf(stderr, "Failed to add in list\n");
                        json_free_list(list);
                        list = NULL;
                        if(err)
                            *err = JSON_ERR_NO_MEM;
                        break;
                    } else {
                        if(err)
                            *err = JSON_ERR_SUCCESS;
                    }
                }
            }
        } else {
            fprintf(stderr, "Memory allocation failure\n");
            if(err)
                *err = JSON_ERR_NO_MEM;
        }
    }
    return list;
}

static struct json* json_clone_obj(const struct json* src_json, int *err)
{
    struct json* json = NULL;
    struct json_dict* dict = NULL;
    struct json_list* list = NULL;
    struct json_val *val = NULL;
    const char *key;

    if(src_json){
        if((json = json_new())){
            if(json->list){
                if((list = json_clone_list(list, err))){
                    json->list = list;
                    if(err)
                        *err = JSON_ERR_SUCCESS;
                } else {
                    fprintf(stderr, "Failed to clone list\n");
                    json_free(json);
                }
            } else {
                for(dict = src_json->dict_start; dict; dict = dict->next){
                    if((key = json_clone_str(dict->key, err))){
                        if((val = json_clone_val(dict->val, err))){
                            if(json_dict_add(json, key, val)){
                                if(err)
                                    *err = JSON_ERR_SUCCESS;
                            } else {
                                fprintf(stderr, "Failed to allocate dict\n");
                                if(err)
                                    *err = JSON_ERR_NO_MEM;
                                json_free_val(val);
                                buffer_free(key);
                                json_free(json);
                                json = NULL;
                            }
                        } else {
                            fprintf(stderr, "Failed to allocate json val\n");
                            buffer_free(key);
                            json_free(json);
                            json = NULL;
                        }
                    } else {
                        fprintf(stderr, "Failed to duplicate key\n");
                        json_free(json);
                        json = NULL;
                    }
                }
            }
        } else {
            fprintf(stderr, "Memory allocation failure\n");
            if(err)
                *err = JSON_ERR_NO_MEM;
        }
    }
    return json;
}

static void* json_clone_type(int type, const void* data, int *err)
{
    void *ret = NULL;
    switch(type){
        case JSON_TYPE_STR:
            ret = json_clone_str(data, err);
        break;
        case JSON_TYPE_LIST:
            ret = json_clone_list(data, err);
        break;
        case JSON_TYPE_OBJ:
            ret = json_clone_obj(data, err);
        break;
        case JSON_TYPE_VAL:
            ret = json_clone_val(data, err);
        break;
        default:
        ret = (void*)data;
        break;
    }

    return ret;
}

/*
* Load Json from buffer from memory
*/
struct json* json_loads(const char *start, const char* end, int *err)
{
    struct json *json = NULL;

    /* Check data */
    if( start && end ){
        /* Process Buffer */
        json = json_parse(start, end, err);
    } else {
        /* Invalid input */
        fprintf(stderr, "Invalid params\n");
        if(err) *err = JSON_ERR_ARGS;
    }

    return json;
}
/* 
* Load JSON data from a file
*/
struct json* json_load(const char* fname, int *err)
{
    struct json* json = NULL;
    const char* temp = NULL;
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
            fprintf(stderr, "Failed to read file : %s\n", fname);

            if(err)
                *err = JSON_ERR_SYS;
            json = NULL;
        }
    } else {
        /* Invalid arguments*/
        fprintf(stderr, "Invalid args");
        if(err)
            *err = JSON_ERR_ARGS;
    }
    return json;
}


/*
* Print json data on screen
*/
int json_print(const struct json *json, unsigned int indent)
{
    if(json->list){
        return json_print_list(stdout, json->list,indent, 0 );
    }
    return json_print_obj(stdout, json, indent, 0);
}

/*
* Print json data to File
*/
int json_printf(const struct json *json, const char *fname, unsigned int indent)
{
    FILE *fp = fopen(fname, "w");
    int len = -1;
    if(fp){
        if(json->list){
            len = json_print_list(fp, json->list,indent, 0 );
        } else {
            len = json_print_obj(fp, json, indent, 0);
        }
        fclose(fp);
        return len;
    } else {
        fprintf(stderr, "Failed to open file : %s", fname);
    }
   return -1;
}

/*
* Print json data to string
*/
int json_prints(const struct json *json, char *buffer, unsigned int size, unsigned int indent)
{
    int len = -1;
    FILE *fp = fmemopen((void*)buffer, size,  "w");
    if(fp){
        if(json->list){
            len = json_print_list(fp, json->list,indent, 0 );
         } else {
            len = json_print_obj(fp, json, indent, 0);
         }
         buffer[len] = 0;
         fclose(fp);
         return len;
    } else {
        fprintf(stderr, "Failed to initialized buffer as file");
    }
   return -1;
}

const char* json_str(const struct json *json, int *len, unsigned int indent)
{
    char **buf_ptr;
    char *buffer;
    size_t *size_ptr = NULL;
    size_t size = 0;
    int print_length = 0;
    FILE *fp = fdmemopen(&buf_ptr, &size_ptr);
    if(fp){
         if(json->list){
            print_length = json_print_list(fp, json->list,indent, 0 );
         } else {
            print_length = json_print_obj(fp, json, indent, 0);
         }
         buffer = *buf_ptr;
         size = *size_ptr;
         fclose(fp);
         if(len)*len = print_length;
         buffer[print_length] = 0;
         return buffer;
    } else {
        fprintf(stderr, "Failed to initialize buffer as file");
    }
   return NULL;

}
/*
* Get value for key from json
* List and json object value is not returned, instead iterator is returned 
*/
const void* json_get(const struct json *json, const char *key, int *type)
{
    struct json_dict *dict = NULL;

    /* Check for valid data  */
    if(json && type){
        if(json->list){
            *type = JSON_TYPE_ITER;
            return json_get_iter(JSON_TYPE_LIST, json->list);
        } else if(key){
            /* Find key in json object */
            for(dict = json->dict_start; dict; dict = dict->next){
                /* Check Key */
                if(strcmp(dict->key, key) == 0 ){
                    /* List and json objects are not returned, instead iterators are returned */
                    if(dict->val->type == JSON_TYPE_LIST){
                        *type = JSON_TYPE_ITER;
                        return json_get_iter(JSON_TYPE_LIST, dict->val->list);
                    }
                    /* Rest of data can be returned */
                    *type = dict->val->type;
                    return dict->val->data;
                }
            }
        }
    }
    return NULL;
}

/*
* Set Key in JSON object
* If Provided value is NULL key will be removed
* For List the key will be added
* For others values will be replcaed, if key exists or it will be added
*/
int json_set(struct json *json, char *key, int type, void *val)
{
    int err = JSON_ERR_ARGS;
    struct json_dict *dict = NULL;
    struct json_val  *json_val = NULL;
    struct json_list  *json_list = NULL;

    /* Check data */
    if(json && (type <= JSON_TYPE_LIST)){
        /**Validate*/
        if(json->list && key){
            fprintf(stderr, " Invalid operation on list object, key should be null\n");
            err = JSON_ERR_ARGS;
            return err; 
        } else if(!json->list && !key){
            fprintf(stderr, " Key is neede for json object\n");
            err = JSON_ERR_ARGS;
            return err; 
        }

        /* Make duplicate of key*/
        if(key && ((key = json_clone_str(key, &err)) == NULL )){
            return err; 
        }

        /* Make duplicate of value*/
        if(val && ((val = json_clone_type(type, val, &err)) == NULL )){
            return err; 
        }
        
         /* Allocate json value */
        if(val && (json_val = json_alloc_val(type, val)) == NULL ){
            /* Memeory allocation failed*/
            fprintf(stderr, " Memeory allocation failed\n");
            json_free_type(JSON_TYPE_STR, key);
            json_free_type(type, val);
            err = JSON_ERR_NO_MEM;
            return err;
        }

        /* For List value is added*/
        if(json->list){
            if(json_list_add(json->list, json_val)){
                fprintf(stderr, " Failed to add in list \n");
                json_free_type(JSON_TYPE_STR, key);
                json_free_val(json_val);
                err = JSON_ERR_NO_MEM;
            } else {
                /* Success*/
                err = JSON_ERR_SUCCESS;
            }   
        }

        /* Find key in json object */
        for(dict = json->dict_start; dict; dict = dict->next){
            /* Match dict key */
            if(strcmp(dict->key, key) == 0 ){
                /* NULL value is used to delete the key*/
                if( val ){
                    /* For List value is added*/
                    if(dict->val->type != JSON_TYPE_LIST){
                        json_free_val(dict->val);
                        dict->val = json_val;
                        err = JSON_ERR_SUCCESS;
                    } else {
                        /* If not List then replace the value*/
                        if(!json_list_add(dict->val->list, json_val)){
                            fprintf(stderr, " Failed to add in list \n");
                            json_free_val(json_val);
                            json_free_type(JSON_TYPE_STR, key);
                            err = JSON_ERR_NO_MEM;
                        } else {
                            /* Success*/
                            err = JSON_ERR_SUCCESS;
                        }
                    }
                } else {
                    /* NULL Value Free dict entry*/
                    if(dict->prev){
                        dict->prev->next = dict->next;
                    }
                    if(dict->next){
                        dict->next->prev = dict->prev;
                    }
                    dict->next = dict->prev = NULL;
                    json_free_dict(dict);
                    err = JSON_ERR_SUCCESS;
                }

                break;
            }
        }
        /* Check if No match found */
        if((dict == NULL) &&  val){
            /* No match found */
            if(!json_dict_add(json, key, json_val)){
                fprintf(stderr, " Memeory allocation failed\n");
                err = JSON_ERR_NO_MEM;
                json_free_val(json_val);
                json_free_type(JSON_TYPE_STR, key);
            } else {
                err = JSON_ERR_SUCCESS;
            }   
        }
    }

    return err;
}
/*
* Iterator
*/
const void* json_iter_next(struct json_iter *iter,  int *type)
{
    if(iter && type){
        if(iter->type == JSON_TYPE_LIST){
            /* If Current is not set then move to start*/
            if(iter->current_val){
                iter->current_val = iter->current_val->next;
            } else {
                iter->current_val = iter->list->start;
            }
            
            if(iter->current_val){
                /* For List iterator is returned*/
                if(iter->current_val->type == JSON_TYPE_LIST){
                    *type = JSON_TYPE_ITER;
                    return json_get_iter( JSON_TYPE_LIST, iter->current_val->list);
                }
                /* Rest of data can be returned */
                *type = iter->current_val->type;
                return iter->current_val->data;
            } else {
                *type = JSON_TYPE_NULL;
            }
        } else if(iter->type == JSON_TYPE_OBJ){
            /* If Current is not set then move to start*/
            if(iter->current_dict){
                iter->current_dict = iter->current_dict->next;
            } else {
                iter->current_dict = iter->json->dict_start;
            }
            
            if(iter->current_dict){
                /* For List iterator is returned*/
                /* Rest of data can be returned */
                *type = JSON_TYPE_STR;
                return iter->current_dict->key;
            } else {
                *type = JSON_TYPE_NULL;
            }
        } else if(iter->type == JSON_TYPE_STR){
            /* If Current is not set then move to start*/
            if(iter->current_ptr){
                iter->current_ptr++;
            } else {
                iter->current_ptr = iter->str;
            }
            
            if(iter->current_ptr && *(iter->current_ptr)){
                /* For List iterator is returned*/
                /* Rest of data can be returned */
                *type = JSON_TYPE_INT;
                return (iter->current_ptr);
            } else {
                *type = JSON_TYPE_NULL;
            }
        }
    }

    return NULL;
}

struct json* json_new(void)
{
    return json_alloc_obj();
}

void json_del(struct json* json)
{
    json_free(json);
}

struct json_iter *json_keys(const struct json* json)
{
    return json_get_iter(JSON_TYPE_OBJ, json);
}

struct json* json_clone(struct json* json)
{
    int err;
    return json_clone_obj(json, &err);
}
