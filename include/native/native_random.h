#ifndef RANDOM_NATIVE_H
#define RANDOM_NATIVE_H

#include "memory.h"
#include "native.h"
#include "types.h"
#include "obj.h"
#include "types_utils.h"
#include "value.h"
#include "xoshiro256.h"
#include "vmu.h"

typedef struct random_native{
	NativeHeader header;
	XOShiro256 xos256;
}RandomNative;

RandomNative *random_native_create(Allocator *allocator);
CREATE_VALIDATE_NATIVE_DECLARATION(random_native, RandomNative)

#endif
