#include "heap.h"

static heap_block_t* heap_head = 0;
static uint32_t heap_start;
static bool kmalloc_initalized = false;
uint32_t heap_size;

void kmalloc_init(void){
    if (kmalloc_initalized) return;
    heap_start = KERNEL_MALLOC;
    heap_size = 0;

    change_heap_size(HEAP_THRESHOLD);
    kmalloc_initalized = true;

    heap_head = (heap_block_t*)heap_start;
    heap_head->magic = HEAP_MAGIC;
    heap_head->size = heap_size - sizeof(heap_block_t);
    heap_head->used = false;
    heap_head->next = 0;
    heap_head->prev = 0;
}

void change_heap_size(int new_heap_size){
    int old_page_top = CEIL_DIV(heap_size, 0x1000);
    int new_page_top = CEIL_DIV(new_heap_size, 0x1000);

    if (new_page_top > old_page_top){
        int diff = new_page_top - old_page_top;

        for (int i = 0; i < diff; i++){
            uint32_t phys = pmm_alloc_page_frame();
            if (phys == 0){
                print("No memory left for heap\n");
                kernel_panic();
            }
            
            mem_map_page(KERNEL_MALLOC + old_page_top * 0x1000 + i * 0x1000, phys, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT | PAGE_FLAG_OWNER);
        }
    }
    
    heap_size = new_page_top * 0x1000;
}

void *kmalloc(uint32_t size){
    if (!kmalloc_initalized) kmalloc_init();
    if (size == 0) return 0;
    
    size = (size + 7) & ~7;

    heap_block_t* current = heap_head;
    heap_block_t* prev = 0;

    while (current != 0){
        if (!current->used && current->size >= size){
            uint32_t remaning = current->size - size;

            if (remaning > sizeof(heap_block_t) + 5) {
                heap_block_t* new_block = (heap_block_t*)((uint32_t)current + sizeof(heap_block_t) + size);

                new_block->magic = HEAP_MAGIC;
                new_block->size = remaning - sizeof(heap_block_t);
                new_block->used = false;
                new_block->next = current->next;
                new_block->prev = current;

                if (current->next){
                    current->next->prev = new_block;
                }
                
                current->size = size;
                current->next = new_block;
            }
            
            current->used = true;
            return (void*)(current + 1);
        }
        
        prev = current;
        current = prev->next;
    }

    uint32_t total_needed = size + sizeof(heap_block_t);
    uint32_t total_chunks = CEIL_DIV(total_needed, HEAP_THRESHOLD);
    uint32_t current_chunks = CEIL_DIV(heap_size, HEAP_THRESHOLD);
    uint32_t chunks_needed = total_chunks - current_chunks;  

    uint32_t old_heap_size = heap_size;
    if (chunks_needed > 0){
        uint32_t expand_size = chunks_needed * HEAP_THRESHOLD;
        change_heap_size(heap_size + expand_size);
    }

    heap_block_t* new_block = (heap_block_t*)(heap_start + old_heap_size);
    new_block->magic = HEAP_MAGIC;
    new_block->size = size;
    new_block->used = true;
    new_block->next = 0;
    new_block->prev = prev;

    if (prev){
        prev->next = new_block;
    }

    return (void*)(new_block + 1);
}

void kfree(void* ptr){
    if (!kmalloc_initalized) kmalloc_init();

    if (!ptr) return;

    uint32_t addr = (uint32_t)ptr;
    if (addr < KERNEL_MALLOC || addr >= KERNEL_MALLOC + heap_size){
        print("kfree error: pointer outside heap\n");
        return;
    }

    heap_block_t* block = (heap_block_t*)ptr - 1;
    
    if (block->magic != HEAP_MAGIC){
        print("kfree error: a block magic is not equal 0xDEADBEEF\n");
        return;
    }
    
    if (!block->used){
        print("kfree error: freeing an unused block\n");
        return;
    }

    block->used = false;
    
    if (block->next && !block->next->used){
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next){
            block->next->prev = block;
        }
    }

    if (block->prev && !block->prev->used){
        block->prev->size += sizeof(heap_block_t) + block->size;
        block->prev->next = block->next;
        if (block->next){
            block->next->prev = block->prev;
        }
    }
}