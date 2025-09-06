#include "lzflist.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
    #include <sysinfoapi.h>
    #include <windows.h>
#elif __linux__
    #include <unistd.h>
    #include <sys/mman.h>
#endif

#ifdef _WIN32
    static DWORD windows_page_size(){
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwPageSize;
    }

    #define PAGE_SIZE windows_page_size()
#elif __linux__
    #define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif
//--------------------------------------------------------------------------//
//                            PRIVATE INTERFACE                             //
//--------------------------------------------------------------------------//
#define HEADER_SIZE (sizeof(LZFLHeader))
#define REGION_SIZE (sizeof(LZFLRegion))
#define LIST_SIZE (sizeof(LZFList))

#define MAGIC_NUMBER 0xDEADBEEF
//--------------------------------  MEMORY  --------------------------------//
static inline void *lzalloc(size_t size, LZFListAllocator *allocator);
static inline void *lzrealloc(void *ptr, size_t old_size, size_t new_size, LZFListAllocator *allocator);
static inline void lzdealloc(void *ptr, size_t size, LZFListAllocator *allocator);

#define MEMORY_ALLOC(_type, _count, _allocator)((_type *)lzalloc(sizeof(_type) * (_count), (_allocator)))
#define MEMORY_REALLOC(_ptr, _type, _old_count, _new_count, _allocator)((_type *)(lzrealloc((_ptr), sizeof(_type) * (_old_count), sizeof(_type) * (_new_count), (_allocator))))
#define MEMORY_DEALLOC(_ptr, _type, _count, _allocator)(lzdealloc((_ptr), sizeof(_type) * (_count), (_allocator)))

static void *alloc_backend(size_t size);
static void dealloc_backend(void *ptr, size_t size);
//--------------------------------  UTILS  ---------------------------------//
static inline size_t round_size(size_t to, size_t size);
static inline uintptr_t align_addr(size_t alignment, uintptr_t addr);
//--------------------------------  REGION  --------------------------------//
static LZFLRegion *create_region(size_t size);
static void destroy_region(LZFLRegion *region);
static int insert_region(LZFLRegion *region, LZFLRegionList *list);
static void remove_region(LZFLRegion *region, LZFLRegionList *list);
//---------------------------------  AREA  ---------------------------------//
static LZFLHeader *create_area(size_t size, LZFLRegion *region, size_t *out_consume_bytes, void **out_chunk);
static inline void *area_chunk(LZFLHeader *area);
static inline void *area_chunk_end(LZFLHeader *area);
static inline LZFLHeader *chunk_area(void *ptr);
static inline size_t calc_area_size(LZFLHeader *header);
static inline LZFLHeader *area_next_to(LZFLHeader *area);
static inline LZFLHeader *is_next_area_free(LZFLHeader *area, LZFLHeader **out_next);
static inline void *alloc_area(LZFLHeader *area, LZFLAreaList *list);
static inline void dealloc_area(LZFLHeader *area, LZFLAreaList *list);
static void insert_area_at_end(LZFLHeader *area, LZFLAreaList *list);
static void remove_area(LZFLHeader *area, LZFLAreaList *list);
//-----------------------------  REGION UTILS  -----------------------------//
static void replace_current_region(LZFLRegion *region, LZFList *list);
static int create_and_insert_region(size_t size, LZFList *list);
static void *alloc_from_region(size_t size, LZFLRegion *region, LZFLHeader **out_area);
//--------------------------------  OTHERS  --------------------------------//
static void *look_first_fit(size_t size, LZFList *list, LZFLHeader **out_area);
//--------------------------------------------------------------------------//
//                          PRIVATE IMPLEMENTATION                          //
//--------------------------------------------------------------------------//
static inline void *lzalloc(size_t size, LZFListAllocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

static inline void *lzrealloc(void *ptr, size_t old_size, size_t new_size, LZFListAllocator *allocator){
    return allocator ? allocator->realloc(ptr, old_size, new_size, allocator->ctx) : realloc(ptr, new_size);
}

static inline void lzdealloc(void *ptr, size_t size, LZFListAllocator *allocator){
    if(allocator){
        allocator->dealloc(ptr, size, allocator->ctx);
    }else{
        free(ptr);
    }
}

static void *alloc_backend(size_t size){
#ifndef LZFLIST_BACKEND
    #error "A backend must be defined"
#endif

#if LZFLIST_BACKEND == LZFLIST_BACKEND_MALLOC
    return malloc(size);
#elif LZFLIST_BACKEND == LZFLIST_BACKEND_MMAP
    void *ptr = (char *)mmap(
		NULL,
		size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0
    );

    return ptr == MAP_FAILED ? NULL : ptr;
#elif LZFLIST_BACKEND == LZFLIST_BACKEND_VIRTUALALLOC
    return VirtualAlloc(
        NULL,
        size,
        MEM_COMMIT,
        PAGE_READWRITE
    );
#else
    #error "Unknown backend"
#endif
}

static void dealloc_backend(void *ptr, size_t size){
    if(!ptr){
        return;
    }

#ifndef LZFLIST_BACKEND
    #error "A backend must be defined"
#endif

#if LZFLIST_BACKEND == LZFLIST_BACKEND_MALLOC
    free(ptr);
#elif LZFLIST_BACKEND == LZFLIST_BACKEND_MMAP
    if(munmap(ptr, size) == -1){
        perror(NULL);
    }
#elif LZFLIST_BACKEND == LZFLIST_BACKEND_VIRTUALALLOC
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    #error "Unknown backend"
#endif
}

static inline size_t round_size(size_t to, size_t size){
    size_t mod = size % to;
    size_t padding = mod == 0 ? 0 : to - mod;
    return padding + size;
}

static inline uintptr_t align_addr(size_t alignment, uintptr_t addr){
    size_t mod = addr % alignment;
    size_t padding = mod == 0 ? 0 : alignment - mod;
    return padding + addr;
}

static LZFLRegion *create_region(size_t requested_size){
    requested_size +=
        round_size(LZFLIST_DEFAULT_ALIGNMENT, REGION_SIZE) +
        round_size(LZFLIST_DEFAULT_ALIGNMENT, HEADER_SIZE);

    size_t page_size = (size_t)PAGE_SIZE;
    size_t needed_pages = requested_size / page_size;

    size_t needed_size =
        ((((size_t) 0) - ((needed_pages * page_size) >= requested_size)) & (needed_pages * page_size)) |
        ((((size_t) 0) - ((needed_pages * page_size) < requested_size)) & ((needed_pages + 1) * page_size));

    void *raw_buff = alloc_backend(needed_size);

    if (!raw_buff){
        return NULL;
    }

    uintptr_t region_uiptr = (uintptr_t)raw_buff;
    uintptr_t subregion_uiptr = align_addr(LZFLIST_DEFAULT_ALIGNMENT, region_uiptr + REGION_SIZE);
    uintptr_t subregion_end_uiptr = region_uiptr + needed_size;

    LZFLRegion *region = (LZFLRegion *)region_uiptr;
    size_t subregion_size = subregion_end_uiptr - subregion_uiptr;
    void *subregion = (void *)subregion_uiptr;

    region->used_bytes = 0;
    region->consumed_bytes = 0;
    region->subregion_size = subregion_size;
    region->offset = subregion;
    region->subregion = subregion;
    region->prev = NULL;
    region->next = NULL;

    return region;
}

static inline void destroy_region(LZFLRegion *region){
    if(!region){
        return;
    }

    dealloc_backend(region, ((uintptr_t)region->subregion) - ((uintptr_t)region) + region->subregion_size);
}

static int insert_region(LZFLRegion *region, LZFLRegionList *list){
    region->prev = NULL;
    region->next = NULL;

    if(list->tail){
        region->prev = list->tail;
        list->tail->next = region;
    }else{
        list->head = region;
    }

    list->len++;
    list->tail = region;

    return 0;
}

static void remove_region(LZFLRegion *region, LZFLRegionList *list){
    if(region == list->head){
        list->head = region->next;
    }
    if(region == list->tail){
        list->tail = region->prev;
    }

    if(region->prev){
        region->prev->next = region->next;
    }
    if(region->next){
        region->next->prev = region->prev;
    }

    list->len--;

    region->prev = NULL;
    region->next = NULL;
}

// A 'area' represents a portion of memory from the subregion of
// the specified region, which is subdivided in: header and chunk.
// May exists some padding between end of header and start of chunk
static LZFLHeader *create_area(size_t size, LZFLRegion *region, size_t *out_consumed_bytes, void **out_chunk){
    uintptr_t subregion_start = (uintptr_t)region->subregion;
    uintptr_t subregion_end = subregion_start + region->subregion_size;
    uintptr_t offset = (uintptr_t)region->offset;

    if(offset >= subregion_end){
        return NULL;
    }

    uintptr_t area_start = align_addr(LZFLIST_DEFAULT_ALIGNMENT, offset);
    uintptr_t chunk_start = align_addr(LZFLIST_DEFAULT_ALIGNMENT, area_start + HEADER_SIZE);
    uintptr_t chunk_end = chunk_start + size;
    size_t consumed_bytes = chunk_end - area_start;

    if(chunk_end > subregion_end){
        return NULL;
    }

    LZFLHeader *area = (LZFLHeader *)area_start;

    area->magic = MAGIC_NUMBER;
    area->used = 0;
    area->size = size;
    area->prev = NULL;
    area->next = NULL;
    area->region = region;

    region->consumed_bytes += consumed_bytes;
    region->offset = (void *)chunk_end;

    if(out_consumed_bytes){
        *out_consumed_bytes = consumed_bytes;
    }

    if(out_chunk){
        *out_chunk = (void *)chunk_start;
    }

    return area;
}

static inline void *area_chunk(LZFLHeader *area){
    uintptr_t offset = (uintptr_t)area;
    return (void *)(align_addr(LZFLIST_DEFAULT_ALIGNMENT, offset + HEADER_SIZE));
}

static inline void *area_chunk_end(LZFLHeader *area){
    return (void *)(((uintptr_t)area_chunk(area)) + area->size);
}

static inline LZFLHeader *chunk_area(void *ptr){
    uintptr_t offset = ((uintptr_t)ptr) - HEADER_SIZE;
    size_t alignment = offset % LZFLIST_DEFAULT_ALIGNMENT;
    LZFLHeader *header = (LZFLHeader *)(offset - alignment);

    assert(header->magic == MAGIC_NUMBER && "Corrupted area");

    return header;
}

static inline size_t calc_area_size(LZFLHeader *area){
    uintptr_t area_start = (uintptr_t)area;
    uintptr_t chunk_start = align_addr(LZFLIST_DEFAULT_ALIGNMENT, area_start + HEADER_SIZE);
    uintptr_t chunk_end = chunk_start + area->size;

    return chunk_end - area_start;
}

static inline LZFLHeader *area_next_to(LZFLHeader *area){
    LZFLRegion *region = area->region;
    uintptr_t offset = (uintptr_t)region->offset;

    uintptr_t area_start = (uintptr_t)area;
    uintptr_t chunk_start = align_addr(LZFLIST_DEFAULT_ALIGNMENT, area_start + HEADER_SIZE);
    uintptr_t chunk_end = chunk_start + area->size;

    if(chunk_end < offset){
        LZFLHeader *next_area = (LZFLHeader *)align_addr(LZFLIST_DEFAULT_ALIGNMENT, chunk_end);
        assert(next_area->magic == MAGIC_NUMBER && "Currupted area");
        return next_area;
    }

    return NULL;
}

static inline LZFLHeader *is_next_area_free(LZFLHeader *header, LZFLHeader **out_next){
    LZFLHeader *next = area_next_to(header);

    if(next){
        if(out_next){
            *out_next = next;
        }

        return next->used ? NULL : next;
    }

    return NULL;
}

static inline void *alloc_area(LZFLHeader *area, LZFLAreaList *list){
    assert(!area->used && "Trying to alloc used memory");

    if(list){
        remove_area(area, list);
    }

    area->used = 1;
    area->region->used_bytes += calc_area_size(area);

    return area_chunk(area);
}

static inline void dealloc_area(LZFLHeader *area, LZFLAreaList *list){
    assert(area->used && "Trying to free unused memory");

    LZFLRegion *region = area->region;

    area->used = 0;
    region->used_bytes -= calc_area_size(area);

    insert_area_at_end(area, list);
}

static void insert_area_at_end(LZFLHeader *header, LZFLAreaList *list){
    header->prev = NULL;
    header->next = NULL;

    if(list->tail){
        list->tail->next = header;
        header->prev = list->tail;
    }else{
        list->head = header;
    }

    list->len++;
    list->tail = header;
}

static void remove_area(LZFLHeader *area, LZFLAreaList *list){
    if(area == list->head){
        list->head = area->next;
    }
    if(area == list->tail){
        list->tail = area->prev;
    }

    list->len--;

    if(area->prev){
        area->prev->next = area->next;
    }
    if(area->next){
        area->next->prev = area->prev;
    }

    area->prev = NULL;
    area->next = NULL;
}

static void replace_current_region(LZFLRegion *region, LZFList *list){
    list->current_region = region;
}

static int create_and_insert_region(size_t size, LZFList *list){
    LZFLRegion *region = create_region(size);

    if(region){
        insert_region(region, &list->regions);
        replace_current_region(region, list);

        return 0;
    }

    return 1;
}

static void *alloc_from_region(size_t size, LZFLRegion *region, LZFLHeader **out_area){
    LZFLHeader *area = create_area(size, region, NULL, NULL);

    if(!area){
        return NULL;
    }

    if(out_area){
        *out_area = area;
    }

    return alloc_area(area, NULL);
}

static void *look_first_fit(size_t size, LZFList *list, LZFLHeader **out_area){
    LZFLAreaList *free_areas = &list->free_areas;
    LZFLHeader *current_area = free_areas->head;
    LZFLHeader *next_area = NULL;

    while (current_area){
        next_area = current_area->next;

        if(current_area->size >= size){
            if(out_area){
                *out_area = current_area;
            }

            return alloc_area(current_area, free_areas);
        }

        current_area = next_area;
    }

    return NULL;
}
//--------------------------------------------------------------------------//
//                          PUBLIC IMPLEMENTATION                           //
//--------------------------------------------------------------------------//
LZFList *lzflist_create(LZFListAllocator *allocator){
    LZFList *list = MEMORY_ALLOC(LZFList, 1, allocator);

    if(!list){
        return NULL;
    }

    memset(list, 0, LIST_SIZE);

    return list;
}

void lzflist_destroy(LZFList *list){
    if(!list){
        return;
    }

    LZFLRegion *current = list->regions.head;
    LZFLRegion *next = NULL;

    while (current){
        next = current->next;
        destroy_region(current);
        current = next;
    }

    MEMORY_DEALLOC(list, LZFList, 1, list->allocator);
}

inline size_t lzflist_ptr_size(void *ptr){
    return chunk_area(ptr)->size;
}

int lzflist_prealloc(size_t size, LZFList *list){
    assert(size % PAGE_SIZE == 0);

    LZFLRegion *region = create_region(size);

    if(region){
        insert_region(region, &list->regions);
        replace_current_region(region, list);

        return 0;
    }

    return 1;
}

void *lzflist_alloc(size_t size, LZFList *list){
    void *ptr = look_first_fit(size, list, NULL);

    if(ptr){
        return ptr;
    }

    LZFLRegion *current_region = list->current_region;

    if(current_region && (ptr = alloc_from_region(size, current_region, NULL))){
        return ptr;
    }

    if(create_and_insert_region(size, list)){
        return NULL;
    }

    return alloc_from_region(size, list->current_region, NULL);
}

inline void *lzflist_calloc(size_t size, LZFList *list){
    void *ptr = lzflist_alloc(size, list);

    if(ptr){
        memset(ptr, 0, size);
    }

    return ptr;
}

void *lzflist_realloc(void *ptr, size_t new_size, LZFList *list){
    if(!ptr){
        return new_size == 0 ? NULL : lzflist_alloc(new_size, list);
    }

    if(new_size == 0){
        lzflist_dealloc(ptr, list);
        return NULL;
    }

    LZFLHeader *old_area = chunk_area(ptr);
    size_t old_size = old_area->size;

    if(new_size <= old_size){
        return ptr;
    }

    void *new_ptr = lzflist_alloc(new_size, list);

    if(new_ptr){
        memcpy(new_ptr, ptr, old_size);
        dealloc_area(old_area, &list->free_areas);
    }

    return new_ptr;
}

void lzflist_dealloc(void *ptr, LZFList *list){
    if(!ptr){
        return;
    }

    LZFLHeader *area = chunk_area(ptr);
    LZFLRegion *region = area->region;

    dealloc_area(area, &list->free_areas);

    if(region->used_bytes == 0){
        uintptr_t subregion = (uintptr_t)region->subregion;
        LZFLHeader *current_area = (LZFLHeader *)align_addr(LZFLIST_DEFAULT_ALIGNMENT, subregion);

        while(current_area){
            remove_area(current_area, &list->free_areas);
            current_area = area_next_to(current_area);
        }

        if(region == list->current_region){
            list->current_region = NULL;
        }

        remove_region(region, &list->regions);
        destroy_region(region);
    }
}