#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "json.h"
#include "fsutils.h"
#include "utils.h"
#define MODULE "JSON"
#include "trace.h"

#define isJsonErr(x)   (inRange((x), JSON_ERR_BEGIN, JSON_ERR_LAST))
#define JsonErr(x)    (-(JSON_ERR_BEGIN + (x)))

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

/* Json Iterator */
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

/* Function declarations */
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
static void json_free(struct json * json);
static void json_free_list(struct json_list * list);
static void json_free_dict(struct json_dict *dict);
static void json_free_val(struct json_val * va);
static void json_free_type(int type, void *data);
void json_free_iter(struct json_iter *iter);
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
* @brief Json Object allocate 
* @return pointer to allocated json object
*/
static struct json* json_alloc_obj()
{
    struct json *json = NULL;
    if((json = malloc(sizeof(struct json)))){
        /* Set Params*/
        json->count = 0;
        json->dict_start = NULL;
        json->dict_end = NULL;
        json->list = NULL;
    } else {
        TRACE(ERROR, "Failed to allocate object");
    }
    return json;
}

/*
* @brief Json Value allocate
* @param json_type Json Type fpr the value
* @param Pointer to data 
* @return pointer to json value object
*/
static struct json_val * json_alloc_val(int json_type, const void* data)
{
    struct json_val  *val = NULL;
    if((val = malloc(sizeof(struct json_val )))){
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
* @brief Json List allocate
* @return Pointer to json list
*/
static struct json_list * json_alloc_list()
{
    struct json_list  *list = NULL;
    if((list = malloc(sizeof(struct json_list )))){
        /* Set Params*/
        list->start = list->end = NULL;
        list->count = 0;
    } else {
        TRACE(ERROR, "Failed to allocate list");
    }
    return list;
}

/*
* @brief Json dict allocate
* @param key Dict key
* @param val Json Value 
* @return Pointer to dict object 
*/
static struct json_dict * json_alloc_dict(const char* key, const struct json_val  * val)
{
    struct json_dict *dict = NULL;
    if((dict = malloc(sizeof(struct json_dict)))){
        /* Set params*/
        dict->key = key;
        dict->val = (struct json_val *)val;
        dict->next = dict->prev = NULL;
    }else {
        TRACE(ERROR, "Failed to allocate dict");
    }
    return dict;
}

/*
* @brief Free Json Iterator
* @param iter Json Iterator
*/
void json_free_iter(struct json_iter *iter)
{
    free(iter);
}
/*
* @brief Json Object free
* @param json Json Object 
*/
static void json_free( struct json* json)
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
    free(json);
}

/*
* @param Json dict free 
* @param dict Json dict object pointer
*/
static void json_free_dict(struct json_dict *dict)
{
    /* Dict Key is also allcated at the time of parsing*/
    buffer_free(dict->key);

    /* Free Value */
    json_free_val(dict->val);

    free(dict);
}

/*
* @brief Json list free 
* @param list Json list object
*/
static void json_free_list(struct json_list * list)
{
    struct json_val * val = NULL;
    struct json_val * temp = NULL;
    /* Traverse all list*/
    for(val = list->start; val; val = temp){
        /* Save next and free current pointer*/
        temp = val->next;
        json_free_val(val);
    }

    free(list);
}

/*
* @brief Json value free 
* @param val Json value object
*/
static void json_free_val(struct json_val * val)
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
    free(val);
}

/*
* @brief Json Free based on type
* @param type Json Type for data
* @param data Pointer to data to be freed
*/
static void json_free_type(int type, void *data)
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
* @brief Buffer allocate 
* @param size Size of buffer
* @return Pointer to allocated buffer
*/
static char *buffer_alloc(size_t size)
{
    return (char*)malloc(size);
}

/*
* @brief Buffer free
* @param buffer Pointer to buffer
*/
static void buffer_free(const char *buffer)
{
    free((void*)buffer);
}

/*
* @brief A simple dict based on linked list
* @param json Json object
* @param key Dict Key
* @param val Dict Value
* @return Pointer to allocated buffer
*/
static int json_dict_add(struct json *json, const char *key, const struct json_val * val)
{
    int err = JsonErr(JSON_ERR_SUCCESS);
    struct json_dict* dict = NULL;
    /* Check data */
    if( json && key && val ){
        /* Travers dict */
        for(dict = json->dict_start; dict; dict = dict->next){
            if(strcmp(dict->key, key)== 0){
                TRACE(WARN,"Key %s repeated", key);
                return JsonErr(JSON_ERR_KEY_REPEAT);
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
        } else {
             TRACE(ERROR, "Dict Error");
             err = JsonErr(JSON_ERR_NO_MEM);
        }
    } else {
       TRACE(WARN,"Arguments Error");
       err = JsonErr(JSON_ERR_ARGS);
    }

    return err;
}
/*
* @brief Append json value in json list
* @param list Json List
* @param val Json Value
* @return 0 for Failure, 1 for success
*/
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
        err = JsonErr(JSON_ERR_SUCCESS);
    } else {
       TRACE(WARN,"Arguments Error");
       err = JsonErr(JSON_ERR_ARGS);
    }
    return err;
}

/*
* @brief Parse a json list in buffer
* @param start Pointer to start of buffer
* @param end Pointer to end of buffer
* @param raw Pointer to plcae holder for data remaining after parsing
* @param err Pointer for error status
* @return Json list
*/
static struct json_list * json_parse_list(const char *start, const char *end,const char **raw, int *err)
{
    struct json_list * list = NULL;
    struct json_val  *val = NULL;
    const char *begin = start;
    const char *temp = NULL;
    int ret = 0;

    /* Check data */
    if(start && end && raw && (start < end)){

        /* Check beginning */
        if(*start != '['){
            TRACE(ERROR, "List must begin with [");
            *raw = begin;
            return NULL;
        }

        /* Trim Data */
        start = trim(start + 1, end);
        if(start == end){
            TRACE(ERROR, "Missing ]");
            *raw = begin;
            return NULL;
        }

        /* Allocate List */
        list = json_alloc_list();
        if(!list){
            TRACE(ERROR, "Failed to init List");
            if(err)
                *err = JsonErr(JSON_ERR_NO_MEM);
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
                TRACE(ERROR, "Failed to parse value");
                json_free_list(list);
                *raw = begin;
                if(err)
                    *err = JsonErr(JSON_ERR_PARSE);
                return NULL;
            }

            /* Add Value in List */
            json_list_add(list, val);
            if(JsonIsError(ret)){
                TRACE(ERROR, "Failed to add value in List %s", json_sterror(ret));
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
                    TRACE(ERROR, "Missing ,");
                    *raw = begin;
                    json_free_list(list);
                    if(err)
                        *err = JsonErr(JSON_ERR_PARSE);
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
        *err = JsonErr(JSON_ERR_PARSE);

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
static struct json_val * json_parse_val(const char *start, const char *end,const  char **raw, int *err)
{
    struct json_val  *j_val = NULL;
    const char *begin = start;
    const char *temp = NULL;
    double double_val = 0.0f;
    unsigned int uint_number;
    bool bool_val = false;
    long long_val = 0;
    int json_type = JSON_TYPE_INVALID;
    bool overflow = false;
    bool unsigned_flag = false; 
    int len = 0;
    int sign = 1;
    void *val = NULL;

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
                    TRACE(ERROR,"Failed to parse object\n");
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
                    TRACE(ERROR,"Failed to parse object\n");
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
                    TRACE(ERROR,"Failed to parse object\n");
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
                    TRACE(ERROR, "Failed to parse object\n");
                    if(err)
                        *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }
                *raw = temp;
                break;
            case 'n': case 'N':
                json_type = JSON_TYPE_NULL;
                if(((end - start) < 4) || ((strncasecmp(start, "null", 4)) != 0)){
                    TRACE(ERROR, "Failed to parse null\n");
                    if(err)
                        *err = JsonErr(JSON_ERR_PARSE);
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
                    TRACE(ERROR,"Failed to parse number, only sign found\n");
                    if(err)
                        *err = JsonErr(JSON_ERR_PARSE);
                    return NULL;
                }
                /* Parse Number */
                if(*start == '.'){
                    start++;
                    /* Parse double value*/
                    double_val = get_fraction(start, end, &temp);
                    if(start == temp){
                        TRACE(ERROR,"Failed to parse double value\n");
                        if(err)
                            *err = JsonErr(JSON_ERR_PARSE);
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
                        TRACE(ERROR,"Failed to parse hex value\n");
                        if(err)
                            *err = JsonErr(JSON_ERR_PARSE);
                        return NULL;
                    } else if(sign == -1 ){
                        TRACE(ERROR,"Hex value can not be negative\n");
                        if(err)
                            *err = JsonErr(JSON_ERR_PARSE);
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
                        TRACE(ERROR,"Failed to parse octal value\n");
                        if(err)
                            *err = JsonErr(JSON_ERR_PARSE);
                        return NULL;
                    } else if(sign == -1 ){
                        TRACE(ERROR,"Octal value can not be negative\n");
                        if(err)
                            *err = JsonErr(JSON_ERR_PARSE);
                        return NULL;
                    }
                    /* Parsing successful*/
                    val = (void*)&uint_number;
                    json_type = JSON_TYPE_OCTAL;
                } else {
                    long_val = get_integer(start, end, &temp, &overflow, &unsigned_flag);
                    if(start == temp){
                        TRACE(ERROR,"Failed to parse octal value\n");
                        if(err)
                            *err = JsonErr(JSON_ERR_PARSE);
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
                            TRACE(ERROR,"Failed to parse decimal value\n");
                            if(err)
                                *err = JsonErr(JSON_ERR_PARSE);
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
                    *err = JsonErr(JSON_ERR_SUCCESS);

                break;

            default:
                TRACE(ERROR,"Missing Value\n");
                if(err)
                    *err = JsonErr(JSON_ERR_PARSE);
                return NULL;
        }
    }

    /* Allocate json vlue*/
    if((j_val = json_alloc_val(json_type, val)) == NULL ){
        TRACE(ERROR," Failed to alocate memeory");
        *err = JsonErr(JSON_ERR_NO_MEM);
        *raw = begin;
    }
    
    return j_val;
}


/*
* @brief Parse a json object in buffer
* @param start Pointer to start of buffer
* @param end Pointer to end of buffer
* @param raw Pointer to plcae holder for data remaining after parsing
* @param err Pointer for error status
* @return Json object
*/
static struct json * json_parse_obj(const char *start, const  char *end, const  char **raw, int *err)
{
    const char *begin = start;
    const char *key = NULL;
    const char *temp = NULL;
    struct json * json = NULL;
    struct json_val  *val = NULL;
    struct json_list *list = NULL;
    int ret = 0;
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
                    TRACE(ERROR,"Invalid json data\n");
                    json_free_list(list);
                    list = NULL;
                    return NULL;
                } else {
                    /* Alocate JSON object */
                    if(!(json=json_alloc_obj())){
                        TRACE(ERROR,"Failed to allocate json object\n");
                        if(err)
                            *err = JsonErr(JSON_ERR_NO_MEM);
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
                
                TRACE(ERROR,"Missing {");
                if(err)
                    *err = JsonErr(JSON_ERR_PARSE);
                *raw = begin;
                return NULL;
            }
        }
        /* Trim after { */
        start = trim(start + 1, end);

        /* Check if we are at the end */
        if(start >= end){
            TRACE(ERROR,"Missing }\n");
            if(err)
                *err = JsonErr(JSON_ERR_PARSE);
            *raw = begin;
            return NULL;
        }

        /* Alocate JSON object */
        if(!(json=json_alloc_obj())){
            TRACE(ERROR,"Failed to allocate json object\n");
            if(err)
                *err = JsonErr(JSON_ERR_NO_MEM);
            *raw = begin;
            return NULL;
        }

        if(*start == '}'){
            temp = trim(start + 1, end);
            if(err)
                *err = JsonErr(JSON_ERR_SUCCESS);
            *raw = temp;
            return json;
        }
        
        /* Traverse all string */
        for(*raw = begin; *start && (start < end); *raw = start){

            /* Rest Should bey Key:Val pair */
            key = get_string(start, end, &temp, &len);
            if(!key || (temp == start)){
                TRACE(ERROR,"Missing Key, should start with \"\n");
                json_free(json);
                if(err)
                    *err = JsonErr(JSON_ERR_PARSE);
                return NULL;
            }
            
            /* Trim whitespace */
            start = trim(temp, end);
            if(( start == end ) || *start != ':'){
                TRACE(ERROR,"Missing :\n");
                
                json_free(json);
                if(err)
                    *err = JsonErr(JSON_ERR_PARSE);
                return NULL;
            }

            /* Trim white space after collon */
            start = trim(start + 1, end);
            if( start >= end ){
                TRACE(ERROR,"Missing Value after :\n");
                
                json_free(json);
                if(err)
                    *err = JsonErr(JSON_ERR_PARSE);
                return NULL;
            }

            /* Parse current Value */
            val = json_parse_val(start, end, &temp, err);

            /* Check for success */
            if(!val || (start == temp)){
                TRACE(ERROR,"Failed to parse value for %s\n", key);
                
                json_free(json);
                if(err)
                    *err = JsonErr(JSON_ERR_PARSE);
                return NULL;
            }

            /* Add Key value pair in json object */
            ret = json_dict_add(json, key, val);
            if(JsonIsError(ret)){
                TRACE(ERROR,"Failed to add value for %s in json object\n", key);
                json_free(json);
                if(err)
                    *err = JsonErr(JSON_ERR_NO_MEM);
                return NULL;
            }

            /* Trim */
            start = trim(temp, end);
            if(start == end ){
                TRACE(ERROR,"Missing }\n");
                
                json_free(json);
                if(err)
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
                    return json;

                default:
                    TRACE(ERROR,"Missing ,\n");
                    *raw = begin;
                     json_free(json);
                    if(err)
                        *err = JsonErr(JSON_ERR_PARSE);
                return NULL;
            }
        }
        
    }

    if(err)
        *err = JsonErr(JSON_ERR_SUCCESS);

    return json;
}
/*
* @brief Parse a json in buffer
* @param start Pointer to start of buffer
* @param end Pointer to end of buffer
* @param err Pointer for error status
* @return Json object
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
                TRACE(ERROR,"Invalid characters after json object");
            }
        }
    } else {
        TRACE(ERROR, "Invalid Params");
        if(err)
            *err = JsonErr(JSON_ERR_ARGS);
    }

    return json;
}

/*
* @brief Print Json List to stream
* @param stream Stream for output
* @param list Json List
* @param indent Indentation to be used for pertty printing
* @param level Depth level inside Json Obect
* @return number of bytes printed
*/
static int json_print_list(FILE *stream, const struct json_list *list, unsigned int indent, unsigned int level)
{
    int ret = 0;
    ret+=fprintf(stream, "[");
    int count = 0;
    struct json_val *val;
    for(val=list->start;val; val=val->next){
        if(count){
            ret += fprintf(stream, ",");
        }
        count++;
        ret += json_print_val(stream, val, indent, level);
    }  
    ret+=fprintf(stream, "]");

    return ret;
}

/*
* @brief Print Json Value to stream
* @param stream Stream for output
* @param val Json Value
* @param indent Indentation to be used for pertty printing
* @param level Depth level inside Json Obect
* @return number of bytes printed
*/
static int json_print_val(FILE *stream, const struct json_val  *val, unsigned int indent, unsigned int level)
{
    int ret = 0;
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
            ret = fprintf(stream, "%lu",(unsigned long)val->long_number);
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
* @brief Print Indentation to stream
* @param stream Stream for output
* @param indent Indentation to be used for pertty printing
* @param level Depth level inside Json Obect
* @return number of bytes printed
*/
static int json_indent(FILE *stream, int indent, int level)
{
    int indentation;
    if((level <= 0) ||( indent <= 0))
        return 0;

    indentation = indent * level;
    fprintf(stream, "%*c",indentation, ' ');
    return indentation;
}

/*
* @brief Print Json object to stream
* @param stream Stream for output
* @param json Json object
* @param indent Indentation to be used for pertty printing
* @param level Depth level inside Json Obect
* @return number of bytes printed
*/
static int json_print_obj(FILE *stream, const struct json *json, unsigned int indent, unsigned int level)
{
    int ret = 0;
    int count = 0;
    struct json_dict *dict = NULL;
    
    /* Check data */
    if(json){
        ret +=fprintf(stream, "{");
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

/*
* @brief Crate json iterator
* @param type Json Iterator type
* @param data Data for json iterator
* @return Json Iterator
*/
static struct json_iter *json_get_iter(int type, const void* data)
{
    struct json_iter *iter = NULL;

    if((iter = malloc(sizeof(struct json_iter)))){
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
            TRACE(ERROR,"Invalid type for iterator\n");
            free(iter);
            iter = NULL;
        }
    } else {
        TRACE(ERROR,"Failed to allocate memory for iterator\n");
    }

    return iter;
}


/*
* @brief Clone simple string
* @param src_str String to be cloned
* @param err plcaeholder for error
* @return Pointer to new generated string
*/
static char* json_clone_str(const char* src_str, int *err)
{
    char *str = NULL;
    int len = 0;
    if(src_str){
        len = strlen(src_str);
        if( len > 0 ){
            if((str = buffer_alloc(len+1))){
                strcpy(str, src_str);
            } else {
                TRACE(ERROR,"Memory allocation failure\n");
                if(err)
                    *err = JsonErr(JSON_ERR_NO_MEM);
            }
        } else {
            TRACE(ERROR," Null String\n");
            if(err)
                *err = JsonErr(JSON_ERR_ARGS);
        }
    }

    return str;
}

/*
* @brief Clone json value
* @param src_val Json value to be cloned
* @param err plcaeholder for error
* @return Pointer to new generated json value
*/
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
                    *err = JsonErr(JSON_ERR_SUCCESS);
            } else {
                TRACE(ERROR,"Failed to allocate json val\n");
                if(err)
                    *err = JsonErr(JSON_ERR_NO_MEM);
                json_free_type(src_val->type, data);
            }
        } else {
            TRACE(ERROR,"Failed to allocate data\n");
            if(err)
                *err = JsonErr(JSON_ERR_NO_MEM);
        }
    }

    return val;
}
/*
* @brief Clone json list
* @param src_list Json list to be cloned
* @param err plcaeholder for error
* @return Pointer to new generated json list
*/
static struct json_list* json_clone_list(const struct json_list *src_list, int *err)
{
    struct json_list* list = NULL;
    struct json_val* src_val = NULL;
    struct json_val* val = NULL;
    int ret = 0;

    if(src_list){
        if((list = json_alloc_list())){
            for(src_val = src_list->start; src_val; src_val = src_val->next){
                if((val = json_clone_val(src_val, err))){
                    ret = json_list_add(list, val);
                    if(JsonIsError(ret)){
                        TRACE(ERROR,"Failed to add in list\n");
                        json_free_list(list);
                        list = NULL;
                        if(err)
                            *err = JsonErr(JSON_ERR_NO_MEM);
                        break;
                    } else {
                        if(err)
                            *err = JsonErr(JSON_ERR_SUCCESS);
                    }
                }
            }
        } else {
            TRACE(ERROR,"Memory allocation failure\n");
            if(err)
                *err = JsonErr(JSON_ERR_NO_MEM);
        }
    }
    return list;
}

/*
* @brief Clone json object
* @param src_val Json object to be cloned
* @param err plcaeholder for error
* @return Pointer to new generated json object
*/
static struct json* json_clone_obj(const struct json* src_json, int *err)
{
    struct json* json = NULL;
    struct json_dict* dict = NULL;
    struct json_list* list = NULL;
    struct json_val *val = NULL;
    int ret = 0;
    const char *key;

    if(src_json){
        if((json = json_new())){
            if(json->list){
                if((list = json_clone_list(list, err))){
                    json->list = list;
                    if(err)
                        *err = JsonErr(JSON_ERR_SUCCESS);
                } else {
                    TRACE(ERROR,"Failed to clone list\n");
                    json_free(json);
                }
            } else {
                for(dict = src_json->dict_start; dict; dict = dict->next){
                    if((key = json_clone_str(dict->key, err))){
                        if((val = json_clone_val(dict->val, err))){
                            ret = json_dict_add(json, key, val);
                            if(JsonIsSuccess(ret)){
                                if(err)
                                    *err = JsonErr(JSON_ERR_SUCCESS);
                            } else {
                                TRACE(ERROR,"Failed to allocate dict\n");
                                if(err)
                                    *err = ret;
                                json_free_val(val);
                                buffer_free(key);
                                json_free(json);
                                json = NULL;
                            }
                        } else {
                            TRACE(ERROR,"Failed to allocate json val\n");
                            buffer_free(key);
                            json_free(json);
                            json = NULL;
                        }
                    } else {
                        TRACE(ERROR,"Failed to duplicate key\n");
                        json_free(json);
                        json = NULL;
                    }
                }
            }
        } else {
            TRACE(ERROR,"Memory allocation failure\n");
            if(err)
                *err = JsonErr(JSON_ERR_NO_MEM);
        }
    }
    return json;
}

/*
* @brief Clone json type
* @param type Type of data
* @param data data to be cloned
* @param err plcaeholder for error
* @return Pointer to new cloned data
*/
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
* @brief Load a json oject from buffer
* @param start Pointer to start of buffer
* @param end Pointer to end of buffer
* @param err Pointer for error status
* @return Json object
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
        TRACE(ERROR,"Invalid params\n");
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
struct json* json_load(const char* fname, int *err)
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
            TRACE(ERROR,"Failed to read file : %s\n", fname);

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
int json_print(const struct json *json, unsigned int indent)
{
    if(json->list){
        return json_print_list(stdout, json->list,indent, 0 );
    }
    return json_print_obj(stdout, json, indent, 0);
}



/*
* @brief Print json object to file
* @param json Json object
* @param fname Filename for output
* @param indent Indetation for pretty printing
* @return Number of bytes printed
*/
int json_printf(const struct json *json, const char *fname, unsigned int indent)
{
    FILE *fp = fopen(fname, "w");
    int len = 0;
    if(fp){
        if(json->list){
            len = json_print_list(fp, json->list,indent, 0 );
        } else {
            len = json_print_obj(fp, json, indent, 0);
        }
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
int json_prints(const struct json *json, char *buffer, unsigned int size, unsigned int indent)
{
    int len = 0;
    FILE *fp = fmemopen((void*)buffer, size,  "w");
    if(fp){
        if(json->list){
            len = json_print_list(fp, json->list,indent, 0 );
         } else {
            len = json_print_obj(fp, json, indent, 0);
         }
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

/*
* @brief Get value for key from json
* List and json object value is not returned, instead iterator is returned 
* @param json Json object
* @param key Key to fetch
* @param type type of value
* @return value stored in json
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
int json_set(struct json *json, char *key, int type, void *val)
{
    int ret = 0;
    int err = JSON_ERR_ARGS;
    struct json_dict *dict = NULL;
    struct json_val  *json_val = NULL;

    /* Check data */
    if(json && (type <= JSON_TYPE_LIST)){
        /**Validate*/
        if(json->list && (key || !val)){
            TRACE(ERROR," Invalid operation on list object, key should be null\n");
            err = JsonErr(JSON_ERR_ARGS);
            return err; 
        } else if(!json->list && !key){
            TRACE(ERROR," Key is neede for json object\n");
            err = JsonErr(JSON_ERR_ARGS);
            return err; 
        }

        /* Make duplicate of key*/
        if(val && key && ((key = json_clone_str(key, &err)) == NULL )){
            return err; 
        }

        /* Make duplicate of value*/
        if(val && ((val = json_clone_type(type, val, &err)) == NULL )){
            return err; 
        }
        
         /* Allocate json value */
        if(val && (json_val = json_alloc_val(type, val)) == NULL ){
            /* Memeory allocation failed*/
            TRACE(ERROR," Memeory allocation failed\n");
            json_free_type(JSON_TYPE_STR, key);
            json_free_type(type, val);
            err = JsonErr(JSON_ERR_NO_MEM);
            return err;
        }

        /* For List value is added*/
        if(json->list){
            ret = json_list_add(json->list, json_val);
            if(JsonIsError(ret)){
                TRACE(ERROR," Failed to add in list \n");
                json_free_type(JSON_TYPE_STR, key);
                json_free_val(json_val);
                err = JsonErr(JSON_ERR_NO_MEM);
            } else {
                /* Success*/
                err = JsonErr(JSON_ERR_SUCCESS);
                return err;
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
                        err = JsonErr(JSON_ERR_SUCCESS);
                    } else {
                        /* If not List then replace the value*/
                        ret = json_list_add(dict->val->list, json_val);
                        if(JsonIsError(ret)){
                            TRACE(ERROR," Failed to add in list \n");
                            json_free_val(json_val);
                            json_free_type(JSON_TYPE_STR, key);
                            err = JsonErr(JSON_ERR_NO_MEM);
                        } else {
                            /* Success*/
                            err = JsonErr(JSON_ERR_SUCCESS);
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
                    err = JsonErr(JSON_ERR_SUCCESS);
                }

                break;
            }
        }
        /* Check if No match found */
        if((dict == NULL) &&  val){
            /* No match found */
            ret = json_dict_add(json, key, json_val);
            if(JsonIsError(ret)){
                TRACE(ERROR," Memeory allocation failed\n");
                err = JsonErr(JSON_ERR_NO_MEM);
                json_free_val(json_val);
                json_free_type(JSON_TYPE_STR, key);
            } else {
                err = JsonErr(JSON_ERR_SUCCESS);
            }   
        }
    }

    return err;
}

/*
* @brief Next item for iterator
* @param iter Json Iterator
* @param type Type for current value
* @return Current value referenced by iterator
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

/*
* @brief Get a new empty json object
* @return json object
*/
struct json* json_new(void)
{
    return json_alloc_obj();
}

/*
* @brief Delete json object and free all data
* @param json Json data
*/
void json_del(struct json* json)
{
    json_free(json);
}

/*
* @brief Get iterator for json keys
* @param json Json object
* @return Iterator for json  keys
*/
struct json_iter *json_keys(const struct json* json)
{
    return json_get_iter(JSON_TYPE_OBJ, json);
}

/*
* @brief Clone json object
* @param json Json object
* @return Json object
*/
struct json* json_clone(struct json* json)
{
    int err;
    return json_clone_obj(json, &err);
}

/*
* @brief Delete Json Iterator
* @param iter Json Iterator
*/
void json_iter_del(struct json_iter *iter)
{
    json_free_iter(iter);
}

/*
* @brief Get string representation of json error
* @param err error code
* @return string representation of error
*/
const char* json_sterror(int err)
{
    static const char* _err_table[] ={
        [JSON_ERR_SUCCESS]      = "Success", 
        [JSON_ERR_NO_MEM]       = "Not enough memory",
        [JSON_ERR_KEY_NOT_FOUND]= "Key not found",
        [JSON_ERR_ARGS]         = "Invalid arguments",
        [JSON_ERR_PARSE]        = "Parsing error",
        [JSON_ERR_OVERFLOW]     = "Integer Overflow detected",
        [JSON_ERR_SYS]          = "System error",
        [JSON_ERR_LAST]         = "Invalid error",
    };

    /* Error is negative value of error code*/
    err = -err;

    /* Check for range of error*/
    if((err > 0) && (err >= JSON_ERR_SUCCESS) && (err < JSON_ERR_LAST)){
        return _err_table[err];
    } else {
        return "Invalid error code";
    }
}
