#include "uthash.h"
   
/* Represents one mapping line in /proc/[pid]/maps */
struct map{

  /* The path to binary */
  char path[250];
  /* virtual start address */
  void *start;
  /* virtual end address */
  void *end;  

  /* Make this hashable */
  UT_hash_handle hh;
} map_struct;


/* Parses /proc/[pid]/maps
   Returns: hashlist including all entries. key = path
 */
struct map *parse(int);
