#ifndef LZFLIST_H
#define LZFLIST_H

#include <stddef.h>

#define LZFLIST_DETAULT_ALIGNMENT 16
#define LZFLIST_DEFAULT_BUFFER_SIZE 4096

typedef struct lzflheader LZFLHeader;
typedef struct lzflfoot LZFLFoot;
typedef struct lzflregion LZFLRegion;
typedef struct lzflist LZFList;

struct lzflheader{
	size_t magic;
	char used;
	size_t size;
    size_t padding;
	LZFLHeader *prev;
	LZFLHeader *next;
	LZFLHeader *free_prev;
	LZFLHeader *free_next;
};

struct lzflregion{
	size_t buff_len;
	char *buff;
	char *offset;
	LZFLHeader *head;
  LZFLHeader *tail;
};

struct lzflist{
	size_t rused;
	size_t rcount;
	LZFLRegion *regions;
	size_t len;
	LZFLHeader *head;
  LZFLHeader *tail;
};

LZFList *lzflist_create();
void lzflist_destroy(LZFList *list);

size_t lzflist_ptr_size(void *ptr);
void *lzflist_alloc(size_t size, LZFList *list);
void *lzflist_calloc(size_t size, LZFList *list);
void *lzflist_realloc(void *ptr, size_t new_size, LZFList *list);
void lzflist_dealloc(void *ptr, LZFList *list);

#endif