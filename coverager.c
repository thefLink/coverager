#include <unistd.h>
#include <sys/types.h>
#include <wait.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "uthash.h"

void br_mapping(FILE *);

struct br_struct{
	unsigned br_address;	
	int orig_byte;
	UT_hash_handle hh;
};

struct br_struct *breakpoints = NULL;
struct br_struct *br_struct_by_addr(long);

int 
main(int argc, char* argv[])
{

  if(argc < 4)
  {
    printf("Usage: ./coverager <patched_binary> <argument> <descriptor_file>\n");
    return 1;
  }

  char *bin_name = argv[1];
  char *argument = argv[2];
  char *descriptor_file = argv[3];

  printf("[*] Coverage\n");
  FILE *fp = fopen(descriptor_file, "r");
  if( fp == NULL )
  {
    exit(EXIT_FAILURE);
  }

  printf("[*] Parsing description ... \n");
  br_mapping(fp);
  printf("[*] Now starting the program analysis\n");

  pid_t child_pid;
  child_pid = fork();

  if(!child_pid)
  {
    /* Child process */	
    printf("[*] Childprocess calls execlp!\n");
    ptrace(PTRACE_TRACEME, 0, 0, 0, 0);
    execlp(bin_name, "traced_and_patched", argument, NULL);
    /* Never reached */
  }
  else
  {
    int status;
    int bbs_hit;
    struct user_regs_struct regs;

    while ( 1 )
    {

	wait(&status);
	if(WIFEXITED(status))
	{
		 printf("[!] Child exited, aborting ... \n");
		 printf("[!] Hit: %d\n", bbs_hit);
		 break;

	}else if (WIFSTOPPED(status)) 
	{

		 if(!strcmp(strsignal(WSTOPSIG(status)), "Segmentation fault"))
		 {
			 printf("Child got a signal: %s\n", strsignal(WSTOPSIG(status)));
			 printf("segfault\n");

			 ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
			 printf("[+] RIP: %lx\n", regs.rip);
			 break;
		 }

		 ptrace(PTRACE_GETREGS, child_pid, 0, &regs);

		 int lsb = 0x00000000000FFFFF & (regs.rip - 1);
		 struct br_struct *result =  br_struct_by_addr(lsb);
		 if( result )
		 {
		//	 printf("Child got a signal: %s\n", strsignal(WSTOPSIG(status)));

			 void *br_position = (void *) regs.rip - 1;
			 unsigned readback_data1 = ptrace(PTRACE_PEEKTEXT, child_pid, br_position + 2, 0);

			 long text_patched = ptrace(PTRACE_PEEKTEXT, child_pid, br_position, 0);
			 long text_restored = (text_patched & ~0xff) | result->orig_byte;

			 ptrace(PTRACE_POKETEXT, child_pid, (void *)br_position, text_restored);

			 unsigned readback_data2 = ptrace(PTRACE_PEEKTEXT, child_pid, br_position + 2, 0);

		//	 printf("[+] RIP: %lx\n", regs.rip);
		//	 printf("[+] br at: %lx\n", br_position);
		//	 printf("[+] Patched bytes:  0x%08x\n", text_patched); 
		//	 printf("[+] restored bytes: 0x%08x\n", text_restored); 
		//	 printf("[+] readback1 bytes: 0x%08x\n", readback_data1); 
		//	 printf("[+] readback2 bytes: 0x%08x\n", readback_data2); 

			 bbs_hit++;
			 regs.rip -= 1;
			 ptrace(PTRACE_SETREGS, child_pid, 0, &regs);
		 }else
		 {
			printf("[!] Missed\n");
		 }

		ptrace(PTRACE_CONT, child_pid, 0, 0);
	}
    }
}
}

struct 
br_struct *br_struct_by_addr(long addr)
{
  struct br_struct *f_struct;
  HASH_FIND_INT( breakpoints, &addr, f_struct );

  return f_struct;
}

/*
* Creates a hashtable that maps breakpoints to the original
* bytes that were replaced with cc.
* */
void
br_mapping(FILE *fp)
{

  char *line = NULL;
  size_t len;
  ssize_t read;

  struct br_struct *br;
  while(read = getline(&line, &len, fp) != -1)
  {
	
	line[strlen(line) - 1] = '\0';	

	char *end;
	long address = strtol(line, &end, 10);
	int byte = strtol(end + 1, &end, 10);

	br = malloc(sizeof(struct br_struct));
	br->br_address = address;
	br->orig_byte = byte;

	HASH_ADD_INT( breakpoints, br_address, br );  
  }
  printf("[+] Parsing Done!\n");
}
