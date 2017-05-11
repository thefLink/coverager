#include <unistd.h>
#include <sys/types.h>
#include <wait.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/signal.h>

#include "mem_map_parser.c"
#include "cov_measurer.h"
#include "uthash.h"

#ifdef DEBUG
#define printf_dbg printf
#endif

static struct br_map *hlist_br_mapping;
static char *path_pbin;
/* The pid of the forked process */
static pid_t c_main_pid;
/* List of thread ids for forked child */
static int t_childs[100];
static int t_cntr = 0;
static unsigned int bbs_hit = 0;
static void *base_addr = 0;

static void handlebr(pid_t);
static void handleclone(pid_t);
static void start_monitoring();
static void find_base();
static void restore_byte(void *, struct br_map *, pid_t);


void 
init(struct br_map *hlist, char *p_binary)
{
  hlist_br_mapping = hlist;
  path_pbin = p_binary;
}

void 
start_program (char *call_string, char *argv[], char *envp[])
{
  c_main_pid = fork();
  if (!c_main_pid){
    /* The child process */
    printf("[*] about to call execve ... \n");
    ptrace(PTRACE_TRACEME, 0, 0, 0, 0);
    execve(call_string, argv, envp);
    /* Not to be reached when execve succeeds */
  }else{
    /* Parent */
    start_monitoring();
  }
}

static void 
start_monitoring()
{
    printf("[+] Starting monitoring pid: %d\n", c_main_pid);
#ifdef DEBUG
    printf_dbg("[DEBUG] start_monitoring called\n");
#endif

   int status;
   long ptraceOption = PTRACE_O_TRACECLONE;
   pid_t c_waiting, c_new = 0;

   /* Wait for first signal to set the TRACECLONE flag*/
   wait(&status);
   ptrace(PTRACE_SETOPTIONS, c_main_pid, NULL, ptraceOption);
   ptrace(PTRACE_CONT, c_main_pid, 0, 0);
#ifdef DEBUG
    printf_dbg("[DEBUG] traceclone flag set\n");
#endif

   while ( 1 ) {
       c_waiting = waitpid(-1, &status, __WALL);

       if (WIFEXITED(status) && c_waiting == c_main_pid) {
            /* Child exited normally */
           printf("[*] Child exited normally\n");
           printf("[!] BBS hit: %d\n", bbs_hit);
           break;
       } else if (WIFSTOPPED(status)) {
            /* Process creates a new thread */
            if (status >> 16 == PTRACE_EVENT_CLONE){
                handleclone(c_waiting);
            }else if (WSTOPSIG(status) == SIGSEGV) {
            /* Fuck ... :) */
                printf("[!] segfault\n");
                break;
            }else if(WSTOPSIG(status) == SIGTRAP){
                /* Super ugly way to parse virtual mapping 
                 * I do not know at which precise signal
                 * the libs are already loaded in memory 
                 */
                if (!base_addr){
                    find_base();
                }

                /* Handle breakpoint */
                handlebr(c_waiting);
            }else {
            /* Something unimportant happened */
#ifdef DEBUG
                printf_dbg("[DEBUG] Signal: %s\n", strsignal(WSTOPSIG(status)));
#endif
            }

            ptrace(PTRACE_CONT, c_waiting, 0, 0);
        }
   }
}

static void 
handlebr(pid_t c_waiting)
{
    struct user_regs_struct regs;
    struct br_map *br_mapping;
    void *lsb;

    ptrace(PTRACE_GETREGS, c_waiting, 0, &regs);
    lsb =  (void *)(regs.rip - 1 - (long) base_addr);

    HASH_FIND_PTR(hlist_br_mapping, &lsb, br_mapping);

    if (!br_mapping) {
      printf("[!] failed to map br: %llu\n", regs.rip - 1);  
    } else { 
        restore_byte((void *)regs.rip - 1, br_mapping, c_waiting);
         regs.rip -= 1;
         ptrace(PTRACE_SETREGS, c_waiting, 0, &regs);
         bbs_hit++;
    }


}

static void 
restore_byte(void *ip, struct br_map *br_mapping, pid_t c_waiting)
{
  long text_patched;
  long text_restored; 

  text_patched = ptrace(PTRACE_PEEKTEXT, c_waiting, ip, 1);
  text_restored = (text_patched & ~0xff) | br_mapping->orig_byte;

  ptrace(PTRACE_POKETEXT, c_waiting, ip, text_restored);
}

static void
find_base()
{
#ifdef debug
    printf_db("[DEBUG] Trying to resolve base address for: %s\n",
            path_pbin);
#endif
   struct mem_map *memory_mapping;
   struct mem_map *memory_mapping_found;

   memory_mapping = parse_mem(c_main_pid);
   HASH_FIND_STR(memory_mapping, path_pbin, memory_mapping_found);

   if (!memory_mapping_found){
        printf("[!] Failed to resolve base address for: %s\n", 
                path_pbin);
        printf("[!] Retrying ... \n");
   }else{
        printf("[+] %s is at %p\n", memory_mapping_found->path,
                memory_mapping_found->start);
        base_addr = memory_mapping_found->start;
   }
}

static void 
handleclone(pid_t c_waiting)
{
#ifdef DEBUG
    printf_dbg("[DEBUG] handling clone call for pid: %d\n", c_waiting);
#endif
  pid_t c_new;
  int i = 0;

  if (ptrace(PTRACE_GETEVENTMSG, c_waiting, 0, &c_new) != -1) {
      for (; i < t_cntr; i++){
        if (t_childs[i] == c_new){
            break;
        }
      }

      if (i == t_cntr ) {
        t_childs[t_cntr] = c_new;
        t_cntr++;
        printf("[+] new thread detected, t_id: %d\n", c_new);
      }
  }
}
