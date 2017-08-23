#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>

#include "libcoverager.h"
#include "uthash.h"

/* Global variables */
struct_br_map             *hlist_br_mapping = NULL;
meta_infos_run_binary     *meta_infos;

/* Functions */
char             **parse_args(char *);
void             find_base(pid_t, char*, unsigned long *);
void             restore_byte(pid_t, unsigned long, int );
void             monitor_tracee(pid_t c_pid, char *, int *);
void             *kill_child(void *);
void             handle_int3(pid_t, char *, unsigned long, int * );

void
generate_bitmap(execution_infos *exec_infos)
{
    char **c_argv;
    pid_t c_pid;
    pthread_t t_kill;
    
    int num_bbs, i, fd_devnull;

    /* Create argv for child process */
    c_argv = parse_args(exec_infos->arguments);

    /* Initialise bitmap */
    num_bbs = HASH_COUNT(hlist_br_mapping);
    exec_infos->bitmap= malloc(num_bbs / 8 + 1);
    exec_infos->bitmap_size = num_bbs / 8 + 1;
    memset(exec_infos->bitmap, 0, num_bbs / 8 + 1);

    /* Create child */
    c_pid = fork();

    if ( !c_pid ) {
        /* Child process */

        /* We dont want to see childs stdin, stdout or stderr  */
        fd_devnull = open("/dev/null", O_WRONLY | O_CREAT, 0666);
        dup2(fd_devnull, 0);
        dup2(fd_devnull, 1);
        dup2(fd_devnull, 2);
        
        ptrace(PTRACE_TRACEME, 0, 0, 0, 0);

        if ( execv(meta_infos->run_path, c_argv) < 0 ) {
            perror("execv");
            exit(-1);
        }
    }

    /* Kill the child after specified timeout */
    pthread_create(&t_kill, NULL, kill_child, (void *) &c_pid);

    /* Loop to handle signals */
    monitor_tracee(c_pid, exec_infos->bitmap, &exec_infos->bbs_counter);

    /* Free all the memory \o/ */
    pthread_join(t_kill, NULL);
    free(c_argv);

}

/* Handles signals of tracee */
void 
monitor_tracee( pid_t c_pid, char *bitmap, int *bbs_counter)
{
    int status;
    pid_t waiting_pid = 0;
    long newpid = 0;
    unsigned long base_addr = 0;

    /* We want threads to be traced as well */
    waitpid(c_pid, &status, __WALL);
    ptrace(PTRACE_SETOPTIONS, c_pid, NULL, PTRACE_O_TRACECLONE);

    if ( ptrace(PTRACE_CONT , c_pid , 0, 0) < 0 ) {
        perror("ptrace_cont 1");
        sleep(meta_infos->timeout);
        return;
    }

    while ( 1 ) {

        waiting_pid = waitpid(-1, &status, __WALL);
        if ( waiting_pid < c_pid) 
            continue;
        
        if ( !base_addr ) 
            find_base(c_pid, meta_infos->patched_path, &base_addr);
        
        if ( status == SIGKILL ) {
            break;
        } else if ( WIFEXITED(status) ) {

            if ( waiting_pid == c_pid ) 
                break;
            else
                /* No ptrace_cont for terminated thread */
                continue;

        } else if WIFSTOPPED(status) {

           if ( WSTOPSIG(status) == SIGSEGV ){
             printf("[!] SIGSEGV\n");
             exit(0);
             
           } else if ( WSTOPSIG(status) == SIGTRAP ) {
               /* Child hit a breakpoint */
               if ( status >> 16 == PTRACE_EVENT_CLONE ) 
                   /* Trap cause of clone call */
                   ptrace(PTRACE_GETEVENTMSG, waiting_pid, 0, (long)&newpid);
               else 
                    /* Trap is a breakpoint */
                   handle_int3(waiting_pid, bitmap, base_addr, bbs_counter);
           }
        }

        /* Continue execution */
        if ( ptrace(PTRACE_CONT, waiting_pid, 0, 0) < 0 ) {
            sleep(meta_infos->timeout);
            perror("ptrace_cont 2");
            break;
        }
    }
}

/* Called when child hits a breakpoint */
void
handle_int3(pid_t pid_waiting, char* bitmap, unsigned long base_addr, int *bbs_counter)
{

    struct user_regs_struct registers;
    unsigned long offset;
    struct_br_map *br_mapping;

    if ( ptrace(PTRACE_GETREGS, pid_waiting, 0, &registers) < 0 ) {
        perror("getregs");
        sleep(meta_infos->timeout);
    }

   offset = registers.rip - 1 - base_addr;
   HASH_FIND_INT(hlist_br_mapping, &offset, br_mapping);

    if ( !br_mapping ) {
        /* This breakpoint could not be mapped to an original byte */
        fprintf(stderr, "[!] Failed to map br at offset %lu\n", offset);
    } else {
       /* Overwrite CC with original byte*/
       restore_byte(pid_waiting, registers.rip - 1, br_mapping->orig_byte);

       /* Decrement RIP */
       registers.rip -= 1;
       if ( ptrace(PTRACE_SETREGS, pid_waiting, 0, &registers) < 0 ) {
           perror("setregs");
           sleep(meta_infos->timeout);
       }

       /* Set bit in bitmap for hit BB */
       bitmap[br_mapping->id / 8] |= 1 << (br_mapping->id % 8);        
       (*bbs_counter)++;
    }
}

/* Overwrites the CC byte with a given byte at specified offset */
void 
restore_byte(pid_t pid_waiting, unsigned long offset, int orig_byte)
{
    long text_patched, text_restored;

    text_patched = ptrace(PTRACE_PEEKTEXT, pid_waiting, offset, 1);
    text_restored = (text_patched & ~0xff) | orig_byte;

    if ( ptrace(PTRACE_POKETEXT, pid_waiting, offset, text_restored) < 0 ){
         perror("poketext");
         sleep(meta_infos->timeout);
    }
}

/* Runs in a thread and kills the child after specified timeout */
void *
kill_child(void *args)
{
    pid_t *c_pid;

    c_pid = (pid_t *) args;
    sleep(meta_infos->timeout);

    if ( kill(*c_pid, SIGKILL ) < 0 ){
        perror("kill");
        sleep(meta_infos->timeout);
    }

    return NULL;
}

/* Creates a char pointer array */
char **
parse_args(char *argv)
{

    char ** c_argv = (char **) malloc(sizeof(char *) * 5);
    char *proc_name = "gen by coverager";
    int cntr_args = 0;

    c_argv[cntr_args++] = proc_name;

    argv = strtok(argv, " ");
    while ( argv != NULL && cntr_args < 19 ) {

        c_argv[cntr_args++] = argv;
        argv= strtok(NULL, " ");

    }

    c_argv[cntr_args] = NULL;

    return c_argv;

}

/* Init with content of description file
 * Creates a hashmap */
void
init_coveraging(meta_infos_run_binary *meta)
{

   char *line, *end;
   int orig_byte;
   /* Id is the unique id for a breakpoint */
   uint32_t offset, id = 0;
   struct_br_map *br_mapping;

   /* Store meta infos in global variable */
   meta_infos = meta;

   line = strtok(meta->mapping, "\n");
   while ( line != NULL ) {

       offset = strtol(line, &end, 10);
       orig_byte = strtol(end + 1, &end, 10);
         
       br_mapping = malloc(sizeof(struct_br_map));
       br_mapping->offset = offset;
       br_mapping->orig_byte = orig_byte;
       br_mapping->id = id;

       HASH_ADD_INT( hlist_br_mapping, offset, br_mapping );  
       ++id;

       line = strtok(NULL, "\n"); 
   }
}
/* Finds base address in memory layout for a given binary path */
void
find_base(pid_t c_main_pid, char *path_binary, unsigned long *base_addr)
{
    char proc_map_path[100];
    char *line, *pEnd, *element;
    size_t len = 0;
    ssize_t read;
    int cntr = 0, found = 0;
    FILE *f_mapping; 
    unsigned long v_addr_start;

    sprintf(proc_map_path, "/proc/%d/maps", c_main_pid);
    f_mapping = fopen(proc_map_path, "r");

    if (!f_mapping){
     perror("[!] Could not open mapfile");
     exit(1);
    }

    while ((read = getline(&line, &len, f_mapping)) != -1 && !found){
        element = strtok(line, " ");
        cntr = 0;
        while ( element != NULL ) {
            if ( cntr == 0 ){
               /* element = start and end addr in vaddress */
               v_addr_start = strtol(element, &pEnd, 16);
            }else if ( cntr == 5 ){
                /* path in FS for binary */
                element[strlen (element) - 1] = 0;
                if ( !strcmp(path_binary, element )){
                    found = 1;
                    break;
                }
            }

            element = strtok(NULL, " ");
            ++cntr;
        }
        
    }

    free(line);
    fclose(f_mapping);
    *base_addr = v_addr_start;

}
