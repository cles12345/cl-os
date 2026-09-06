#include "process.h"

static bool is_init_process = false;
static process_t* process_head;
static uint8_t pid_bitmap[MAX_PROCESSES / 8];
static process_t* current_process_running;

void init_process(void){
    process_head = kmalloc(sizeof(process_t));
    memset(process_head, NULL, sizeof(process_t));
    memset(pid_bitmap, NULL, MAX_PROCESSES / 8);
    process_head->state = PROC_IDLE;

    current_process_running = process_head;

    is_init_process = true;
}

static uint32_t allocate_pid(void){
    for (uint32_t i = 1; i < MAX_PROCESSES; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        
        if (!(pid_bitmap[byte] & (1 << bit))) {
            pid_bitmap[byte] |= (1 << bit);
            return i;
        }
    }
    
    return 0;
}

static void deallocate_pid(uint32_t pid){
    if (pid >= MAX_PROCESSES) return;
    
    uint32_t byte = pid / 8;
    uint32_t bit = pid % 8;
    
    pid_bitmap[byte] &= ~(1 << bit); 
}

process_t* create_process(const char* path){
    if (is_init_process == false) return NULL;

    fd_table_t* old_fd_table = vfs_fd_table;
    vfs_fd_table = kernel_fd_table;
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    uint32_t file_size = vfs_size(fd);

    uint8_t *elf_file = kmalloc(file_size);
    if (elf_file == NULL) {
        kernel_panic();
        return NULL;
    }
    uint32_t bytes_read = vfs_read(fd, elf_file, file_size);
    if (bytes_read != file_size) {
        kfree(elf_file);
        return NULL;
    }

    vfs_close(fd);
    vfs_fd_table = old_fd_table;

    process_t* process = kmalloc(sizeof(process_t));
    memset(process, NULL, sizeof(process_t));
    process->page_dir = vmm_new_page_dir();
    process->fd_table = vfs_fd_table_create();
    process->pid = allocate_pid();
    process->state = PROC_RUNNING;
    memcpy(process->name, path, PROC_NAME_MAX);
    
    if (elf_load(elf_file, &process->entry_point, process->page_dir) == 0){
        kfree(elf_file);

        process->regs.eip = process->entry_point;
        process->regs.csm = 0x1B;
        process->regs.eflags = 0x202;  

        process->regs.ds = 0x23;
        process->regs.ss = 0x23; 

        process->regs.useresp = STACK_ELF_START + 0x1000;
        process->regs.esp = process->regs.useresp;

        process->regs.cr2 = 0;
        process->regs.eax = 0;
        process->regs.ebx = 0;
        process->regs.ecx = 0;
        process->regs.edx = 0;
        process->regs.esi = 0;
        process->regs.edi = 0;
        process->regs.ebp = 0;

        process_t* current_process = process_head;
        while (current_process->next){
            current_process = current_process->next;
        }

        current_process->next = process;
        process->prev = current_process;
        return process;
    }
    else{
        kfree(elf_file);
        return NULL;
    }
}

intrupt_registers_t* schedule(intrupt_registers_t* regs){
    outb(0x20, 0x20);

    if (is_init_process == false) return regs;

    memcpy(&current_process_running->regs, regs, sizeof(intrupt_registers_t));

    process_t* start = current_process_running;
    
    do {
        if (current_process_running->next) {
            current_process_running = current_process_running->next;
        } else if (process_head->next) {
            current_process_running = process_head->next;
        } else {
            current_process_running = process_head;
            break;
        }
    } while (current_process_running->state != PROC_RUNNING && current_process_running != start);

    if (current_process_running->state == PROC_RUNNING) {
        mem_change_page_dir(current_process_running->page_dir);
        vfs_fd_table = current_process_running->fd_table;
        return &current_process_running->regs;
    }

    return regs;
}