SRC := ./src
OUT_DIR := ./build
BUILD := debug

cflags.common := -Wall -Wextra --std=gnu99 -I./include
cflags.debug := -g2 -O0 -Wno-unused-parameter -fsanitize=address,undefined,alignment
cflags.release := -O2

CFLAGS := ${cflags.common} ${cflags.${BUILD}}

COMPILER = gcc
OBJS = lzarena.o lzflist.o \
bstr.o \
dynarr.o lzhtable.o \
memory.o utils.o \
lexer.o parser.o \
compiler.o dumpper.o \
vm_utils.o vm.o

zeus: $(OBJS)
	$(COMPILER) -o $(OUT_DIR)/zeus $(CFLAGS) $(OUT_DIR)/*.o $(SRC)/zeus.c -lm
vm.o:
	$(COMPILER) -c -o $(OUT_DIR)/vm.o $(CFLAGS) $(SRC)/vm.c
vm_utils.o:
	$(COMPILER) -c -o $(OUT_DIR)/vm_utils.o $(CFLAGS) $(SRC)/vm_utils.c
dumpper.o:
	$(COMPILER) -c -o $(OUT_DIR)/dumpper.o $(CFLAGS) $(SRC)/dumpper.c
compiler.o:
	$(COMPILER) -c -o $(OUT_DIR)/compiler.o $(CFLAGS) $(SRC)/compiler.c
parser.o:
	$(COMPILER) -c -o $(OUT_DIR)/parser.o $(CFLAGS) $(SRC)/parser.c
lexer.o:
	$(COMPILER) -c -o $(OUT_DIR)/lexer.o $(CFLAGS) $(SRC)/lexer.c
utils.o:
	$(COMPILER) -c -o $(OUT_DIR)/utils.o $(CFLAGS) $(SRC)/utils.c
memory.o:
	$(COMPILER) -c -o $(OUT_DIR)/memory.o $(CFLAGS) $(SRC)/memory.c
lzhtable.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzhtable.o $(CFLAGS) $(SRC)/lzhtable.c
dynarr.o:
	$(COMPILER) -c -o $(OUT_DIR)/dynarr.o $(CFLAGS) $(SRC)/dynarr.c
bstr.o:
	$(COMPILER) -c -o $(OUT_DIR)/bstr.o $(CFLAGS) $(SRC)/bstr.c
lzarena.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzarena.o  $(CFLAGS) $(SRC)/lzarena.c
lzflist.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzflist.o  $(CFLAGS) $(SRC)/lzflist.c