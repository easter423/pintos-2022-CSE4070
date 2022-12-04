#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "vm/page.h"
#include "lib/user/syscall.h"

void syscall_init(void);
void check_user(int *, int);
void check_valid_string (const void *, void *);
struct vm_entry *check_address(void *, void *);
void check_valid_buffer (void *, unsigned, void *, bool);
#endif /* userprog/syscall.h */
