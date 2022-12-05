#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/off_t.h"
#include "threads/synch.h"
#include "vm/page.h"

struct lock syn_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
	lock_init(&syn_lock);
  	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
	int *args = (int *)f->esp;
	check_address((void*) args, f->esp);
	switch(args[0])
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			check_user(args, 1);
			exit(args[1]);
			break;
		case SYS_EXEC:
			check_user(args, 1);
			check_valid_string((void *)args[1], f->esp);
			f->eax = exec((char *)args[1]);
			break;
		case SYS_WAIT:
			check_user(args, 1);
			f->eax = wait((pid_t)args[1]);
			break;
		case SYS_CREATE:
			check_user(args, 2);
			check_valid_string((void *)args[1], f->esp);
			f->eax = create ((char *)args[1], (unsigned)args[2]);
      		break;
		case SYS_REMOVE:
			check_user(args, 1);
			check_valid_string((void *)args[1], f->esp);
			f->eax = remove ((char *)args[1]);
			break;
		case SYS_OPEN:
			check_user(args, 1);
			check_valid_string((void *)args[1], f->esp);
			lock_acquire(&syn_lock);
			f->eax = open ((char *)args[1]);
			lock_release(&syn_lock);
			break;
		case SYS_FILESIZE:
			check_user(args, 1);
			f->eax = filesize (args[1]);
			break;
		case SYS_READ:
			check_user(args, 3);
			check_valid_strlen((void *)args[2], (unsigned)args[3], f->esp);
			f->eax = read(args[1], (void *)args[2], (unsigned)args[3]);
			break;
		case SYS_WRITE:
			check_user(args, 3);
			check_valid_strlen((void *)args[2], (unsigned)args[3], f->esp);
			f->eax = write(args[1], (void *)args[2], (unsigned)args[3]);
			break;
		case SYS_SEEK:
			check_user(args, 2);
			seek (args[1], (unsigned)args[2]);
      		break;
		case SYS_TELL:
			check_user(args, 1);
			f->eax = tell (args[1]);
			break;
		case SYS_CLOSE:
			check_user(args, 1);
			close (args[1]);
			break;
		case SYS_FIBO:
			check_user(args, 1);
			f->eax = fibonacci(args[1]);
			break;
		case SYS_MAX:
			check_user(args, 4);
			f->eax = max_of_four_int(args[1], args[2], args[3], args[4]);
			break;
	}

}

void check_user(int *args, int num)
{
	for(int i = 1; i < num + 1; i++)
		check_address((void *)&args[i], (void *)args);
}

struct vm_entry *check_address(void *addr, void *esp)
{
	//printf("[%p, %p]",addr, esp);
	if(addr < 0x8048000 || 0xc0000000 <= addr) {
		exit(-1);
	}
	struct vm_entry *vme = find_vme(addr);
	if (!vme){
		if (!verify_stack(addr, esp))
            exit(-1);
         expand_stack(addr);
		 vme = find_vme(addr);
	}
	return vme;
}

void check_valid_buffer (void *buffer, unsigned size, void *esp, bool to_write UNUSED)
{
	for(void *addr = pg_round_down(buffer); addr < buffer + size; addr += PGSIZE)
	{
		struct vm_entry *vme = check_address(addr, esp);
		if (!vme || !vme->writable)
			exit(-1);
	}
}

void check_valid_string (void *str, void *esp)
{
	struct vm_entry *vme = check_address(str, esp);
	if(!vme)
		exit(-1);
	int size = strlen(str);
	for(void *addr = pg_round_down(str); addr < str + size; addr += PGSIZE)
	{
		vme = check_address(addr, esp);
		if (!vme)
			exit(-1);
	}
}

void check_valid_strlen (void *str, unsigned size, void *esp)
{
	for (int i = 0; i < size; i++){
		struct vm_entry *vme = check_address(str+i, esp);
		if(!vme)
			exit(-1);
	}
}

void halt(void)
{
	shutdown_power_off();
}

void exit(int status)
{
	printf("%s: exit(%d)\n",thread_name(),status);
	thread_current()->exit_status = status;
	for (int i=3;i<131;i++)
	{
		if(thread_current()->fdt[i])
		{
			close(i);
		}
	}
	thread_exit();
}

pid_t exec(const char *cmd_line)
{
	return process_execute(cmd_line);
}

int wait(pid_t pid)
{
	return process_wait(pid);
}

bool create (const char *file, unsigned initial_size)
{
	if (!file)
	{
		exit(-1);
	}
	return filesys_create(file, initial_size);
}

bool remove (const char *file)
{
	if (!file)
	{
		exit(-1);
	}
	return filesys_remove(file);
}

int open (const char *file)
{
	if (!file)
	{
		exit(-1);
	}
	struct file *fs = filesys_open(file);
	if(fs)
	{
		int i=3;
		while (i<131)
		{
			if(!thread_current()->fdt[i]){
				int j=0;
				while (thread_name()[j]==file[j]){
					if (file[j]=='\0')
					{
						file_deny_write(fs);
						break;
					}
					j++;
				}
				thread_current()->fdt[i]=fs;
				return i;
			}
			i++;
		}
	}
	return -1;
}

int filesize (int fd)
{
	struct file *fs=thread_current()->fdt[fd];
	if (!fs)
	{
		exit(-1);
	}
	return file_length(fs);
}

int read(int fd, void *buffer, unsigned size)
{
	int ret=-1;
	lock_acquire(&syn_lock);
	pin_vme(buffer, size);
	if(fd == 0)
	{
		unsigned i;
		for(i = 0; i < size; i++)
		{
			((char *)buffer)[i] = input_getc();
		}
		((char *)buffer)[i] = '\0';
		ret = size;
	}
	else if(fd>2)
	{
		struct file *fs=thread_current()->fdt[fd];
		if (!fs)
		{
			exit(-1);
		}
		ret=file_read(fs, buffer, size);
	}
	unpin_vme(buffer, size);
	lock_release(&syn_lock);
	return ret;
}

int write(int fd, const void *buffer, unsigned size)
{
	int ret=-1;
	lock_acquire(&syn_lock);
	pin_vme(buffer, size);
	if(fd == 1)
	{
		putbuf(buffer, size);
		ret = size;
	}
	else if(fd>2){
		struct file *fs=thread_current()->fdt[fd];
		if (!fs)
		{
			exit(-1);
		}
		
		ret= file_write(fs, buffer, size);
	}
	unpin_vme(buffer, size);
	lock_release(&syn_lock);
	return ret;
}

void seek (int fd, unsigned position)
{
	struct file *fs=thread_current()->fdt[fd];
	if (!fs)
	{
		exit(-1);
	}
	file_seek(fs, position);
}

unsigned tell (int fd)
{
	struct file *fs=thread_current()->fdt[fd];
	if (!fs)
	{
		exit(-1);
	}
	return file_tell(fs);
}

void close (int fd)
{
	struct file *fs=thread_current()->fdt[fd];
	if (!fs)
	{
		exit(-1);
	}
	thread_current()->fdt[fd]=NULL;
	file_close(fs);
}

int fibonacci(int n)
{
	int temp1 = 0;
	int temp2 = 1;
	int temp = 1;
	for(int i=0;i<n-1;i++)
	{
		temp=temp1+temp2;
		temp1=temp2;
		temp2=temp;
	}
	return temp;
}

int max_of_four_int(int a, int b, int c, int d)
{
	if(b>a)
		a=b;
	if(c>a)
		a=c;
	if(d>a)
		a=d;
	return a;
}