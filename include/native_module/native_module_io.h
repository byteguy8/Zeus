#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include "vmu.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>

static NativeModule *io_native_module;

void io_module_init(Allocator *allocator){}

#endif
