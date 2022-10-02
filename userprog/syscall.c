#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	check_user((int *)f->esp, 1);
	int *args = (int *)f->esp;
	//printf("syscall : %d\n", args[0]);
	//hex_dump((uintptr_t)args, args, 100, 1);
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
		case SYS_READ:
			check_user(args, 3);
			f->eax = read(args[1], (void *)args[2], (unsigned)args[3]);
			break;
		case SYS_WRITE:
			check_user(args, 3);
			f->eax = write(args[1], (void *)args[2], (unsigned)args[3]);
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

int read(int fd, void *buffer, unsigned size)
{
	if(fd == 0)
	{
		unsigned i;
		for(i = 0; i < size; i++)
		{
			((char *)buffer)[i] = input_getc();
		}
		((char *)buffer)[i] = '\0';
		return size;
	}
	return -1;
}

int write(int fd, const void *buffer, unsigned size)
{
	if(fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
	return -1;
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