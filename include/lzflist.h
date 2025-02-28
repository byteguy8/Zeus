#ifndef LZFLIST_H
#define LZFLIST_H

#include <stddef.h>

#define LZFLIST_MIN_CHUNK_SIZE 32
#define LZFLIST_MIN_SPLIT_SIZE 512
#define LZFLIST_MIN_SPLIT_PERCENTAGE 50
#define LZFLIST_DETAULT_ALIGNMENT 16

typedef struct lzflheader LZFLHeader;
typedef struct lzflregion LZFLRegion;
typedef struct lzflist LZFList;

struct lzflheader{
    size_t magic;
    char used;
    size_t size;
    LZFLHeader *prev;
    LZFLHeader *next;
};

struct lzflregion{
    size_t buff_len;
    char *buff;
	char *offset;
    LZFLRegion *prev;
    LZFLRegion *next;
};

struct lzflist{
//> REGION LIST
    size_t regions_len;
    LZFLRegion *regions_head;
    LZFLRegion *regions_tail;
//< REGION LIST
//> FREE LIST
    size_t bytes;
    size_t frees_len;
	LZFLHeader *frees_head;
    LZFLHeader *frees_tail;
    LZFLHeader *auxiliar;
//< FREE LIST
};

LZFList *lzflist_create();
void lzflist_destroy(LZFList *list);

size_t lzflist_free_regions(LZFList *list);
size_t lzflist_ptr_size(void *ptr);

void *lzflist_alloc(size_t size, LZFList *list);
void *lzflist_calloc(size_t size, LZFList *list);
void *lzflist_realloc(void *ptr, size_t new_size, LZFList *list);
void lzflist_dealloc(void *ptr, LZFList *list);

#endif
