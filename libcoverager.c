#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <pthread.h>

#include "libcoverager.h"
#include "uthash.h"
#include "mem_map_parser.h"

/* Global variables */
static int              timeout;
static char             *c_executable, *patched_executable;
static struct           struct_br_map *hlist_br_mapping = NULL;
unsigned long           base_addr;
char                    *bitmap = NULL;
struct                  mem_map *memory_mapping;

/* Functions */
static char             **parse_args(char *);
static unsigned long    find_base(pid_t, char*);
static void             restore_byte(pid_t, unsigned long, int );
static void             monitor_tracee(pid_t c_pid, char *);
static void             *kill_child(void *);
static void             handle_int3(pid_t, char *);

// int
// main()
// {
//     FILE *f = fopen("patched_description", "rb");
//     fseek(f, 0, SEEK_END);
//     long fsize = ftell(f);
//     fseek(f, 0, SEEK_SET);  //same as rewind(f);
// 
//     char *string = malloc(fsize + 1);
//     fread(string, fsize, 1, f);
//     fclose(f);
// 
//     string[fsize] = 0;
// 
//     init_coveraging(string, "/usr/bin/evince", "/usr/lib/libevview3.so.3.0.0", 1);
//     generate_bitmap("asdf");
// 
//     free(string);
// }

char *
generate_bitmap(char *c_parameters)
{
    char **c_argv;
    pid_t c_pid;
    pthread_t t_kill;
    int num_bbs;
    base_addr = 0;

    /* Create argv for child process */
    c_argv = parse_args(c_parameters);

    /* Initialise bitmap */
    num_bbs = HASH_COUNT(hlist_br_mapping);
    if ( !bitmap ){
        bitmap = malloc(num_bbs / 8 + 1);
    }
    memset(bitmap, 0, num_bbs / 8 + 1);

    /* Create child */
    c_pid = fork();
    if ( !c_pid ) {
        /* Child process */
        ptrace(PTRACE_TRACEME, 0, 0, 0, 0);
        if ( execv(c_executable, c_argv) < 0 ) {
            perror("execv");
            exit(-1);
        }
    }

    /* Kill the child after specified timeout */
    pthread_create(&t_kill, NULL, kill_child, (void *) &c_pid);

    /* Loop to handle signals */
    monitor_tracee(c_pid, bitmap);

    pthread_join(t_kill, NULL);
    free(c_argv);

    return bitmap;
}

/* Handles signals of tracee */
static void monitor_tracee( pid_t c_pid, char *bitmap)
{
    int status;
    pid_t waiting_pid = 0;

    /* We want threads to be traced as well */
    waitpid(c_pid, &status, __WALL);
    ptrace(PTRACE_SETOPTIONS, c_pid, NULL, PTRACE_O_TRACECLONE);

    if ( ptrace(PTRACE_CONT , c_pid , 0, 0) < 0 ) {
        perror("ptrace_cont");
        exit(-1);
    }


    while ( 1 ) {

        waiting_pid = waitpid(-1, &status, __WALL);
        if ( waiting_pid < c_pid) {
            continue;
        }


        if ( !base_addr ) {
            base_addr = find_base(c_pid, patched_executable);
        }

        if ( status == SIGKILL ) {
            printf("[!] Child killed by timeout\n");
            break;
        } else if ( WIFEXITED(status) ) {

            if ( waiting_pid == c_pid ){
                printf("[!] Child exited normally\n");
                break;
            }
            else{
                /* No ptrace_cont for terminated thread */
                continue;
            }

        } else if WIFSTOPPED(status) {

           if ( WSTOPSIG(status) == SIGSEGV ){
             perror("[!] SIGSEGV\n");    
             break;
           } else if ( WSTOPSIG(status) == SIGTRAP ) {
               /* Child hit a breakpoint */
               if ( status >> 16 == PTRACE_EVENT_CLONE ) {
                   /* Trap cause of clone call */
                   pid_t newpid;
                   ptrace(PTRACE_GETEVENTMSG, waiting_pid, 0, &newpid);
               }else {
                    /* Trap is a breakpoint */
                   handle_int3(waiting_pid, bitmap);
               }
           }
        }

        /* Continue execution */
        if ( ptrace(PTRACE_CONT, waiting_pid, 0, 0) < 0 ) {
            perror("ptrace_cont");
            break;
        }
    }
}

/* Called when child hits a breakpoint */
static void
handle_int3(pid_t pid_waiting, char* bitmap)
{
    struct user_regs_struct registers;
    unsigned long offset;
    struct struct_br_map *br_mapping;

    if ( ptrace(PTRACE_GETREGS, pid_waiting, 0, &registers) < 0 ) {
        perror("getregs");
        exit(-1);
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
           exit(-1);
       }

       /* Set bit in bitmap for hit BB */
       bitmap[br_mapping->id / 8] |= 1 << (br_mapping->id % 8);        
    }
}

/* Overwrites the CC byte with a given byte at specified offset */
static void 
restore_byte(pid_t pid_waiting, unsigned long offset, int orig_byte)
{
    long text_patched, text_restored;

    text_patched = ptrace(PTRACE_PEEKTEXT, pid_waiting, offset, 1);
    text_restored = (text_patched & ~0xff) | orig_byte;

    if ( ptrace(PTRACE_POKETEXT, pid_waiting, offset, text_restored) < 0 ){
         perror("poketext");
         exit(-1);
    }
}

/* Runs in a thread and kills the child after specified timeout */
static void *
kill_child(void *args)
{
    pid_t *c_pid;

    c_pid = (pid_t *) args;
    sleep(timeout);


    if ( kill(*c_pid, SIGKILL ) < 0 ){
        perror("kill");
        exit(-1);
    }

    wait(NULL);

    return NULL;
}

/* Creates a char pointer array */
static char **
parse_args(char *argv)
{
   char **c_argv = (char **) malloc(sizeof(char *) * 20);
   char *parameter = malloc(sizeof(char) * strlen(argv));

   int cntr = 1;
   c_argv[0] = "ptraced_child";
   strncpy(parameter, argv, strlen(argv) + 1);

   parameter = strtok(parameter, " ");
   while (parameter != NULL && cntr < 19) {

       c_argv[cntr] = parameter; 
       ++cntr;

       parameter = strtok(NULL, " ");
   }

   c_argv[cntr] = NULL;

   free(parameter);
   return c_argv;
}

/* Init with content of description file
 * Creates a hashmap */
void
init_coveraging(char *mapping, char *path_child_executable, char *path_patched_executable, int to)
{

   char *line, *end;
   int orig_byte;
   /* Id is the unique id for a breakpoint */
   uint32_t offset, id = 0;
   struct struct_br_map *br_mapping;

   timeout = to;
   c_executable = path_child_executable;
   patched_executable = path_patched_executable; 

   line = strtok(mapping, "\n");
   while ( line != NULL ) {

       offset = strtol(line, &end, 10);
       orig_byte = strtol(end + 1, &end, 10);
         
       br_mapping = malloc(sizeof(struct struct_br_map));
       br_mapping->offset = offset;
       br_mapping->orig_byte = orig_byte;
       br_mapping->id = id;

       HASH_ADD_INT( hlist_br_mapping, offset, br_mapping );  
       ++id;

       line = strtok(NULL, "\n"); 
   }
}
/* Finds base address in memory layout for a given binary path */
static unsigned long
find_base(pid_t c_main_pid, char *path_binary)
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
                    printf("gotcha: %s\n", element);
                    found = 1;
                    break;
                }
            }

            element = strtok(NULL, " ");
            ++cntr;
        }
    }

    fclose(f_mapping);
    return v_addr_start;
}
// static unsigned long 
// find_base(pid_t c_main_pid, char* path_binary)
// {
// 
//    struct mem_map *memory_mapping_found;
//    unsigned long base_addr;
// 
//    memory_mapping = parse_mem(c_main_pid);
//    HASH_FIND_STR(memory_mapping, path_binary, memory_mapping_found);
// 
//    if (!memory_mapping_found){
//         printf("[!] Failed to resolve base address for: %s\n", path_binary);
//         sleep(1);
//    }else{
//         base_addr = (unsigned long) memory_mapping_found->start;
//    }
// 
//   return base_addr;
//}

/* To be called by ctypes */
void 
destroy_pointer(char *pointer)
{
    free(pointer);
}
