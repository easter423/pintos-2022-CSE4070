#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "lib/user/syscall.h"

void syscall_init(void);
void check_user(int *, int);
#endif /* userprog/syscall.h */
