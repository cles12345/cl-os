#include "memory.h"

extern uint32_t initial_page_dir[1024];
static uint32_t page_frame_min;
static uint32_t page_frame_max;
static uint32_t total_alloc;
static uint32_t mem_num_vpages;
static uint32_t page_dirs[NUM_PAGE_DIRS][1024] __attribute__((aligned(4096)));
static uint32_t page_dirs_used[NUM_PAGE_DIRS];

static uint8_t physical_memory_bitmap[NUM_PAGE_FRAMES / 8];

void init_memory(uint32_t mem_high, uint32_t physical_alloc_start){
    mem_num_vpages = 0;
    initial_page_dir[0] = 0;
    invalidate(0);
    initial_page_dir[1023] = ((uint32_t) initial_page_dir - KERNEL_START) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
    invalidate(0xFFFFF000);

    pmm_init(physical_alloc_start, mem_high);
    memset(page_dirs, 0, 0x1000 * NUM_PAGE_DIRS);
    memset(page_dirs_used, 0, NUM_PAGE_DIRS);

    memcpy(page_dirs[0], initial_page_dir, 4096);
    page_dirs_used[0] = 1;
}

void pmm_init(uint32_t mem_low, uint32_t mem_high){
    page_frame_min = CEIL_DIV(mem_low, 0X1000);
    page_frame_max = mem_high / 0x1000;
    total_alloc = 0;

    memset(physical_memory_bitmap, 0, sizeof(physical_memory_bitmap));
}

void invalidate(uint32_t vaddr){
    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

uint32_t pmm_alloc_page_frame(void){
    for (uint32_t frame = page_frame_min; frame < page_frame_max; frame++) {
        uint32_t b = frame / 8;
        uint32_t i = frame % 8;

        if (!(physical_memory_bitmap[b] & (1 << i))) {
            physical_memory_bitmap[b] |= (1 << i);
            total_alloc++;
            
            return frame * 0x1000;
        }
    }

    return 0;
}

void pmm_free_page_frame(uint32_t addr){
    uint32_t frame_index = addr / 0x1000;
    if (frame_index < page_frame_min || frame_index > page_frame_max){
        return;
    }
    uint32_t byte_index = frame_index / 8;
    uint32_t bit_index = frame_index % 8;
    uint8_t byte = physical_memory_bitmap[byte_index];
    bool used = byte >> bit_index & 1;
    if (used){
        total_alloc--;
        physical_memory_bitmap[byte_index] &= ~(1 << bit_index);
    }    
}

uint32_t* mem_get_current_page_dir(void){
    uint32_t pd = 0;
    asm volatile("mov %%cr3, %0" : "=r"(pd));
    pd += KERNEL_START;

    return (uint32_t*)pd;
}

void mem_change_page_dir(uint32_t* pd){
    pd = (uint32_t*) (((uint32_t)pd)-KERNEL_START);
    asm volatile("mov %0, %%eax; mov %%eax, %%cr3" : : "r"(pd) : "eax");
}

void sync_page_dirs(void){
    for (int i = 0; i < NUM_PAGE_DIRS; i++){
        if(page_dirs_used[i]){
            uint32_t* page_dir = page_dirs[i];

            for (int x = 786; x < 1024; x++){
                page_dir[x] = initial_page_dir[x] & ~PAGE_FLAG_OWNER;
            }
        }
    }
}

void mem_map_page(uint32_t virt_addr, uint32_t phy_addr, uint32_t flags){
    uint32_t *prev_page_dir = 0;

    if (virt_addr >= KERNEL_START){
        prev_page_dir = mem_get_current_page_dir();
        if(prev_page_dir != initial_page_dir){
            mem_change_page_dir(initial_page_dir);
        }
    }
    
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;

    uint32_t* page_dir = REC_PAGE_DIR;
    uint32_t* pt = REC_PAGE_TABLE(pd_index);

    uint32_t pde_flags = flags & (PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_USER | PAGE_FLAG_WRITETHROUGH | PAGE_FLAG_CACHE_DISABLE | PAGE_FLAG_ACCESSED | PAGE_FLAG_DIRTY | PAGE_FLAG_PAT | PAGE_FLAG_GLOBAL | PAGE_FLAG_OWNER);    
    if(!(page_dir[pd_index] & PAGE_FLAG_PRESENT)){
        uint32_t pt_padrr = pmm_alloc_page_frame();
        page_dir[pd_index] = pt_padrr | pde_flags | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;

        invalidate((uint32_t)pt);
        
        for (uint32_t i = 0; i < 1024; i++){
            pt[i] = 0;
        }    
    }
    pt[pt_index] = phy_addr | pde_flags;
    mem_num_vpages++;
    invalidate(virt_addr);

    if (prev_page_dir != 0 && prev_page_dir != initial_page_dir){
        sync_page_dirs();
        mem_change_page_dir(prev_page_dir);
    }
}

void mem_unmap_page(uint32_t virt_addr){
    uint32_t *prev_page_dir = 0;

    if (virt_addr >= KERNEL_START){
        prev_page_dir = mem_get_current_page_dir();
        if(prev_page_dir != initial_page_dir){
            mem_change_page_dir(initial_page_dir);
        }
    }
    
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;

    uint32_t* page_dir = REC_PAGE_DIR;

    if(!(page_dir[pd_index] & PAGE_FLAG_PRESENT)){
        if (prev_page_dir != 0 && prev_page_dir != initial_page_dir){
            mem_change_page_dir(prev_page_dir);
        }
        return;
    }

    uint32_t* pt = REC_PAGE_TABLE(pd_index);

    if(!(pt[pt_index] & PAGE_FLAG_PRESENT)){
        if (prev_page_dir != 0 && prev_page_dir != initial_page_dir){
            mem_change_page_dir(prev_page_dir);
        }
        return;
    }

    uint32_t phys_addr = pt[pt_index] & PAGE_FLAG_FRAME;
    if (phys_addr != 0){
        pmm_free_page_frame(phys_addr);
    }

    
    pt[pt_index] = 0;
    mem_num_vpages--;
    invalidate(virt_addr);
    

    bool pt_empty = true;
    for (uint32_t i = 0; i < 1024; i++){
        if (pt[i] & PAGE_FLAG_PRESENT){
            pt_empty = false;
            break;
        }
    }

    if (pt_empty && virt_addr < KERNEL_START){
        uint32_t pt_paddr = page_dir[pd_index] & PAGE_FLAG_FRAME;
        page_dir[pd_index] = 0;
        pmm_free_page_frame(pt_paddr);
        invalidate((uint32_t)pt);
    }
    

    if (prev_page_dir != 0 && prev_page_dir != initial_page_dir){
        sync_page_dirs();
        mem_change_page_dir(prev_page_dir);
    }
}

uint32_t* vmm_new_page_dir(void){
    for (int i = 0; i < NUM_PAGE_DIRS; i++){
        if (!page_dirs_used[i]){
            page_dirs_used[i] = 1;
            uint32_t *pd = page_dirs[i];

            for (int x = 0; x < 1024; x++){
                pd[x] = 0;
            }
            
            sync_page_dirs();

            return pd;
        }
    }
    return 0;
}

void vmm_free_page_dir(uint32_t* pd){
    if (pd == initial_page_dir){
        return;
    }
    
    for (int i = 0; i < NUM_PAGE_DIRS; i++){
        if (page_dirs[i] == pd){
            uint32_t* old_pd = mem_get_current_page_dir();
            mem_change_page_dir(pd);

            for (int x = 0; x < 786; x++){
                if (pd[x] & PAGE_FLAG_PRESENT){
                    uint32_t* pt = REC_PAGE_TABLE(x);

                    for (int y = 0; y < 1024; y++){
                        if (pt[y] & PAGE_FLAG_PRESENT){
                            uint32_t phys = pt[y] & ~0xFFF;
                            pmm_free_page_frame(phys);
                        }
                    }

                    uint32_t pt_paddr = pd[x] & ~0xFFF;
                    pmm_free_page_frame(pt_paddr);
                }
            }

            mem_change_page_dir(old_pd);

            uint32_t pd_paddr = ((uint32_t)pd) - KERNEL_START;
            pmm_free_page_frame(pd_paddr);

            page_dirs_used[i] = 0;
            return;
        }
    }
}