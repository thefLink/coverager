#include <unistd.h>
#include <sys/types.h>
#include <wait.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <pthread.h>

#include "mem_map_parser.c"
#include "cov_measurer.h"
#include "uthash.h"

#ifdef DEBUG
#define printf_dbg printf
#endif

/* The pid of the forked process */
static pid_t c_main_pid;
/* List of thread ids for forked child */
static int t_childs[100];
static int t_cntr = 0;
static int amount_bbs = 0;
static unsigned int bbs_hit = 0;

static void *base_addr = 0;
static char *bbs_map = NULL;

static void handlebr(pid_t);
static void handleclone(pid_t);
static void start_monitoring();
static void find_base();
static void restore_byte(void *, struct br_map *, pid_t);
static void *kill_child();
static void showregs(pid_t);
static struct struct_init *s_init;

int
start_program (struct struct_init *init)
{
  pthread_t t_kill;
  int bbs = 0;

  s_init = init;

  amount_bbs = s_init->amount_bbs;
  s_init->bbs_map = malloc(amount_bbs / 8 + 1);
  bbs_map = s_init->bbs_map;
  memset(bbs_map, 0, amount_bbs / 8 + 1);

  c_main_pid = fork();
  if (!c_main_pid){
    /* The child process */
    printf("[*] about to call execve ... \n");
    ptrace(PTRACE_TRACEME, 0, 0, 0, 0);
    execve(s_init->callstr, s_init->callargv, s_init->callenvp);
    /* Not to be reached when execve succeeds */
  }else{
    /* Parent */
    pthread_create(&t_kill, NULL, kill_child, NULL);
    start_monitoring();
  }

  return bbs_hit;
}

static void *kill_child()
{
    sleep(s_init->timeout);
    kill(c_main_pid, SIGKILL);

    return NULL;
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

       if (status == 9) {
           printf("[!] Child was killed by timeout\n");
           break;
       } else if (WIFEXITED(status) && c_waiting == c_main_pid) {
            /* Child exited normally */
           printf("[*] Child exited normally\n");
           break;
       } else if (WIFSTOPPED(status)) {
            /* Process creates a new thread */
            if (status >> 16 == PTRACE_EVENT_CLONE){
                handleclone(c_waiting);
            }else if (WSTOPSIG(status) == SIGSEGV) {
            /* Fuck ... :) */
                perror("[!] segfault\n");
                showregs(c_waiting);
                break;
            }else if(WSTOPSIG(status) == SIGTRAP){
                /* Super ugly way to parse virtual mapping 
                 * I do not know at which precise signal
                 * the libs are already loaded in memory 
                 */
                if (!base_addr){
                    find_base();
                }

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
showregs(pid_t c_waiting)
{
    struct user_regs_struct regs;
    void *rip;

    ptrace(PTRACE_GETREGS, c_waiting, 0, &regs);

    fprintf(stderr, "[!] RIP: %p\n", regs.rip - 1);
}

static void 
handlebr(pid_t c_waiting)
{
    struct user_regs_struct regs;
    struct br_map *br_mapping;
    void *lsb;

    ptrace(PTRACE_GETREGS, c_waiting, 0, &regs);
    lsb =  (void *)(regs.rip - 1 - (long) base_addr);

    HASH_FIND_PTR(s_init->hlist, &lsb, br_mapping);

    if (!br_mapping) {
      printf("[!] failed to map br: %p\n", (void *)regs.rip - 1);  
    } else { 
         restore_byte((void *)regs.rip - 1, br_mapping, c_waiting);
         regs.rip -= 1;
         ptrace(PTRACE_SETREGS, c_waiting, 0, &regs);
         bbs_hit++;

         bbs_map[br_mapping->id / 8] |= 1 << (br_mapping->id % 8);
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
            s_init->p_binary);
#endif
   struct mem_map *memory_mapping;
   struct mem_map *memory_mapping_found;

   memory_mapping = parse_mem(c_main_pid);
   HASH_FIND_STR(memory_mapping, s_init->p_binary, memory_mapping_found);

   if (!memory_mapping_found){
        printf("[!] Failed to resolve base address for: %s\n", 
                s_init->p_binary);
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
