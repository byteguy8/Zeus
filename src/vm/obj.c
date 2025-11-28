#include "obj.h"

void obj_list_insert(Obj *obj, ObjList *list){
    obj->list = list;

    if(list->tail){
        list->tail->next = obj;
        obj->prev = list->tail;
    }else{
        list->head = obj;
    }

    list->len++;
    list->tail = obj;
}

void obj_list_remove(Obj *obj){
    ObjList *list = obj->list;

    if(obj->prev){
        obj->prev->next = obj->next;
    }
    if(obj->next){
        obj->next->prev = obj->prev;
    }

    if(list->head == obj){
        list->head = obj->next;
    }
    if(list->tail == obj){
        list->tail = obj->prev;
    }

    list->len--;

    obj->prev = NULL;
    obj->next = NULL;
    obj->list = NULL;
}