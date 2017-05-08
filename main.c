#include <stdio.h>
#include <stdlib.h>

#include "coverager.c"
#include "uthash.h"

void usage();
struct br_map *parse_description(char *);

int main(int argc, char *argv[])
{
  if(argc < 5){
    usage();
  }

  char *callstring = argv[1];
  char *parameters = argv[2];
  char *description_path = argv[3];
  char *binarypath = argv[4];

  struct br_map *br_mapping;

  br_mapping  = parse_description(description_path);
  init(br_mapping, binarypath);
  start_program(callstring, parameters);

  return 0;
}

struct br_map *parse_description(char *path)
{
  struct br_map *mapping = NULL;

  FILE *fp = fopen(path, "r");
  if (!fp){
    perror("[!] Could not open description file");
    exit(1);
  }

  char *line = NULL;
  size_t len;
  ssize_t read;
  struct br_map *br;
  while((read = getline(&line, &len, fp)) != -1){
	
    line[strlen(line) - 1] = '\0';	

    char *end;
    long lsb = strtol(line, &end, 10);
    int byte = strtol(end + 1, &end, 10);

    br = malloc(sizeof(struct br_map));
    br->lsb = (void *)lsb;
    br->orig_byte = byte;

    HASH_ADD_PTR( mapping, lsb, br );  
  }

  printf("[+] Parsing Done!\n");

  return br;
}

void usage()
{
  printf("Usage: ./prog.o <call_string> <argument> <patched_description> <path_to_patched_object>\n");
  exit(0);
}
