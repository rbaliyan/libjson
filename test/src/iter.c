#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "iter.h"
#include "test.h"

#define MODULE "IterTest"
#include "trace.h"
#define TEST_INT_LIMIT 100

const char* test_str ="A very long ";


void* int_get(void* data, void* current)
{
    return current;
}
void* int_next(void* data, void* current)
{
    long count;
    count = (int)current;
    if(count < TEST_INT_LIMIT){
        count = count + 1;
    } else {
        count = 0;
    }
    return (void*)count;
}

int int_size(void* data)
{
    return TEST_INT_LIMIT;
}

int test_int()
{
    int status = 1;
    int val = 0;
    int x = 0;
    struct iter* iter = NULL; 
    if(!(iter = iter_new(NULL, NULL, int_get, int_next, int_size))){
        TRACE(ERROR,"Failed to create iter");
        status = 0;
    } else {
        for(x = 1,val = (int)iter_next(iter); val; val = (int)iter_next(iter), x++){
            if(val != x){
                TRACE(ERROR,"Value Mismatch");
                status = 0;
            }
        }
    }
    return status;
}

int test_iter_run(void)
{
    TEST_SUITE_INIT("JSON Test");
    TEST_SUITE_BEGIN();
    TEST_RUN(test_int, "Integer Iterator");
    TEST_SUITE_RESULTS();
    return 1;
}