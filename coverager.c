#include <unistd.h>
#include "coverager.h"
#include <sys/types.h>
#include <wait.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/signal.h>

#include "pmap_parser.c"
#include "uthash.h"

struct br_map *br_mapping;
char *patched_binary;
pid_t child_pid;
void *offset;

void start_monitoring();
void handle_br(struct user_regs_struct regs);
void handle_offset();
void restore_byte(void *, struct br_map *);
void showinfo(struct user_regs_struct regs, void *lsb);

/* Use the first signal to read /proc/[pid]/maps */
short firstbr = 1;

int bbs_hit = 0;

void 
start_program(char *call_string, char *arg)
{  
  child_pid = fork();
  if (!child_pid){
     /* I am the child */
     printf("[*] Child calls execlp now\n");
     ptrace(PTRACE_TRACEME, 0, 0, 0, 0);
     execlp(call_string, "traced_and_patched", arg, NULL);
     /* Never reached */
  }else{
    /* Parent */
    start_monitoring();
  }
}

void
start_monitoring()
{
   int status;
   struct user_regs_struct regs;

   while ( 1 ){
     wait(&status); 
     ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
     printf("%s\n",strsignal(WSTOPSIG(status)));

     if(WIFEXITED(status)){
       /* Child exited normally */
       printf("[!] Child exited, aborting ... \n");
       printf("[!] Hits: %d\n", bbs_hit);
       break;
     }else if (WIFSTOPPED(status)){
       if(!strcmp(strsignal(WSTOPSIG(status)),
			       "Segmentation fault")){
	 /* Fuck :) */
	 printf("[!] sigsev ... :(\n");
	 showinfo(regs, NULL);
	 printf("[+] Hits: %d\n", bbs_hit);
	 break;
       }else{
         handle_br(regs);
       }
     }

     /* Continue child */
     ptrace(PTRACE_CONT, child_pid, 0, 0);
   }
}

void
handle_br(struct user_regs_struct regs)
{
  if (!offset){
    handle_offset();
  }
    struct br_map *br_mapped;
    void *lsb =  (void *)(regs.rip - 1 - (long) offset);

    if ((long) lsb < 0){
      lsb = (void *)((long) offset - regs.rip - 1);
    }

    HASH_FIND_PTR(br_mapping, &lsb, br_mapped);

    if (!br_mapped){
      printf("[!] Missed signal for lsb: %p\n", lsb);
      showinfo(regs, lsb);
    }else{
      /* Breakpoint in list */       
      restore_byte(regs.rip - 1, br_mapped);
      regs.rip -= 1;
      ptrace(PTRACE_SETREGS, child_pid, 0, &regs);
      bbs_hit++;
    }
}

void 
restore_byte(void *ip, struct br_map *br_mapped)
{
  long text_patched;
  long text_restored; 

  text_patched = ptrace(PTRACE_PEEKTEXT, child_pid, ip, 1);
  text_restored = (text_patched & ~0xff) | br_mapped->orig_byte;

  //printf("%lx\n", text_patched);
  //printf("%lx\n", text_restored);

  ptrace(PTRACE_POKETEXT, child_pid, ip, text_restored);
}

void 
handle_offset()
{
  struct map *memory_mapping;
  memory_mapping = parse(child_pid);  
    
  struct map *found;
  HASH_FIND_STR(memory_mapping, patched_binary, found);

  if (!found){
    perror("[!] Could not find specified binary path in mapping, trying again at next BR\n");
    //exit(1);
  }else{
    printf("[+] %s is at %p\n", found->path, found->start);
    offset = found->start;
  }
}

void 
init(struct br_map *mapping, char *p_binary)
{
  br_mapping = mapping;
  patched_binary = p_binary;
}

void 
showinfo(struct user_regs_struct regs, void *lsb){
  printf("[!] lsb: %p\n", lsb);
  printf("[!] RIP at: %p\n", regs.rip - 1);
  printf("[!] Offset at: %p\n", offset);
}
