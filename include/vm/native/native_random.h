#ifndef RANDOM_NATIVE_H
#define RANDOM_NATIVE_H

#include "essentials/memory.h"

#include "native.h"

#include "vm/obj.h"
#include "vm/types_utils.h"
#include "vm/xoshiro256.h"
#include "vm/vmu.h"

typedef struct random_native{
	NativeHeader header;
	XOShiro256 xos256;
}RandomNative;

RandomNative *random_native_create(Allocator *allocator);
CREATE_VALIDATE_NATIVE_DECLARATION(random_native, RandomNative)

#endif
