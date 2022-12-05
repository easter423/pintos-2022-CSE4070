#include "vm/swap.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/vaddr.h"

#define BLOCK_PAGE PGSIZE / BLOCK_SECTOR_SIZE     // page에 사용되는 블럭 수

void swap_init(void)
{
    swap_bitmap = bitmap_create(8*1024);
}

void swap_in(size_t used_index, void* kaddr)
{
    struct block *swap_block = block_get_role(BLOCK_SWAP);
    if(bitmap_test(swap_bitmap, used_index))
    {
        for (int i = 0; i < BLOCK_PAGE; i++){
            block_read(swap_block, (uint32_t)(used_index * BLOCK_PAGE + i), (void *)(kaddr + i * BLOCK_SECTOR_SIZE));
        }
        bitmap_reset(swap_bitmap, used_index);
    }
}

size_t swap_out(void* kaddr)
{
    struct block *swap_block = block_get_role(BLOCK_SWAP);
    size_t first_fit = bitmap_scan(swap_bitmap, 0, 1, false);

    if (first_fit == BITMAP_ERROR)
        return BITMAP_ERROR;
    for (int i = 0; i < BLOCK_PAGE; i++){
            block_write(swap_block, (uint32_t)(first_fit * BLOCK_PAGE + i), (void *)(kaddr + i * BLOCK_SECTOR_SIZE));
        }
        bitmap_set(swap_bitmap, first_fit, true);

    return first_fit;
}