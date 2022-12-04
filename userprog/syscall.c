#include "userprog/syscall.h"
#include <stdio.h>
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

struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

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
	check_user((int *)f->esp, 0);
	int *args = (int *)f->esp;
	thread_current()->current_esp = f->esp;
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
			f->eax = exec((char *)args[1]);
			break;
		case SYS_WAIT:
			check_user(args, 1);
			f->eax = wait((pid_t)args[1]);
			break;
		case SYS_CREATE:
			check_user(args, 2);
			f->eax = create ((const char *)args[1], (unsigned)args[2]);
      		break;
		case SYS_REMOVE:
			check_user(args, 1);
			f->eax = remove ((const char *)args[1]);
			break;
		case SYS_OPEN:
			check_user(args, 1);
			lock_acquire(&syn_lock);
			f->eax = open ((const char *)args[1]);
			lock_release(&syn_lock);
			break;
		case SYS_FILESIZE:
			check_user(args, 1);
			f->eax = filesize (args[1]);
			break;
		case SYS_READ:
			check_user(args, 3);
			f->eax = read(args[1], (void *)args[2], (unsigned)args[3]);
			break;
		case SYS_WRITE:
			check_user(args, 3);
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
	for(int i=0;i<num+1;i++)
		if(!is_user_vaddr((void *)args[i])){
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
	lock_release(&syn_lock);
	return ret;
}

int write(int fd, const void *buffer, unsigned size)
{
	int ret=-1;
	lock_acquire(&syn_lock);
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