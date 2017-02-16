#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iter.h"

#define MODULE "Iterator"
#include "trace.h"


struct iter
{
    void* data;
    void* current;
    iter_free_t free;
    iter_next_t next;
    iter_get_t get;
    iter_size_t size;
};

struct iter* iter_new(void *data, iter_free_t f, iter_get_t get, iter_next_t next, iter_size_t size)
{
    struct iter*  iter = NULL;
    if(next && get){
        if((iter = malloc(sizeof(struct iter)))){
            iter->data = data;
            iter->current = NULL;
            iter->free = f;
            iter->next = next;
            iter->get = get;
            iter->size = size;
        } else {
            TRACE(ERROR, "Failed to allocate Iterator");
        }
    } else {
        TRACE(ERROR, "Invalid argumennts");
    }
    return iter;
}

void* iter_next(struct iter* iter)
{
    void *data = NULL;
    if(iter && iter->next){
        if((iter->current = iter->next(iter->data, iter->current))){
            data = iter->get(iter->data, iter->current);
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
    }
    return data;
}

void* iter_get(struct iter* iter)
{
    void *data = NULL;
    if(iter && iter->next){
        if(iter->current){
            data = iter->get(iter->data,iter->current);
        } else {
            TRACE(ERROR, "Call Next first");
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
    }
    return data;
}

void iter_reset(struct iter*iter)
{
    if(iter && iter->next){
        iter->current = NULL;
    } else {
        TRACE(ERROR, "Invalid arguments");
    }
}

void iter_del(struct iter *iter)
{
    if(iter){
        if(iter->free){
            iter->free(iter->data);
            free(iter);
        }
    } else {
        TRACE(ERROR, "Invalid arguments");
    }
}

int iter_size(struct iter *iter)
{
    if(iter){
        if(iter->size){
            return iter->size(iter->data);
        } else {
             TRACE(ERROR, "Unsupported size operation");
        }
    }else {
        TRACE(ERROR, "Invalid arguments");
    }
    return -1;
}