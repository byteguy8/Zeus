#include "lzflist.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

//#define DEBUG_LOG

#ifdef DEBUG_LOG
    #define LOG(fmt, ...) do {                                                   \
        fprintf(stdout, "%s(%d): " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        fflush(stdout);                                                          \
    }while(0)
#else
    #define LOG(fmt, ...)
#endif

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
#define HEADER_FOOT(header)((((char *)(header)) + HEADER_SIZE) + ((header)->padding) + ((header)->size))

#define IS_HEADER_VALID(header)((header)->magic == MAGIC_NUMBER)
#define IS_HEADER_FOOT_VALID(header)((*(uint32_t *)HEADER_FOOT(header)) == MAGIC_NUMBER)

#define VALIDATE_HEADER(header)(assert(IS_HEADER_VALID(header) && "memory's header invalid state"))
#define VALIDATE_HEADER_USED(header)(assert((header)->used && "memory's header should be in use"))
#define VALIDATE_HEADER_NOT_USED(header)(assert(!((header)->used) && "memory's header should not be in use"))
#define VALIDATE_HEADER_FOOT(header)(assert(IS_HEADER_FOOT_VALID(header) && "memory's foot invalid state"))

static int grow(LZFList *list){
    size_t new_count = list->rcount == 0 ? 8 : list->rcount * 2;    
    void *new_regions = realloc(list->regions, REGION_SIZE * new_count);
    
    LOG("growing regions array from %ld to %ld", list->rcount, new_count);

    if(!new_regions){
        LOG("failed to grow regions array");
        return 1;
    }

    list->rcount = new_count;
    list->regions = (LZFLRegion *)new_regions;

    LOG("regions array grew");
    return 0;
}

static int append_region(size_t buff_len, LZFList *list){
    LOG("appending new region of %ld bytes", buff_len);
    
    char *buff = (char *)mmap(
        NULL,
		    buff_len,
		    PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS,
		    -1,
		    0
    );
    
    if (buff == MAP_FAILED){
        LOG("failed to append region. MAP_FAILED");
        return 1;
    }

    if(list->rused >= list->rcount && grow(list)){
        LOG("failed to append region");
        munmap(buff, buff_len);
        return 1;
    }

    LOG("region appended");

    LZFLRegion *region = &list->regions[list->rused++];

    region->buff_len = buff_len;
    region->buff = buff;
    region->offset = buff;
    region->head = NULL;
    region->tail = NULL;
    
    return 0;
}

static int region_has_space(size_t size, LZFLRegion *region){
    LOG("verifying region available space");

    uintptr_t start = (uintptr_t)region->buff;
    uintptr_t end = start + region->buff_len;
    uintptr_t offset = (uintptr_t)region->offset;

#ifdef LZFLIST_DEBUG
    assert(offset <= end && "I'm sure I did something wrong");
#endif

    size_t available = end - offset;
    size_t required = CALC_AREA_SIZE(size);
    
    LOG("region (start: %ld - end: %ld) with offset %ld", start, end, offset);
    LOG("region has %ld bytes available for %ld bytes required", available, required);

    if(available < required){
        LOG("region has not enough space");
    }else{
        LOG("region has enough space");
    }
    
    return available >= required;
}

static uintptr_t align_size(size_t alignment, size_t size){
    size_t mod = size % alignment;
    size_t padding = mod == 0 ? 0 : alignment - mod;
    return padding + size;
}

static uintptr_t align_addr(size_t alignment, uintptr_t addr){
    size_t mod = addr % alignment;
    size_t padding = mod == 0 ? 0 : alignment - mod;
    return padding + addr;
}

static LZFLHeader *get_header(void *ptr){
    uintptr_t chunk_start = (uintptr_t)ptr;
    uintptr_t header_start = chunk_start - HEADER_SIZE;
    size_t mod = header_start % LZFLIST_DETAULT_ALIGNMENT;
    
    return (LZFLHeader *)(header_start - mod);
}

static void *alloc_from_region_aligned(size_t alignment, size_t size, LZFLRegion *region){
    uintptr_t buff_start = (uintptr_t)region->buff;
    uintptr_t buff_end = buff_start + region->buff_len;
    uintptr_t offset = (uintptr_t)region->offset;

    size_t aligned_size = align_size(LZFLIST_DETAULT_ALIGNMENT, size);
    
    uintptr_t header_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, offset);
    uintptr_t header_end = header_start + HEADER_SIZE;

    uintptr_t chunk_start = align_addr(alignment, header_end);
    uintptr_t chunk_end = chunk_start + aligned_size;

    header_start = chunk_start - HEADER_SIZE;
    size_t mod = header_start % LZFLIST_DETAULT_ALIGNMENT;
    header_start -= mod;
    header_end = header_start + HEADER_SIZE;

    uintptr_t foot_start = chunk_end;
    uintptr_t foot_end = foot_start + FOOT_SIZE;

    if(foot_end > buff_end){
        LOG("region with %ld bytes need %ld bytes to allocate %ld bytes", buff_end - offset, foot_end - offset, size);
        return NULL;
    }

    LZFLHeader *header = (LZFLHeader *)header_start;
    char *chunk = (char *)chunk_start;
    uint32_t *foot = (uint32_t *)foot_start;

    header->magic = MAGIC_NUMBER;
    header->used = 1;
    header->size = aligned_size;
    header->padding = chunk_start - header_end;
    header->prev = NULL;
    header->next = NULL;
    header->free_prev = NULL;
    header->free_next = NULL;

    *foot = MAGIC_NUMBER;

    header->prev = region->tail;

    if(region->tail){
        region->tail->next = header;
    }else{
        region->head = header;
    }

    region->tail = header;

    offset = foot_end;
    region->offset = (char *)offset;

    return chunk;
}

static void insert_to_free_list(LZFLHeader *header, LZFList *list){
    LOG("inserting chunk %p of %ld bytes in free list", HEADER_CHUNK(header), header->size);
    LOG("validating chunk's header and foot");

    VALIDATE_HEADER(header);
    VALIDATE_HEADER_FOOT(header);
    VALIDATE_HEADER_USED(header);

    LOG("chunk's header and foot validated");

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

    LOG("chunk inserted in free list");
}

static void remove_from_free_list(LZFLHeader *header, LZFList *list){
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

static void *look_first_find(size_t size, LZFList *list){
    LOG("looking free chunk of %ld bytes by first find", size);
    
    if(!list->head){
        LOG("there aren't free chunks");
        return NULL;
    }

    LZFLHeader *selected = NULL;

    for(LZFLHeader *header = list->head; header; header = header->free_next){
        if(header->size >= size){
            selected = header;
            break;
        }
    }

    if(selected){
        LOG("free chunk found of %ld bytes", selected->size);
        remove_from_free_list(selected, list);
        return HEADER_CHUNK(selected);
    }

    LOG("free chunk not found");
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
        munmap(buff, region->buff_len);
    }

    free(list->regions);
    free(list);
}

size_t lzflist_ptr_size(void *ptr){
    LZFLHeader *header = PTR_HEADER(ptr);
    assert(header->used && "block of memory not available");
    return header->size;
}

size_t calc_required_size_aligned(size_t alignment, size_t size){
    size_t aligned_size = align_size(LZFLIST_DETAULT_ALIGNMENT, size);
    
    uintptr_t header_start = 0;
    uintptr_t header_end = header_start + HEADER_SIZE;

    uintptr_t chunk_start = align_addr(alignment, header_end);
    uintptr_t chunk_end = chunk_start + aligned_size;

    uintptr_t foot_start = chunk_end;
    uintptr_t foot_end = foot_start + FOOT_SIZE;

    return foot_end;
}

size_t region_has_space_aligned(size_t alignment, size_t size, LZFLRegion *region){
    size_t required_size = calc_required_size_aligned(alignment, size);
    
    uintptr_t start = (uintptr_t)region->buff;
    uintptr_t end = start + region->buff_len;
    uintptr_t offset = (uintptr_t)region->offset;

    assert(offset <= end && "UPS!");

    offset = align_addr(LZFLIST_DETAULT_ALIGNMENT, offset);
    if(offset > end){return NULL;}

    return ((end - offset) >= required_size);
}

void *lzflist_alloc(size_t size, LZFList *list){
    LOG("----------------ALLOC REQUEST OF %ld BYTES----------------", size);

    void *ptr = look_first_find(size, list);

    if(!ptr){
        size_t buff_len = PAGE_SIZE;
        size_t required_size = calc_required_size_aligned(LZFLIST_DETAULT_ALIGNMENT, size);
        
        if(required_size >= buff_len){
            buff_len = PAGE_SIZE * (required_size / PAGE_SIZE + 1);
        }

        if(list->rused == 0){
            if(append_region(buff_len, list)){
                LOG("alloc request failed");
                return NULL;
            }
        }else{
            LZFLRegion *region = &list->regions[list->rused - 1];

            if(!region_has_space_aligned(LZFLIST_DETAULT_ALIGNMENT, size, region)){
                if(append_region(buff_len, list)){
                    LOG("alloc request failed");
                    return NULL;    
                }
            }
        }
        
        LZFLRegion *region = &list->regions[list->rused - 1];
        void *ptr = alloc_from_region_aligned(LZFLIST_DETAULT_ALIGNMENT, size, region);

        LOG("alloc request succeed");

        return ptr;
    }

    LOG("alloc request succeed");

    return ptr;
}

void *lzflist_calloc(size_t size, LZFList *list){
    LOG("---------------CALLOC REQUEST OF %ld BYTES----------------", size);
    
    void *ptr = lzflist_alloc(size, list);
    
    if(ptr){
        memset(ptr, 0, size);

        LOG("allocated chunk %p zeroed", ptr);
        LOG("calloc request succeed");
    }else{
        LOG("calloc request failed");
    }
    
    return ptr;
}

void *lzflist_realloc(void *ptr, size_t new_size, LZFList *list){
    LOG("---------------REALLOC REQUEST OF %ld BYTES----------------", new_size);

    if(!ptr){
        LOG("pointer is NULL. normal allocation to be realized");

        void *ptr = lzflist_alloc(new_size, list);
        
        if(ptr){LOG("realloc request succeed");}
        else{LOG("realloc request failed");}

        return ptr;
    }
    
    LZFLHeader *header = PTR_HEADER(ptr);
    VALIDATE_HEADER(header);
    VALIDATE_HEADER_FOOT(header);
    VALIDATE_HEADER_USED(header);

    LOG("chunk %p to be reallocated is of %ld bytes", ptr, header->size);

    size_t old_size = header->size;
    void *new_ptr = lzflist_alloc(new_size, list);

    if(new_ptr){
        LOG("new chunk %p of requested size %ld allocated", new_ptr, new_size);

        size_t cpy_size = new_size < old_size ? new_size : old_size;
        memcpy(new_ptr, ptr, cpy_size);
        
        LOG("content of previous chunk %p copied to new one %p", ptr, new_ptr);
        
        lzflist_dealloc(ptr, list);

        LOG("old chunk deallocated");
        LOG("realloc request succeed");
    }else{
        LOG("realloc request failed");
    }
    
    return new_ptr;
}

void lzflist_dealloc(void *ptr, LZFList *list){
    LOG("---------------DEALLOC REQUEST----------------");
    
    if(!ptr){
        LOG("pointer is NULL. deallocation skipped");
        return;
    }

#ifdef LZFLIST_VALIDATE_PTR
    validate_ptr(ptr, list);
#endif

    LZFLHeader *header = get_header(ptr);
    
    LOG("chunk %p to deallocate is of %ld bytes", ptr, header->size);

    insert_to_free_list(header, list);

    LOG("dealloc request succeed");
}