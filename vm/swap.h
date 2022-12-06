#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include <list.h>
#include <bitmap.h>
#include <stddef.h>
#include <inttypes.h>
#include "devices/block.h"

struct bitmap *swap_bitmap;

void swap_init(void);
void swap_in(size_t, void*);
size_t swap_out(void*);

#endif