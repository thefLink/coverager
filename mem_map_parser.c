#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem_map_parser.h"

struct mem_map *next_mem_map(FILE *);

/*
 * Parses /proc/[pid]/maps for a given pid
*/
struct mem_map *
parse_mem(int pid)
{
    struct mem_map *hlist_mem_map = NULL;
    struct mem_map *map_tmp = NULL;

    char proc_map_path[100];

    sprintf(proc_map_path, "/proc/%d/maps", pid);
    FILE *f_mapping = fopen(proc_map_path, "r");

    if (!f_mapping){
     perror("[!] Could not open mapfile");
     exit(1);
    }

    /* Add mem_maps to hashlist, if not already in hashlist */
    struct mem_map* check;
    while ((map_tmp = next_mem_map(f_mapping)) != NULL) {
       HASH_FIND_STR(hlist_mem_map, map_tmp->path, check);
       if ( !check ){
            HASH_ADD_STR(hlist_mem_map, path, map_tmp);
       }
    }

    fclose(f_mapping);

    return hlist_mem_map;
}

/* Parses maps file line for line
 * Returns: struct mem_map, representing one line only*/
struct mem_map *
next_mem_map(FILE *f_mapping)
{
  char *line, *pEnd , *element = NULL;
  size_t len = 0;
  ssize_t read;
  struct mem_map *next_mem_map = NULL;
  int cntr = 0;
  long v_addr_start, v_addr_end;

  /*If there is another line parse it */
  if ((read = getline(&line, &len, f_mapping)) != -1) {
      next_mem_map = malloc(sizeof(struct mem_map));
      element = strtok(line, " ");  

      while (element != NULL) {
        if (cntr == 0) {
          /* element = start and end addr in vaddress */
           v_addr_start = strtol(element, &pEnd, 16);
           v_addr_end   = strtol(pEnd + 1, &pEnd, 16);

           next_mem_map->start = (void *) v_addr_start;
           next_mem_map->end   = (void *) v_addr_end;
        }else if (cntr == 5) {
            /* path in FS for binary */
            element[strlen (element) - 1] = 0;
            strncpy(next_mem_map->path, element, strlen(element));
        }
          
        element = strtok(NULL, " ");
        ++cntr;

      }

      free(element);
      free(line);

    }

      return next_mem_map;
}
