#include "page.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/pte.h"

static unsigned hash_hash(const struct hash_elem *e, void *aux UNUSED){
    uint32_t va = (uint32_t)hash_entry(e, struct spt_entry, elem)->vaddr;
    return hash_int(va);
}

static bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
    uint32_t vaa = (uint32_t)hash_entry(a, struct spt_entry, elem)->vaddr;
    uint32_t vab = (uint32_t)hash_entry(b, struct spt_entry, elem)->vaddr;
    return vaa < vab;
}

static void hash_hash_destroy(struct hash_elem *e, void *aux UNUSED){
    struct spt_entry *page = hash_entry(e, struct page, elem);
    free(page);
}

void spt_init(struct hash *spt){
    hash_init(&spt, &hash_hash, &hash_less, NULL);
}

bool pg_insert(struct hash *spage, struct spt_entry *e){
    return hash_insert(spage, &e->elem) == NULL ? true : false;
}

bool pg_delete(struct hash *spage, struct spt_entry *e){
    return hash_delete(spage, &e->elem) != NULL ? true : false;
}

struct spt_entry *find_page(struct hash *spage, void *vaddr){
    struct spt_entry page;
    struct hash_elem *e;

    page.vaddr = pg_round_down(vaddr);
    e = hash_find(spage, &page.elem);
    return e != NULL ? hash_entry(e, struct spt_entry, elem) : NULL;
}

void spt_destroy(struct hash *spage){
    hash_destroy(spage, hash_hash_destroy);
}

bool handle_mm_fault(struct spt_entry *e){

}