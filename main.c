#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "libcoverager.h"

extern char *__progname;

/* Functions */
char *read_mapping_file(char *);
void usage(void);


int
main( int argc, char *argv[] )
{

    int pid, ch, show_count = 0;
    char *params;

    meta_infos_run_binary *meta_run_binary;
    execution_infos *exec_infos;

    meta_run_binary = (meta_infos_run_binary *) malloc(sizeof(meta_infos_run_binary));
    exec_infos = (execution_infos *) malloc(sizeof(execution_infos));

    if ( argc < 6 )
        usage();

    while ((ch = getopt(argc, argv, "r:p:m:t:a:c")) != -1)

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

            case 'a':
                params = optarg;
                break;

            case 'c':
                show_count = 1;
                break;


            default:
                usage();

            }

    init_coveraging(meta_run_binary);
    exec_infos->arguments = params;
    generate_bitmap(exec_infos);

    if ( show_count ) 
        printf("%d\n", exec_infos->bbs_counter);
    else
        for ( int i = 0; i < exec_infos->bitmap_size; i++ ) {
            if ( exec_infos->bitmap[i] == '\x00' ) 
                printf("%c", "\\0");
            else
                printf("%c", exec_infos->bitmap[i]);
        }
    
    free(meta_run_binary);
    free(exec_infos);

    return ( 0 );

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
    "usage: %s [-r run_path] [-p path to patchex binary] [-m path to mapping file] [-t timeout] [-a parameters] [[-c prints the number of bbs hit]]\n", __progname);

    exit(1);
}
