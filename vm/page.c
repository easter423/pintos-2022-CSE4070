#include <string.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

static unsigned vm_hash_func (const struct hash_elem *e_, void *aux UNUSED)
{
    struct vm_entry *e = hash_entry(e_, struct vm_entry, elem);
    return hash_int((unsigned)(e->vaddr));
}

static bool vm_less_func (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
    struct vm_entry *a = hash_entry(a_, struct vm_entry, elem);
    struct vm_entry *b = hash_entry(b_, struct vm_entry, elem);
    return a->vaddr < b->vaddr;
}

static void vm_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
	struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
    free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
	free(vme);
}

void vm_init (struct hash *vm)
{
    hash_init(vm, vm_hash_func, vm_less_func, NULL);
}

bool insert_vme (struct hash *vm, struct vm_entry *vme)
{
    return (hash_insert(vm, &vme->elem) == NULL);
}

bool delete_vme (struct hash *vm, struct vm_entry *vme)
{
    struct hash_elem *elem = hash_delete(vm, &vme->elem);
    if (elem != NULL){
        free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
        free(vme);
        return true;
    }
    return false;
}

struct vm_entry *find_vme (void *vaddr)
{
    struct vm_entry f;

    f.vaddr = pg_round_down(vaddr);
    struct hash_elem *e = hash_find(&thread_current()->vm, &f.elem);

    return (e != NULL) ? hash_entry(e, struct vm_entry, elem) : NULL;
}

void vm_destroy (struct hash *vm)
{
	hash_destroy (vm, vm_destroy_func);
}

bool load_file(void *kaddr, struct vm_entry *vme)
{
	if (file_read_at (vme->file, kaddr, vme->read_bytes, vme->offset) != (int) vme->read_bytes)
      return false;
	memset (kaddr + vme->read_bytes, 0, vme->zero_bytes);
	return true;
}

void pin_vme (void *front, int size)
{
	for(void *addr = front; addr < front + size; addr += PGSIZE)
	{
		struct vm_entry *vme = find_vme(addr);
		vme->pinned = true;
		if(!vme->is_loaded)
			handle_mm_fault(vme);
	}
}

void unpin_vme (void *front, int size)
{
	for(void *addr = front; addr < front + size; addr += PGSIZE)
	{
		struct vm_entry *vme = find_vme(addr);
		vme->pinned = false;
	}
}