#pragma once

#include <stdint.h>

#define PAGE_DIRECTORY_ENTRY_AMOUNT 512

#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_WRITE (1 << 1)
#define PAGE_FLAG_USER_SUPERVISOR (1 << 2)
#define PAGE_FLAG_WRITE_TROUGH (1 << 3)
#define PAGE_FLAG_CACHE_DISABLED (1 << 4)
#define PAGE_FLAG_ACCESSED (1 << 5)
#define PAGE_FLAG_GLOBAL (1 << 6)
#define PAGE_FLAG_PAGE_SIZE (1 << 7)
#define PAGE_FLAG_DONT_OWN (1 << 9)

#define PAGE_DIRECTORY_GET_FLAG(entry, flag) (((entry) & (flag)) != 0)
#define PAGE_DIRECTORY_GET_ADDRESS(entry) ((void*)((entry) & 0x000FFFFFFFFFF000))

#define PAGE_DIRECTORY_GET_INDEX(address, level) \
    (((uint64_t)address & ((uint64_t)0x1FF << ((level - 1) * 9 + 12))) >> ((level - 1) * 9 + 12))

#define PAGE_DIRECTORY_LOAD(pageDirectory) asm volatile ("movq %0, %%cr3" : : "r" ((uint64_t)pageDirectory))

typedef uint64_t PageDirectoryEntry;

typedef struct
{ 
    PageDirectoryEntry entries[PAGE_DIRECTORY_ENTRY_AMOUNT];
} PageDirectory;

extern void page_directory_invalidate_page(void* virtualAddress);

PageDirectory* page_directory_new();

void page_directory_free(PageDirectory* pageDirectory);

void page_directory_copy_range(PageDirectory* dest, PageDirectory* src, uint64_t lowerIndex, uint64_t upperIndex);

void page_directory_populate_range(PageDirectory* pageDirectory, uint64_t lowerIndex, uint64_t upperIndex, uint64_t flags);

void page_directory_map_pages(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags);

void page_directory_map(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint16_t flags);

void page_directory_change_flags(PageDirectory* pageDirectory, void* virtualAddress, uint16_t flags);