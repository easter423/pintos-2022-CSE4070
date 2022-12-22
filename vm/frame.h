#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/palloc.h"
#include "threads/synch.h"

struct list lru_list;
struct lock lru_list_lock;
struct list_elem *lru_clock;

void lru_list_init(void);
void add_page_to_lru_list(struct page*);
void del_page_from_lru_list(struct page*);

struct page* alloc_page(enum palloc_flags);
void free_page(void*);
void __free_page(struct page*);

void try_to_free_pages(void);
#endif