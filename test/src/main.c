#include <stdio.h>
#include <stdlib.h>
#include "json.h"
#include "list.h"
#include "dict.h"

#define MODULE "Main"
#include "trace.h"

#define TEST_JSON "data/test.json"

int int_cmp(void *d1, void*d2)
{
    return *(int*)d2 - *(int*)d1;
}
int int_print(void *stream, unsigned int index, void *data)
{
    if(!stream)
        stream = stdout;
    return fprintf(stream, "%d: %d\n", index, *(int*)(data));
}
int int_print_dict(void *stream, unsigned int index, const char *key, const void *data)
{
    if(!stream)
        stream = stdout;
    return fprintf(stream, "%d]%s = %d\n", index, key, *(int*)(data));
}

int main(int argc, char *argv[])
{
    /*
    int len = 0;
    int type = 0;
    const void *data = NULL;
    char buffer[1024] = {0};
    const char *buff_new = NULL;
    struct json *json = NULL;
    json = json_load(TEST_JSON, NULL);
    struct json *json_1 = NULL;
    struct json *json_2 = NULL;
    long n = 1234;
    json_1 = json_new();
    json_set(json_1, "int", JSON_TYPE_INT, &n);
    json_set(json_1, "str", JSON_TYPE_STR, "str value");
    json_set(json, "json", JSON_TYPE_OBJ, json_1);
    json_2 = json_clone(json_1);
    json_set(json, "json_clone", JSON_TYPE_OBJ, json_2);
    json_del(json_1);
    json_del(json_2);
    if(json){
        len = json_prints(json, buffer, sizeof(buffer), 2);
        printf("\n%s\n", buffer);
        json_set(json, "json", 0, NULL);
        json_print(json, 2);
        buff_new = json_str(json, &len, 0);
        if(buff_new){
            printf("\n%s\n", buff_new);
            free((void*)buff_new);
        }
        json_print(json, 2);  
        struct json_iter *iter = json_keys(json);
        while((data = json_iter_next(iter, &type))){
            printf("%s, %d\n", (char*)data, type);
        }
        json_del(json);
        json_iter_del(iter);
    }
    */
    struct list* list = NULL;
    list = list_new(NULL, int_cmp, int_print);
    #define COUNT 10
    int arr[COUNT] = {0};
    int i=0;
    for(i = COUNT - 1;i >= 0;i--){
        arr[i] = i;
        list_add(list, &arr[i]);
    }
    void *data = NULL;
    struct iter* iter = NULL;
    i = 0;
    list_print(list, NULL);
    //list_sort(list);
    //list_print(list, NULL);
    iter = list_iter(list);
    printf("\nPrint With Iter\n");
    for(i = 0, data = iter_next(iter);  data; data = iter_next(iter), i++){
        if(i>COUNT){
            printf("ERROR\n");
            break;
        }
        printf("%d: %d\n", i, *(int*)data);
    }
    printf("\nPrint With Index\n");
    for(i = 0;i < COUNT; i++){
        data = list_get(list, i);
        if(!data){
            TRACE(ERROR, "Not Found");
            break;
        }
        printf("%d: %d\n", i, *(int*)data);
    }
    int index = 0;
    printf("\nPrint With Find\n");
    for(i = 0;i < COUNT; i++){
        index = list_find(list, &i);
        if(index<0){
            TRACE(ERROR,"Failed to find %d", i);
            continue;
        }
        data = list_get(list, i);
        if(!data){
            TRACE(ERROR, "Not Found");
            break;
        }
        printf("%d: %d, %d\n", i, *(int*)data, index);
    }
    
    struct dict *dict = NULL;
    char msg[120];
    dict = dict_new(NULL, int_cmp, int_print_dict);
    for(i = 0;i < COUNT; i++){
        sprintf(msg, "%d", i);
        dict_set(dict, msg, &arr[i]);
    }
    dict_print(dict, NULL);
    dict_set(dict, "1", NULL);
    dict_print(dict, NULL);

}


