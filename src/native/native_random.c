#include "native_random.h"
#include "native.h"
#include "vmu.h"

static void random_native_destroy(void *native, Allocator *allocator){
	MEMORY_DEALLOC(RandomNative, 1, native, allocator);
}

RandomNative *random_native_create(Allocator *allocator){
	RandomNative *native = MEMORY_ALLOC(RandomNative, 1, allocator);

	native_init_header(
		(NativeHeader *)native,
		RANDOM_NATIVE_TYPE,
		"random",
		random_native_destroy, allocator
	);

	return native;
}

CREATE_VALIDATE_NATIVE("random", random_native, RANDOM_NATIVE_TYPE, RandomNative)
