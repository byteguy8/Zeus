#include "native/native.h"
#include "memory.h"
#include "types.h"
#include <string.h>

void native_init_header(
	NativeHeader *header,
	NativeType type,
	char *name,
	NativeDestroyHelper destroy_helper,
	Allocator *allocator
){
	size_t name_len = strlen(name);
	char *cloned_name = MEMORY_ALLOC(char, name_len + 1, allocator);

	memcpy(cloned_name, name, name_len);
	cloned_name[name_len] = 0;

	header->type = type;
	header->name = name;
	header->destroy_helper = destroy_helper;
}
