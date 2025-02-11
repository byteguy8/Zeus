#ifndef NATIVE_LIB_H
#define NATIVE_LIB_H

#include "types.h"
#include "value.h"
#include "obj.h"
#include "vm_utils.h"
#include <dlfcn.h>

static Value temp_value = {0};
static VM *vm = NULL;

static Value *value_at(size_t at, Value *values){
    return &values[at];
}

static uint8_t to_empty(void *raw){
    return 0;
}

static uint8_t to_bool(void *raw){
    Value *value = (Value *)raw;
    return (uint8_t)TO_INT(value);
}

static int64_t to_int(void *raw){
    Value *value = (Value *)raw;
    return TO_INT(value);
}

static void *to_obj(void *raw){
    Value *value = (Value *)raw;
    return value->literal.obj;
}

static Value *empty_value(void *raw){
    memset(&temp_value, 0, sizeof(Value));
    return &temp_value;
}

static Value *bool_value(void *raw){
    memset(&temp_value, 0, sizeof(Value));
    temp_value = BOOL_VALUE(*(uint8_t *)raw);
    return &temp_value;
}

static Value *int_value(void *raw){
    memset(&temp_value, 0, sizeof(Value));
    temp_value = INT_VALUE(*(int64_t *)raw);
    return &temp_value;
}

static Value *obj_value(void *raw){
    memset(&temp_value, 0, sizeof(Value));
    temp_value = OBJ_VALUE((Obj *)raw);
    return &temp_value;
}

static void *str_create(char *buff){
    Obj *str_obj = vm_utils_clone_str_obj(buff, NULL, vm);
    
    if(!str_obj){
        vm_utils_error(vm, "Out of memory");
    }

    return str_obj;
}

char *str_buff(void *raw){
    Obj *str_obj = (Obj *)raw;
    Str *str = str_obj->value.str;
    return str->buff;
};

static void *array_create(int64_t len){
    if(len < 0 || len >= INT16_MAX){
        vm_utils_error(vm, "Illegal length. Must be 0 <= length <= %d", INT16_MAX);
    }
    
    Obj *array_obj = vm_utils_array_obj(len, vm);
    
    if(!array_obj){
        vm_utils_error(vm, "Out of memory");
    }

    return array_obj;
}

static void array_set_bool_at(int64_t index, uint8_t value, void *raw){
    Obj *array_obj = (Obj *)raw;
    Array *array = array_obj->value.array;

    if(index < 0 || index >= array->len){
        vm_utils_error(vm, "Index out of bounds. Must be 0 <= index < %ld", array->len);
    }

    array->values[index] = BOOL_VALUE(value);
}

static void array_set_int_at(int64_t index, int64_t value, void *raw){
    Obj *array_obj = (Obj *)raw;
    Array *array = array_obj->value.array;

    if(index < 0 || index >= array->len){
        vm_utils_error(vm, "Index out of bounds. Must be 0 <= index < %ld", array->len);
    }

    array->values[index] = INT_VALUE(value);
}

static void array_set_str_at(int64_t index, void * value, void *raw){
    Obj *array_obj = (Obj *)raw;
    Array *array = array_obj->value.array;

    if(index < 0 || index >= array->len){
        vm_utils_error(vm, "Index out of bounds. Must be 0 <= index < %ld", array->len);
    }

    Obj *value_obj = (Obj *)value;

    if(value_obj->type != STR_OTYPE){
        vm_utils_error(vm, "Expect string, but got something else");
    }

    array->values[index] = OBJ_VALUE(value);
}

static void array_set_array_at(int64_t index, void * value, void *raw){
    Obj *array_obj = (Obj *)raw;
    Array *array = array_obj->value.array;

    if(index < 0 || index >= array->len){
        vm_utils_error(vm, "Index out of bounds. Must be 0 <= index < %ld", array->len);
    }

    Obj *value_obj = (Obj *)value;

    if(value_obj->type != ARRAY_OTYPE){
        vm_utils_error(vm, "Expect array, but got something else");
    }

    array->values[index] = OBJ_VALUE(value);
}

//> LIST RELATED
static void *list_create(){
    Obj *list_obj = vm_utils_list_obj(vm);
    
    if(!list_obj){
        vm_utils_error(vm, "Out of memory");
    }

    return list_obj;
}

static void list_add_bool(uint8_t value, void *raw){
    Obj *list_obj = (Obj *)raw;
    DynArr *list = list_obj->value.list;
    Value literal_value = BOOL_VALUE(value);
    
    if(dynarr_insert(&literal_value, list)){
        vm_utils_error(vm, "Out of memory");
    }
}

static void list_add_int(int64_t value, void *raw){
    Obj *list_obj = (Obj *)raw;
    DynArr *list = list_obj->value.list;
    Value literal_value = INT_VALUE(value);
    
    if(dynarr_insert(&literal_value, list)){
        vm_utils_error(vm, "Out of memory");
    }
}

static void list_add_str(void * value, void *raw){
    Obj *list_obj = (Obj *)raw;
    DynArr *list = list_obj->value.list;
    Obj *value_obj = (Obj *)value;

    if(value_obj->type != STR_OTYPE){
        vm_utils_error(vm, "Expect string, but got something else");
    }

    Value obj_value = OBJ_VALUE(value_obj);

    if(dynarr_insert(&obj_value, list)){
        vm_utils_error(vm, "Out of memory");
    }
}

static void list_add_array(void * value, void *raw){
    Obj *list_obj = (Obj *)raw;
    DynArr *list = list_obj->value.list;
    Obj *value_obj = (Obj *)value;

    if(value_obj->type != ARRAY_OTYPE){
        vm_utils_error(vm, "Expect array, but got something else");
    }

    Value obj_value = OBJ_VALUE(value_obj);
    
    if(dynarr_insert(&obj_value, list)){
        vm_utils_error(vm, "Out of memory");
    }
}

static void list_add_list(void * value, void *raw){
    Obj *list_obj = (Obj *)raw;
    DynArr *list = list_obj->value.list;
    Obj *value_obj = (Obj *)value;

    if(value_obj->type != LIST_OTYPE){
        vm_utils_error(vm, "Expect list, but got something else");
    }

    Value obj_value = OBJ_VALUE(value_obj);
    
    if(dynarr_insert(&obj_value, list)){
        vm_utils_error(vm, "Out of memory");
    }
}
//< LIST RELATED

static void *get_native_lib_symbol(char *name, void *handler){
    void *symbol = dlsym(handler, name);
    
    if(!symbol){
        vm_utils_error(vm, "Failed to initialize native library. Essential interface function not found");
    }

    return symbol;
}

void native_lib_init(void *handler, VM *v){
    vm = v;

    void (*znative_init)(void) = get_native_lib_symbol("znative_init", handler);
    void (*znative_set_value_at)(Value *(*value_at)(size_t, Value *)) = get_native_lib_symbol("znative_set_value_at", handler);

    void (*znative_set_to_empty)(uint8_t (*to_empty)(void *)) = get_native_lib_symbol("znative_set_to_empty", handler);
    void (*znative_set_to_bool)(uint8_t (*to_bool)(void *)) = get_native_lib_symbol("znative_set_to_bool", handler);
    void (*znative_set_to_int)(int64_t (*to_int)(void *)) = get_native_lib_symbol("znative_set_to_int", handler);
    void (*znative_set_to_str)(void *(*to_str)(void *)) = get_native_lib_symbol("znative_set_to_str", handler);
    void (*znative_set_to_array)(void *(*to_array)(void *)) = get_native_lib_symbol("znative_set_to_array", handler);
    void (*znative_set_to_list)(void *(*to_list)(void *)) = get_native_lib_symbol("znative_set_to_list", handler);
    
    void (*znative_set_empty_value)(Value *(*empty_value)(void *)) = get_native_lib_symbol("znative_set_empty_value", handler);
    void (*znative_set_bool_value)(Value *(*bool_value)(void *)) = get_native_lib_symbol("znative_set_bool_value", handler);
    void (*znative_set_int_value)(Value *(*int_value)(void *)) = get_native_lib_symbol("znative_set_int_value", handler);
    void (*znative_set_str_value)(Value *(*str_value)(void *)) = get_native_lib_symbol("znative_set_str_value", handler);
    void (*znative_set_array_value)(Value *(*array_value)(void *)) = get_native_lib_symbol("znative_set_array_value", handler);
    void (*znative_set_list_value)(Value *(*list_value)(void *)) = get_native_lib_symbol("znative_set_list_value", handler);
    
    void (*znative_set_str_create)(void *(*create_str)(char *)) = get_native_lib_symbol("znative_set_str_create", handler);
    void (*znative_set_str_buff)(char *(*str_buff)(void *)) = get_native_lib_symbol("znative_set_str_buff", handler);
    
    void (*znative_set_array_create)(void *(*array_create)(int64_t)) = get_native_lib_symbol("znative_set_array_create", handler);
    void (*znative_set_array_set_bool_at)(void (*array_set_bool_at)(int64_t, uint8_t, void *)) = get_native_lib_symbol("znative_set_array_set_bool_at", handler);
    void (*znative_set_array_set_int_at)(void (*array_set_int_at)(int64_t, int64_t, void *)) = get_native_lib_symbol("znative_set_array_set_int_at", handler);
    void (*znative_set_array_set_str_at)(void (*array_set_str_at)(int64_t, void *, void *)) = get_native_lib_symbol("znative_set_array_set_str_at", handler);
    void (*znative_set_array_set_array_at)(void (*array_set_array_at)(int64_t, void *, void *)) = get_native_lib_symbol("znative_set_array_set_array_at", handler);

    void (*znative_set_list_create)(void *(*array_create)(void)) = get_native_lib_symbol("znative_set_list_create", handler);
    void (*znative_set_list_add_bool)(void (*array_add_bool)(uint8_t, void *)) = get_native_lib_symbol("znative_set_list_add_bool", handler);
    void (*znative_set_list_add_int)(void (*array_add_int)(int64_t, void *)) = get_native_lib_symbol("znative_set_list_add_int", handler);
    void (*znative_set_list_add_str)(void (*array_add_str)(void *, void *)) = get_native_lib_symbol("znative_set_list_add_str", handler);
    void (*znative_set_list_add_array)(void (*array_add_array)(void *, void *)) = get_native_lib_symbol("znative_set_list_add_array", handler);
    void (*znative_set_list_add_list)(void (*array_add_list)(void *, void *)) = get_native_lib_symbol("znative_set_list_add_list", handler);

    znative_set_value_at(value_at);
    
    znative_set_to_empty(to_empty);
    znative_set_to_bool(to_bool);
    znative_set_to_int(to_int);
    znative_set_to_str(to_obj);
    znative_set_to_array(to_obj);
    znative_set_to_list(to_obj);

    znative_set_empty_value(empty_value);
    znative_set_bool_value(bool_value);
    znative_set_int_value(int_value);
    znative_set_str_value(obj_value);
    znative_set_array_value(obj_value);
    znative_set_list_value(obj_value);

    znative_set_str_create(str_create);
    znative_set_str_buff(str_buff);
    
    znative_set_array_create(array_create);
    znative_set_array_set_bool_at(array_set_bool_at);
    znative_set_array_set_int_at(array_set_int_at);
    znative_set_array_set_str_at(array_set_str_at);
    znative_set_array_set_array_at(array_set_array_at);

    znative_set_list_create(list_create);
    znative_set_list_add_bool(list_add_bool);
    znative_set_list_add_int(list_add_int);
    znative_set_list_add_str(list_add_str);
    znative_set_list_add_array(list_add_array);
    znative_set_list_add_list(list_add_list);

    znative_init();
}

#endif