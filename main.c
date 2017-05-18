#include <stdio.h>
#include <stdlib.h>


#include "cov_measurer.c"
#include "uthash.h"

static void usage();
static struct br_map *parse_description(char *);
static char **parse_child_args(char *);
static void write_result(struct struct_init *);

int 
main(int argc, char *argv[], char *envp[])
{
  printf("[*] Coverager\n");

  if(argc < 6){
    usage();
  }

  int bbs_hit, timeout = 0;
  unsigned int amount_bbs;
  char *callstring = argv[1];
  char *parameters = argv[2];
  char *description_path = argv[3];
  char *binarypath = argv[4];
  char *timeout_str = argv[5];

  char **c_argv = parse_child_args(parameters);
  struct br_map *br_mapping;
  struct struct_init *s_init;

  s_init = malloc(sizeof(struct struct_init));
  sscanf (timeout_str, "%d", &timeout);

  printf("[*] Parsing description file\n");
  br_mapping  = parse_description(description_path);
  amount_bbs = HASH_COUNT(br_mapping);
  printf("[+] Found %d bbs\n", amount_bbs);

  s_init->hlist = br_mapping;
  s_init->p_binary = binarypath;
  s_init->callstr = callstring;
  s_init->callargv = c_argv;
  s_init->callenvp = envp;
  s_init->timeout = timeout;
  s_init->amount_bbs = amount_bbs;
  s_init->bbs_map = NULL;

  bbs_hit = start_program(s_init);
  s_init->bbs_hit = bbs_hit;
  
  printf("[!] BBS hit: %d\n", bbs_hit);

  write_result(s_init);

  return 0;
}

static void 
write_result(struct struct_init *s_init)
{
    FILE *logfile;
    FILE *mapfile;

    logfile = fopen("result.txt", "a");
    mapfile = fopen("bbs_map", "w");

    fprintf(logfile, "%s => %d hits\n", s_init->callargv[1], s_init->bbs_hit);
    fwrite(s_init->bbs_map, 1, s_init->amount_bbs / 8 + 1, mapfile);

    fclose(logfile);
    fclose(mapfile);
}

static char **
parse_child_args(char *parameters)
{
   char **c_argv = (char **) malloc(sizeof(char *) * 20);
   char *parameter = NULL;
   int cntr = 1;
   c_argv[0] = "ptraced_child";

   parameter = strtok(parameters, " ");
   while (parameter != NULL && cntr < 19) {
       c_argv[cntr] = parameter; 
       ++cntr;

       parameter = strtok(NULL, " ");
   }

   c_argv[cntr] = NULL;

   return c_argv;
}

/* Parses file that maps lsb of a CC event address to original byte */
static struct br_map *parse_description(char *path)
{
  struct br_map *hlist_br_map = NULL, *br_mapping;
  FILE *fp = fopen(path, "r");
  char *line, *end; 
  size_t len = 0;
  ssize_t read;

  int byte;
  long lsb;

  if (!fp){
    perror("[!] Could not open description file");
    exit(1);
  }

  int id = 0;
  while ((read = getline(&line, &len, fp)) != -1) {
	
    line[strlen(line) - 1] = '\0';	

    lsb = strtol(line, &end, 10);
    byte = strtol(end + 1, &end, 10);

    br_mapping = malloc(sizeof(struct br_map));
    br_mapping->lsb = (void *)lsb;
    br_mapping->orig_byte = byte;
    br_mapping->id = id;
    id++;

    HASH_ADD_PTR( hlist_br_map , lsb, br_mapping );  
  }

  printf("[+] Parsing Done!\n");
  return hlist_br_map;
}

static void usage()
{
  printf("Usage: ./prog.o <call_string> <argument> <patched_description> <path_to_patched_object> <timeout>\n");
  exit(0);
}
