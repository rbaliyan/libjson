#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dict.h"
#include "iter.h"
#include "test.h"

#define MOUDULE "TestDict"
#include "trace.h"

#define TEST_LIST_SIZE 10
#define TEST_STR_SIZE_MAX 15

char _dict_keys[TEST_LIST_SIZE][TEST_STR_SIZE_MAX];
char _dict_vals[TEST_LIST_SIZE][TEST_STR_SIZE_MAX];

static int str_cmp(const void *d1, const void*d2)
{
    return strcmp((char*)d1, (char*)d2);
}

static int str_print(const void *stream, unsigned int index, const char *key, const void *data)
{
    if(!stream)
        stream = stdout;
    return fprintf((FILE*)stream, "%s:%s\n", key, (char*)(data));
}

static int rand_range(int min, int max)
{
    int limit = (max - min);
    int n = rand();
    if(limit > 0)
        return n % limit;
    return n;
}

static char *rand_string(char *str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (size) {
        --size;
        size = rand_range(1, size);
        str[size] = '\0';
        for(size_t n = 0; n < size; n++) {
            int key = rand_range(0, (int) (sizeof charset - 1));
            str[n] = charset[key];
        }
    }
    return str;
}


static struct dict* dict_create(void)
{
    int i;
    struct dict* dict = NULL;
    if(!(dict = dict_new(NULL, str_cmp, str_print))){
        TRACE(ERROR, "Failed to allocate dict");
    } else {
        for(i = 0; i < TEST_LIST_SIZE; i++){
            if((dict_set(dict, _dict_keys[i], _dict_vals[i])) < 0){
                 TRACE(ERROR, "Failed to set dict entry for Key:%s, Val:%s", _dict_keys[i], _dict_vals[i]);
            }
        }
    }
    return dict;
}
static int test_generate(void)
{
    int status = 0;
    struct dict* dict = NULL;
    if((dict = dict_create())){
        status = 1;
        dict_del(dict);
    }
    return status;
}

static int test_print(void)
{
    int status = 0;
    struct dict* dict = NULL;
    if((dict = dict_create())){
        if((dict_print(dict, NULL))< 0){
            TRACE(ERROR, "Dict print failed");
            status = 0;
        } else {
            status = 1;
        }
        dict_del(dict);
    }
    return status;
}

static int test_get(void)
{
    int status = 0;
    struct dict* dict = NULL;
    void *val = NULL;
    int i;
    if((dict = dict_create())){
        status = 1;
        for(i = 0; i < TEST_LIST_SIZE; i++){
            if(!(val = dict_get(dict, _dict_keys[i]))){
                TRACE(ERROR, "Dict get failed");
                status = 0;
            }
        }
        dict_del(dict);
    }
    return status;
}

static int test_replace(void)
{
    int status = 0;
    struct dict* dict = NULL;
    void *val = NULL;
    int i;
    char temp[TEST_STR_SIZE_MAX];
    if((dict = dict_create())){
        status = 1;
        for(i = 0; i < TEST_LIST_SIZE; i++){
            strcpy(temp, _dict_vals[i]);
            rand_string(_dict_vals[i], TEST_STR_SIZE_MAX);
            if((dict_set(dict,  _dict_keys[i], _dict_vals[i])) < 0){
                TRACE(ERROR, "Dict set failed");
                status = 0;
            } else {
                if(!(val = dict_get(dict, _dict_keys[i])) || (strcmp(_dict_vals[i], val) != 0)){
                    TRACE(ERROR, "Dict get failed");
                    status = 0;
                }
            }
            strcpy(_dict_vals[i], temp);
        }
        dict_del(dict);
    }
    return status;
}

static int test_add(void)
{
    int status = 0;
    struct dict* dict = NULL;
    void *val = NULL;
    char temp[TEST_STR_SIZE_MAX];
    char temp1[TEST_STR_SIZE_MAX];
    if((dict = dict_create())){
        status = 1;
        rand_string(temp, TEST_STR_SIZE_MAX);
        rand_string(temp1, TEST_STR_SIZE_MAX);
        TRACE(DEBUG,"Key:%s", temp);
        TRACE(DEBUG,"Val:%s", temp1);
        if((dict_set(dict,  temp1, temp)) < 0){
            TRACE(ERROR, "Dict set failed");
            status = 0;
        } else {
            if(!(val = dict_get(dict, temp1)) || (strcmp(temp, val) != 0)){
                TRACE(ERROR, "Dict get failed");
                status = 0;
            }
        }
        dict_del(dict);
    }
    return status;
}

static int test_set_null(void)
{
    int status = 0;
    struct dict* dict = NULL;
    int i;
    int x;
    if((dict = dict_create())){
        status = 1;
        for(i = 0; i < TEST_LIST_SIZE; i++){
            if((x=dict_set(dict, _dict_keys[i],NULL))<0){
                TRACE(ERROR, "Dict set failed:%d",x);
                status = 0;
            } else {
                if((dict_get(dict, _dict_keys[i]))){
                    TRACE(ERROR, "Dict get failed");
                    status = 0;
                }
            }
        }
        dict_del(dict);
    }
    return status;
}

static int test_iter(void)
{
    int status = 0;
    struct dict* dict = NULL;
    struct iter* iter = NULL;
    char *key;
    void *data = NULL;
    int i, j;
    if((dict = dict_create())){
        status = 1;
        if((iter = dict_iter(dict))){
            for(i = 0, data = iter_next(iter);  data; data = iter_next(iter), i++){
                if(i > TEST_LIST_SIZE){
                    TRACE(ERROR, "Iter Pass throgh length of dict");
                    break;
                }
                key = (char*)data;
                for(j = 0; j < TEST_LIST_SIZE; j++){
                    if((strcmp(key, _dict_keys[j])) == 0){
                        break;
                    }
                }
                if(j < TEST_LIST_SIZE){
                    if(!(data = dict_get(dict, key))){
                        TRACE(ERROR, "Failed to get dict");
                    } else {
                        if((strcmp((char*)data, _dict_vals[j]))!=0){
                            TRACE(ERROR, "Value mismatch %s", key);
                        }
                    }
                } else {
                    TRACE(ERROR, "Unknown key : %s", key);
                }
            }
            iter_del(iter);
        } else {
            TRACE(ERROR, "Iter failed");
        }
        dict_del(dict);
    }
    return status;
}


static void init()
{
    int i;
    for(i = 0; i < TEST_LIST_SIZE; i++){
        rand_string(_dict_keys[i], TEST_STR_SIZE_MAX);
        rand_string(_dict_vals[i], TEST_STR_SIZE_MAX);
    }
}

int test_dict_run(void)
{
    init();
    TEST_SUITE_INIT("Dict Test");
    TEST_SUITE_BEGIN();
    TEST_RUN(test_generate, "Create dict");
    TEST_RUN(test_print, "Print dict");
    TEST_RUN(test_get, "Get key");
    TEST_RUN(test_add, "Add New Key");
    TEST_RUN(test_replace, "Replace");
    TEST_RUN(test_set_null, "Set Null");
    TEST_RUN(test_iter, "Iteration");
    TEST_SUITE_RESULTS();

    return 1;
}