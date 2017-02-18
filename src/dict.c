#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dict.h"
#include "list.h"
#include "iter.h"

#define MODULE "Dict"
#include "trace.h"

struct node
{
    struct dict *dict;
    unsigned int flags;
    char* key;
    void* val;
};

struct dict
{
    unsigned int flags;
    struct list *list;
    dict_cmp_t cmp;
    dict_free_t free; 
    dict_print_t print;
};

static void* dict_iter_next(void* data, void* current);
static void* dict_iter_get(void* data, void* current);

static struct node* node_new(struct dict *dict, char* key, void* val)
{
    struct node* node = NULL;
    if( key && val ){
        if((node = malloc(sizeof(struct node)))){
            node->key = strdup(key);
            node->val = val;
            node->dict = dict;
            node->flags = 0;
        } else {
        TRACE(ERROR,"Failed to allocate dict node");
    }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }

    return node;
}

static void node_del(void *data)
{
    struct node *node = (struct node *)data;
    if(node){
        if(node->dict && node->dict->free){
            node->dict->free((void*)node->val);
        }
        free((void*)node->key);
        free(node);
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
}

static int node_print(void* stream, unsigned int index, void *data)
{
    struct node *node = (struct node *)data;
    int ret = 0;
    if(node && node->dict){
        ret = node->dict->print(stream, index, node->key, node->val);
    } else {
        TRACE(ERROR,"Invalid arguments");
    }

    return ret;
}

static int node_cmp(void* data1, void* data2)
{
    struct node *node1 = (struct node *)data1;
    struct node *node2 = (struct node *)data2;
    return strcmp(node1->key, node2->key);
}

struct dict* dict_new(dict_free_t f, dict_cmp_t cmp, dict_print_t print)
{
    struct dict* dict = NULL;
    if((dict = malloc(sizeof(struct dict)))){
        dict->print = print;
        dict->free = f;
        dict->cmp = cmp;
        dict->list = list_new((list_free_t)node_del, (list_cmp_t)node_cmp, (list_print_t)node_print);
    } else {
        TRACE(ERROR,"Failed to allocate dict");
    }
    return dict;
}

int dict_set(struct dict *dict, char* key, void* val)
{
    int ret = -1;
    struct node *node = NULL;
    struct node temp = {0};
    temp.key = key;
    temp.val = val;
    temp.dict = dict;
    int index = 0;

    if(dict && key){
        if((index = list_find(dict->list, &temp)) >= 0){
            TRACE(INFO, "Found entry @%d", index);
            if(val){
                if((node = (struct node*)list_get(dict->list, index))){
                    if(dict->free){
                        TRACE(DEBUG, "Free Previous Value %p", node->val);
                        dict->free(node->val);
                    }
                    node->val = val;
                    ret = list_size(dict->list);
                } else {
                    TRACE(ERROR, "Internal error dict entry at %d got deleted", index);
                }
            } else {
                TRACE(INFO, "Delete Key:%s", key);
                list_remove(dict->list, index);
                ret = list_size(dict->list);
            }
        } else if( val && ((node = node_new(dict, key, val)))){
            if((list_add(dict->list, node)) < 0){
                TRACE(ERROR, "Insertion failed");
                node_del(node);
            } else {
                ret = list_size(dict->list);
            }
        } else if(!val){
            TRACE(WARN, "Key Not Found : %s", key);
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return ret;
}

void* dict_get(struct dict* dict, char* key)
{
    void *data = NULL;
    struct node *node = NULL;
    struct node temp = {0};
    temp.key = (char*)key;
    temp.val = NULL;
    temp.dict = (struct dict*)dict;
    int index = 0;
    if(dict && key){
         if((index = list_find(dict->list, &temp)) >= 0){
             if((node = list_get(dict->list, index))){
                data = node->val;
             } else {
                 TRACE(ERROR, "Internal error dict entry at %d got deleted", index);
             }
         } else {
            //TRACE(INFO,"Failed to find key:%s", key);
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return data;
}

void dict_del(struct dict* dict)
{
    if(dict){
        list_del(dict->list);
        free(dict);
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
}

int dict_print(struct dict* dict, void* stream)
{
    int ret = 0;
    if(dict){
        ret = list_print(dict->list, stream);
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return ret;
}

static void* dict_iter_next(void* data, void* current)
{
    struct iter *iter = (struct iter*)data;
    if(iter){
        if(!current){
            /* Make Sure to reset*/
            iter_reset(iter);
        }
        return iter_next(iter);
    } else {
        TRACE(ERROR,"Null iter");
    }
    return NULL;
}

static void* dict_iter_get(void* data, void* current)
{
    struct iter *iter = (struct iter*)data;
    struct node *node = NULL;
    if(iter){
        if((node = iter_get(iter)))
            return node->key;
    } else {
        TRACE(ERROR,"Null Dict");
    }
    return NULL;
}

static int dict_iter_size(void* data)
{
    struct iter *iter = (struct iter*)data;
    if(iter){
        return iter_size(iter);
    } else {
        TRACE(ERROR, "Invalid argumets");
    }
    return -1;
}

struct iter* dict_iter(struct dict* dict)
{
    struct iter* l_iter = NULL;
    struct iter* d_iter = NULL;
    if(dict){
        if((l_iter = list_iter(dict->list))){
            if((d_iter = iter_new(l_iter, NULL, dict_iter_get, dict_iter_next, dict_iter_size))){
                return d_iter;
            } else {
                 TRACE(ERROR, "Failed to create iter");
                 iter_del(l_iter);
            }
        } else {
            TRACE(ERROR, "Failed to create iter");
        }
    } else {
        TRACE(ERROR,"Null Dict");
    }
    return NULL;
}

int dict_size(struct dict *dict)
{
    int len = -1;
    if(dict){
        len = list_size(dict->list);
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return len;
}
