#include "vm.h"
#include "memory.h"
#include "opcode.h"
#include "function.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <setjmp.h>

//> ERROR RELATED
static jmp_buf err_jmp;
static void error(VM *vm, char *msg, ...);
//< ERROR RELATED

//> VALUE RELATED
static int is_i64(Value *value, int64_t *i64);
static int is_str(Value *value, Str **str);
static int is_list(Value *value, DynArr **list);
static int is_dict(Value *value, LZHTable **dict);
static int is_function(Value *value, Function **out_fn);
static int is_native_function(Value *value, NativeFunction **out_native_fn);

static Value *clone_value(Value *value, VM *vm);
static Obj *create_obj(ObjType type, VM *vm);
static void destroy_dict_values(void *ptr);
static void destroy_obj(Obj *obj, VM *vm);
static NativeFunction *create_native_function(
    int arity,
    char *name,
    void *target,
    void(native)(void *target, VM *vm), VM *vm
);
static void clean_up(VM *vm);
static char *clone_range_buff(size_t from, size_t to, char *buff, size_t *out_len, VM *vm);
static Str *create_str(char *buff, char core, VM *vm);
static Str *create_range_str(size_t from, size_t to, char *buff, VM *vm);
static Obj *create_range_str_obj(size_t from, size_t to, char *buff, Value *out_value, VM *vm);
static DynArr *create_dyarr(VM *vm);
static LZHTable *create_table(VM *vm);
#define INSERT_VALUE(value, list, vm) \
    if(dynarr_insert(value, list)) error(vm, "Out of memory");
static uint32_t hash_obj(Obj *obj);
static uint32_t hash_value(Value *value);
#define PUT_VALUE(key, value, table, vm) \
    {uint32_t hash = hash_value(key); if(lzhtable_hash_put(hash, value, table)) error(vm, "Out of memory");}
//< VALUE RELATED
//> STACK RELATED
static void push(Value value, VM *vm);
static Value *pop(VM *vm);
static Value *peek(VM *vm);
static void push_bool(uint8_t bool, VM *vm);
static void push_empty(VM *vm);
static void push_i64(int64_t i64, VM *vm);
static void push_str(Str *str, VM *vm);
static void push_list(DynArr *list, VM *vm);
static void push_native_fn(NativeFunction *native, VM *vm);
//< STACK RELATED

#define VALIDATE_INDEX(value, index, len)\
    if(!is_i64(value, &index))\
        error(vm, "Unexpected index value. Expect int, but got something else");\
    if(index < 0 || index >= (int64_t)len)\
        error(vm, "Index out of bounds. Must be 0 >= index(%ld) < len(%ld)", index, len);

#define VALIDATE_INDEX_NAME(value, index, len, name)\
    if(!is_i64(value, &index))\
        error(vm, "Unexpected value for '%s'. Expect int, but got something else", name);\
    if(index < 0 || index >= (int64_t)len)\
        error(vm, "'%s' out of bounds. Must be 0 >= index(%ld) < len(%ld)", name, index, len);

//> Native functions related to strings
void native_fn_char_at(void *target, VM *vm){
    int64_t index = -1;
    Str *in_str = (Str *)target;

    VALIDATE_INDEX(pop(vm), index, in_str->len)

    Str *out_str = create_range_str(index, index, in_str->buff, vm);

    pop(vm);
    push_str(out_str, vm);
}

void native_fn_sub_str(void *target, VM *vm){
    int64_t to = -1;
    int64_t from = -1;
    Str *in_str = (Str *)target;

    VALIDATE_INDEX_NAME(pop(vm), to, in_str->len, "to")
    VALIDATE_INDEX_NAME(pop(vm), from, in_str->len, "from")

    if(from > to) 
        error(vm, "Illegal index 'from'(%ld). Must be less or equals to 'to'(%ld)", from, to);

    Str *out_str = create_range_str(from, to, in_str->buff, vm);

    pop(vm);
    push_str(out_str, vm);
}

void native_fn_char_code(void *target, VM *vm){
    int64_t index = -1;
    Str *str = (Str *)target;

    VALIDATE_INDEX(pop(vm), index, str->len)
    int64_t code = (int64_t)str->buff[index];

    pop(vm);
    push_i64(code, vm);
}

void native_fn_split(void *target, VM *vm){
    Str *by = NULL;
    Str *str = (Str *)target;

    if(!is_str(pop(vm), &by))
        error(vm, "Expect string as 'by' to split");

    if(by->len != 1)
        error(vm, "Expect string of length 1 as 'by' to split");

    DynArr *arr = create_dyarr(vm);

    size_t from = 0;
    size_t to = 0;
    size_t i = 0;

    while (i < str->len){
        char c = str->buff[i++];
        
        if(c == by->buff[0]){
            to = i - 1;

            Value str_value = {0};
            create_range_str_obj(from, to, str->buff, &str_value, vm);

            INSERT_VALUE(&str_value, arr, vm)

            from = i;
        }
    }

    pop(vm);
    push_list(arr, vm);
}
//< Native functions related to strings

void native_fn_list_get(void *target, VM *vm){
    int64_t index = -1;
    DynArr *list = (DynArr *)target;

    VALIDATE_INDEX(pop(vm),index, list->used)

    Value *value = (Value *)dynarr_get((size_t)index, list);
    
    pop(vm); // pop native function
    push(*value, vm);
}

void native_fn_list_insert(void *target, VM *vm){
    Value *value = pop(vm);
    DynArr *list = (DynArr *)target;

    if(dynarr_insert(value, list))
        error(vm, "Failed to insert value in list: out of memory");

    pop(vm); // pop native function
    push_empty(vm);
}

void native_fn_list_insert_at(void *target, VM *vm){
    int64_t index = -1;
    Value *value = pop(vm);
    DynArr *list = (DynArr *)target;

    VALIDATE_INDEX(pop(vm), index, list->used)

    Value out_value = {0};
    memcpy(&out_value, dynarr_get((size_t)index, list), sizeof(Value));

    if(dynarr_insert_at((size_t) index, value, list))
        error(vm, "Failed to insert value in list: out of memory");

    pop(vm); // pop native function
    push(out_value, vm);
}

void native_fn_list_set(void *target, VM *vm){
    int64_t index = -1;
    Value *value = pop(vm);
    DynArr *list = (DynArr *)target;

    VALIDATE_INDEX(pop(vm), index, list->used)

    Value out_value = {0};
    memcpy(&out_value, dynarr_get((size_t)index, list), sizeof(Value));

    dynarr_set(value, (size_t)index, list);

    pop(vm); // pop native function
    push(out_value, vm);
}

void native_fn_list_remove(void *target, VM *vm){
    int64_t index = -1;
    DynArr *list = (DynArr *)target;

    VALIDATE_INDEX(pop(vm), index, list->used)

    Value out_value = {0};
    memcpy(&out_value, dynarr_get((size_t)index, list), sizeof(Value));

    dynarr_remove_index((size_t)index, list);
    
    pop(vm); // pop native function
    push(out_value, vm);
}

void native_fn_list_append(void *target, VM *vm){
    DynArr *from = NULL;
    DynArr *to = (DynArr *)target;

    if(!is_list(pop(vm), &from))
        error(vm, "Failed to append list: expect another list, but got something else");

    int64_t from_len = (int64_t)from->used;

    if(dynarr_append(from, to))
        error(vm, "Failed to append to list: out of memory");
    
    pop(vm); // pop native function
    push_i64(from_len, vm);
}

void native_fn_list_clear(void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    int64_t list_len = (int64_t)list->used;

    dynarr_remove_all(list);

    pop(vm); // pop native function
    push_i64(list_len, vm);
}

void native_dict_contains(void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *value = pop(vm);

    uint32_t hash = hash_value(value);
    uint8_t contains = lzhtable_hash_contains(hash, dict, NULL) != NULL;

    pop(vm);
    push_bool(contains, vm);
}

void native_dict_get(void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *key = pop(vm);

    uint32_t hash = hash_value(key);
    LZHTableNode *node = NULL;

    lzhtable_hash_contains(hash, dict, &node);

    pop(vm);

    if(node) push(*(Value *)node->value, vm);
    else push_empty(vm);
}

void native_dict_put(void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *value = pop(vm);
    Value *key = pop(vm);

    uint32_t hash = hash_value(key);
    lzhtable_hash_put(hash, clone_value(value, vm), dict);

    pop(vm);
    push_empty(vm);
}

char *join_buff(char *buffa, size_t sza, char *buffb, size_t szb, VM *vm){
    size_t szc = sza + szb;
    char *buff = malloc(szc + 1);

	if(!buff)
		error(vm, "Failed to create buffer: out of memory");

    memcpy(buff, buffa, sza);
    memcpy(buff + sza, buffb, szb);
    buff[szc] = '\0';

    return buff;
}

char *multiply_buff(char *buff, size_t szbuff, size_t by, VM *vm){
	size_t sz = szbuff * by;
	char *b = malloc(sz + 1);

	if(!b)
		error(vm, "failed to create buffer: out of memory.");

	for(size_t i = 0; i < by; i++)
		memcpy(b + (i * szbuff), buff, szbuff);

	b[sz] = '\0';

	return b;
}

Frame *current_frame(VM *vm){
    if(vm->frame_ptr == 0)
        error(vm, "Frame stack is empty");

    return &vm->frame_stack[vm->frame_ptr - 1];
}

#define CURRENT_LOCALS(vm)(current_frame(vm)->locals)

Frame *frame_up(size_t fn_index, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH)
        error(vm, "FrameOverFlow");

    DynArrPtr *functions = vm->functions;
    
    if(fn_index >= functions->used)
        error(vm, "Illegal function index.");

    Function *fn = DYNARR_PTR_GET(fn_index, functions);
    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->name = fn->name;
    frame->chunks = fn->chunks;

    return frame;
}

Frame *frame_up_fn(Function *fn, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH)
        error(vm, "FrameOverFlow");
        
    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->name = fn->name;
    frame->chunks = fn->chunks;

    return frame;
}

void frame_down(VM *vm){
    if(vm->frame_ptr == 0)
        error(vm, "FrameUnderFlow");

    vm->frame_ptr--;
}

#define TO_STR(v)(v->literal.obj->value.str)

int32_t compose_i32(uint8_t *bytes){
    return ((int32_t)bytes[3] << 24) | ((int32_t)bytes[2] << 16) | ((int32_t)bytes[1] << 8) | ((int32_t)bytes[0]);
}

void print_obj(Obj *object){
    switch (object->type){
        case STRING_OTYPE:{
            Str *str = object->value.str;
            printf("%s\n", str->buff);
            break;
        }
   		case LIST_OTYPE:{
			DynArr *list = object->value.list;
			printf("<list %ld at %p>\n", list->used, list);
			break;
		}
        case DICT_OTYPE:{
            LZHTable *table = object->value.dict;
            printf("<dict %ld %p>\n", table->n, table);
            break;
        }
        case FN_OTYPE:{
            Function *fn = (Function *)object->value.fn;
            printf("<fn '%s' - %d at %p>\n", fn->name, (uint8_t)(fn->params ? fn->params->used : 0), fn);
            break;
        }
        case NATIVE_FN_OTYPE:{
            NativeFunction *native_fn = object->value.native_fn;
            printf("<native fn '%s' - %d at %p>\n", native_fn->name, native_fn->arity, native_fn);
            break;
        }
        default:{
            assert("Illegal object type");
        }
    }
}

void print_value(Value *value){
    switch (value->type){
        case EMPTY_VTYPE:{
            printf("empty\n");
            break;
        }
        case BOOL_VTYPE:{
            uint8_t bool = value->literal.bool;
            printf("%s\n", bool == 0 ? "false" : "true");
            break;
        }
        case INT_VTYPE:{
            printf("%ld\n", value->literal.i64);   
            break;
        }
        case OBJ_VTYPE:{
            print_obj(value->literal.obj);
            break;
        }
        default:{
            assert("Illegal value type");
        }
    }
}

void print_stack(VM *vm){
    for (int i = 0; i < vm->stack_ptr; i++)
    {
        Value *value = &vm->stack[i];
        printf("%d --> ", i + 1);
        print_value(value);
    }
}

int is_at_end(VM *vm){
    Frame *frame = current_frame(vm);
    DynArr *chunks = frame->chunks;
    return frame->ip >= chunks->used;
}

uint8_t advance(VM *vm){
    Frame *frame = current_frame(vm);
    DynArr *chunks = frame->chunks;
    return *(uint8_t *)dynarr_get(frame->ip++, chunks);
}

int32_t read_i32(VM *vm){
	uint8_t bytes[4];

	for(size_t i = 0; i < 4; i++)
		bytes[i] = advance(vm);

	return compose_i32(bytes);
}

int64_t read_i64_const(VM *vm){
    DynArr *constants = vm->constants;
    int32_t index = read_i32(vm);
    return *(int64_t *)dynarr_get(index, constants);
}

char *read_str(VM *vm, uint32_t *out_hash){
    uint32_t hash = (uint32_t)read_i32(vm);
    if(out_hash) *out_hash = hash;
    return lzhtable_hash_get(hash, vm->strings);
}

void error(VM *vm, char *msg, ...){
    va_list args;
	va_start(args, msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

	va_end(args);
    clean_up(vm);

    longjmp(err_jmp, 1);
}

int is_i64(Value *value, int64_t *i64){
    if(value->type != INT_VTYPE) return 0;
    if(i64) *i64 = value->literal.i64;
    return 1;
}

int is_str(Value *value, Str **str){
    if(value->type != OBJ_VTYPE) return 0;
    
    Obj *obj = value->literal.obj;

    if(obj->type == STRING_OTYPE){
        Str *ostr = obj->value.str;
        if(str) *str = ostr;
    
        return 1;
    }

    return 0;
}

int is_list(Value *value, DynArr **list){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == LIST_OTYPE){
		DynArr *olist = obj->value.list;
		if(list) *list = olist;
		return 1;
	}

	return 0;
}

int is_dict(Value *value, LZHTable **dict){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == DICT_OTYPE){
		LZHTable *odict = obj->value.dict;
		if(dict) *dict = odict;
		return 1;
	}

	return 0;
}

int is_function(Value *value, Function **out_fn){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == FN_OTYPE){
		Function *ofn = obj->value.fn;
		if(out_fn) *out_fn = ofn;
		return 1;
	}

	return 0;
}

int is_native_function(Value *value, NativeFunction **out_native_fn){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == NATIVE_FN_OTYPE){
		NativeFunction *ofn = obj->value.native_fn;
		if(out_native_fn) *out_native_fn = ofn;
		return 1;
	}

	return 0;
}

Value *clone_value(Value *value, VM *vm){
    Value *clone = (Value *)malloc(sizeof(Value));
    if(!clone) error(vm, "Out of memory");
    memcpy(clone, value, sizeof(Value));
    return clone;
}

Obj *create_obj(ObjType type, VM *vm){
    Obj *obj = malloc(sizeof(Obj));
    
    if(!obj) error(vm, "Failed to allocate object: out of memory.");

    memset(obj, 0, sizeof(Obj));
    obj->type = type;
    
    if(vm->tail){
        obj->prev = vm->tail;
        vm->tail->next = obj;
    }else vm->head = obj;

    vm->tail = obj;

    return obj;
}

void destroy_dict_values(void *ptr){
    free(ptr);
}

void destroy_obj(Obj *obj, VM *vm){
    Obj *prev = obj->prev;
    Obj *next = obj->next;

    if(prev) prev->next = next;
	if(next) next->prev = prev;

	if(vm->head == obj) vm->head = next;
	if(vm->tail == obj) vm->tail = prev;

	switch(obj->type){
		case STRING_OTYPE:{
			Str *str = obj->value.str;
			if(!str->core) free(str->buff);
            free(str);
			break;
		}
		case LIST_OTYPE:{
			DynArr *list = obj->value.list;
			dynarr_destroy(list);
			break;
		}
        case DICT_OTYPE:{
            LZHTable *table = obj->value.dict;
            lzhtable_destroy(destroy_dict_values, table);
            break;
        }
        case NATIVE_FN_OTYPE:{
            NativeFunction *native_fn = obj->value.native_fn;
            free(native_fn);
            break;
        }
		default:{
			assert("Illegal object type");
		}
	}

	free(obj);
}

NativeFunction *create_native_function(
    int arity,
    char *name,
    void *target,
    void(native)(void *target, VM *vm), VM *vm
){
    size_t name_len = strlen(name);
    assert(name_len < NAME_LEN - 1);

    NativeFunction *native_fn = (NativeFunction *)malloc(sizeof(NativeFunction));
    
    if(!native_fn)
        error(vm, "Failed to create native function: out of memory");

    native_fn->arity = arity;
    memcpy(native_fn->name, name, name_len);
    native_fn->name[name_len] = '\0';
    native_fn->name_len = name_len;
    native_fn->target = target;
    native_fn->native = native;

    return native_fn;
}

void clean_up(VM *vm){
    while (vm->head)
        destroy_obj(vm->head, vm);
}

char *clone_range_buff(size_t from, size_t to, char *buff, size_t *out_len, VM *vm){
    size_t len = to - from + 1;
    char *rstr = (char *)malloc(len + 1);

    if(!rstr) error(vm, "Out of memory");

    memcpy(rstr, buff + from, len);
    rstr[len] = '\0';
    
    return rstr;
}

Str *create_str(char *buff, char core, VM *vm){
    size_t buff_len = strlen(buff);
    Str *str = (Str *)malloc(sizeof(Str));

    if(!str) error(vm, "Out of memory");

    str->core = core;
    str->len = buff_len;
    str->buff = buff;

    return str;
}

Str *create_range_str(size_t from, size_t to, char *buff, VM *vm){
    size_t buff_len = 0;
    char *clone_buff = clone_range_buff(from, to, buff, &buff_len, vm);
    Str *str = (Str *)malloc(sizeof(Str));

    if(!str) error(vm, "Out of memory");

    str->core = 0;
    str->len = buff_len;
    str->buff = clone_buff;

    return str;
}

Obj *create_range_str_obj(
    size_t from,
    size_t to,
    char *buff,
    Value *out_value,
    VM *vm
){
    Str *str = create_range_str(from, to, buff, vm);
    Obj *str_obj = create_obj(STRING_OTYPE, vm);

    str_obj->value.str = str;

    if(out_value){
        out_value->type = OBJ_VTYPE;
        out_value->literal.obj = str_obj;
    }

    return str_obj;
}

DynArr *create_dyarr(VM *vm){
    DynArr *dynarr = dynarr_create(sizeof(Value), NULL);
    if(!dynarr) error(vm, "Out of memory");
    return dynarr;
}

LZHTable *create_table(VM *vm){
    LZHTable *table = lzhtable_create(17, NULL);
    if(!table) error(vm, "Out of memory");
    return table;
}

uint32_t hash_obj(Obj *obj){
    switch (obj->type){
        case STRING_OTYPE :{
            Str *str = obj->value.str;
            uint32_t hash = lzhtable_hash((uint8_t *)str->buff, str->len);
            return hash;
        }
        default:{
            uint32_t hash = lzhtable_hash((uint8_t *)&obj->value, sizeof(obj->value));
            return hash;
        }
    }
}

uint32_t hash_value(Value *value){
    switch (value->type){
        case EMPTY_VTYPE:
        case BOOL_VTYPE:
        case INT_VTYPE:{
            uint32_t hash = lzhtable_hash((uint8_t *)&value->literal, sizeof(value->literal));
            return hash;
        }
        default:{
            Obj *obj = value->literal.obj;
            return hash_obj(obj);
        }
    }   
}

void push(Value value, VM *vm){
    if(vm->stack_ptr >= STACK_LENGTH) error(vm, "StackOverFlow");
    Value *current_value = &vm->stack[vm->stack_ptr++];
    memcpy(current_value, &value, sizeof(Value));
}

Value *pop(VM *vm){
    if(vm->stack_ptr == 0) error(vm, "StackUnderFlow");
    return &vm->stack[--vm->stack_ptr];
}

Value *peek(VM *vm){
    if(vm->stack_ptr == 0) error(vm, "StackUnderFlow");
    return &vm->stack[vm->stack_ptr - 1];
}

Value *peek_at(int offset, VM *vm){
    if(1 + offset > vm->stack_ptr) error(vm, "Illegal offset");
    size_t at = vm->stack_ptr - 1 - offset;
    return &vm->stack[at];
}

void push_bool(uint8_t bool, VM *vm){
    Value value = {0};
    value.type = BOOL_VTYPE;
    value.literal.bool = bool;
    
    push(value, vm);
}

void push_empty(VM *vm){
    Value value = {0};
    value.type = EMPTY_VTYPE;
    
    push(value, vm);   
}

void push_i64(int64_t i64, VM *vm){
    Value value = {0};
    value.type = INT_VTYPE;
    value.literal.i64 = i64;
    
    push(value, vm);
}

void push_str(Str *str, VM *vm){
    Obj *str_obj = create_obj(STRING_OTYPE, vm);
    str_obj->value.str = str;

    Value value = {0};
    value.type = OBJ_VTYPE;
    value.literal.obj = str_obj;

    push(value, vm);
}

void push_list(DynArr *list, VM *vm){
    Obj *obj = create_obj(LIST_OTYPE, vm);
    obj->value.list = list;

    Value value = {0};
    value.type = OBJ_VTYPE;
    value.literal.obj = obj;

    push(value, vm);
}

void push_native_fn(NativeFunction *native, VM *vm){
    Obj *obj = create_obj(NATIVE_FN_OTYPE, vm);
    obj->value.native_fn = native;

    Value value = {0};
    value.type = OBJ_VTYPE;
    value.literal.obj = obj;

    push(value, vm);
}

uint8_t pop_bool_assert(VM *vm, char *err_msg, ...){
    Value *value = pop(vm);
    
    if(value->type == BOOL_VTYPE) return value->literal.bool;

    va_list args;
	va_start(args, err_msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, err_msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

int64_t pop_i64_assert(VM *vm, char *err_msg, ...){
    Value *value = pop(vm);
    
    if(value->type == INT_VTYPE) return value->literal.i64;

    va_list args;
	va_start(args, err_msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, err_msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

void assert_value_type(ValueType type, Value *value, char *err_msg, ...){
    ValueType vtype = value->type;
    if(vtype == type) return;

    va_list args;
	va_start(args, err_msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, err_msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

void execute(uint8_t chunk, VM *vm){
    switch (chunk){
        case EMPTY_OPCODE:{
            Value value = {0};
            push(value, vm);
            break;
        }
        case FALSE_OPCODE:{
            push_bool(0, vm);
            break;
        }
        case TRUE_OPCODE:{
            push_bool(1, vm);
            break;
        }
        case INT_OPCODE:{
            int64_t i64 = read_i64_const(vm);
            push_i64(i64, vm);
            break;
        }
		case STRING_OPCODE:{
			uint32_t hash = 0;
            char *buff = read_str(vm, &hash);
            Str *str = create_str(buff, 1, vm);
            
            push_str(str, vm);
			
            break;
		}
        case ADD_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(is_str(va, NULL)){
                Str *astr = TO_STR(va);
                Str *bstr = NULL;

                if(!is_str(vb, &bstr))
                    error(vm, "Expect string at right side of string concatenation.");

                char *buff = join_buff(astr->buff, astr->len, bstr->buff, bstr->len, vm);
                Str *out_str = create_str(buff, 0, vm);

                push_str(out_str, vm);

                break;
            }

            if(!is_i64(va, NULL))
                error(vm, "Expect int at left side of string sum.");

            if(!is_i64(vb, NULL))
                error(vm, "Expect int at right side of string sum.");

            int64_t right = vb->literal.i64;
            int64_t left = va->literal.i64;
            
            push_i64(left + right, vm);
            
            break;
        }
        case SUB_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_i64(left - right, vm);
            break;
        }
        case MUL_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(is_str(va, NULL)){
                int64_t by = -1;
                Str *in_str = TO_STR(va);

                if(!is_i64(vb, &by))
                    error(vm, "Expect integer at right side of string multiplication.");

				if(by < 0)
					error(vm, "Negative values not allowed in string multiplication.");

                char *buff = multiply_buff(in_str->buff, in_str->len, (size_t)by, vm);
                Str *out_str = create_str(buff, 0, vm);
                
                push_str(out_str, vm);

                break;
            }

			if(is_str(vb, NULL)){
                int64_t by = -1;
                Str *in_str = TO_STR(vb);

                if(!is_i64(va, &by))
                    error(vm, "Expect integer at left side of string multiplication.");

				if(by < 0)
					error(vm, "Negative values not allowed in string multiplication.");

                char *buff = multiply_buff(in_str->buff, in_str->len, (size_t)by, vm);
                Str *out_str = create_str(buff, 0, vm);
                
                push_str(out_str, vm);

                break;
            }


            if(!is_i64(va, NULL))
                error(vm, "Expect int at left side of string sum.");

            if(!is_i64(vb, NULL))
                error(vm, "Expect int at right side of string sum.");

            int64_t right = vb->literal.i64;
            int64_t left = va->literal.i64;
            
            push_i64(left * right, vm);
            
            break;

        }
        case DIV_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_i64(left / right, vm);
            break;
        }
		case MOD_OPCODE:{
			int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_i64(left % right, vm);
            break;

		}
        case LT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left < right, vm);
            break;
        }
        case GT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left > right, vm);
            break;
        }
        case LE_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left <= right, vm);
            break;
        }
        case GE_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left >= right, vm);
            break;
        }
        case EQ_OPCODE:{
            Value *left_value = pop(vm);
            Value *right_value = pop(vm);

            if((left_value->type == BOOL_VTYPE && right_value->type == BOOL_VTYPE) ||
            (left_value->type == INT_VTYPE && right_value->type == INT_VTYPE)){
                push_bool(left_value->literal.i64 == right_value->literal.i64, vm);
                break;
            }

            error(vm, "Illegal type in equals comparison expression.");

            break;
        }
        case NE_OPCODE:{
            Value *left_value = pop(vm);
            Value *right_value = pop(vm);

            if((left_value->type == BOOL_VTYPE && right_value->type == BOOL_VTYPE) ||
            (left_value->type == INT_VTYPE && right_value->type == INT_VTYPE)){
                push_bool(left_value->literal.i64 != right_value->literal.i64, vm);
                break;
            }

            error(vm, "Illegal type in not equals comparison expression.");

            break;
        }
        case LSET_OPCODE:{
            Value *value = peek(vm);
            uint8_t index = advance(vm);
            memcpy(&current_frame(vm)->locals[index], value, sizeof(Value));
            break;
        }
        case LGET_OPCODE:{
            uint8_t index = advance(vm);
            Value value = current_frame(vm)->locals[index];
            push(value, vm);
            break;
        }
        case SGET_OPCODE:{
            int32_t index = read_i32(vm);
            DynArrPtr *functions = vm->functions;
            Function *function = (Function *)DYNARR_PTR_GET((size_t)index, functions);

            Obj *fn_obj = create_obj(FN_OTYPE, vm);
            fn_obj->value.fn = function;

            Value value = {0};
            value.type = OBJ_VTYPE;
            value.literal.obj = fn_obj;

            push(value, vm);

            break;
        }
        case OR_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            uint8_t left = pop_bool_assert(vm, "Expect bool at left side.");
            push_bool(left || right, vm);
            break;
        }
        case AND_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            uint8_t left = pop_bool_assert(vm, "Expect bool at left side.");
            push_bool(left && right, vm);
            break;
        }
        case NNOT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            push_i64(-right, vm);
            break;
        }
        case NOT_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            push_bool(!right, vm);
            break;
        }
        case PRT_OPCODE:{
            Value *value = pop(vm);
            print_value(value);
            break;
        }
        case POP_OPCODE:{
            pop(vm);
            break;
        }
        case JMP_OPCODE:{
            int32_t jmp_value = read_i32(vm);
            if(jmp_value == 0) break;
            
            if(jmp_value > 0) current_frame(vm)->ip += jmp_value - 1;
            else current_frame(vm)->ip += jmp_value - 5;

            break;
        }
		case JIF_OPCODE:{
			uint8_t condition = pop_bool_assert(vm, "Expect 'bool' as conditional value.");
			int32_t jmp_value = read_i32(vm);
			if(jmp_value == 0) break;

            if(!condition){
                if(jmp_value > 0) current_frame(vm)->ip += jmp_value - 1;
                else current_frame(vm)->ip += jmp_value - 5;
            }

			break;
		}
		case JIT_OPCODE:{
			uint8_t condition = pop_bool_assert(vm, "Expect 'bool' as conditional value.");
			int32_t jmp_value = read_i32(vm);
			if(jmp_value == 0) break;

			if(condition){
                if(jmp_value > 0) current_frame(vm)->ip += jmp_value - 1;
                else current_frame(vm)->ip += jmp_value - 5;
            }

			break;
		}
		case LIST_OPCODE:{
			int32_t len = read_i32(vm);
			DynArr *list = create_dyarr(vm);

			for(int32_t i = 0; i < len; i++){
				Value *value = pop(vm);

				if(dynarr_insert(value, list))
					error(vm, "Failed to insert value at list: out of memory\n");		
			}

			Obj *obj = create_obj(LIST_OTYPE, vm);
			obj->value.list = list;

			Value value = {0};
			value.type = OBJ_VTYPE;
			value.literal.obj = obj;

			push(value, vm);

			break;
		}
        case DICT_OPCODE:{
            int32_t len = read_i32(vm);
            LZHTable *dict = create_table(vm);

            for (int32_t i = 0; i < len; i++){
                Value *value = pop(vm);
                Value *key = pop(vm);
                PUT_VALUE(key, clone_value(value, vm), dict, vm);
            }
            
            Obj *obj = create_obj(DICT_OTYPE, vm);
            obj->value.dict = dict;

            Value value = {0};
            value.type = OBJ_VTYPE;
            value.literal.obj = obj;

            push(value, vm);

            break;
        }
        case CALL_OPCODE:{
            Function *fn = NULL;
            NativeFunction *native_fn = NULL;
            uint8_t args_count = advance(vm);

            if(is_function(peek_at(args_count, vm), &fn)){
                uint8_t params_count = fn->params ? fn->params->used : 0;

                if(params_count != args_count)
                    error(vm, "Failed to call function '%s'.\n\tDeclared with %d parameter(s), but got %d argument(s).", fn->name, params_count, args_count);

                frame_up_fn(fn, vm);

                if(args_count > 0){
                    int from = (int)(args_count == 0 ? 0 : args_count - 1);

                    for (int i = from; i >= 0; i--)
                        memcpy(current_frame(vm)->locals + i, pop(vm), sizeof(Value));
                }

                pop(vm);
            }else if(is_native_function(peek_at(args_count, vm), &native_fn)){
                void *target = native_fn->target;
                void (*native)(void *target, VM *vm) = native_fn->native;

                if(native_fn->arity != args_count)
                    error(
                        vm,
                        "Failed to call function '%s'.\n\tDeclared with %d parameter(s), but got %d argument(s).",
                        native_fn->name,
                        native_fn->arity,
                        args_count
                    );

                native(target, vm);
            }else
                error(vm, "Expect function after %d parameters count.", args_count);

            break;
        }
        case ACCESS_OPCODE:{
            Str *str = NULL;
            DynArr *list = NULL;
            LZHTable *dict = NULL;
            char *symbol = read_str(vm, NULL);
            Value *value = pop(vm);

            if(is_str(value, &str)){
                if(strcmp(symbol, "len") == 0){
                    push_i64((int64_t)str->len, vm);
                }else if(strcmp(symbol, "is_core") == 0){
                    push_bool((uint8_t)str->core, vm);
                }else if(strcmp(symbol, "char_at") == 0){
                    NativeFunction *native_fn = create_native_function(1, "char_at", str, native_fn_char_at, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "sub_str") == 0){
                    NativeFunction *native_fn = create_native_function(2, "sub_str", str, native_fn_sub_str, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "char_code") == 0){
                    NativeFunction *native_fn = create_native_function(1, "char_code", str, native_fn_char_code, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "split") == 0){
                    NativeFunction *native_fn = create_native_function(1, "split", str, native_fn_split, vm);
                    push_native_fn(native_fn, vm);
                }else{
                    error(vm, "string do not have symbol named as '%s'", symbol);
                }

                break;
            }

            if(is_list(value, &list)){
                if(strcmp(symbol, "size") == 0){
                    push_i64((int64_t)list->used, vm);
                }else if(strcmp(symbol, "capacity") == 0){
                    push_i64((int64_t)(list->count - DYNARR_LEN(list)), vm);
                }else if(strcmp(symbol, "first") == 0){
                    if(DYNARR_LEN(list) == 0) push_empty(vm);
                    else push(*(Value *)dynarr_get(0, list), vm);
                }else if(strcmp(symbol, "last") == 0){
                    if(DYNARR_LEN(list) == 0) push_empty(vm);
                    else push(*(Value *)dynarr_get(DYNARR_LEN(list) - 1, list), vm);
                }else if(strcmp(symbol, "get") == 0){
                    NativeFunction *native_fn = create_native_function(1, "get", list, native_fn_list_get, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "insert") == 0){
                    NativeFunction *native_fn = create_native_function(1, "insert", list, native_fn_list_insert, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "insert_at") == 0){
                    NativeFunction *native_fn = create_native_function(2, "insert_at", list, native_fn_list_insert_at, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "set") == 0){
                    NativeFunction *native_fn = create_native_function(2, "set", list, native_fn_list_set, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "remove") == 0){
                    NativeFunction *native_fn = create_native_function(1, "remove", list, native_fn_list_remove, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "append") == 0){
                    NativeFunction *native_fn = create_native_function(1, "append", list, native_fn_list_append, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "clear") == 0){
                    NativeFunction *native_fn = create_native_function(0, "clear", list, native_fn_list_clear, vm);
                    push_native_fn(native_fn, vm);
                }else{
                    error(vm, "list do not have symbol named as '%s'", symbol);
                }

                break;
            }

            if(is_dict(value, &dict)){
                if(strcmp(symbol, "contains") == 0){
                    NativeFunction *native_fn = create_native_function(1, "contains", dict, native_dict_contains, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "get") == 0){
                    NativeFunction *native_fn = create_native_function(1, "get", dict, native_dict_get, vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "put") == 0){
                    NativeFunction *native_fn = create_native_function(2, "put", dict, native_dict_put, vm);
                    push_native_fn(native_fn, vm);
                }else{
                    error(vm, "dictionary do not have symbol named as '%s'", symbol);
                }

                break;
            }

            error(vm, "Illegal target to access.");

            break;
        }
        case RET_OPCODE:{
            frame_down(vm);
            break;
        }
        default:{
            assert("Illegal opcode");
        }
    }
}

VM *vm_create(){
    VM *vm = (VM *)memory_alloc(sizeof(VM));
    memset(vm, 0, sizeof(VM));
    return vm;
}

void vm_print_stack(VM *vm){
    print_stack(vm);
}

int vm_execute(
    DynArr *constants,
    LZHTable *strings,
    DynArrPtr *functions,
    VM *vm
){
    if(setjmp(err_jmp) == 1) return 1;
    else{
        vm->stack_ptr = 0;
        vm->constants = constants;
		vm->strings = strings;
        vm->functions = functions;

        frame_up(0, vm);

        while (!is_at_end(vm)){
            uint8_t chunk = advance(vm);
            execute(chunk, vm);
        }

        frame_down(vm);

        clean_up(vm);

        return 0;
    }
}
