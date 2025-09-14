#include "vmu.h"
#include "memory.h"
#include "factory.h"
#include "bstr.h"
#include "lzbstr.h"
#include "fn.h"
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#define POOL_DEFAULT_ALLOC_LEN 1024

#define ALLOC_STR_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_STR_OBJS_POOL))
#define DEALLOC_STR_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_ARRAY_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_ARRAY_OBJS_POOL))
#define DEALLOC_ARRAY_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_LIST_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_LIST_OBJS_POOL))
#define DEALLOC_LIST_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_DICT_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_DICT_OBJS_POOL))
#define DEALLOC_DICT_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_RECORD_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_RECORD_OBJS_POOL))
#define DEALLOC_RECORD_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_FN_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_FN_OBJS_POOL))
#define DEALLOC_FN_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_NATIVE_FN_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_NATIVE_FN_OBJS_POOL))
#define DEALLOC_NATIVE_FN_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_CLOSURE()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_CLOSURES_POOL))
#define DEALLOC_CLOSURE(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_CLOSURE_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_CLOSURE_OBJS_POOL))
#define DEALLOC_CLOSURE_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_NATIVE_MODULE_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_NATIVE_MODULE_OBJS_POOL))
#define DEALLOC_NATIVE_MODULE_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define ALLOC_MODULE_OBJ()(lzpool_alloc_x(POOL_DEFAULT_ALLOC_LEN, VMU_MODULE_OBJS_POOL))
#define DEALLOC_MODULE_OBJ(_ptr)(lzpool_dealloc(_ptr))

#define FIND_LOCATION(index, arr)(dynarr_find(&((OPCodeLocation){.offset = index, .line = -1}), compare_locations, arr))
#define FRAME_AT(at, vm)(&vm->frame_stack[at])

#define VMU_IS_VALUE_IN(_value)((_value).type == INT_VTYPE)
#define VMU_IS_VALUE_OBJ(_value)((_value).type == OBJ_VTYPE)
#define VMU_VALUE_TO_OBJ(_value)((Obj *)((_value).content.obj))

static inline void init_obj(ObjType type, Obj *obj, VM *vm){
    obj->type = type;
    obj->marked = 0;
    obj->color = WHITE_OBJ_COLOR;
    obj->prev = NULL;
    obj->next = NULL;
    obj_list_insert(obj, &vm->white_objs);
}
//--------------------------------------------------
//               PRIVATE INTERFACE                //
//--------------------------------------------------
//----------      GARBAGE COLLECTOR       --------//
void prepare_module_globals(Module *module, VM *vm);
void prepare_worklist(VM *vm);
void mark_objs(VM *vm);
void sweep_objs(VM *vm);
void normalize_objs(VM *vm);
//----------            OTHERS            --------//
static int compare_locations(const void *a, const void *b);
static BStr *prepare_stacktrace(unsigned int spaces, VM *vm);
static int prepare_stacktrace_new(unsigned int spaces, LZBStr *str, VM *vm);

static void obj_to_str(Obj *obj, Obj *parent, LZBStr *str);
static void value_to_str(Value value, Obj *parent, LZBStr *str);
//--------------------------------------------------
//             PRIVATE IMPLEMENTATION             //
//--------------------------------------------------
void prepare_module_globals(Module *module, VM *vm){
    LZOHTable *globals = MODULE_GLOBALS(module);

    for (size_t i = 0; i < globals->m; i++){
        LZOHTableSlot *slot = &globals->slots[i];

        if(!slot->used){
            continue;
        }

        GlobalValue *global_value = (GlobalValue *)slot->value;
        Value *value = &global_value->value;

        if(IS_VALUE_OBJ(value) && VALUE_TO_OBJ(value)->color == WHITE_OBJ_COLOR){
            Obj *obj = VALUE_TO_OBJ(value);
            obj->color = GRAY_OBJ_COLOR;
            obj_list_remove(obj);
            obj_list_insert(obj, &vm->gray_objs);

            if(obj->type == MODULE_OBJ_TYPE){
                Module *module = OBJ_TO_MODULE(obj)->module;
                prepare_module_globals(module, vm);
            }
        }
    }
}

void prepare_worklist(VM *vm){
    prepare_module_globals(vm->modules_stack, vm);

    const Value *stack_top = vm->stack_top;

    for (Value *value = vm->stack; value < stack_top; value++){
        if(IS_VALUE_OBJ(value) && VALUE_TO_OBJ(value)->color == WHITE_OBJ_COLOR){
            Obj *obj = VALUE_TO_OBJ(value);
            obj->color = GRAY_OBJ_COLOR;
            obj_list_remove(obj);
            obj_list_insert(obj, &vm->gray_objs);

            if(obj->type == MODULE_OBJ_TYPE){
                Module *module = OBJ_TO_MODULE(obj)->module;
                prepare_module_globals(module, vm);
            }
        }
    }
}

void mark_objs(VM *vm){
    ObjList *gray_objs = &vm->gray_objs;

    while (gray_objs->head){
        Obj *current = gray_objs->head;

        switch (current->type){
            case STR_OBJ_TYPE:{
                break;
            }case ARRAY_OBJ_TYPE:{
                ArrayObj *array_obj = OBJ_TO_ARRAY(current);
                size_t len = array_obj->len;
                Value *values = array_obj->values;

                for (size_t i = 0; i < len; i++){
                    Value raw_value = values[i];

                    if(VMU_IS_VALUE_OBJ(raw_value) && VMU_VALUE_TO_OBJ(raw_value)->color == WHITE_OBJ_COLOR){
                        Obj *obj = VMU_VALUE_TO_OBJ(raw_value);
                        obj->color = GRAY_OBJ_COLOR;
                        obj_list_remove(obj);
                        obj_list_insert(obj, &vm->gray_objs);
                    }
                }

                break;
            }case LIST_OBJ_TYPE:{
                ListObj *list_obj = OBJ_TO_LIST(current);
                DynArr *items = list_obj->items;
                size_t len = DYNARR_LEN(items);

                for (size_t i = 0; i < len; i++){
                    Value raw_value = DYNARR_GET_AS(Value, i, items);

                    if(VMU_IS_VALUE_OBJ(raw_value) && VMU_VALUE_TO_OBJ(raw_value)->color == WHITE_OBJ_COLOR){
                        Obj *obj = VMU_VALUE_TO_OBJ(raw_value);
                        obj->color = GRAY_OBJ_COLOR;
                        obj_list_remove(obj);
                        obj_list_insert(obj, &vm->gray_objs);
                    }
                }

                break;
            }case DICT_OBJ_TYPE:{
                DictObj *dict_obj = OBJ_TO_DICT(current);
                LZOHTable *key_values = dict_obj->key_values;
                size_t m = key_values->m;

                for (size_t i = 0; i < m; i++){
                    LZOHTableSlot slot = key_values->slots[i];

                    if(slot.used){
                        continue;
                    }

                    Value raw_key = *(Value *)(slot.key);
                    Value raw_value = *(Value *)(slot.value);

                    if(VMU_IS_VALUE_OBJ(raw_key) && VMU_VALUE_TO_OBJ(raw_key)->color == WHITE_OBJ_COLOR){
                        Obj *obj = VMU_VALUE_TO_OBJ(raw_key);
                        obj->color = GRAY_OBJ_COLOR;
                        obj_list_remove(obj);
                        obj_list_insert(obj, &vm->gray_objs);
                    }

                    if(VMU_IS_VALUE_OBJ(raw_value) && VMU_VALUE_TO_OBJ(raw_value)->color == WHITE_OBJ_COLOR){
                        Obj *obj = VMU_VALUE_TO_OBJ(raw_value);
                        obj->color = GRAY_OBJ_COLOR;
                        obj_list_remove(obj);
                        obj_list_insert(obj, &vm->gray_objs);
                    }
                }

                break;
            }case RECORD_OBJ_TYPE:{
                RecordObj *record_obj = OBJ_TO_RECORD(current);
                LZOHTable *attrs = record_obj->attrs;
                LZOHTableSlot *slots = attrs->slots;
                size_t len = attrs->n;

                for (size_t i = 0; i < len; i++){
                    LZOHTableSlot slot = slots[i];

                    if(!slot.used){
                        continue;
                    }

                    Value raw_value = *(Value *)(slot.value);

                    if(VMU_IS_VALUE_OBJ(raw_value) && VMU_VALUE_TO_OBJ(raw_value)->color == WHITE_OBJ_COLOR){
                        Obj *obj = VMU_VALUE_TO_OBJ(raw_value);
                        obj->color = GRAY_OBJ_COLOR;
                        obj_list_remove(obj);
                        obj_list_insert(obj, &vm->gray_objs);
                    }
                }

                break;
            }case NATIVE_FN_OBJ_TYPE:{
                NativeFnObj *native_fn_obj = OBJ_TO_NATIVE_FN(current);
                Value target = native_fn_obj->target;

                if(VMU_IS_VALUE_OBJ(target) && VMU_VALUE_TO_OBJ(target)->color == WHITE_OBJ_COLOR){
                    Obj *obj = VMU_VALUE_TO_OBJ(target);
                    obj->color = GRAY_OBJ_COLOR;
                    obj_list_remove(obj);
                    obj_list_insert(obj, &vm->gray_objs);
                }

                break;
            }case FN_OBJ_TYPE:{
                break;
            }case CLOSURE_OBJ_TYPE:{
                break;
            }case NATIVE_MODULE_OBJ_TYPE:{
                break;
            }case MODULE_OBJ_TYPE:{
                break;
            }default:{
                break;
            }
        }

        current->color = BLACK_OBJ_COLOR;

        obj_list_remove(current);
        obj_list_insert(current, &vm->black_objs);
    }
}

void sweep_objs(VM *vm){
    ObjList *white_objs = &vm->white_objs;
    Obj *current = white_objs->head;
    Obj *next = NULL;

    while (current){
        next = current->next;

        switch(current->type){
            case STR_OBJ_TYPE:{
                vmu_destroy_str(OBJ_TO_STR(current), vm);
                break;
            }case ARRAY_OBJ_TYPE:{
                vmu_destroy_array(OBJ_TO_ARRAY(current), vm);
                break;
            }case LIST_OBJ_TYPE:{
                vmu_destroy_list(OBJ_TO_LIST(current), vm);
                break;
            }case DICT_OBJ_TYPE:{
                vmu_destroy_dict(OBJ_TO_DICT(current), vm);
                break;
            }case RECORD_OBJ_TYPE:{
                vmu_destroy_record(OBJ_TO_RECORD(current), vm);
                break;
            }case NATIVE_FN_OBJ_TYPE:{
                vmu_destroy_native_fn(OBJ_TO_NATIVE_FN(current), vm);
                break;
            }case FN_OBJ_TYPE:{
                vmu_destroy_fn(OBJ_TO_FN(current), vm);
                break;
            }case CLOSURE_OBJ_TYPE:{
                vmu_destroy_closure(OBJ_TO_CLOSURE(current), vm);
                break;
            }case NATIVE_MODULE_OBJ_TYPE:{
                vmu_destroy_native_module_obj(OBJ_TO_NATIVE_MODULE(current), vm);
                break;
            }case MODULE_OBJ_TYPE:{
                vmu_destroy_module_obj(OBJ_TO_MODULE(current), vm);
                break;
            }default:{
                assert(0 && "Illegal object type");
            }
        }

        current = next;
    }

    white_objs->len = 0;
    white_objs->head = NULL;
    white_objs->tail = NULL;
}

void normalize_objs(VM *vm){
    ObjList *white_objs = &vm->white_objs;
    ObjList *black_objs = &vm->black_objs;
    Obj *current = black_objs->head;
    Obj *next = NULL;

    while (current){
        next = current->next;

        current->color = WHITE_OBJ_COLOR;
        obj_list_remove(current);
        obj_list_insert(current, white_objs);

        current = next;
    }
}

static int compare_locations(const void *a, const void *b){
	OPCodeLocation *location_a = (OPCodeLocation *)a;
	OPCodeLocation *location_b = (OPCodeLocation *)b;

	if(location_a->offset < location_b->offset){
        return -1;
    }else if(location_a->offset > location_b->offset){
        return 1;
    }else{
        return 0;
    }
}

static BStr *prepare_stacktrace(unsigned int spaces, VM *vm){
	BStr *st = FACTORY_BSTR(vm->allocator);

    for(Frame *frame = vm->frame_stack; frame < vm->frame_ptr; frame++){
        Fn *fn = frame->fn;
		DynArr *locations = fn->locations;
		int index = FIND_LOCATION(frame->last_offset, locations);
		OPCodeLocation *location = index == -1 ? NULL : (OPCodeLocation *)dynarr_get_raw(index, locations);

		if(location){
			bstr_append_args(
				st,
				"%*sin file: '%s' at %s:%d\n",
                spaces,
                "",
                location->filepath,
				frame->fn->name,
				location->line
			);
		}else{
			bstr_append_args(st, "inside function '%s'\n", fn->name);
        }
    }

	return st;
}

static int prepare_stacktrace_new(unsigned int spaces, LZBStr *str, VM *vm){
    for(Frame *frame = vm->frame_stack; frame < vm->frame_ptr; frame++){
        Fn *fn = frame->fn;
		DynArr *locations = fn->locations;
		int idx = FIND_LOCATION(frame->last_offset, locations);
		OPCodeLocation *location = idx == -1 ? NULL : (OPCodeLocation *)dynarr_get_raw(idx, locations);

		if(location){
            if(lzbstr_append_args(
                str,
                "%*sin file: '%s' at %s:%d\n",
                spaces,
                "",
                location->filepath,
				frame->fn->name,
				location->line
            )){
                return 1;
            }
		}else{
            if(lzbstr_append_args(str, "inside function '%s'\n", fn->name)){
                return 1;
            }
        }
    }

    return 0;
}

static void obj_to_str(Obj *obj, Obj *parent, LZBStr *str){
    if(obj == parent){
        lzbstr_append("self", str);
        return;
    }

    switch (obj->type){
        case STR_OBJ_TYPE:{
            StrObj *str_obj = OBJ_TO_STR(obj);
            lzbstr_append(str_obj->buff, str);
            break;
        }case ARRAY_OBJ_TYPE:{
            ArrayObj *array_obj = OBJ_TO_ARRAY(obj);
            size_t len = array_obj->len;
            Value *values = array_obj->values;

            lzbstr_append("[", str);

            for (size_t i = 0; i < len; i++){
                Value *value = &values[i];

                if(IS_VALUE_STR(value)){
                    lzbstr_append("'", str);
                    value_to_str(*value, obj, str);
                    lzbstr_append("'", str);
                }else{
                    value_to_str(*value, obj, str);
                }

                if(i + 1 < len){
                    lzbstr_append(", ", str);
                }
            }

            lzbstr_append("]", str);

            break;
        }case LIST_OBJ_TYPE:{
            ListObj *list_obj = OBJ_TO_LIST(obj);
            DynArr *items = list_obj->items;
            size_t len = DYNARR_LEN(items);

            lzbstr_append("(", str);

            for (size_t i = 0; i < len; i++){
                Value *value = (Value *)dynarr_get_raw(i, items);

                if(IS_VALUE_STR(value)){
                    lzbstr_append("'", str);
                    value_to_str(*value, obj, str);
                    lzbstr_append("'", str);
                }else{
                    value_to_str(*value, obj, str);
                }

                if(i + 1 < len){
                    lzbstr_append(", ", str);
                }
            }

            lzbstr_append(")", str);

            break;
        }case DICT_OBJ_TYPE:{
            DictObj *dict_obj = OBJ_TO_DICT(obj);
            LZOHTable *key_values = dict_obj->key_values;

            size_t count = 0;
            size_t m = key_values->m;
            size_t n = key_values->n;

            lzbstr_append("{", str);

            for (size_t i = 0; i < m; i++){
                LZOHTableSlot slot = key_values->slots[i];

                if(!slot.used){
                    continue;
                }

                Value *key = (Value *)slot.key;
                Value *value = (Value *)slot.value;

                if(IS_VALUE_STR(key)){
                    lzbstr_append("'", str);
                    value_to_str(*key, obj, str);
                    lzbstr_append("'", str);
                }else{
                    value_to_str(*key, obj, str);
                }

                lzbstr_append(": ", str);

                if(IS_VALUE_STR(value)){
                    lzbstr_append("'", str);
                    value_to_str(*value, obj, str);
                    lzbstr_append("'", str);
                }else{
                    value_to_str(*value, obj, str);
                }

                if(count + 1 < n){
                    lzbstr_append(", ", str);
                }

                count++;
            }

            lzbstr_append("}", str);

            break;
        }case RECORD_OBJ_TYPE:{
            RecordObj *record_obj = OBJ_TO_RECORD(obj);
            LZOHTable *attrs = record_obj->attrs;

            size_t count = 0;
            size_t n = attrs->n;
            size_t m = attrs->m;
            LZOHTableSlot *slots = attrs->slots;

            lzbstr_append("{", str);

            for (size_t i = 0; i < m; i++){
                LZOHTableSlot slot = slots[i];

                if(!slot.used){
                    continue;
                }

                char *key = (char *)slot.key;
                Value *value = (Value *)slot.value;

                lzbstr_append_args(str, "%s: ", key);

                if(IS_VALUE_STR(value)){
                    lzbstr_append("'", str);
                    value_to_str(*value, obj, str);
                    lzbstr_append("'", str);
                }else{
                    value_to_str(*value, obj, str);
                }

                if(count + 1 < n){
                    lzbstr_append(", ", str);
                }

                count++;
            }

            lzbstr_append("}", str);

            break;
        }case NATIVE_FN_OBJ_TYPE:{
            NativeFnObj *native_fn_obj = OBJ_TO_NATIVE_FN(obj);
            NativeFn *native_fn = native_fn_obj->native_fn;

            lzbstr_append_args(str, "<native function " PRIu8 " %p>", native_fn->arity, native_fn_obj);

            break;
        }case FN_OBJ_TYPE:{
            FnObj *fn_obj = OBJ_TO_FN(obj);
            Fn *fn = fn_obj->fn;

            lzbstr_append_args(str, "<function %zu %p>", DYNARR_LEN(fn->params), fn_obj);

            break;
        }case CLOSURE_OBJ_TYPE:{
            ClosureObj *closure_obj = OBJ_TO_CLOSURE(obj);
            lzbstr_append_args(str, "<closure %p>", closure_obj);
            break;
        }case NATIVE_MODULE_OBJ_TYPE:{
            NativeModuleObj *native_module_obj = OBJ_TO_NATIVE_MODULE(obj);
            lzbstr_append_args(str, "<native module %p>", native_module_obj);
            break;
        }case MODULE_OBJ_TYPE:{
            ModuleObj *module_obj = OBJ_TO_MODULE(obj);
            lzbstr_append_args(str, "<module %p>", module_obj);
            break;
        }default:{
            assert(0 && "Illegal object type");
            break;
        }
    }
}

static void value_to_str(Value value, Obj *parent, LZBStr *str){
    switch (value.type){
        case EMPTY_VTYPE:{
            lzbstr_append("empty", str);
            break;
        }case BOOL_VTYPE:{
            uint8_t bool_value = value.content.bool;
            lzbstr_append_args(str, "%s", bool_value ? "true" : "false");
            break;
        }case INT_VTYPE:{
            int64_t int_value = value.content.ivalue;
            lzbstr_append_args(str, "%" PRId64, int_value);
            break;
        }case FLOAT_VTYPE:{
            double float_value = value.content.fvalue;
            lzbstr_append_args(str, "%f", float_value);
            break;
        }case OBJ_VTYPE:{
            Obj *obj = VMU_VALUE_TO_OBJ(value);
            obj_to_str(obj, parent, str);
            break;
        }default:{
            assert(0 && "Illegal value type");
            break;
        }
    }
}
//--------------------------------------------------
//             PUBLIC IMPLEMENTATION              //
//--------------------------------------------------
int vmu_error(VM *vm, char *msg, ...){
	BStr *st = prepare_stacktrace(4, vm);

    va_list args;
	va_start(args, msg);

    fprintf(stderr, "Runtime error: ");
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

    if(st && st->buff){
        fprintf(stderr, "%s", (char *)st->buff);
    }

	va_end(args);
	bstr_destroy(st);

    vm->exit_code = ERR_VMRESULT;

    longjmp(vm->exit_jmp, 1);

    return 0;
}

static inline void print_x_times(FILE *stream, char *format, unsigned int times){
    for (unsigned int i = 0; i < times; i++){
        fprintf(stream, "%s", format);
    }
}

int vmu_internal_error(VM *vm, char *msg, ...){
    BStr *st = prepare_stacktrace(4, vm);
    LZBStr *err_str = FACTORY_LZBSTR(vm->allocator);

    if(lzbstr_append("FATAL RUNTIME ERROR.\nA UNEXPECTED OPERATION IN THE VIRTUAL MACHINE WITH AN ILLEGAL STATE HAVE BEEN DETECTED.\nPLEASE, READ WITH CARE THE CAUSE:\n\n\n", err_str)){
        return 0;
    }

    va_list args;

	va_start(args, msg);
    int len = vsnprintf(NULL, 0, msg, args);
    char *allocated_err_msg = MEMORY_ALLOC(char, len + 1, vm->allocator);

    if(allocated_err_msg){
        va_start(args, msg);
        vsnprintf(allocated_err_msg, len + 1, msg, args);

        lzbstr_append(allocated_err_msg, err_str);
        lzbstr_append_args(err_str, "\n%s", st->buff);

        int line_len = 0;
        int max_line_len = 0;

        for (int i = 0; i < err_str->offset; i++){
            char c = err_str->buff[i];

            if(c == '\n'){
                if(line_len > max_line_len){
                    max_line_len = line_len;
                }

                line_len = 0;

                continue;
            }

            line_len++;
        }

        if(max_line_len == 0){
            max_line_len = line_len;
        }

        fprintf(stderr, "//**");
        print_x_times(stderr, "*", max_line_len);
        fprintf(stderr, "**//\n");

        fprintf(stderr, "//  ");
        print_x_times(stderr, " ", max_line_len);
        fprintf(stderr, "  //\n");

        fprintf(stderr, "//  ");

        int chars_count = 0;

        for (int i = 0; i < err_str->offset; i++){
            char c = err_str->buff[i];

            if(c == '\n'){
                int diff = max_line_len - chars_count;

                print_x_times(stderr, " ", diff);
                fprintf(stderr, "  //\n");
                fprintf(stderr, "//  ");

                chars_count = 0;

                continue;
            }

            fprintf(stderr, "%c", c);
            chars_count++;
        }

        int diff = max_line_len - chars_count;

        print_x_times(stderr, " ", diff);
        fprintf(stderr, "  //\n");

        fprintf(stderr, "//  ");
        print_x_times(stderr, " ", max_line_len);
        fprintf(stderr, "  //\n");

        fprintf(stderr, "//**");
        print_x_times(stderr, "*", max_line_len);
        fprintf(stderr, "**//\n");

        MEMORY_DEALLOC(char, len + 1, allocated_err_msg, vm->allocator);
    }else{
        fprintf(stderr, "FATAL RUNTIME ERROR:\n\t");
        vfprintf(stderr, msg, args);

        if(st && st->buff){
            fprintf(stderr, "%s", (char *)st->buff);
        }

        fprintf(stderr, "\n--------------------");
        fprintf(stderr, "\n");
    }

	va_end(args);
	bstr_destroy(st);

    vm->exit_code = ERR_VMRESULT;

    longjmp(vm->exit_jmp, 1);

    return 0;
}

inline uint8_t validate_value_bool_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(!IS_VALUE_BOOL(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'bool'", param, name);
    }

    return VALUE_TO_BOOL(value);
}

inline int64_t validate_value_int_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(!IS_VALUE_INT(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'int'", param, name);
    }

    return VALUE_TO_INT(value);
}

inline double validate_value_float_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(!IS_VALUE_FLOAT(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'float'", param, name);
    }

    return VALUE_TO_FLOAT(value);
}

inline double validate_value_ifloat_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(IS_VALUE_INT(value)){
        return (double)VALUE_TO_INT(value);
    }

    if(IS_VALUE_FLOAT(value)){
        return VALUE_TO_FLOAT(value);
    }

    vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'int' or 'float'", param, name);

    return -1;
}

inline int64_t validate_value_int_range_arg(Value *value, uint8_t param, char *name, int64_t from, int64_t to, VM *vm){
    if(!IS_VALUE_INT(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'int'", param, name);
    }

    int64_t i64_value = VALUE_TO_INT(value);

    if(i64_value < from){
        vmu_error(vm, "Illegal value of argument %" PRIu8 ": expect '%s' be greater or equals to %" PRId64 ", but got %" PRId64, param, name, from, i64_value);
    }

    if(i64_value > to){
        vmu_error(vm, "Illegal value of argument %" PRIu8 ": expect '%s' be less or equals to %" PRId64 ", but got %" PRId64, param, name, to, i64_value);
    }

    return i64_value;
}

inline StrObj *validate_value_str_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(!IS_VALUE_STR(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'str'", param, name);
    }

    return VALUE_TO_STR(value);
}

void vmu_clean_up(VM *vm){
    ObjList *white_objs = &vm->white_objs;
    Obj *current = white_objs->head;
    Obj *next = NULL;

    while (current){
        next = current->next;
        obj_list_remove(current);

        switch(current->type){
            case STR_OBJ_TYPE:{
                vmu_destroy_str(OBJ_TO_STR(current), vm);
                break;
            }case ARRAY_OBJ_TYPE:{
                vmu_destroy_array(OBJ_TO_ARRAY(current), vm);
                break;
            }case LIST_OBJ_TYPE:{
                vmu_destroy_list(OBJ_TO_LIST(current), vm);
                break;
            }case DICT_OBJ_TYPE:{
                vmu_destroy_dict(OBJ_TO_DICT(current), vm);
                break;
            }case RECORD_OBJ_TYPE:{
                vmu_destroy_record(OBJ_TO_RECORD(current), vm);
                break;
            }case NATIVE_FN_OBJ_TYPE:{
                vmu_destroy_native_fn(OBJ_TO_NATIVE_FN(current), vm);
                break;
            }case FN_OBJ_TYPE:{
                vmu_destroy_fn(OBJ_TO_FN(current), vm);
                break;
            }case CLOSURE_OBJ_TYPE:{
                vmu_destroy_closure(OBJ_TO_CLOSURE(current), vm);
                break;
            }case NATIVE_MODULE_OBJ_TYPE:{
                vmu_destroy_native_module_obj(OBJ_TO_NATIVE_MODULE(current), vm);
                break;
            }case MODULE_OBJ_TYPE:{
                vmu_destroy_module_obj(OBJ_TO_MODULE(current), vm);
                break;
            }default:{
                assert(0 && "Illegal object type");
            }
        }

        current = next;
    }
}

void vmu_gc(VM *vm){
    prepare_worklist(vm);
    mark_objs(vm);
    sweep_objs(vm);
    normalize_objs(vm);
}

inline Frame *vmu_current_frame(VM *vm){
    return vm->frame_ptr - 1;
}

inline void vmu_value_to_str_w(Value value, LZBStr *str){
    value_to_str(value, NULL, str);
}

char *vmu_value_to_str(Value value, VM *vm, size_t *out_len){
    char *str_value = NULL;
    LZBStr *str = FACTORY_LZBSTR(vm->allocator);

    value_to_str(value, NULL, str);
    str_value = lzbstr_rclone_buff((LZBStrAllocator *)VMU_FRONT_ALLOCATOR, str, out_len);
    lzbstr_destroy(str);

    return str_value;
}

void vmu_print_obj(FILE *stream, Obj *object){
    switch (object->type){
        case STR_OBJ_TYPE:{
            StrObj *str = OBJ_TO_STR(object);
            fprintf(stream, "%s", str->buff);
            break;
        }case ARRAY_OBJ_TYPE:{
			ArrayObj *array = OBJ_TO_ARRAY(object);
            fprintf(stream, "<array %zu at %p>", array->len, array);
			break;
		}case LIST_OBJ_TYPE:{
			ListObj *list_obj = OBJ_TO_LIST(object);
            DynArr *list = list_obj->items;
            fprintf(stream, "<list %zu at %p>", list->used, list);
			break;
		}case DICT_OBJ_TYPE:{
            DictObj *dict_obj = OBJ_TO_DICT(object);
            LZOHTable *dict = dict_obj->key_values;
            fprintf(stream, "<dict %zu at %p>", dict->n, dict);
            break;
        }case RECORD_OBJ_TYPE:{
			RecordObj *record_obj = OBJ_TO_RECORD(object);
            fprintf(stream, "<record %zu at %p>", record_obj->attrs ? record_obj->attrs->n : 0, record_obj);
			break;
		}case NATIVE_FN_OBJ_TYPE:{
            NativeFnObj *native_fn_obj = OBJ_TO_NATIVE_FN(object);
            NativeFn *native_fn = native_fn_obj->native_fn;
            fprintf(stream, "<native function '%s' - %d at %p>", native_fn->name, native_fn->arity, native_fn);
            break;
        }case FN_OBJ_TYPE:{
            FnObj *fn_obj = OBJ_TO_FN(object);
            Fn *fn = fn_obj->fn;
            fprintf(stream, "<function '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);
            break;
        }case CLOSURE_OBJ_TYPE:{
            ClosureObj *closure_obj = OBJ_TO_CLOSURE(object);
            Closure *closure = closure_obj->closure;
            Fn *fn = closure->meta->fn;
            fprintf(stream, "<closure '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);
            break;
        }case NATIVE_MODULE_OBJ_TYPE:{
            NativeModuleObj *native_module_obj = OBJ_TO_NATIVE_MODULE(object);
            NativeModule *module = native_module_obj->native_module;
            fprintf(stream, "<native module '%s' at %p>", module->name, module);
            break;
        }case MODULE_OBJ_TYPE:{
            ModuleObj *module_obj = OBJ_TO_MODULE(object);
            Module *module = module_obj->module;
            fprintf(stream, "<module '%s' '%s' at %p>", module->name, module->pathname, module);
            break;
        }default:{
            assert("Illegal object type");
        }
    }
}

void vmu_print_value(FILE *stream, Value *value){
    switch (value->type){
        case EMPTY_VTYPE:{
            fprintf(stream, "empty");
            break;
        }case BOOL_VTYPE:{
            uint8_t bool = VALUE_TO_BOOL(value);
            fprintf(stream, "%s", bool == 0 ? "false" : "true");
            break;
        }case INT_VTYPE:{
            fprintf(stream, "%" PRId64, VALUE_TO_INT(value));
            break;
        }case FLOAT_VTYPE:{
			fprintf(stream, "%.8f", VALUE_TO_FLOAT(value));
            break;
		}case OBJ_VTYPE:{
            vmu_print_obj(stream, VALUE_TO_OBJ(value));
            break;
        }default:{
            assert("Illegal value type");
        }
    }
}

inline Value *vmu_clone_value(Value value, VM *vm){
    Value *cloned_value = MEMORY_ALLOC(Value, 1, VMU_FRONT_ALLOCATOR);
    *cloned_value = value;
    return cloned_value;
}

inline void vmu_destroy_value(Value *value, VM *vm){
    if(!value){
        return;
    }

    MEMORY_DEALLOC(Value, 1, value, VMU_FRONT_ALLOCATOR);
}

inline int vmu_create_str(char runtime, size_t raw_str_len, char *raw_str, VM *vm, StrObj **out_str_obj){
    size_t len = ((((size_t)0) - (raw_str_len == 0)) & 1) | ((((size_t)0) - (raw_str_len > 0)) & raw_str_len);
    LZOHTable *runtime_strs = vm->runtime_strs;
    StrObj *str_obj = NULL;

    if(lzohtable_lookup(len, raw_str, runtime_strs, (void **)&str_obj)){
        *out_str_obj = str_obj;
        return 1;
    }

    str_obj = ALLOC_STR_OBJ();
    Obj *obj = (Obj *)str_obj;

    init_obj(STR_OBJ_TYPE, obj, vm);
    str_obj->runtime = runtime;
    str_obj->len = raw_str_len;
    str_obj->buff = raw_str;

    lzohtable_put_ck(len, raw_str, str_obj, runtime_strs, NULL);
    *out_str_obj = str_obj;

    return 0;
}

inline void vmu_destroy_str(StrObj *str_obj, VM *vm){
    if(!str_obj){
        return;
    }

    char *key = str_obj->buff;
    size_t key_len = str_obj->len;
    LZOHTable *runtime_strs = vm->runtime_strs;

    LZOHTABLE_REMOVE(key_len, key, runtime_strs);

    if(str_obj->runtime){
        MEMORY_DEALLOC(char, key_len + 1, key, VMU_FRONT_ALLOCATOR);
    }

    lzpool_dealloc(str_obj);
}

int vmu_str_is_int(StrObj *str_obj){
    size_t len = str_obj->len;
    char *buff = str_obj->buff;

    if(len == 0){
        return 0;
    }

    if(buff[0] == '-' && len == 1){
        return 0;
    }

    for (size_t i = buff[0] == '-' ? 1 : 0; i < len; i++){
        char c = buff[i];

        if(c < '0' || c > '9'){
            return 0;
        }
    }

    return 1;
}

int vmu_str_is_float(StrObj *str_obj){
    size_t len = str_obj->len;
    char *buff = str_obj->buff;

    if(len == 0){
        return 0;
    }

    char ndot = 1;
    char is_negative = buff[0] == '-';
    size_t dot_from = is_negative ? 2 : 1;

    if(is_negative && len == 1){
        return 0;
    }

    for (size_t i = is_negative ? 1 : 0; i < len; i++){
        char c = buff[i];

        if(c == '.' && i >= dot_from && ndot){
            ndot = 0;
            continue;
        }

        if(c < '0' || c > '9'){
            return 0;
        }
    }

    return 1;
}

inline int64_t vmu_str_len(StrObj *str_obj){
    return (int64_t)(str_obj->len);
}

inline StrObj *vmu_str_char(int64_t idx, StrObj *str_obj, VM *vm){
    size_t len = str_obj->len;
    char *buff = str_obj->buff;

    if((uint64_t)idx >= len){
        vmu_error(vm, "Index (%" PRId64 ") out of bounds (%zu)", idx, len);
    }

    StrObj *char_str_obj = NULL;
    char *new_buff = MEMORY_ALLOC(char, 2, VMU_FRONT_ALLOCATOR);

    new_buff[0] = buff[idx];
    new_buff[1] = 0;

    if(lzohtable_lookup(1, new_buff, vm->runtime_strs, (void **)(&char_str_obj))){
        MEMORY_DEALLOC(char, 2, new_buff, VMU_FRONT_ALLOCATOR);
        return char_str_obj;
    }

    vmu_create_str(1, 1, new_buff, vm, &char_str_obj);

    return char_str_obj;
}

inline int64_t vmu_str_code(int64_t idx, StrObj *str_obj, VM *vm){
    if(idx < 0){
        vmu_error(vm, "Failed to get string's character code: 'at' index (%" PRId64 ") less than 0", idx);
    }

    size_t at = (size_t)idx;
    size_t len = str_obj->len;
    char *buff = str_obj->buff;

    if(at >= len){
        vmu_error(vm, "Failed to get string's character code: 'at' index (%zu) out of bounds", at);
    }

    return (int64_t)buff[at];
}

inline StrObj *vmu_str_concat(StrObj *a_str_obj, StrObj *b_str_obj, VM *vm){
    size_t a_len = a_str_obj->len;
    size_t b_len = b_str_obj->len;
    size_t new_len = a_len + b_len;
    char *new_buff = MEMORY_ALLOC(char, new_len + 1, VMU_FRONT_ALLOCATOR);
    StrObj *new_str_obj = NULL;

    memcpy(new_buff, a_str_obj->buff, a_len);
    memcpy(new_buff + a_len, b_str_obj->buff, b_len);
    new_buff[new_len] = 0;

    if(vmu_create_str(1, new_len, new_buff, vm, &new_str_obj)){
        MEMORY_DEALLOC(char, new_len + 1, new_buff, VMU_FRONT_ALLOCATOR);
        return new_str_obj;
    }

    return new_str_obj;
}

inline StrObj *vmu_str_mul(int64_t by, StrObj *str_obj, VM *vm){
    size_t old_len = str_obj->len;

    char junk = (old_len * by + 1 > SIZE_MAX) && vmu_error(vm, "Failed to multiply string: resulting length exceed max capacity");

    size_t new_len = old_len * by;
    char *new_buff = MEMORY_ALLOC(char, new_len + 1, VMU_FRONT_ALLOCATOR);
    StrObj *new_str_obj = NULL;

    for (size_t i = 0; i < new_len; i += old_len){
        memcpy(new_buff + i, str_obj->buff, old_len);
    }

    new_buff[new_len] = 0;

    if(vmu_create_str(1, new_len, new_buff, vm, &new_str_obj)){
        MEMORY_DEALLOC(char, new_len + 1, new_buff, VMU_FRONT_ALLOCATOR);
    }

    return new_str_obj + junk;
}

StrObj *vmu_str_insert_at(int64_t idx, StrObj *a_str_obj, StrObj *b_str_obj, VM *vm){
    if(idx < 0){
        vmu_error(vm, "Failed to insert string: 'at' index %" PRId64 " is negative", idx);
    }

    size_t at = (size_t)idx;
    size_t a_len = a_str_obj->len;
    size_t b_len = b_str_obj->len;
    char *a_buff = a_str_obj->buff;
    char *b_buff = b_str_obj->buff;

    if(at > a_len){
        vmu_error(vm, "Failed to insert string: 'at' index (%zu) pass string length (%zu)", at, a_len);
    }

    size_t c_len = a_len + b_len;
    char *c_buff = MEMORY_ALLOC(char, c_len + 1, VMU_FRONT_ALLOCATOR);
    StrObj *c_str_obj = NULL;

    if(at < a_len){
        memcpy(c_buff, a_buff, at);
        memcpy(c_buff + at, b_buff, b_len);
        memcpy(c_buff + at + b_len, a_buff + at, a_len - at);
    }else{
        memcpy(c_buff, a_buff, a_len);
        memcpy(c_buff + a_len, b_buff, b_len);
    }

    c_buff[c_len] = 0;

    if(lzohtable_lookup(c_len, c_buff, vm->runtime_strs, (void **)(&c_str_obj))){
        MEMORY_DEALLOC(char, c_len + 1, c_buff, VMU_FRONT_ALLOCATOR);
        return c_str_obj;
    }

    vmu_create_str(1, c_len, c_buff, vm, &c_str_obj);

    return c_str_obj;
}

StrObj *vmu_str_remove(int64_t from, int64_t to, StrObj *str_obj, VM *vm){
    if(from < 0){
        vmu_error(vm, "Failed to remove string: 'from' index %" PRId64 " is negative", from);
    }

    if(from >= to){
        vmu_error(vm, "Failed to remove string: 'from' index %" PRId64 " is equals or bigger than 'to' index %" PRId64, from, to);
    }

    size_t start = (size_t)from;
    size_t end = (size_t)to;
    size_t old_len = str_obj->len;
    char *old_buff = str_obj->buff;

    if(end > old_len){
        vmu_error(vm, "Failed to remove string: 'to' index (%zu) pass string length (%zu)", end, old_len);
    }

    size_t new_len = old_len - (end - start);
    char *new_buff = MEMORY_ALLOC(char, new_len + 1, VMU_FRONT_ALLOCATOR);
    size_t left_len = start;
    size_t right_len = old_len - end;
    StrObj *new_str_obj = NULL;

    memcpy(new_buff, old_buff, left_len);
    memcpy(new_buff + left_len, old_buff + end, right_len);
    new_buff[new_len] = 0;

    if(lzohtable_lookup(new_len, new_buff, vm->runtime_strs, (void **)(&str_obj))){
        MEMORY_DEALLOC(char, new_len + 1, new_buff, VMU_FRONT_ALLOCATOR);
        return str_obj;
    }

    vmu_create_str(1, new_len, new_buff, vm, &new_str_obj);

    return new_str_obj;
}

StrObj *vmu_str_sub_str(int64_t from, int64_t to, StrObj *str_obj, VM *vm){
    if(from < 0){
        vmu_error(vm, "Failed to sub-string string: 'from' index %" PRId64 " is negative", from);
    }

    if(from >= to){
        vmu_error(vm, "Failed to sub-string string: 'from' index %" PRId64 " is equals or bigger than 'to' index %" PRId64, from, to);
    }

    size_t start = (size_t)from;
    size_t end = (size_t)to;
    size_t old_len = str_obj->len;
    char *old_buff = str_obj->buff;

    if(end > old_len){
        vmu_error(vm, "Failed to sub-string string: 'to' index (%zu) pass string length (%zu)", end, old_len);
    }

    size_t new_len = end - start;
    char *new_buff = MEMORY_ALLOC(char, new_len + 1, VMU_FRONT_ALLOCATOR);
    StrObj *new_str_obj = NULL;

    memcpy(new_buff, old_buff + start, new_len);
    new_buff[new_len] = 0;

    if(lzohtable_lookup(new_len, new_buff, vm->runtime_strs, (void **)(&str_obj))){
        MEMORY_DEALLOC(char, new_len + 1, new_buff, VMU_FRONT_ALLOCATOR);
        return str_obj;
    }

    vmu_create_str(1, new_len, new_buff, vm, &new_str_obj);

    return new_str_obj;
}

inline ArrayObj *vmu_create_array(int64_t len, VM *vm){
    Value *values = MEMORY_ALLOC(Value, (size_t)len, VMU_FRONT_ALLOCATOR);
    ArrayObj *array_obj = ALLOC_ARRAY_OBJ();
    Obj *obj = (Obj *)array_obj;

    memset(values, 0, VALUE_SIZE * (size_t)len);
    init_obj(ARRAY_OBJ_TYPE, obj, vm);
    array_obj->len = len;
    array_obj->values = values;

    return array_obj;
}

inline void vmu_destroy_array(ArrayObj *array_obj, VM *vm){
    if(!array_obj){
        return;
    }

    MEMORY_DEALLOC(Value, array_obj->len, array_obj->values, VMU_FRONT_ALLOCATOR);
    DEALLOC_ARRAY_OBJ(array_obj);
}

inline int64_t vmu_array_len(ArrayObj *array_obj){
    return (int64_t)array_obj->len;
}

inline Value vmu_array_get_at(int64_t idx, ArrayObj *array_obj, VM *vm){
    size_t len = array_obj->len;
    Value *values = array_obj->values;

    if((uint64_t)idx >= len){
        vmu_error(vm, "Index (%" PRId64 ") out of bounds (%zu)", idx, len);
    }

    return values[idx];
}

inline void vmu_array_set_at(int64_t idx, Value value, ArrayObj *array_obj, VM *vm){
    if(idx < 0){
        vmu_error(vm, "Failed to update array slot: index (%" PRId64 ") is negative");
    }

    size_t at = (size_t)idx;
    size_t len = array_obj->len;
    Value *values = array_obj->values;

    if(at >= len){
        vmu_error(vm, "Failed to update array slot: index (%zu) out of bounds", idx);
    }

    values[(size_t)idx] = value;
}

inline Value vmu_array_first(ArrayObj *array_obj, VM *vm){
    size_t len = array_obj->len;
    Value *values = array_obj->values;
    return len == 0 ? EMPTY_VALUE : values[0];
}

inline Value vmu_array_last(ArrayObj *array_obj, VM *vm){
    size_t len = array_obj->len;
    Value *values = array_obj->values;
    return len == 0 ? EMPTY_VALUE : values[len - 1];
}

inline ArrayObj *vmu_array_grow(int64_t by, ArrayObj *array_obj, VM *vm){
    if(by <= 1){
        vmu_error(vm, "Expect 'by' value greater than 1");
    }

    size_t len = array_obj->len;
    Value *values = array_obj->values;

    size_t new_len = len * by;
    Value *new_values = MEMORY_ALLOC(Value, new_len, VMU_FRONT_ALLOCATOR);
    ArrayObj *new_array_obj = ALLOC_ARRAY_OBJ();
    Obj *obj = (Obj *)new_array_obj;

    memcpy(new_values, values, VALUE_SIZE * len);
    memset(new_values + len, 0, VALUE_SIZE * len);
    init_obj(ARRAY_OBJ_TYPE, obj, vm);
    new_array_obj->len = new_len;
    new_array_obj->values = new_values;

    return new_array_obj;
}

inline ArrayObj *vmu_array_join(ArrayObj *a_array_obj, ArrayObj *b_array_obj, VM *vm){
    size_t a_len = a_array_obj->len;
    Value *a_values = a_array_obj->values;

    size_t b_len = b_array_obj->len;
    Value *b_values = b_array_obj->values;

    size_t new_len = a_len + b_len;
    Value *new_values = MEMORY_ALLOC(Value, new_len, VMU_FRONT_ALLOCATOR);
    ArrayObj *new_array_obj = ALLOC_ARRAY_OBJ();
    Obj *obj = (Obj *)new_array_obj;

    memmove(new_values, a_values, VALUE_SIZE * a_len);
    memmove(new_values + a_len, b_values, VALUE_SIZE * b_len);
    init_obj(ARRAY_OBJ_TYPE, obj, vm);
    new_array_obj->len = new_len;
    new_array_obj->values = new_values;

    return new_array_obj;
}

inline ArrayObj *vmu_array_join_value(Value value, ArrayObj *array_obj, VM *vm){
    size_t len = array_obj->len;
    Value *values = array_obj->values;
    ArrayObj *new_array_obj = ALLOC_ARRAY_OBJ();
    Value *new_values = new_array_obj->values;

    memcpy(new_values, values, VALUE_SIZE * len);
    new_values[len] = value;

    return new_array_obj;
}

inline ListObj *vmu_create_list(VM *vm){
    DynArr *values = FACTORY_DYNARR_TYPE(Value, VMU_FRONT_ALLOCATOR);
    ListObj *list_obj = ALLOC_LIST_OBJ();
    Obj *obj = (Obj *)list_obj;

    init_obj(LIST_OBJ_TYPE, obj, vm);
    list_obj->items = values;

    return list_obj;
}

inline void vmu_destroy_list(ListObj *list_obj, VM *vm){
    if(!list_obj){
        return;
    }

    dynarr_destroy(list_obj->items);
    DEALLOC_LIST_OBJ(list_obj);
}

inline int64_t vmu_list_len(ListObj *list_obj){
    return (int64_t)DYNARR_LEN(list_obj->items);
}

inline int64_t vmu_list_clear(ListObj *list_obj){
    DynArr *items = list_obj->items;
    int64_t len = (int64_t)DYNARR_LEN(items);

    dynarr_remove_all(items);

    return len;
}

inline ListObj *vmu_list_join(ListObj *a_list_obj, ListObj *b_list_obj, VM *vm){
    DynArr *a_list = a_list_obj->items;
    DynArr *b_list = b_list_obj->items;
    DynArr *c_list = dynarr_append_new(a_list, b_list, (DynArrAllocator *)VMU_FRONT_ALLOCATOR);
    ListObj *list_obj = ALLOC_LIST_OBJ();
    Obj *obj = (Obj *)list_obj;

    init_obj(LIST_OBJ_TYPE, obj, vm);
    list_obj->items = c_list;

    return list_obj;
}

inline Value vmu_list_get_at(int64_t idx, ListObj *list_obj, VM *vm){
    if(idx < 0){
        vmu_error(vm, "Failed to get item from list: 'at' index is negative");
    }

    size_t at = (size_t)idx;
    DynArr *items = list_obj->items;
    size_t len = DYNARR_LEN(items);

    if(at >= len){
        vmu_error(vm, "Failed to get item from list: 'at' index (%zu) out of bounds", at);
    }

    return DYNARR_GET_AS(Value, (size_t)idx, items);
}

inline void vmu_list_insert(Value value, ListObj *list_obj, VM *vm){
    DynArr *items = list_obj->items;
    dynarr_insert(&value, items);
}

inline ListObj *vmu_list_insert_new(Value value, ListObj *list_obj, VM *vm){
    DynArr *items = list_obj->items;
    size_t items_len = DYNARR_LEN(items);
    DynArr *new_items = dynarr_create_by(items->rsize, items_len + 1, (DynArrAllocator *)VMU_FRONT_ALLOCATOR);
    ListObj *new_list_obj = ALLOC_LIST_OBJ();
    Obj *obj = (Obj *)new_list_obj;

    dynarr_append(items, new_items);
    dynarr_insert(&value, new_items);

    init_obj(LIST_OBJ_TYPE, obj, vm);
    new_list_obj->items = new_items;

    return new_list_obj;
}

void vmu_list_insert_at(int64_t idx, Value value, ListObj *list_obj, VM *vm){
    if(idx < 0){
        vmu_error(vm, "Failed to insert item to list: 'at' index is negative");
    }

    size_t at = (size_t)idx;
    DynArr *items = list_obj->items;
    size_t len = DYNARR_LEN(items);

    if(at > len){
        vmu_error(vm, "Failed to insert item to list: 'at' index (%zu) out of bounds", at);
    }

    dynarr_insert_at(at, &value, items);
}

inline Value vmu_list_set_at(int64_t idx, Value value, ListObj *list_obj, VM *vm){
    if(idx < 0){
        vmu_error(vm, "Failed to set item to list: 'at' index is negative");
    }

    size_t at = (size_t)idx;
    DynArr *items = list_obj->items;
    size_t len = DYNARR_LEN(items);

    if(at >= len){
        vmu_error(vm, "Failed to set item to list: 'at' index (%zu) out of bounds", at);
    }

    Value out_value = DYNARR_GET_AS(Value, at, items);

    dynarr_set_at(idx, &value, items);

    return out_value;
}

inline Value vmu_list_remove_at(int64_t idx, ListObj *list_obj, VM *vm){
    if(idx < 0){
        vmu_error(vm, "Failed to remove item from list: 'at' index is negative");
    }

    size_t at = (size_t)idx;
    DynArr *items = list_obj->items;
    size_t len = DYNARR_LEN(items);

    if(at >= len){
        vmu_error(vm, "Failed to remove item from list: 'at' index out of bounds");
    }

    Value value = DYNARR_GET_AS(Value, at, items);

    dynarr_remove_index(at, items);
    dynarr_reduce(items);

    return value;
}

inline DictObj *vmu_create_dict(VM *vm){
    LZOHTable *key_values = FACTORY_LZOHTABLE(VMU_FRONT_ALLOCATOR);
    DictObj *dict_obj = ALLOC_DICT_OBJ();
    Obj *obj = (Obj *)dict_obj;

    init_obj(DICT_OBJ_TYPE, obj, vm);
    dict_obj->key_values = key_values;

    return dict_obj;
}

inline void vmu_destroy_dict(DictObj *dict_obj, VM *vm){
    if(!dict_obj){
        return;
    }

    LZOHTABLE_DESTROY(dict_obj->key_values);
    DEALLOC_DICT_OBJ(dict_obj);
}

inline void vmu_dict_put(Value key, Value value, DictObj *dict_obj, VM *vm){
    if(IS_VALUE_EMPTY(&value)){
        vmu_error(vm, "Failed to put value into dict: key cannot be 'empty'");
    }

    LZOHTable *keys_values = dict_obj->key_values;

    lzohtable_put_ckv(VALUE_SIZE, &key, VALUE_SIZE, &value, keys_values, NULL);
}

inline void vmu_dict_raw_put_str_value(char *str, Value value, DictObj *dict_obj, VM *vm){
    size_t str_len = strlen(str);
    LZOHTable *keys_values = dict_obj->key_values;
    lzohtable_put_ckv(str_len, str, VALUE_SIZE, &value, keys_values, NULL);
}

inline Value vmu_dict_get(Value key, DictObj *dict_obj, VM *vm){
    LZOHTable *keys_values = dict_obj->key_values;
    Value *raw_value = NULL;

    if(lzohtable_lookup(VALUE_SIZE, &key, keys_values, (void **)(&raw_value))){
        return *raw_value;
    }

    return (Value){0};
}

inline RecordObj *vmu_create_record(uint16_t length, VM *vm){
    LZOHTable *attrs = length == 0 ? NULL : FACTORY_LZOHTABLE(VMU_FRONT_ALLOCATOR);
    RecordObj *record_obj = ALLOC_RECORD_OBJ();
    Obj *obj = (Obj*)record_obj;

    init_obj(RECORD_OBJ_TYPE, obj, vm);
    record_obj->attrs = attrs;

	return record_obj;
}

inline void vmu_destroy_record(RecordObj *record_obj, VM *vm){
    if(!record_obj){
        return;
    }

    LZOHTABLE_DESTROY(record_obj->attrs);
    DEALLOC_RECORD_OBJ(record_obj);
}

inline void vmu_record_insert_attr(size_t key_size, char *key, Value value, RecordObj *record_obj, VM *vm){
    Value *attr_value = NULL;
    LZOHTable *attrs = record_obj->attrs;

    if(!attrs){
        vmu_internal_error(vm, "Cannot set attributes on an empty record");
    }

    if(lzohtable_lookup(key_size, key, attrs, (void **)(&attr_value))){
        *attr_value = value;
        return;
    }

    lzohtable_put_ckv(key_size, key, VALUE_SIZE, &value, attrs, NULL);
}

inline void vmu_record_set_attr(size_t key_size, char *key, Value value, RecordObj *record_obj, VM *vm){
    Value *attr_value = NULL;
    LZOHTable *attrs = record_obj->attrs;

    if(attrs && lzohtable_lookup(key_size, key, attrs, (void **)(&attr_value))){
        *attr_value = value;
        return;
    }

    vmu_error(vm, "Failed to update record: attribute '%s' does not exist", key);
}

inline Value vmu_record_get_attr(size_t key_size, char *key, RecordObj *record_obj, VM *vm){
    LZOHTable *attrs = record_obj->attrs;
    Value *out_value = NULL;

    if(attrs && lzohtable_lookup(key_size, key, attrs, (void **)(&out_value))){
        return *out_value;
    }

    vmu_error(vm, "Failed to get attribute: record does not contain attribute '%s'", key);

    return (Value){0};
}

inline NativeFnObj *vmu_create_native_fn(Value target, NativeFn *native_fn, VM *vm){
    NativeFnObj *native_fn_obj = ALLOC_NATIVE_FN_OBJ();
    Obj *obj = (Obj *)native_fn_obj;

    init_obj(NATIVE_FN_OBJ_TYPE, obj, vm);
    native_fn_obj->target = target;
    native_fn_obj->native_fn = native_fn;

    return native_fn_obj;
}

inline void vmu_destroy_native_fn(NativeFnObj *native_fn_obj, VM *vm){
    DEALLOC_NATIVE_FN_OBJ(native_fn_obj);
}

inline FnObj *vmu_create_fn(Fn *fn, VM *vm){
    FnObj *fn_obj = ALLOC_FN_OBJ();
    Obj *obj = (Obj *)fn_obj;

    init_obj(FN_OBJ_TYPE, obj, vm);
    fn_obj->fn = fn;

    return fn_obj;
}

inline void vmu_destroy_fn(FnObj *fn_obj, VM *vm){
    if(!fn_obj){
        return;
    }

    DEALLOC_FN_OBJ(fn_obj);
}

ClosureObj *vmu_create_closure(MetaClosure *meta, VM *vm){
    size_t meta_out_values_len = meta->meta_out_values_len;
    OutValue *out_values = MEMORY_ALLOC(OutValue, meta_out_values_len, VMU_FRONT_ALLOCATOR);
    Closure *closure = ALLOC_CLOSURE();
    ClosureObj *closure_obj = ALLOC_CLOSURE_OBJ();
    Obj *obj = (Obj *)closure_obj;

    for (size_t i = 0; i < meta_out_values_len; i++){
        OutValue *out_value = &out_values[i];

        out_value->linked = 0;
        out_value->at = -1;
        out_value->value = (Value){0};
        out_value->prev = NULL;
        out_value->next = NULL;
        out_value->closure_obj = closure_obj;
    }

    closure->meta = meta;
    closure->out_values = out_values;
    init_obj(CLOSURE_OBJ_TYPE, obj, vm);
    closure_obj->closure = closure;

    return closure_obj;
}

inline void vmu_destroy_closure(ClosureObj *closure_obj, VM *vm){
    if(!closure_obj){
        return;
    }

    Closure *closure = closure_obj->closure;
    OutValue *out_values = closure->out_values;
    MetaClosure *meta_closure = closure->meta;

    MEMORY_DEALLOC(OutValue, meta_closure->meta_out_values_len, out_values, VMU_FRONT_ALLOCATOR);
    DEALLOC_CLOSURE(closure);
    DEALLOC_CLOSURE_OBJ(closure_obj);
}

inline NativeModuleObj *vmu_create_native_module(NativeModule *native_module, VM *vm){
    NativeModuleObj *native_module_obj = ALLOC_NATIVE_MODULE_OBJ();
    Obj *obj = (Obj *)native_module_obj;

    init_obj(NATIVE_MODULE_OBJ_TYPE, obj, vm);
    native_module_obj->native_module = native_module;

    return native_module_obj;
}

inline void vmu_destroy_native_module_obj(NativeModuleObj *native_module_obj, VM *vm){
    if(!native_module_obj){
        return;
    }

    DEALLOC_NATIVE_MODULE_OBJ(native_module_obj);
}

inline ModuleObj *vmu_create_module_obj(Module *module, VM *vm){
    ModuleObj *module_obj = ALLOC_MODULE_OBJ();
    Obj *obj = (Obj *)module_obj;

    init_obj(MODULE_OBJ_TYPE, obj, vm);
    module_obj->module = module;

    return module_obj;
}

inline void vmu_destroy_module_obj(ModuleObj *module_obj, VM *vm){
    if(!module_obj){
        return;
    }

    DEALLOC_MODULE_OBJ(module_obj);
}