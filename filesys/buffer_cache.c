#include <string.h>
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/buffer_cache.h"

#define NUM_CACHE 64

static struct buffer_cache_entry cache[NUM_CACHE];
static struct lock buffer_cache_lock;

void buffer_cache_init(void)
{
    lock_init(&buffer_cache_lock);
    for(int i=0;i<NUM_CACHE;i++){
        cache[i].valid_bit = false;
    }
}

void buffer_cache_terminate(void)
{
    buffer_cache_flush_all();
}

void buffer_cache_read(block_sector_t sector, void *buffer, off_t offset, int chunk_size, int sector_ofs)
{
    lock_acquire(&buffer_cache_lock);

    struct buffer_cache_entry *entry = buffer_cache_lookup(sector);
    if(!entry){
        entry = buffer_cache_select_victim();
        block_read(fs_device, sector, entry->buffer);
        entry->valid_bit = true;
        entry->disk_sector = sector;
    }
    entry->reference_bit = true;
    memcpy(buffer + offset, &entry->buffer[sector_ofs], chunk_size);

    lock_release(&buffer_cache_lock);
}

void buffer_cache_write(block_sector_t sector, void *buffer, off_t offset, int chunk_size, int sector_ofs)
{
    lock_acquire(&buffer_cache_lock);
    
    struct buffer_cache_entry *entry = buffer_cache_lookup(sector);
    if(!entry){
        entry = buffer_cache_select_victim();
        block_read(fs_device, sector, entry->buffer);
        entry->valid_bit = true;
        entry->disk_sector = sector;
    }
    entry->reference_bit = true;
    entry->dirty_bit = true;
    memcpy(&entry->buffer[sector_ofs], buffer + offset, chunk_size);

    lock_release(&buffer_cache_lock);
}

struct buffer_cache_entry *buffer_cache_lookup(block_sector_t sector)
{
    for (int i=0;i<NUM_CACHE;i++){
        if(cache[i].valid_bit && sector == cache[i].disk_sector)
            return &cache[i];
    }
    return NULL;
}

struct buffer_cache_entry *buffer_cache_select_victim(void)
{
    static int buffer_cache_clk = 0;
    while(true){
        struct buffer_cache_entry *victim = &cache[buffer_cache_clk];
        if (!(victim->reference_bit && victim->valid_bit)){
            if(victim->dirty_bit){
                buffer_cache_flush_entry(victim);
            }
            return victim;
        }
        victim->reference_bit = false;
        buffer_cache_clk = (buffer_cache_clk+1)%NUM_CACHE;
    }
}

void buffer_cache_flush_entry(struct buffer_cache_entry *entry)
{
    if (entry->valid_bit && entry->dirty_bit){
        block_write(fs_device, entry->disk_sector, entry->buffer);
        entry->dirty_bit = false;
    }
}

void buffer_cache_flush_all(void)
{
    lock_acquire(&buffer_cache_lock);
    
    for(int i=0;i<NUM_CACHE;i++)
        buffer_cache_flush_entry(&cache[i]);
    
    lock_release(&buffer_cache_lock);
}
