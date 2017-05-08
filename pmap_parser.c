#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pmap_parser.h"

#define MAX_PATH 250



/* Parses /proc/[pid]/maps 
 *
 * Returns: hashlist containing all entries
*/

struct 
map *parse(int pid)
{
  /* The hashlist */
  struct map *maps = NULL;
  /* Create path to map file */
  char proc_map_path[100];

  sprintf(proc_map_path, "/proc/%d/maps", pid);
  FILE *f_map = fopen(proc_map_path, "r");

  if (!f_map){
    perror("[!] Could not open mapfile");
    exit(1);
  }

  /* Parse file line by line */
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  struct map *map_tmp;

  while ((read = getline(&line, &len, f_map)) != -1) {

    map_tmp = malloc(sizeof(struct map));

    char *element = strtok(line, " ");
    int cntr = 0;
    while(element != NULL){
      if (cntr == 0){
	/* The start end and point */
	long start;
	long end;
	char *pEnd;

	start = strtol(element, &pEnd, 16);
	end   = strtol(pEnd + 1, &pEnd, 16);

	map_tmp->start = (void *)start;
	map_tmp->end   = (void *)end;

      }else if(cntr == 5){
	/* Name + path (last element) */
	element[strlen(element)-1] = 0;
	strncpy(map_tmp->path, element, MAX_PATH);
      }
      
      element = strtok(NULL, " ");
      ++cntr;
    }

    //printf("e: %s -> %p\n", map_tmp->path, map_tmp->start);

    /* Add to hashlish if not in there already */
    struct map *found;
    HASH_FIND_STR(maps, map_tmp->path, found);
    if (!found){
      HASH_ADD_STR(maps, path, map_tmp);
    }
  }

  return maps;
}
