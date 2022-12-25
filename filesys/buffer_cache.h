#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"

struct buffer_cache_entry{
    bool valid_bit;
    bool reference_bit;
    bool dirty_bit;
    block_sector_t disk_sector;
    void *buffer;
    struct lock entry_lock;
};

void buffer_cache_init(void);
void buffer_cache_terminate(void);
void buffer_cache_read(block_sector_t, void*, off_t, int, int);
void buffer_cache_write(block_sector_t, void*, off_t, int, int);
struct buffer_cache_entry *buffer_cache_lookup(block_sector_t);
struct buffer_cache_entry *buffer_cache_select_victim(void);
void buffer_cache_flush_entry(struct buffer_cache_entry*);

#endif
