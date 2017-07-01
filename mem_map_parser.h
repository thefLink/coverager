#include "uthash.h"

#define MAX_PATH 250

/* 
  mem_map represents one mapping of a .so or main executable 
  in virtual address space
*/
struct mem_map{
  /* The path to binary */
  char path[250];
  /* virtual start address */
  void *start;
  /* virtual end address */
  void *end;  

  /* Make this hashable */
  UT_hash_handle hh;
} struct_mem_map;

/* Parses entire /proc/[pid]/maps
 * Returns: hashlist with all found mappings
*/
//struct mem_map *parse_mem(int);
struct mem_map *parse_mem(int);
