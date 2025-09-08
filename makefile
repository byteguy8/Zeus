PLATFORM := LINUX
BUILD := DEBUG

COMPILER.LINUX := gcc
COMPILER.WINDOWS := mingw64
COMPILER := $(COMPILER.$(PLATFORM))

SRC_DIR := ./src
OUT_DIR := ./build

FLAGS.COMMON := -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -I./include
FLAGS.DEBUG := -g2 -O0
FLAGS.DEBUG.LINUX := -fsanitize=address,undefined,alignment
FLAGS.RELEASE := -O3
FLAGS.LINUX := --std=gnu99
FLAGS.WINDOWS := --std=c99
FLAGS := $(FLAGS.COMMON) $(FLAGS.$(BUILD)) $(FLAGS.$(BUILD).$(PLATFORM)) $(FLAGS.$(PLATFORM))

OBJS := splitmix64.o xoshiro256.o \
lzarena.o lzpool.o lzflist.o \
bstr.o lzbstr.o dynarr.o \
lzhtable.o lzohtable.o memory.o \
factory.o utils.o lexer.o \
parser.o compiler.o \
dumpper.o vmu.o \
vm.o obj.o

LINKS.COMMON := -lm
LINKS.WINDOWS := -lshlwapi
LINKS := $(LINKS.COMMON) $(LINKS.$(PLATFORM))

zeus: $(OBJS)
	$(COMPILER) -o $(OUT_DIR)/zeus $(FLAGS) $(OUT_DIR)/*.o $(SRC_DIR)/zeus.c $(LINKS)
vm.o:
	$(COMPILER) -c -o $(OUT_DIR)/vm.o $(FLAGS) $(SRC_DIR)/vm.c
vmu.o:
	$(COMPILER) -c -o $(OUT_DIR)/vmu.o $(FLAGS) $(SRC_DIR)/vmu.c
obj.o:
	$(COMPILER) -c -o $(OUT_DIR)/obj.o $(FLAGS) $(SRC_DIR)/obj.c
dumpper.o:
	$(COMPILER) -c -o $(OUT_DIR)/dumpper.o $(FLAGS) $(SRC_DIR)/dumpper.c
compiler.o:
	$(COMPILER) -c -o $(OUT_DIR)/compiler.o $(FLAGS) $(SRC_DIR)/compiler.c
parser.o:
	$(COMPILER) -c -o $(OUT_DIR)/parser.o $(FLAGS) $(SRC_DIR)/parser.c
lexer.o:
	$(COMPILER) -c -o $(OUT_DIR)/lexer.o $(FLAGS) $(SRC_DIR)/lexer.c
utils.o:
	$(COMPILER) -c -o $(OUT_DIR)/utils.o $(FLAGS) $(SRC_DIR)/utils.c
factory.o:
	$(COMPILER) -c -o $(OUT_DIR)/factory.o $(FLAGS) $(SRC_DIR)/factory.c
memory.o:
	$(COMPILER) -c -o $(OUT_DIR)/memory.o $(FLAGS) $(SRC_DIR)/memory.c
lzohtable.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzohtable.o $(FLAGS) $(SRC_DIR)/lzohtable.c
lzhtable.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzhtable.o $(FLAGS) $(SRC_DIR)/lzhtable.c
dynarr.o:
	$(COMPILER) -c -o $(OUT_DIR)/dynarr.o $(FLAGS) $(SRC_DIR)/dynarr.c
lzbstr.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzbstr.o $(FLAGS) $(SRC_DIR)/lzbstr.c
bstr.o:
	$(COMPILER) -c -o $(OUT_DIR)/bstr.o $(FLAGS) $(SRC_DIR)/bstr.c
lzarena.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzarena.o $(FLAGS) $(SRC_DIR)/lzarena.c
lzpool.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzpool.o $(FLAGS) $(SRC_DIR)/lzpool.c
lzflist.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzflist.o $(FLAGS) $(SRC_DIR)/lzflist.c
xoshiro256.o:
	$(COMPILER) -c -o $(OUT_DIR)/xoshiro256.o $(FLAGS) $(SRC_DIR)/xoshiro256.c
splitmix64.o:
	$(COMPILER) -c -o $(OUT_DIR)/splitmix64.o $(FLAGS) $(SRC_DIR)/splitmix64.c