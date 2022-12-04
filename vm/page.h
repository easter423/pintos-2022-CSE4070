#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2

struct vm_entry{
    uint8_t type;
    void *vaddr;
    bool writable;

    bool is_loaded;
    struct file* file;

    struct list_elem mmap_elem;

    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    size_t swap_slot;

    struct hash_elem elem;
};

void vm_init(struct hash *);
static unsigned vm_hash_func (const struct hash_elem *, void * UNUSED);
static bool vm_less_func (const struct hash_elem *, const struct hash_elem *, void * UNUSED);
bool insert_vme(struct hash *, struct vm_entry *);
bool delete_vme(struct hash *, struct vm_entry *);
struct vm_entry *find_vme(void *);
static void vm_destroy_func (struct hash_elem *, void * UNUSED);
void vm_destroy(struct hash *);

#endif