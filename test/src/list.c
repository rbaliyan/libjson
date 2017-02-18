#include <stdio.h>
#include <stdlib.h>
#include "list.h"
#include "iter.h"
#include "test.h"

#define MOUDULE "TestList"
#include "trace.h"

#define TEST_LIST_SIZE 100

int arr[TEST_LIST_SIZE];

static int int_cmp(const void *d1, const void*d2)
{
    return *(int*)d2 - *(int*)d1;
}
static int int_print(const void *stream, unsigned int index, const void *data)
{
    if(!stream)
        stream = stdout;
    return fprintf((FILE*)stream, "%d ", *(int*)(data));
}


static struct list* list_create(int sorted)
{
    int i;
    struct list* list = NULL;
    if((list = list_new(NULL, int_cmp, int_print))){
        for(i = 0; i < TEST_LIST_SIZE; i++){
            if(sorted){
                if((list_add_sorted(list, &arr[i]))< 0 ){
                    TRACE(ERROR, "List insertion failed");
                }
            } else {
                if((list_add(list, &arr[i]))< 0 ){
                    TRACE(ERROR, "List insertion failed");
                }
            }
        }
    }
    return list;
}

static int test_generate(void)
{
    struct list* list = NULL;
    if((list = list_create(0))){
        list_del(list);
        return 1;
    }
    return 0;
}

static int test_generate_sorted(void)
{
    struct list* list = NULL;
    if((list = list_create(1))){
        list_del(list);
        return 1;
    }
    return 0;
}

int test_print(void)
{
    int status = 0;
    struct list* list = NULL;
    if((list = list_create(0))){
        if((list_print(list, NULL))<0){
            TRACE(ERROR, "List dump failed");
        } else {
            status = 1;
        }
        list_del(list);
    }
    return status;
}

static int test_print_sorted(void)
{
    int status = 0;
    struct list* list = NULL;
    if((list = list_create(1))){
        if((list_print(list, NULL))<0){
            TRACE(ERROR, "List dump failed");
        } else {
            status = 1;
        }
        list_del(list);
    }
    return status;
}

static int test_sort(void)
{
    int prev = 0;
    int current = 0;
    int i;
    void *data = NULL;
    int status = 0;
    struct list* list = NULL;
    if((list = list_create(0))){
        if((list_sort(list))<0){
            TRACE(ERROR, "List sort failed");
        } else {
            status = 1;
            for(i = 0;i < TEST_LIST_SIZE; i++){
                if(!(data = list_get(list, i))){
                    TRACE(ERROR, "Not Found");
                    break;
                }
                if(i){
                    current = *(int*)data;
                    if(prev > current){
                        TRACE(ERROR, "Not Sorted Prev:%d Current:%d", prev, current);
                        status = 0;
                    }
                } else{
                    prev = *(int*)data;
                }
            }
        }
        list_del(list);
    }
    return status;
}

static int test_sorted(void)
{
    int prev = 0;;
    int current = 0;
    int i;
    void *data = NULL;
    int status = 0;
    struct list* list = NULL;
    if((list = list_create(1))){
        status = 1;
        for(i = 0;i < TEST_LIST_SIZE; i++){
            if(!(data = list_get(list, i))){
                TRACE(ERROR, "Not Found");
                break;
            }
            if(i){
                current = *(int*)data;
                if(prev > current){
                    TRACE(ERROR, "Not Sorted Prev:%d Current:%d", prev, current);
                    status = 0;
                }
            } else{
                prev = *(int*)data;
            }
        }
        list_del(list);
    }
    return status;
}


static int test_iter(void)
{
    void *data = NULL;
    struct iter* iter = NULL;
    int status = 0;
    struct list* list = NULL;
    int i;
    if((list = list_create(0))){
        if((iter = list_iter(list))){
            status = 1;
            for(i = 0, data = iter_next(iter);  data; data = iter_next(iter), i++){
                if(i > TEST_LIST_SIZE){
                    status = 0;
                    TRACE(ERROR,"Iterator passed list length");
                    break;
                }
                if(*(int*)data != arr[i]){
                    TRACE(ERROR,"Data mismatch");
                    status = 0;
                }
            }
            iter_del(iter);
        } else {
            TRACE(ERROR, "List iterator failed");
        }
        
        list_del(list);
    }
    return status;
}

static int test_find(void)
{
    void *data = NULL;
    int status = 0;
    int i, index;
    int n = 0, m=0;
    struct list* list = NULL;
    if((list = list_create(0))){
        status = 1;
        for(i = 0;i < TEST_LIST_SIZE; i++){
            n = arr[i];
            if((index = list_find(list, &n)) < 0){
                TRACE(ERROR,"Failed to find %d", i);
                status = 0;
                continue;
            }

            if(!(data = list_get(list, index))){
                TRACE(ERROR, "Not Found");
                status = 0;
                break;
            }
            m = *(int*)data;

            if(m!=n){
                TRACE(ERROR, "Mismatch Find %d and get %d", n, m);
            }
        }
        list_del(list);
    }
    return status;
}

static int test_index(void)
{
    void *data = NULL;
    int status = 0;
    int i;
    int m;
    struct list* list = NULL;
    if((list = list_create(0))){
        status = 1;
        for(i = 0; i < TEST_LIST_SIZE; i++){

            if(!(data = list_get(list, i))){
                TRACE(ERROR, "Not Found");
                break;
            }
            m = *(int*)data;

            if(m != arr[i]){
                TRACE(ERROR, "Mismatch Index %d and get %d", arr[i], m);
            }
        }
        list_del(list);
    }
    return status;
}

static void init()
{
    int i, r;
    for(i = 0; i < TEST_LIST_SIZE; i++){
        r = rand() % 100;
        arr[i] = r;
    }
}

int test_list_run(void)
{
    init();
    TEST_SUITE_INIT("List Test");
    TEST_SUITE_BEGIN();
    TEST_RUN(test_generate, "List Creation");
    TEST_RUN(test_generate_sorted,"Sorted List Creation" );
    TEST_RUN(test_index, "Index Access");
    TEST_RUN(test_print, "Print");
    TEST_RUN(test_print_sorted, "Sorted List Print");
    TEST_RUN(test_find, "Find");
    TEST_RUN(test_iter, "Iter");
    TEST_RUN(test_sort, "Sort");
    TEST_RUN(test_sorted, "Sorted List");
    TEST_SUITE_RESULTS();
    return 1;
}
