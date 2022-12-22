#include "vm/frame.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

static void move_lru_clock(void)
{
    if (list_empty(&lru_list))
        lru_clock = NULL;
    else if (!lru_clock || list_end(&lru_list) == lru_clock || list_end(&lru_list) == list_next(lru_clock))
        lru_clock = list_begin(&lru_list);
    else
        lru_clock = list_next(lru_clock);
}

void lru_list_init(void)
{
    list_init(&lru_list);
    lock_init(&lru_list_lock);
    lru_clock = NULL;
}

void add_page_to_lru_list(struct page* page)
{
    list_push_back(&lru_list, &page->lru);
}

void del_page_from_lru_list(struct page* page)
{
    if(lru_clock == &page->lru)
        move_lru_clock();
    list_remove(&page->lru);
}

struct page* alloc_page(enum palloc_flags flags)
{
    lock_acquire(&lru_list_lock);
    
    uint8_t *kpage = palloc_get_page(flags);
    while (!kpage){
        try_to_free_pages();
        kpage = palloc_get_page(flags);
    }

    struct page *pg = malloc(sizeof(struct page));
    pg->thread = thread_current();
    pg->kaddr = kpage;
    add_page_to_lru_list(pg);

    lock_release(&lru_list_lock);
    return pg;
}

void free_page(void* kaddr)
{
    lock_acquire(&lru_list_lock);
    
    struct list_elem *e;
    struct page *target = NULL;

    for (e = list_begin(&lru_list); e != list_end(&lru_list); e = list_next(e)){
        struct page *pg = list_entry(e, struct page, lru);
        if(kaddr == pg->kaddr){
            target = pg;
            break;
        }
    }
    if(target)
        __free_page(target);
    lock_release(&lru_list_lock);
}

void __free_page(struct page* page)
{
    del_page_from_lru_list(page);
    pagedir_clear_page(page->thread->pagedir, pg_round_down(page->vme->vaddr));
    palloc_free_page(page->kaddr);
    free(page);
}

void try_to_free_pages(void)
{
    move_lru_clock();
    struct page *pg = list_entry(lru_clock, struct page, lru);
    while (pagedir_is_accessed(pg->thread->pagedir, pg->vme->vaddr) || pg->vme->pinned){
        pagedir_set_accessed(pg->thread->pagedir, pg->vme->vaddr, false);
        move_lru_clock();
        pg = list_entry(lru_clock, struct page, lru);
    }
    struct page *target = pg;
    switch(target->vme->type)
    {
        case VM_BIN:
            if(pagedir_is_dirty(target->thread->pagedir, target->vme->vaddr))
            {
                target->vme->swap_slot = swap_out(target->kaddr);
                target->vme->type = VM_ANON;
            }
            break;
        case VM_FILE:
            if(pagedir_is_dirty(target->thread->pagedir, target->vme->vaddr))
                file_write_at(target->vme->file, target->vme->vaddr, target->vme->read_bytes, target->vme->offset);
            break;
        case VM_ANON:
            target->vme->swap_slot = swap_out(target->kaddr);
            break;
    }
    target->vme->is_loaded = false;
    __free_page(target);
}