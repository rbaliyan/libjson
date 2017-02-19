#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "json.h"
#include "iter.h"
#include "test.h"

#define MODULE "JsonTest"
#include "trace.h"
#define TEST_LIST_SIZE 10

char* fname_success[]={"data/success_1.json", 
                       "data/success_2.json", 
                       "data/success_3.json", 
                       "data/success_4.json", 
                       "data/success_5.json"};
char* fname_fail[]={"data/fail_1.json", 
                    "data/fail_2.json", 
                    "data/fail_3.json", 
                    "data/fail_4.json",
                    "data/fail_000000.json"};

static int test_file_success(void)
{
    int status = 1;
    int i, err;
    int file_count = sizeof(fname_success)/sizeof(char*);
    struct json *json;
    for(i = 0;i < file_count; i++){
        if(!(json = json_load(fname_success[i], &err))){
            TRACE(ERROR, "Failed parse file : %s, err : %s",fname_success[i], json_sterror(err));
            status = 0;
        } else {
            json_del(json);
        }
    }
    return status;
}

static int test_file_fail(void)
{
    int status = 1;
    int i, err;
    int file_count = sizeof(fname_fail)/sizeof(char*);
    struct json *json;
    for(i = 0;i < file_count; i++){
        if((json = json_load(fname_fail[i], &err))){
            TRACE(ERROR, "Invalid file %s parsed",fname_fail[i]);
            status = 0;
            json_del(json);
        }
    }
    return status;
}

struct json *json_create(int islist)
{
    struct json *json;
    int err,i;
    long n = 1234;
    if(!(json = json_new())){
        TRACE(ERROR,"Failed to create empty json");
    } else if(!islist){
        err = json_set(json, JSON_TYPE_STR, "name", "Json Test Object");
        if(JsonIsError(err)){
            TRACE(ERROR, "Failed to Set String: %s", json_sterror(err));
        }

        err = json_set(json, JSON_TYPE_INT, "int", &n);
        if(JsonIsError(err)){
            TRACE(ERROR, "Failed to Set Integer: %s", json_sterror(err));
        }

        err = json_set(json, JSON_TYPE_STR, "str", "str value");
        if(JsonIsError(err)){
            TRACE(ERROR, "Failed to Set String: %s", json_sterror(err));
        }
    } else {
        for(i = 0;i < TEST_LIST_SIZE; i++){
            n = rand();
            err = json_set(json, JSON_TYPE_INT, NULL, &n);
            if(JsonIsError(err)){
                TRACE(ERROR, "Failed to Set String: %s", json_sterror(err));
            }
        }
        json_print(json, 2);
    }
    return json; 
} 

static int test_empty(void)
{
    int status = 1;
    struct json *json;
    if(!(json = json_new())){
        TRACE(ERROR,"Failed to create empty json");
        status = 0;
    } else {
        if((json_print(json, 0))<0){
            TRACE(ERROR,"Failed to print empty json");
            status = 0;
        }
        json_del(json);
    } 
    return status;  
}

static int test_set(void)
{
    int status = 1;
    struct json *json;
    if((json = json_create(0))){
        json_print(json, 2);
        json_del(json);
    } else {
        TRACE(ERROR,"Set Failed");
    }
    return status;
}

static int test_set_obj(void)
{
    int status = 1;
    int err;
    struct json *json, *json1;
    if((json = json_create(0))){
        json_print(json, 2);
        if((json1 = json_create(0))){
            err = json_set(json, JSON_TYPE_OBJ, "Json Copy",json1);
            if(JsonIsError(err)){
                status = 0;
                TRACE(ERROR, "Failed to Set String: %s", json_sterror(err));
            }
            json_del(json1);
            json_print(json, 2);
        } else {
            status = 0;
            TRACE(ERROR,"Set Failed");
        }
        json_del(json);
    } else {
        status = 0;
        TRACE(ERROR,"Set Failed");
    }
    return status;
}

static int test_to_str(void)
{
    int status = 1;
    char *buffer = NULL;
    int len = 0;
    struct json *json;
    if((json = json_create(0))){
        if((buffer = json_str(json, &len, 0))){
            free(buffer);
        } else {
             TRACE(ERROR,"To String Failed");
             status = 0;
        }
        
        json_del(json);
    } else {
        status = 0;
        TRACE(ERROR,"Set Failed");
    }
    return status;
}
static int test_buffer(void)
{
    int status = 1;
    char buffer[1024];
    int len = sizeof(buffer);
    struct json *json;
    if((json = json_create(0))){
        if((json_str(json, buffer, len, 0))<0){
             TRACE(ERROR,"Copy to buffer Failed");
             status = 0;
        }
        json_del(json);
    } else {
        status = 0;
        TRACE(ERROR,"Set Failed");
    }
    return status;
}

static int test_get(void)
{
    int status = 1;
    long num = 0;
    char buffer[1024];
    struct json *json = NULL, *val = NULL;
    if((json = json_create(0))){
        if((val = json_get(json, "int"))){
            if((json_type(val)) != JSON_TYPE_INT){
                TRACE(ERROR,"Type mismatch for int");
                status = 0;
            } else if(sizeof(num) != json_size(val)){
                TRACE(ERROR,"size mistmatch");
                status = 0;
            } else if((json_val(val, &num, sizeof(num)))<0){
                TRACE(ERROR,"Failed to get value");
                status = 0;
            } else if(!(val = json_get(json, "str"))){
                TRACE(ERROR,"Failed to get key str");
                status = 0;
            } else if((json_type(val))!= JSON_TYPE_STR){
                TRACE(ERROR,"Type mismatch for str");
                status = 0;
            } else if((json_size(val)) == 0){
                TRACE(ERROR,"Failed to size value");
                status = 0;
            } else if((json_val(val, buffer, sizeof(buffer)))<0){
                TRACE(ERROR,"Failed to get value");
                status = 0;
            }
        } else {
             TRACE(ERROR,"Get Failed for int");
             status = 0;
        }
        json_del(json);
    } else {
        status = 0;
        TRACE(ERROR,"Set Failed");
    }
    return status;
}

int test_iter(void)
{
    int status = 1;
    struct iter* iter = NULL;
    struct json* json = NULL;
    void* data = NULL;
    if((json = json_create(0))){
        if(!(iter = json_iter(json))){
            TRACE(ERROR,"failed get iter");
            status = 0;
        } else {
            if(!(data = iter_next(iter))){
                TRACE(ERROR,"iter next failed");
                status = 0;
            }
            iter_reset(iter);
            while((data = iter_next(iter))){
                printf("%s\n", (char*)data);
            }
            iter_del(iter);
        }
        json_del(json);
    } else {
        status = 0;
        TRACE(ERROR,"Set Failed");
    }
    return status;
}

int test_list(void)
{
    int status = 1;
    struct iter* iter = NULL;
    struct json* json = NULL;
    long n;
    void* data = NULL;
    if((json = json_create(1))){
        if(!(iter = json_iter(json))){
            TRACE(ERROR,"failed get iter");
            status = 0;
        } else {
            if(!(data = iter_next(iter))){
                TRACE(ERROR,"iter next failed");
                status = 0;
            }
            iter_reset(iter);
            while((data = iter_next(iter))){
                if((json_val((struct json*)data, &n, sizeof(n)))){
                    printf("%d\n", (int)n);
                }
            }
            iter_del(iter);
        }
        json_del(json);
    } else {
        status = 0;
        TRACE(ERROR,"Set Failed");
    }
    return status;
}


int test_json_run(void)
{
    TEST_SUITE_INIT("JSON Test");
    TEST_SUITE_BEGIN();
    TEST_RUN(test_file_success, "Test Files for success");
    TEST_RUN(test_file_fail, "Test Files for failures");
    TEST_RUN(test_empty, "Test Empty Json object");
    TEST_RUN(test_set, "Set Json fields");
    TEST_RUN(test_set_obj,"Set JSON Object");
    TEST_RUN(test_buffer, "Print Json to Buffer");
    TEST_RUN(test_to_str, "Convert Json to string representation");
    TEST_RUN(test_get, "Test Json Get Value");
    TEST_RUN(test_iter, "Iterator");
    TEST_RUN(test_list, "List Iterator");
    TEST_SUITE_RESULTS();
    return 1;
}