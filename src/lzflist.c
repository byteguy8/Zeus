#include "lzflist.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

#define HEADER_SIZE (sizeof(LZFLHeader))
#define FOOT_SIZE (sizeof(uint32_t))
#define REGION_SIZE (sizeof(LZFLRegion))
#define LIST_SIZE (sizeof(LZFList))

#define MAGIC_NUMBER 0xDEADBEEF
#define PTR_HEADER(ptr) ((LZFLHeader *)(((char *)(ptr)) - HEADER_SIZE))

#define CALC_AREA_SIZE(size)(HEADER_SIZE + (size) + FOOT_SIZE)
#define AREA_END(header)((void *)((char *)(header)) + HEADER_SIZE + (header)->size + FOOT_SIZE)

#define HEADER_CHUNK(header)(((char *)(header)) + HEADER_SIZE)
#define HEADER_FOOT(header)((((char *)(header)) + (HEADER_SIZE + (header)->size)))

#define IS_HEADER_VALID(header)((header)->magic == MAGIC_NUMBER)
#define IS_HEADER_FOOT_VALID(header)((*(uint32_t *)HEADER_FOOT(header)) == MAGIC_NUMBER)

#define VALIDATE_HEADER(header)(assert(IS_HEADER_VALID(header) && "memory's header invalid state"))
#define VALIDATE_HEADER_USED(header)(assert((header)->used && "memory's header should be in use"))
#define VALIDATE_HEADER_NOT_USED(header)(assert(!((header)->used) && "memory's header should not be in use"))
#define VALIDATE_HEADER_FOOT(header)(assert(IS_HEADER_FOOT_VALID(header) && "memory's foot invalid state"))

static int grow(LZFList *list){
    size_t new_count = list->rcount == 0 ? 8 : list->rcount * 2;
    void *new_regions = realloc(list->regions, REGION_SIZE * new_count);

    if(!new_regions){return 1;}

    list->rcount = new_count;
    list->regions = (LZFLRegion *)new_regions;

    return 0;
}

static int append_region(size_t buff_len, LZFList *list){
    char *buff = (char *)mmap(
		NULL,
		buff_len,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		0,
		0
    );
    if (buff == MAP_FAILED){return 1;}

    if(list->rused >= list->rcount && grow(list)){
        munmap(buff, buff_len);
        return 1;
    }

    LZFLRegion *region = &list->regions[list->rused++];

    region->buff_len = buff_len;
    region->buff = buff;
    region->offset = buff;
    region->head = NULL;
    region->tail = NULL;
    
    return 0;
}

static int region_has_space(size_t size, LZFLRegion *region){
    uintptr_t start = (uintptr_t)region->buff;
    uintptr_t end = start + region->buff_len;
    uintptr_t offset = (uintptr_t)region->offset;

    assert(offset <= end && "I'm sure I did something wrong");

    size_t available = end - offset;
    size_t required = CALC_AREA_SIZE(size);
    
    return available >= required;
}

static void *alloc_from_region(size_t size, LZFLRegion *region){
    if(!region_has_space(size, region)){return NULL;}
    
    LZFLHeader *header = (LZFLHeader *)region->offset;
    
    header->magic = MAGIC_NUMBER;
    header->used = 1;
    header->size = size;
    header->prev = NULL;
    header->next = NULL;
    header->free_prev = NULL;
    header->free_next = NULL;

    char *chunk = HEADER_CHUNK(header);
    uint32_t *foot = (uint32_t *)HEADER_FOOT(header);

    assert(((uintptr_t)chunk) - ((uintptr_t)header) == HEADER_SIZE);
    assert(((uintptr_t)foot) - ((uintptr_t)chunk) == size);
    assert(((uintptr_t)AREA_END(header)) - ((uintptr_t)foot) == FOOT_SIZE);
    assert(((uintptr_t)AREA_END(header)) - ((uintptr_t)header) == CALC_AREA_SIZE(size));

    *foot = MAGIC_NUMBER;

    region->offset += CALC_AREA_SIZE(size);

    assert(((uintptr_t)region->offset) == (((uintptr_t)foot) + FOOT_SIZE));
    assert(((uintptr_t)region->offset) <= (((uintptr_t)region->buff) + region->buff_len));

    header->prev = region->tail;

    if(region->tail){
        region->tail->next = header;
    }else{
        region->head = header;
    }

    region->tail = header;

    return chunk;
}

static LZFLHeader *look_first_find(size_t size, LZFList *list){
    if(!list->head){return NULL;}

    for(LZFLHeader *header = list->head; header; header = header->free_next){
        if(header->size >= size){return header;}
    }

    return NULL;
}

static void coalesce(LZFList *list){
    for (size_t i = 0; i < list->rused; i++){
        LZFLRegion *region = &list->regions[i];

        for (LZFLHeader *header = region->head; header; header = header->next){
            LZFLHeader *next = header->next;

            if(next && !next->used){
                // Updating region list
                if(next->prev){
                    next->prev->next = next->next;
                }
                if(next->next){
                    next->next->prev = next->prev;
                }
                if(next == region->tail){
                    region->tail = next->prev;
                }

                // Updating free list
                if(next->free_prev){
                    next->free_prev->free_next = next->free_next;
                }
                if(next->free_next){
                    next->free_next->free_prev = next->free_prev;
                }
                if(next == list->head){
                    list->head = next->free_next;
                }
                if(next == list->tail){
                    list->tail = next->free_prev;
                }

                list->len--;
                header->size += CALC_AREA_SIZE(next->size);
            }
        }
    }
}

static void insert_chunk(LZFLHeader *header, LZFList *list){
    VALIDATE_HEADER(header);
    VALIDATE_HEADER_FOOT(header);
    VALIDATE_HEADER_USED(header);

    header->used = 0;
    header->free_prev = list->tail;
    header->free_next = NULL;

    if(list->tail){
        list->tail->free_next = header;
    }else{
        list->head = header;
    }

    list->len++;
    list->tail = header;
}

static void remove_chunk(LZFLHeader *header, LZFList *list){
    VALIDATE_HEADER(header);
    VALIDATE_HEADER_FOOT(header);
    VALIDATE_HEADER_NOT_USED(header);

    if(header->free_prev){
        header->free_prev->free_next = header->free_next;
    }
    if(header->free_next){
        header->free_next->free_prev = header->free_prev;
    }
    if(header == list->head){
        list->head = header->free_next;
    }
    if(header == list->tail){
        list->tail = header->free_prev;
    }

    list->len--;

    header->used = 1;
    header->free_prev = NULL;
    header->free_next = NULL;
}
#ifdef LZFLIST_VALIDATE_PTR
static int validate_ptr(void *ptr, LZFList *list){
    printf("validate\n");
    int valid = 0;

    for (size_t i = 0; i < list->rused; i++){
        LZFLRegion *region = &list->regions[i];
        
        uintptr_t ip0 = (uintptr_t)ptr;
        uintptr_t start = (uintptr_t)region->buff;
        uintptr_t end = start + region->buff_len;

        if(ip0 > start && ip0 < end){
            valid = 1;
            break;
        }
    }

    return valid;
}
#endif
LZFList *lzflist_create(){
    LZFList *list = (LZFList *)malloc(LIST_SIZE);

    if(!list){
        free(list);
        return NULL;
    }

    list->rused = 0;
    list->rcount = 0;
    list->regions = NULL;
    list->len = 0;
    list->head = NULL;
    list->tail = NULL;
    
    return list;
}

void lzflist_destroy(LZFList *list){
    if(!list){return;}

    for(size_t i = 0; i < list->rused; i++){
        LZFLRegion *region = &list->regions[i];
        char *buff = region->buff;
        memset(buff, 0, region->buff_len);
        munmap(buff, region->buff_len);
    }

    memset(list->regions, 0, REGION_SIZE * list->rcount);
    free(list->regions);

    memset(list, 0, LIST_SIZE);
    free(list);
}

size_t lzflist_ptr_size(void *ptr){
    LZFLHeader *header = PTR_HEADER(ptr);
    assert(header->used && "block of memory not available");
    return header->size;
}

void *lzflist_alloc(size_t size, LZFList *list){
    LZFLHeader *header = look_first_find(size, list);

    if(!header){
        size_t buff_len = PAGE_SIZE;
        
        if(size >= buff_len){
            buff_len = PAGE_SIZE * (buff_len / PAGE_SIZE + 1) * 2;
        }

        if(list->rused == 0){
            if(append_region(buff_len, list)){return NULL;}
        }else{
            coalesce(list);
            header = look_first_find(size, list);

            if(header){
                remove_chunk(header, list);
                return HEADER_CHUNK(header);
            }

            LZFLRegion *region = &list->regions[list->rused - 1];
            if(!region_has_space(size, region) && append_region(buff_len, list)){return NULL;};
        }
        
        LZFLRegion *region = &list->regions[list->rused - 1];
        
        return alloc_from_region(size, region);
    }

    remove_chunk(header, list);
    
    return HEADER_CHUNK(header);
}

void *lzflist_calloc(size_t size, LZFList *list){
    void *ptr = lzflist_alloc(size, list);
    if(ptr){memset(ptr, 0, size);}
    return ptr;
}

void *lzflist_realloc(void *ptr, size_t new_size, LZFList *list){
    if(!ptr){return lzflist_alloc(new_size, list);}
    
    LZFLHeader *header = PTR_HEADER(ptr);
    VALIDATE_HEADER(header);
    VALIDATE_HEADER_FOOT(header);
    VALIDATE_HEADER_USED(header);

    size_t old_size = header->size;
    void *new_ptr = lzflist_alloc(new_size, list);

    if(new_ptr){
        size_t cpy_size = new_size < old_size ? new_size : old_size;
        memcpy(new_ptr, ptr, cpy_size);
        lzflist_dealloc(ptr, list);
    }

    return new_ptr;
}

void lzflist_dealloc(void *ptr, LZFList *list){
    if(!ptr){return;}
#ifdef LZFLIST_VALIDATE_PTR
    validate_ptr(ptr, list);
#endif
    LZFLHeader *header = PTR_HEADER(ptr);
    insert_chunk(header, list);
}