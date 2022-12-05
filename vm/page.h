#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2

struct vm_entry{
    uint8_t type;
    void *vaddr;
    bool writable;
    bool is_loaded;
    struct file* file;
    uint32_t offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;

    uint32_t swap_slot;

    struct list_elem mmap_elem;
    struct hash_elem elem;
};

void vm_init(struct hash *);
bool insert_vme(struct hash *, struct vm_entry *);
bool delete_vme(struct hash *, struct vm_entry *);
struct vm_entry *find_vme(void *);
void vm_destroy(struct hash *);
bool load_file(void *, struct vm_entry *);

#endif