#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#include "util.h"
#include "libcoverager.h"

#define MAX_CHILDS 8

extern char *__progname;

/* Functions */
void create_childs( meta_infos_run_binary * , int *);
void coverage_mode( meta_infos_run_binary *);
void manage_mode(pid_t *);
void intHandler(int);
void usage(void);
char *read_mapping_file(char *);

/* Global variables */
int num_childs, run = 1;

int
main(int argc, char *argv[])
{

    int ch, timeout;
    char *run_path, *patched_path, *mapping_path;
    meta_infos_run_binary *meta_run_binary;
    pid_t child_pids[MAX_CHILDS];
    struct stat st = {0};

    if ( argc < 10 ) 
        usage();

    meta_run_binary = (meta_infos_run_binary *) malloc(sizeof(meta_infos_run_binary));

    while ((ch = getopt(argc, argv, "r:p:m:t:n:")) != -1)

            switch (ch) {
                
            case 'r':
                meta_run_binary->run_path = optarg;
                break;

            case 'p':
                meta_run_binary->patched_path = optarg;
                break;

            case 'm':
                meta_run_binary->mapping = read_mapping_file(optarg);
                break;

            case 't':
                meta_run_binary->timeout = atoi(optarg);
                break;

            case 'n':
                num_childs = atoi(optarg);

                if ( num_childs > MAX_CHILDS )
                    usage();
                break;

            default:
                usage();

            }

    printf("  _____ \n");
    printf(" /  __ \\ \n");
    printf(" | /  \\/ _____   _____ _ __ __ _  __ _  ___ _ __ \n");
    printf(" | |    / _ \\ \\ / / _ \\ '__/ _` |/ _` |/ _ \\ '__| \n");
    printf(" | \\__/\\ (_) \\ V /  __/ | | (_| | (_| |  __/ |   \n");
    printf("  \\____/\\___/ \\_/ \\___|_|  \\__,_|\\__, |\\___|_|  \n");
    printf("                                  __/ |         \n");
    printf("                                 |___/           \n");
    printf("\n");

    if (stat("sockets", &st) == -1) {
            mkdir("sockets", 0700);
            printf("[*] Created sockets dir\n");
    }

    create_childs(meta_run_binary, child_pids);
    manage_mode(child_pids);

    free(meta_run_binary->mapping);
    free(meta_run_binary);

    return (0);
}

void
create_childs(meta_infos_run_binary *meta_run_binary_infos,
        pid_t * child_pids)
{

    int i;
    pid_t child_pid;

    for ( i = 0; i < num_childs; i++ ) {

        printf("[*] Creating child: %d\n", i + 1);
        child_pid = fork();

        if ( child_pid ) 
            child_pids[i] = child_pid;
         else {
            coverage_mode(meta_run_binary_infos);
         }
        

    }

}

void
coverage_mode(meta_infos_run_binary *meta_run_binary_infos)
{

    int fd_socket_listen, fd_socket_in, i;
    char *argv_received, *bitmap;
    execution_infos *exec_infos;

    exec_infos = malloc(sizeof(execution_infos));
    exec_infos->bitmap = bitmap;

    fd_socket_listen = listen_socket();
    printf("[+] Pid: %d created unix socket with fd: %d\n", getpid(),
            fd_socket_listen);

    /*Init libcoverager */
    init_coveraging(meta_run_binary_infos);

    do {

        fd_socket_in = accept_socket(fd_socket_listen);
        printf("[+] %d Got connection!\n", getpid());

        do {

            argv_received = receive_parameter(fd_socket_in);

            if ( argv_received ) {

                exec_infos->arguments = argv_received;
                generate_bitmap(exec_infos);

                send_bitmap(fd_socket_in, exec_infos->bitmap, exec_infos->bitmap_size);

            }
            

        } while ( argv_received ) ;

        printf("[!] Pid: %d lost client socket\n", getpid());

    } while ( 1 ); 

}

void
manage_mode(pid_t * child_pids)
{
    int i;
    char socket_name[100];
    printf("[*] Now in management mode\n");
    signal(SIGINT, intHandler);

    while ( run ) {
        /* Wait for interrupt */
        pause();
    }

    for( i = 0; i < num_childs; i++ ){

        printf("\n[!] Killing child: %d", child_pids[i]);
        sprintf(socket_name, "sockets/%d", child_pids[i]);
        unlink(socket_name);
        kill(child_pids[i], SIGKILL);

    }

    printf("\n");
}

void
intHandler(int dummy)
{

    run = 0;

}

char *
read_mapping_file(char *path)
{

    char *mapping;
    FILE *f;
    long fsize;

    f = fopen(path, "r");

    if ( f == NULL ) {
        (void)fprintf(stderr,"[!] File %s could not be read\n", path);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  

    mapping= malloc(fsize + 1);
    fread(mapping, fsize, 1, f);
    fclose(f);

    mapping[fsize] = 0;


    return mapping;

}
void
usage(void)
{
    (void)fprintf(stderr,
    "usage: %s [-r run_path] [-p path to patchex binary] [-m path to mapping file] [-t timeout] [-n num of children]\n", __progname);

    exit(1);
}
