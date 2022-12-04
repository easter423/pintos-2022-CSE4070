#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/swap.h"
#include <hash.h>
#include "filesys/off_t.h"

enum page_status {
  ALL_ZERO,         // All zeros
  ON_FRAME,         // Actively in memory
  ON_SWAP,           // Swapped (on swap slot)
  FROM_FILESYS      // from filesystem (or executable)
};

/**
 * Supplemental page table. The scope is per-process.
 */
struct supplemental_page_table
  {
    /* The hash table, page -> spte */
    struct hash page_map;
  };

struct supplemental_page_table_entry
  {
    void *upage;
    void *kpage;
    struct hash_elem elem;
    enum page_status status;
    swap_index_t swap_index;
    struct file *file;
    off_t file_offset;
    uint32_t read_bytes, zero_bytes;
    bool writable;
  };


/*
 * Methods for manipulating supplemental page tables.
 */

struct supplemental_page_table* vm_supt_create (void);
void vm_supt_destroy (struct supplemental_page_table *);
bool vm_supt_install_frame (struct supplemental_page_table *supt, void *upage, void *kpage);
bool vm_supt_install_zeropage (struct supplemental_page_table *supt, void *);
bool vm_supt_set_swap (struct supplemental_page_table *supt, void *, swap_index_t);
struct supplemental_page_table_entry* vm_supt_lookup (struct supplemental_page_table *supt, void *);
bool vm_supt_has_entry (struct supplemental_page_table *, void *page);
bool vm_load_page(struct supplemental_page_table *supt, uint32_t *pagedir, void *upage);
bool vm_supt_install_filesys (struct supplemental_page_table *supt, void *page,
struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

#endif