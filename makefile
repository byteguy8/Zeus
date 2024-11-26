SRC = ./src
OUT_DIR = ./build
CFLAGS = -I./include -Wall -Wextra -g2 -Wno-unused-parameter
COMPILER = gcc
OBJS = lzarena.o lzdynalloc.o \
dynarr.o lzhtable.o \
memory.o utils.o \
lexer.o parser.o \
compiler.o dumpper.o \
vm.o

zeus: $(OBJS)
	$(COMPILER) -o $(OUT_DIR)/zeus $(CFLAGS) $(OUT_DIR)/*.o $(SRC)/zeus.c
vm.o:
	$(COMPILER) -c -o $(OUT_DIR)/vm.o $(CFLAGS) $(SRC)/vm.c
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
lzdynalloc.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzdynalloc.o $(CFLAGS) $(SRC)/lzdynalloc.c
lzarena.o:
	$(COMPILER) -c -o $(OUT_DIR)/lzarena.o  $(CFLAGS) $(SRC)/lzarena.c
