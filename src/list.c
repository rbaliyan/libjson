#include <stdio.h>
#include <stdlib.h>
#include "list.h"

#define MODULE "List"
#include "trace.h"

enum list_flags
{
    LIST_SORTED = 0x01,
};


#define SetFlag(x,y)            ((x) |= (y))
#define ReSetFlag(x,y)          ((x) &= ~(y))
#define ReSetFlagAll(x)         ((x) = 0)
#define IsFlagSet(x,y)          ((x) & (y))
#define IsFlagReSet(x,y)        !((x) & (y))

#define ListSetFlag(x,y)        SetFlag((x)->flags,y)
#define ListReSetFlag(x,y)      ReSetFlag((x)->flags,y)
#define ListIsFlagSet(x,y)      IsFlagSet((x)->flags,y)
#define ListIsFlagReSet(x,y)    IsFlagReSet((x)->flags,y)
#define NodeSetFlag(x,y)        SetFlag((x)->flags,y)
#define NodeReSetFlag(x,y)      ReSetFlag((x)->flags,y)
#define NodeIsFlagSet(x,y)      IsFlagSet((x)->flags,y)
#define NodeIsFlagReSet(x,y)    IsFlagReSet((x)->flags,y)

#define ListSetSorted(x)        ListSetFlag(x, LIST_SORTED)
#define ListSetUnSorted(x)      ListReSetFlag(x, LIST_SORTED)
#define ListIsSorted(x)         ListIsFlagSet(x,LIST_SORTED)
#define ListIsUnSorted(x)       ListIsFlagReSet(x,LIST_SORTED)


struct node
{
    void* data;
    unsigned int flags;
    struct list* list;
    struct node* next;
    struct node* prev;
};
struct list
{
    unsigned int flags;
    list_free_t free;
    list_cmp_t cmp;
    list_print_t print;
    unsigned int count;
    struct node* start;
    struct node* end;
};

struct iter
{
    unsigned int flags;
    struct list *list;
    struct node* current;
};

static struct node* node_new(struct list* list, void *data);
static void node_add(struct list* list, struct node* node);
static void node_add_sorted(struct list* list, struct node* node);
static struct node* node_index(struct node* node, unsigned int index);
static struct node* node_find(struct node* node, void* data, list_cmp_t cmp, int *index);

static struct node* node_new(struct list* list, void *data)
{
     struct node* node = NULL;
     if((node = malloc(sizeof(struct node)))){
         node->data = data;
         node->prev = node->next = NULL;
         node->flags = 0;
         node->list = list;
     } else {
         TRACE(ERROR, "Failed to allocate memory");
     }

     return node;
}

static void node_add(struct list* list, struct node* node)
{
    if(list && node){
        node->prev = list->end;
        if(list->end){
            list->end->next = node;
        } else {
            list->start = node;
        }
        list->end = node;
        list->count++;
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
}

static void node_add_sorted(struct list* list, struct node* node)
{
    struct node* temp = NULL;
    if(list && node ){
        if(!list->cmp){
            TRACE(WARN, "Operation not supported");
            return;
        }

        for(temp = list->start; temp; temp = temp->next){
            if((list->cmp(temp->data, node->data)) < 0){
                break;
            }
        }

        if(temp){
            if(temp->prev){
                temp->prev->next = node;
            }
            node->prev = temp->prev;
            node->next = temp;
            temp->prev = node;
            if(temp == list->start){
                list->start = node;
            }

        } else if(list->end){
            list->end->next = node;
            node->prev = list->end;
            list->end = node;
        } else {
            list->start = list->end = node;
        }
        list->count++;
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
}

static struct node* node_index(struct node* node, unsigned int index)
{
    for(; node && index; node = node->next, index--);
    return node;
}

static struct node* node_find(struct node* node, void* data, list_cmp_t cmp,int *index)
{
    int i = -1;
    if(!node || !cmp || !node->list)
        return NULL;
    
     for(i = 0; node && index; node = node->next, i++){
        if(node->list->cmp(node->data, data) == 0){
            if(index)
                *index = i;
            break;
        }
     }
     return node;
}

struct list* list_new(list_free_t f, list_cmp_t cmp, list_print_t print)
{
    struct list* list = NULL;
    if((list = malloc(sizeof(struct list)))){
        list->start = list->end = NULL;
        list->count = 0;
        list->free = f;
        list->cmp = cmp;
        list->print = print;
        list->flags = 0;
        /* Empty List is sorted */
        ListSetSorted(list); 
    } else {
        TRACE(ERROR, "Failed to allocate memory");
    }
    return list;
}

int list_add(struct list* list, void* data)
{
    struct node* node = NULL;
    int count = -1;
    if(list){
        if((node = node_new(list, data))){
            /* We are appending in the list with out checking content, hence it becomes unsorted*/
            ListSetUnSorted(list);
            node_add(list, node);
            count = list->count;
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return count;
}

int list_add_sorted(struct list* list, void* data)
{
    struct node* node = NULL;
    int count = -1;
    if(list){
        if((node = node_new(list, data))){
            /* Sort List if not sorted */
            if(ListIsUnSorted(list)){
                TRACE(INFO,"Sorting unsorted list");
                list_sort(list);
            }

            node_add_sorted(list, node);
            count = list->count;
        } else {
            TRACE(ERROR, "Failed to allocate node");
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return count;
}


void* list_get(struct list *list, unsigned int index)
{
    void* data = NULL;
    struct node* node = NULL;
    if(list && (list->count > index)){
        if((node = node_index(list->start, index))){
            data = node->data;
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }

    return data;
}

int list_sort(struct list *list)
{
    struct node *temp = NULL;
    struct node *node = NULL;
    int count = -1;
    if(list){
        if(ListIsUnSorted(list)){
            node = list->start;
            list->start = list->end = NULL;
            list->count = 0;
            ListSetSorted(list);
            for(; node; node = temp){
                temp = node->next;
                node->list = NULL;
                node->next = node->prev = NULL;
                node_add_sorted(list, node);
            }
            count = list->count;
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return count;
}

int list_find(struct list *list, void* data)
{
    int index = -1;
    if(list){
        node_find(list->start, data, list->cmp, &index);
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return index;
}

struct iter* list_iter(struct list* list)
{
    struct iter* iter = NULL;
    if(list){
        if((iter = malloc(sizeof(struct iter)))){
            iter->list = list;
            iter->current = NULL;
        } else {
            TRACE(ERROR,"Failed to alloc iter");
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return iter;
}

void* iter_next(struct iter *iter)
{
    void *data = NULL;
    if(iter && iter->list){

        if(iter->current){
            iter->current = iter->current->next;
        } else {
            iter->current = iter->list->start;
        }

        if(iter->current){
            data = iter->current->data;
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return data;
}

void* iter_get(struct iter *iter)
{
    void *data = NULL;
    if(iter && iter->current){
        data = iter->current->data;
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return data;
}

void list_del(struct list* list)
{
    struct node *next = NULL;
    struct node *node = NULL;
    if(list){
        for(node = list->start; node; node = next){
            next = node->next;
            if(list->free){
                list->free(node->data);
            }
            free(node);
        }
        free(list);
    } else {
        TRACE(ERROR,"Invalid arguments");
    }

}

void iter_del(struct iter *iter)
{
    if(iter){
        free(iter);
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
}

int list_print(struct list *list, void *stream)
{
    int i;
    int ret = 0;
    struct node* node = NULL;
    if(list && list->print){
        for(node = list->start, i = 0; node; node = node->next, i++){
            ret += list->print(stream, i, node->data);
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }

    return ret;
}


int list_remove(struct list* list, unsigned int index)
{
    int ret = -1;
    struct node* node = NULL;
    if(list && (index < list->count)){
        if((node = node_index(list->start, index))){
            if(node->prev){
                node->prev->next = node->next;
            }
            if(node->next){
                node->next->prev = node->prev;
            }

            if(list->free){
                list->free(node->data);
            }
            free(node);
        }
    } else {
        TRACE(ERROR,"Invalid arguments");
    }
    return ret;
}