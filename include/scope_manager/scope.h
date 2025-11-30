#ifndef SCOPE_H
#define SCOPE_H

#include "essentials/lzohtable.h"
#include "essentials/lzarena.h"
#include "essentials/lzpool.h"
#include "essentials/memory.h"

#include "token.h"
#include "symbol.h"

#include <setjmp.h>
#include <inttypes.h>

typedef enum scope_type{
    BLOCK_SCOPE_TYPE,
    IF_SCOPE_TYPE,
    ELIF_SCOPE_TYPE,
    ELSE_SCOPE_TYPE,
    WHILE_SCOPE_TYPE,
    FOR_SCOPE_TYPE,
    TRY_SCOPE_TYPE,
    CATCH_SCOPE_TYPE,
	FN_SCOPE_TYPE,
	GLOBAL_SCOPE_TYPE,
}ScopeType;

typedef uint8_t       depth_t;
#define DEPTH_T_MAX   UINT8_MAX
#define DEPTH_T_PRINT PRIu8

typedef uint8_t       local_t;
#define LOCAL_T_MAX   UINT8_MAX
#define LOCAL_T_PRINT PRIu16

typedef struct scope        Scope;
typedef struct local_scope  LocalScope;
typedef struct fn_scope     FnScope;

struct local_scope{
    depth_t depth;
    local_t locals;
    uint8_t returned;
    Scope   *fn_scope;
};

struct fn_scope{
    depth_t depth;
    local_t locals;
    uint8_t returned;
    Scope   *prev_fn;
};

struct scope{
    ScopeType type;
	LZOHTable *symbols;
    void      *arena_state;
    Scope     *prev;

    union{
        LocalScope  local_scope;
        FnScope     fn_scope;
    }content;
};

#define IS_LOCAL_SCOPE(_scope)           ((_scope)->type != GLOBAL_SCOPE_TYPE)
#define IS_BLOCK_SCOPE(_scope)           ((_scope)->type == BLOCK_SCOPE_TYPE)
#define IS_FN_SCOPE(_scope)              ((_scope)->type == FN_SCOPE_TYPE)
#define IS_GLOBAL_SCOPE(_scope)          ((_scope)->type == GLOBAL_SCOPE_TYPE)

#define AS_LOCAL_SCOPE(_scope)           (&((_scope)->content.local_scope))
#define AS_FN_SCOPE(_scope)              (&((_scope)->content.fn_scope))

#define PREV_SCOPE(_scope)               ((_scope)->prev)
#define LOCAL_SCOPE_LOCALS_COUNT(_scope) ((_scope)->locals)

#endif
