#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dict.h"
#include "list.h"

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

static struct node* node_new(struct dict *dict,char* key, void* val)
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
            node->dict->free(node->val);
        }
        free(node->key);
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
        dict->list = list_new(node_del, node_cmp, node_print);
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
                if((node = list_get(dict->list, index))){
                    if(dict->free){
                        TRACE(DEBUG, "Free Previous Value %p", node->val);
                        dict->free(node->val);
                    }
                    node->val = val;
                    ret = 0;
                } else {
                    TRACE(ERROR, "Internal error dict entry at %d got deleted", index);
                }
            } else {
                TRACE(INFO, "Delete Key:%s", key);
                list_remove(dict->list, index);
            }
        } else if( val && ((node = node_new(dict, key, val)))){
            if(list_add_sorted(dict->list, node) > 0){

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
    struct node* node = NULL;
    struct node temp = {0};
    temp.key = (char*)key;
    temp.val = NULL;
    temp.dict = dict;
    int index = 0;
    if(dict && key){
         if((index = list_find(dict->list, &temp)) >= 0){
             if((node = list_get(dict->list, index))){
                data = node->val;
             } else {
                 TRACE(ERROR, "Internal error dict entry at %d got deleted", index);
             }
             
         } else {
            TRACE(INFO,"Failed to find key:%s", key);
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return dict;
}

void dict_del(struct dict* dict)
{
    if(dict){
        list_del(dict->list);
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