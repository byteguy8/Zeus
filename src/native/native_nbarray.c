#include "native_nbarray.h"
#include <string.h>

static void narray_native_destroy(void *native, Allocator *allocator){
	NBArrayNative *buff_native = native;

	MEMORY_DEALLOC(char, buff_native->len, buff_native->bytes, allocator);
	MEMORY_DEALLOC(NBArrayNative, 1, buff_native, allocator);
}

NBArrayNative *nbarray_native_create(size_t len, Allocator *allocator){
	unsigned char *bytes = MEMORY_ALLOC(unsigned char, len, allocator);
	NBArrayNative *nbarray_native = MEMORY_ALLOC(NBArrayNative, 1, allocator);

	memset(bytes, 0, len);

	native_init_header(
		(NativeHeader *)nbarray_native,
		NBARRAY_NATIVE_TYPE,
		"nbuff",
		narray_native_destroy,
		allocator
	);
	nbarray_native->len = len;
	nbarray_native->bytes = bytes;

	return nbarray_native;
}

CREATE_VALIDATE_NATIVE("nbarray", nbarray_native, NBARRAY_NATIVE_TYPE, NBArrayNative)
