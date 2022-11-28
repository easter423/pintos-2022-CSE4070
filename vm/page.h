#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <stdint.h>

#define VM_BIN 0
#define VM_SWAP 1

struct spt_entry{
    uint8_t mode;
    void *vaddr;    //virtual address
    uint32_t offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;

    bool writable;  //쓰기 가능 여부
    bool is_memory; //physical memory 탑재 여부

    uint32_t swap_idx;  //swap

    struct hash_elem elem;  //hash_elem
};

static unsigned hash_hash(const struct hash_elem *, void *);
static bool hash_less(const struct hash_elem *, const struct hash_elem *, void *);
static void hash_hash_destroy(struct hash_elem *, void *);
void spt_init(struct hash *);
bool pg_insert(struct hash *, struct spt_entry *);
bool pg_delete(struct hash *, struct spt_entry *);
struct spt_entry *find_page(struct hash *, void *);
void spt_destroy(struct hash *);

#endif