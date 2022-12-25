#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/buffer_cache.h"
#include "threads/synch.h"
#include "filesys/off_t.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define NUM_DIRECT 124
#define NUM_INDIRECT 128

enum direct_mode{
  DIRECT,
  INDIRECT,
  DOUBLE_INDIRECT,
  OUT_OF_RANGE
};

struct location{
  enum direct_mode directness;
  off_t direct_idx;
  off_t indirect_idx;
  off_t double_indirect_idx;
};

struct inode_indirect_block{
  block_sector_t direct_block[NUM_INDIRECT];
};

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    block_sector_t direct_block[NUM_DIRECT];
    block_sector_t indirect_index_block;
    block_sector_t double_indirect_index_block;
    uint32_t is_dir;
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock inode_lock;             /* lock */
    //struct inode_disk data;             /* Inode content. */
  };

static bool get_disk_inode(const struct inode *, struct inode_disk*);
static void locate_byte (off_t, struct location *);
static bool register_sector(struct inode_disk *, block_sector_t, struct location);
static block_sector_t byte_to_sector(const struct inode_disk *, off_t);
static bool inode_update_file_length(struct inode_disk*, off_t, off_t);
static void free_inode_sectors(struct inode_disk*);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
// static block_sector_t
// byte_to_sector (const struct inode *inode, off_t pos) 
// {
//   ASSERT (inode != NULL);
//   if (pos < inode->data.length)
//     return inode->data.start + pos / BLOCK_SECTOR_SIZE;
//   else
//     return -1;
// }

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, uint32_t is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL){
      //size_t sectors = bytes_to_sectors (length);
      memset(disk_inode, -1, sizeof(struct inode_disk));
      disk_inode->length = 0;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;
      if(!inode_update_file_length(disk_inode, disk_inode->length, length)){
        free(disk_inode); return success;
      }
      buffer_cache_write(sector, disk_inode, (off_t)0, BLOCK_SECTOR_SIZE, 0);
      // if (free_map_allocate (sectors, &disk_inode->start)) 
      //   {
      //     block_write (fs_device, sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         static char zeros[BLOCK_SECTOR_SIZE];
      //         size_t i;
              
      //         for (i = 0; i < sectors; i++) 
      //           block_write (fs_device, disk_inode->start + i, zeros);
      //       }
      //     success = true; 
      //   } 
      free (disk_inode);
      success = true;
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  //block_read (fs_device, inode->sector, &inode->data);
  lock_init(&inode->inode_lock);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          struct inode_disk inode_disk;
          //get_disk_inode(inode, &inode_disk);
          buffer_cache_read(inode->sector, &inode_disk, (off_t)0, BLOCK_SECTOR_SIZE, 0);
          free_inode_sectors(&inode_disk);
          free_map_release (inode->sector, 1);
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  //uint8_t *bounce = NULL;
  struct inode_disk inode_disk;

  lock_acquire(&inode->inode_lock);
  get_disk_inode(inode, &inode_disk);

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (&inode_disk, offset);
      if(sector_idx == (block_sector_t)(-1)) break;

      lock_release(&inode->inode_lock);

      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length(inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0){
        lock_acquire(&inode->inode_lock);
        break;
      }

      buffer_cache_read(sector_idx, buffer, bytes_read, chunk_size, sector_ofs);
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Read full sector directly into caller's buffer. */
      //     block_read (fs_device, sector_idx, buffer + bytes_read);
      //   }
      // else 
      //   {
      //     /* Read sector into bounce buffer, then partially copy
      //        into caller's buffer. */
      //     if (bounce == NULL) 
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //     block_read (fs_device, sector_idx, bounce);
      //     memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      //   }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
      lock_acquire(&inode->inode_lock);
    }
  //free (bounce);
  lock_release(&inode->inode_lock);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  //uint8_t *bounce = NULL;
  struct inode_disk inode_disk;

  if (inode->deny_write_cnt)
    return 0;

  lock_acquire(&inode->inode_lock);
  get_disk_inode(inode, &inode_disk);

  if(size + offset > inode_disk.length){
    inode_update_file_length(&inode_disk, inode_length(inode), size + offset);
    buffer_cache_write(inode->sector, &inode_disk, (off_t)0, BLOCK_SECTOR_SIZE, 0);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (&inode_disk, offset);
      lock_release(&inode->inode_lock);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_disk.length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0){
        lock_acquire(&inode->inode_lock);
        break;
      }

      buffer_cache_write(sector_idx, (void*)buffer, bytes_written, chunk_size, sector_ofs);
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Write full sector directly to disk. */
      //     block_write (fs_device, sector_idx, buffer + bytes_written);
      //   }
      // else 
      //   {
      //     /* We need a bounce buffer. */
      //     if (bounce == NULL) 
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }

      //     /* If the sector contains data before or after the chunk
      //        we're writing, then we need to read in the sector
      //        first.  Otherwise we start with a sector of all zeros. */
      //     if (sector_ofs > 0 || chunk_size < sector_left) 
      //       block_read (fs_device, sector_idx, bounce);
      //     else
      //       memset (bounce, 0, BLOCK_SECTOR_SIZE);
      //     memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      //     block_write (fs_device, sector_idx, bounce);
      //   }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
      lock_acquire(&inode->inode_lock);
    }
  //free (bounce);
  lock_release(&inode->inode_lock);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk inode_disk;
  buffer_cache_read(inode->sector, &inode_disk, (off_t)0, BLOCK_SECTOR_SIZE, 0);
  return inode_disk.length;
}

static bool get_disk_inode(const struct inode *inode, struct inode_disk *inode_disk)
{
  buffer_cache_read(inode->sector, inode_disk, (off_t)0, sizeof(struct inode_disk), 0);
  return true;
}

static void locate_byte (off_t pos, struct location *loc)
{
  pos /= BLOCK_SECTOR_SIZE;
  if(pos < NUM_DIRECT){
    loc->directness = DIRECT;
    loc->direct_idx = pos;
  }
  else if(pos < NUM_DIRECT + NUM_INDIRECT){
    loc->directness = INDIRECT;
    loc->indirect_idx = pos - NUM_DIRECT;
  }
  else if(pos < NUM_DIRECT + NUM_INDIRECT + NUM_INDIRECT*NUM_INDIRECT){
    pos -= (NUM_DIRECT + NUM_INDIRECT);
    loc->directness = DOUBLE_INDIRECT;
    loc->indirect_idx = pos % NUM_INDIRECT;
    loc->double_indirect_idx = pos / NUM_INDIRECT; 
  }
  else{
    loc->directness = OUT_OF_RANGE;
  }
}

static bool register_sector(struct inode_disk *inode_disk, block_sector_t sector, struct location loc)
{
  struct inode_indirect_block first, second;
  bool flag = false;
  switch (loc.directness)
  {
    case DIRECT:
      inode_disk->direct_block[loc.direct_idx] = sector;
      return true;

    case INDIRECT:
      if(inode_disk->indirect_index_block == (block_sector_t)(-1)){
        if(!free_map_allocate(1, &inode_disk->indirect_index_block)) return false;
        memset(&first, -1, sizeof(struct inode_indirect_block));
      }
      else buffer_cache_read(inode_disk->indirect_index_block, &first, (off_t)0, sizeof(struct inode_indirect_block), 0);
      if(first.direct_block[loc.indirect_idx] == (block_sector_t)(-1)) first.direct_block[loc.indirect_idx] = sector;
      else return false;
      buffer_cache_write(inode_disk->indirect_index_block, &first, (off_t)0, sizeof(struct inode_indirect_block), 0);
      return true;

    case DOUBLE_INDIRECT:
      if(inode_disk->double_indirect_index_block == (block_sector_t)(-1)){
        if(!free_map_allocate(1, &inode_disk->double_indirect_index_block)) return false;
        memset(&first, -1, sizeof(struct inode_indirect_block));
      }
      else buffer_cache_read(inode_disk->double_indirect_index_block, &first, (off_t)0, sizeof(struct inode_indirect_block), 0);

      if(first.direct_block[loc.double_indirect_idx] == (block_sector_t)(-1)){
        flag = true;
        if(!free_map_allocate(1, &first.direct_block[loc.double_indirect_idx])) return false;
        memset(&second, -1, sizeof(struct inode_indirect_block));
      }
      else buffer_cache_read(first.direct_block[loc.double_indirect_idx], &second, (off_t)0, sizeof(struct inode_indirect_block), 0);
      if(second.direct_block[loc.indirect_idx] == (block_sector_t)(-1)) second.direct_block[loc.indirect_idx] = sector;
      else return false;
      if(flag){
        buffer_cache_write(inode_disk->double_indirect_index_block, &first, (off_t)0, sizeof(struct inode_indirect_block), 0);
      }
      buffer_cache_write(first.direct_block[loc.double_indirect_idx], &second, (off_t)0, sizeof(struct inode_indirect_block), 0);
      return true;
      
    default:
      return false;
  }
}

static block_sector_t byte_to_sector(const struct inode_disk *inode_disk, off_t pos)
{
  if (inode_disk->length <= pos) return -1;

  struct inode_indirect_block indirect_block;
  struct location loc;
  locate_byte(pos, &loc);

  switch(loc.directness)
  {
    case DIRECT:
      return inode_disk->direct_block[loc.direct_idx];

    case INDIRECT:
      if(inode_disk->indirect_index_block == (block_sector_t)(-1)) return -1;
      buffer_cache_read(inode_disk->indirect_index_block, (void*)&indirect_block, (off_t)0, sizeof(struct inode_indirect_block), 0);
      return indirect_block.direct_block[loc.indirect_idx];

    case DOUBLE_INDIRECT:
      if(inode_disk->double_indirect_index_block == (block_sector_t)(-1)) return -1;
      buffer_cache_read(inode_disk->double_indirect_index_block, (void*)&indirect_block, (off_t)0, sizeof(struct inode_indirect_block), 0);
      if(indirect_block.direct_block[loc.double_indirect_idx] == (block_sector_t)(-1)) return -1;
      buffer_cache_read(indirect_block.direct_block[loc.double_indirect_idx], (void*)&indirect_block, (off_t)0, sizeof(struct inode_indirect_block), 0);
      return indirect_block.direct_block[loc.indirect_idx];

    default:
      return -1;
  }
}

static bool inode_update_file_length(struct inode_disk* inode_disk, off_t old_length, off_t new_length)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  if(old_length > new_length) return false;   //invalid
  else if(old_length == new_length) return true;   //nothing to do

  inode_disk->length = new_length;
  old_length = old_length / BLOCK_SECTOR_SIZE * BLOCK_SECTOR_SIZE;
  new_length = (new_length-1) / BLOCK_SECTOR_SIZE * BLOCK_SECTOR_SIZE;

  for(off_t length = old_length; length <= new_length; length += BLOCK_SECTOR_SIZE){
    struct location loc;
    block_sector_t sector = byte_to_sector (inode_disk, length);

    if(sector != (block_sector_t)(-1)) continue;
    if(!free_map_allocate(1, &sector)) return false;
    locate_byte(length, &loc);
    if(!register_sector(inode_disk, sector, loc)) return false;
    buffer_cache_write(sector, zeros, (off_t)0, BLOCK_SECTOR_SIZE, 0);
  }
  return true;
}

static void free_inode_sectors(struct inode_disk *inode_disk)
{
  struct inode_indirect_block first, second;

  //direct free
  for(int i=0;i<NUM_DIRECT;i++){
    if(inode_disk->direct_block[i] == (block_sector_t)(-1)) return;
    free_map_release(inode_disk->direct_block[i], 1);
  }

  //indirect free
  if(inode_disk->indirect_index_block == (block_sector_t)(-1)) return;
  buffer_cache_read(inode_disk->indirect_index_block, &first, (off_t)0, sizeof(struct inode_indirect_block), 0);
  for(int i=0;i<NUM_INDIRECT;i++){
    if(first.direct_block[i] == (block_sector_t)(-1)) return;
    free_map_release(first.direct_block[i], 1);
  }
  free_map_release(inode_disk->indirect_index_block, 1);

  //double_indirect free
  if(inode_disk->double_indirect_index_block == (block_sector_t)(-1)) return;
  buffer_cache_read(inode_disk->double_indirect_index_block, &first, (off_t)0, sizeof(struct inode_indirect_block), 0);
  for(int i=0;i<NUM_DIRECT;i++){
    if(first.direct_block[i] == (block_sector_t)(-1)) return;

    buffer_cache_read(first.direct_block[i], &second, (off_t)0, sizeof(struct inode_indirect_block), 0);
    for(int j=0;j<NUM_INDIRECT;j++){
      if(second.direct_block[j] == (block_sector_t)(-1)) return;
      free_map_release(second.direct_block[j], 1);
    }

    free_map_release(first.direct_block[i], 1);
  }
  free_map_release(inode_disk->double_indirect_index_block, 1);
}


bool inode_is_dir(const struct inode *inode)
{
  bool result = false;
  struct inode_disk inode_disk;
  if(!inode->removed && get_disk_inode(inode, &inode_disk)){
    result = inode_disk.is_dir;
  }
  return result;
}