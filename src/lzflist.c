#include "lzflist.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

// comment this out to enable logs
// #define DEBUG_LOG

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

static size_t round_size(size_t to, size_t size){
    size_t mod = size % to;
    size_t padding = mod == 0 ? 0 : to - mod;
    return padding + size;
}

static uintptr_t align_addr(size_t alignment, uintptr_t addr){
    size_t mod = addr % alignment;
    size_t padding = mod == 0 ? 0 : alignment - mod;
    return padding + addr;
}

static void *header_chunk(LZFLHeader *header){
    uintptr_t offset = (uintptr_t)header;
    
    offset += HEADER_SIZE;
    offset = align_addr(LZFLIST_DETAULT_ALIGNMENT, offset);
    
    return (void *)offset;
}

static size_t *header_foot(LZFLHeader *header){
    uintptr_t offset = (uintptr_t)header_chunk(header);
    
    offset += header->size;
    offset = align_addr(LZFLIST_DETAULT_ALIGNMENT, offset);

    return (size_t *)offset;
}

static LZFLHeader *ptr_header(void *ptr){
    uintptr_t offset = (uintptr_t)ptr;
    
    offset -= HEADER_SIZE;
    size_t mod = offset % LZFLIST_DETAULT_ALIGNMENT;
    offset -= mod;
    
    return (LZFLHeader *)offset;
}

static void validate_subregion(LZFLHeader *header){
    size_t *foot = header_foot(header);
    assert(header->magic == MAGIC_NUMBER);
    assert(*foot == MAGIC_NUMBER);
}

static size_t calc_required_size(size_t size){
    uintptr_t header_start = 0;
    uintptr_t header_end = header_start + HEADER_SIZE;

    uintptr_t chunk_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, header_end);
    uintptr_t chunk_end = chunk_start + size;
    
    uintptr_t foot_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, chunk_end);
    uintptr_t foot_end = foot_start + FOOT_SIZE;

    return foot_end;
}

static LZFLRegion *create_region(size_t size){
    size += REGION_SIZE;
    
    size_t raw_buff_len = PAGE_SIZE;
    size = calc_required_size(size);
    
    if(size >= raw_buff_len){
        size_t factor = size / PAGE_SIZE;
        if((PAGE_SIZE * factor) < size){factor += 1;}
        raw_buff_len = PAGE_SIZE * factor;
    }

    char *raw_buff = (char *)mmap(
        NULL,
		raw_buff_len,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0
    );
    
    if (raw_buff == MAP_FAILED){return NULL;}

    LZFLRegion *region = (LZFLRegion *)raw_buff;
    char *buff = raw_buff + REGION_SIZE;

    region->buff_len = raw_buff_len - REGION_SIZE;
    region->buff = buff;
    region->offset = buff;
    region->prev = NULL;
    region->next = NULL;

    return region;
}

static int insert_region(LZFLRegion *region, LZFList *list){
    region->prev = NULL;
    region->next = NULL;

    if(list->regions_tail){
        region->prev = list->regions_tail;
        list->regions_tail->next = region;
    }else{
        list->regions_head = region;
    }

    list->regions_len++;
    list->regions_tail = region;

    return 0;
}

static void remove_region(LZFLRegion *region, LZFList *list){
    if(region == list->regions_head){
        list->regions_head = region->next;
    }
    if(region == list->regions_tail){
        list->regions_tail = region->prev;
    }

    if(region->prev){
        region->prev->next = region->next;
    }
    if(region->next){
        region->next->prev = region->prev;
    }

    list->regions_len--;

    region->prev = NULL;
    region->next = NULL;
}

static int create_and_insert_region(size_t size, LZFList *list){
    LZFLRegion *region = create_region(size);
    if(!region){return 1;}

    insert_region(region, list);

    return 0;
}

static void *alloc_from_region(size_t size, LZFLRegion *region){
    uintptr_t buff_start = (uintptr_t)region->buff;
    uintptr_t buff_end = buff_start + region->buff_len;
    uintptr_t offset = (uintptr_t)region->offset;
    
    if(offset >= buff_end){return NULL;}

    uintptr_t header_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, offset);
    uintptr_t header_end = header_start + HEADER_SIZE;
    uintptr_t chunk_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, header_end);
    uintptr_t chunk_end = chunk_start + size;
    uintptr_t foot_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, chunk_end);
    uintptr_t foot_end = foot_start + FOOT_SIZE;

    // there is no room to allocate
    if(foot_end > buff_end){return NULL;}

    LZFLHeader *header = (LZFLHeader *)header_start;
    header->magic = MAGIC_NUMBER;
    header->used = 1;
    header->size = size;
    header->prev = NULL;
    header->next = NULL;

    uint32_t *foot = (uint32_t *)foot_start;
    *foot = MAGIC_NUMBER;

    // UPDATING REGION OFFSET
    region->offset = (char *)foot_end;

    return (void *)chunk_start;
}

static void insert_to_free_list(LZFLHeader *header, LZFList *list){
    validate_subregion(header);

    header->used = 0;
    header->prev = list->frees_tail;
    header->next = NULL;

    if(list->frees_tail){
        list->frees_tail->next = header;
    }else{
        list->frees_head = header;
    }

    list->frees_len++;
    list->bytes += header->size;
    list->frees_tail = header;

    if(!list->auxiliar || header->size > list->auxiliar->size){
        list->auxiliar = header;
    }
}

static void remove_from_free_list(LZFLHeader *header, LZFList *list){
    validate_subregion(header);
    
    if(header->prev){
        header->prev->next = header->next;
    }
    if(header->next){
        header->next->prev = header->prev;
    }
    if(header == list->frees_head){
        list->frees_head = header->next;
    }
    if(header == list->frees_tail){
        list->frees_tail = header->prev;
    }

    header->used = 1;
    header->prev = NULL;
    header->next = NULL;

    assert(list->bytes >= header->size && "ups!");

    list->frees_len--;
    list->bytes -= header->size;
}

static void *look_first_find(size_t size, LZFList *list){
    if(!list->frees_head){return NULL;}

    LZFLHeader *selected = NULL;

    for(LZFLHeader *header = list->frees_head; header; header = header->next){
        if(header->size >= size){
            selected = header;
            break;
        }
    }

    if(selected){
        remove_from_free_list(selected, list);
        return header_chunk(selected);
    }

    return NULL;
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

    //> REGION LIST
    list->regions_len = 0;
    list->regions_head = NULL;
    list->regions_tail = NULL;
    //< REGION LIST
    //> FREE LIST
    list->bytes = 0;
    list->frees_len = 0;
    list->frees_head = NULL;
    list->frees_tail = NULL;
    list->auxiliar = NULL;
    //> FREE LIST
    
    return list;
}

void lzflist_destroy(LZFList *list){
    if(!list){return;}

    for(LZFLRegion *region = list->regions_head; region; region = region->next){
        char *buff = region->buff;
        munmap(buff, region->buff_len);
    }

    free(list);
}

size_t lzflist_free_regions(LZFList *list){
    size_t unallocated_bytes = 0;

    LZFLRegion *current = list->regions_head;
    
    while (current){
        LZFLRegion *next = current->next;
        
        if(current->offset == current->buff){continue;}

        int is_free = 1;
        uintptr_t offset = (uintptr_t)current->offset;
        uintptr_t current_offset = (uintptr_t)current->buff;

        while (1){
            current_offset = align_addr(LZFLIST_DETAULT_ALIGNMENT, current_offset);
            LZFLHeader *header = (LZFLHeader *)current_offset;

            if(header->used){
                is_free = 0;
                break;
            }

            current_offset = (uintptr_t)(((char *)header_foot(header)) + FOOT_SIZE);
            if(current_offset >= offset){break;}
        }
        
        if(is_free){
            size_t size = REGION_SIZE + current->buff_len;
            unallocated_bytes += size;
            
            remove_region(current, list);
            munmap(current, size);
        }

        current = next;
    }

    return unallocated_bytes;
}

size_t lzflist_ptr_size(void *ptr){
    LZFLHeader *header = ptr_header(ptr);
    validate_subregion(header);
    return header->size;
}

size_t has_region_space(size_t size, LZFLRegion *region){
    size_t required_size = calc_required_size(size);
    
    uintptr_t start = (uintptr_t)region->buff;
    uintptr_t end = start + region->buff_len;
    uintptr_t offset = (uintptr_t)region->offset;

    assert(offset <= end && "ups!");

    offset = align_addr(LZFLIST_DETAULT_ALIGNMENT, offset);
    if(offset > end){return 0;}

    return ((end - offset) >= required_size);
}

LZFLHeader *split_chunk(size_t size, void *ptr){
    LZFLHeader *left_header = ptr_header(ptr);

    uintptr_t buff_start = (uintptr_t)ptr;
    uintptr_t buff_end = ((uintptr_t)header_foot(left_header)) + FOOT_SIZE;

    uintptr_t left_chunk_start = buff_start;
    uintptr_t left_chunk_end = left_chunk_start + size;
    uintptr_t left_foot_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, left_chunk_end);
    uintptr_t left_foot_end = left_foot_start + FOOT_SIZE;

    if(left_foot_end >= buff_end){return NULL;}

    size_t available = buff_end - left_foot_end;
    
    if(HEADER_SIZE + FOOT_SIZE >= available){return NULL;}
    size_t remaining = available - (HEADER_SIZE + FOOT_SIZE);

    uintptr_t right_header_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, left_foot_end);
    uintptr_t right_header_end = right_header_start + HEADER_SIZE;
    uintptr_t right_chunk_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, right_header_end);
    uintptr_t right_chunk_end = right_chunk_start + remaining;
    uintptr_t right_foot_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, right_chunk_end);
    uintptr_t right_foot_end = right_foot_start + FOOT_SIZE;

    if(right_foot_end > buff_end){
        size_t header_chunk_padding = right_chunk_start - right_header_end;
        size_t chunk_foot_padding = right_foot_start - right_chunk_end;
        
        if(header_chunk_padding + chunk_foot_padding >= remaining){return NULL;}
        remaining -= header_chunk_padding + chunk_foot_padding;

        right_chunk_end = right_chunk_start + remaining;
        right_foot_start = align_addr(LZFLIST_DETAULT_ALIGNMENT, right_chunk_end);
        right_foot_end = right_foot_start + FOOT_SIZE;

        if(right_foot_end > buff_end){return NULL;}
    }

    size_t *left_foot = (size_t *)left_foot_start;

    left_header->size = size;
    *left_foot = MAGIC_NUMBER;

    LZFLHeader *right_header = (LZFLHeader *)right_header_start;
    size_t *right_foot = (size_t *)right_foot_start;

    right_header->magic = MAGIC_NUMBER;
    right_header->used = 0;
    right_header->size = remaining;
    right_header->prev = NULL;
    right_header->next = NULL;

    *right_foot = MAGIC_NUMBER;

    return right_header;
}

void *lzflist_alloc(size_t size, LZFList *list){
    LOG("----------------ALLOC REQUEST OF %ld BYTES----------------", size);

    size = round_size(LZFLIST_MIN_CHUNK_SIZE, size);
    
    LOG("requested allocation rounded to %ld bytes", size);
    LOG("looking chunk in frees list");

    void *ptr = look_first_find(size, list);

    if(ptr){
        LZFLHeader *left_header = ptr_header(ptr);

        LOG("chunk of %ld found", left_header->size);
        
        if(left_header->size >= LZFLIST_MIN_SPLIT_SIZE){
            LOG("chunk of %ld bytes is candidate for splitting", left_header->size);
            
            double available = ((double)(left_header->size - size)) / ((double)left_header->size) * 100.0;
            
            LOG("requested bytes %ld let %f%% available from the chunk", size, available);

            if(available >= LZFLIST_MIN_SPLIT_PERCENTAGE){
                LZFLHeader *right_header = split_chunk(size, ptr);
                
                if(right_header){
                    LOG("chunk splitted and created a %ld bytes one", right_header->size);
                    insert_to_free_list(right_header, list);
                }else{
                    LOG("chunk not splitted");
                }
            }else{
                LOG("chunk has not enough space to be splitted");
            }
        }
    }else{
        LOG("no chunk found in frees list to satifies the request");

        if(list->regions_len == 0){
            LOG("appending new region");

            if(create_and_insert_region(size, list)){
                LOG("region not appended");
                LOG("alloc request failed");
                return NULL;
            }

            LOG("new region appended");
        }else{
            LZFLRegion *region = list->regions_tail;

            if(!has_region_space(size, region)){
                LOG("current region has not space for %ld bytes", size);
                LOG("appending new region");

                if(create_and_insert_region(size, list)){
                    LOG("failed to append new region");
                    LOG("alloc request failed");
                    return NULL;    
                }

                LOG("new region appended");
            }
        }
        
        LZFLRegion *region = list->regions_tail;
        void *ptr = alloc_from_region(size, region);

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
    
    LZFLHeader *header = ptr_header(ptr);

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

    LZFLHeader *header = ptr_header(ptr);
    
    LOG("chunk %p to deallocate is of %ld bytes", ptr, header->size);

    insert_to_free_list(header, list);

    LOG("dealloc request succeed");
}
