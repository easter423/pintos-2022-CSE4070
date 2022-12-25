#include <string.h>
#include <debug.h>
#include "filesys/filesys.h"
#include "filesys/buffer_cache.h"

#define NUM_CACHE 64

static struct buffer_cache_entry cache[NUM_CACHE];
static struct lock buffer_cache_lock;
static struct buffer_cache_entry *lru;
static char buffer_cache[NUM_CACHE * BLOCK_SECTOR_SIZE];

void buffer_cache_init(void)
{
    lock_init(&buffer_cache_lock);

    void *buffer = buffer_cache;
    for(struct buffer_cache_entry *entry = cache; entry < cache + NUM_CACHE; entry++){
        memset(entry, 0, sizeof(struct buffer_cache_entry));
        entry->valid_bit = false;
        entry->buffer = buffer;
        lock_init(&entry->entry_lock);
        buffer += BLOCK_SECTOR_SIZE;
    }

    lru = cache;
}

void buffer_cache_terminate(void)
{
    for(struct buffer_cache_entry *entry = cache; entry < cache + NUM_CACHE; entry++){
        lock_acquire(&entry->entry_lock);
        buffer_cache_flush_entry(entry);
        lock_release(&entry->entry_lock);
    }
}

void buffer_cache_read(block_sector_t sector, void *buffer, off_t offset, int chunk_size, int sector_ofs)
{
    struct buffer_cache_entry *entry = buffer_cache_lookup(sector);
    if(!entry){
        entry = buffer_cache_select_victim();

        buffer_cache_flush_entry(entry);
        entry->valid_bit = true;
        entry->dirty_bit = false;
        entry->disk_sector = sector;
        lock_release(&buffer_cache_lock);

        block_read(fs_device, sector, entry->buffer);
    }
    entry->reference_bit = true;
    memcpy(buffer + offset, entry->buffer + sector_ofs, chunk_size);
    // if(chunk_size!=512){
    //     printf("-------------\n");
    //     printf("(%d, %d)",chunk_size, sector_ofs);
    //     printf("{w_sector: %d, offset:%d, sector_ofs:%d}\n", sector, offset, sector_ofs);
    //     hex_dump(entry->buffer, entry->buffer, chunk_size+ sector_ofs, true);
    //     printf("-------------\n");
    // }

    lock_release(&entry->entry_lock);
}

void buffer_cache_write(block_sector_t sector, void *buffer, off_t offset, int chunk_size, int sector_ofs)
{
    struct buffer_cache_entry *entry = buffer_cache_lookup(sector);
    if(!entry){
        entry = buffer_cache_select_victim();

        buffer_cache_flush_entry(entry);
        entry->valid_bit = true;
        entry->disk_sector = sector;
        lock_release(&buffer_cache_lock);

        block_read(fs_device, sector, entry->buffer);
    }
    entry->reference_bit = true;
    entry->dirty_bit = true;
    //printf("-------------\n");
    //printf("{w_sector: %d, offset: %d}\n", sector, offset);
    //hex_dump(buffer, buffer, chunk_size, true);
    //printf("-------------\n");
    memcpy(entry->buffer + sector_ofs, buffer + offset, chunk_size);
    lock_release(&entry->entry_lock);
}

struct buffer_cache_entry *buffer_cache_lookup(block_sector_t sector)
{
    lock_acquire(&buffer_cache_lock);

    for(struct buffer_cache_entry *entry = cache; entry < cache + NUM_CACHE; entry++){
        if((entry->valid_bit == true) && (sector == entry->disk_sector)){
            lock_acquire(&entry->entry_lock);
            lock_release(&buffer_cache_lock);
            return entry;   // cache HIT!
        }
    }
    return NULL;            // cache MISS!
}

struct buffer_cache_entry *buffer_cache_select_victim(void)
{
    while(true){
        lock_acquire(&lru->entry_lock);
        if(!(lru->valid_bit && lru->reference_bit))
            return lru++;
        lru->reference_bit = false;
        lock_release(&lru->entry_lock);

        lru++;
        if(lru == cache + NUM_CACHE){
            lru = cache;
        }
    }
}

void buffer_cache_flush_entry(struct buffer_cache_entry *entry)
{
    if (entry->valid_bit && entry->dirty_bit){
        block_write(fs_device, entry->disk_sector, entry->buffer);
        entry->dirty_bit = false;
    }
}
