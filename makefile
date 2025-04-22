COMPILER := gcc

SRC_DIR := ./src
OUT_DIR := ./build

BUILD := DEBUG

FLAGS.COMMON := -Wall -Wextra --std=gnu99 -I./include -Wno-unused-parameter -Wno-unused-function
FLAGS.DEBUG := -g2 -O0 -fsanitize=address,undefined,alignment
FLAGS.RELEASE := -O3
FLAGS := ${FLAGS.COMMON} ${FLAGS.${BUILD}}

OBJS = lzarena.o lzflist.o \
bstr.o dynarr.o \
lzhtable.o memory.o \
factory.o utils.o lexer.o \
parser.o compiler.o \
dumpper.o vmu.o \
vm.o

zeus: $(OBJS)
	$(COMPILER) -o $(OUT_DIR)/zeus $(FLAGS) $(OUT_DIR)/*.o $(SRC_DIR)/zeus.c -lm
vm.o:
	$(COMPILER) -c -o $(OUT_DIR)/vm.o $(FLAGS) $(SRC_DIR)/vm.c
vmu.o:
	$(COMPILER) -c -o $(OUT_DIR)/vmu.o $(FLAGS) $(SRC_DIR)/vmu.c
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
lzhtable.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzhtable.o $(FLAGS) $(SRC_DIR)/lzhtable.c
dynarr.o:
	$(COMPILER) -c -o $(OUT_DIR)/dynarr.o $(FLAGS) $(SRC_DIR)/dynarr.c
bstr.o:
	$(COMPILER) -c -o $(OUT_DIR)/bstr.o $(FLAGS) $(SRC_DIR)/bstr.c
lzarena.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzarena.o $(FLAGS) $(SRC_DIR)/lzarena.c
lzflist.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzflist.o $(FLAGS) $(SRC_DIR)/lzflist.c