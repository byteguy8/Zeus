SRC = ./src
OUT_DIR = ./build
CFLAGS = -I./include -Wall -Wextra -g2 -Wno-unused-parameter

OBJS = lzarena.o dynarr.o \
lzhtable.o memory.o \
utils.o lexer.o \
parser.o compiler.o \
vm.o

zeus: $(OBJS)
	gcc -o $(OUT_DIR)/zeus $(CFLAGS) $(OUT_DIR)/*.o $(SRC)/zeus.c
vm.o:
	gcc -c -o $(OUT_DIR)/vm.o $(CFLAGS) $(SRC)/vm.c
compiler.o:
	gcc -c -o $(OUT_DIR)/compiler.o $(CFLAGS) $(SRC)/compiler.c
parser.o:
	gcc -c -o $(OUT_DIR)/parser.o $(CFLAGS) $(SRC)/parser.c
lexer.o:
	gcc -c -o $(OUT_DIR)/lexer.o $(CFLAGS) $(SRC)/lexer.c
utils.o:
	gcc -c -o $(OUT_DIR)/utils.o $(CFLAGS) $(SRC)/utils.c
memory.o:
	gcc -c -o $(OUT_DIR)/memory.o $(CFLAGS) $(SRC)/memory.c
lzhtable.o:
	gcc -c -o $(OUT_DIR)/lzhtable.o $(CFLAGS) $(SRC)/lzhtable.c
dynarr.o:
	gcc -c -o $(OUT_DIR)/dynarr.o $(CFLAGS) $(SRC)/dynarr.c
lzarena.o:
	gcc -c -o $(OUT_DIR)/lzarena.o  $(CFLAGS) $(SRC)/lzarena.c