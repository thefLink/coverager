#include "uthash.h"

/* 
 * Represents a mapping between the lsb of a CC byte's address
 * and the original byte in the executable
 * */
struct br_map{
   void *lsb;
   short orig_byte;
   UT_hash_handle hh; 
};

/* Initialize the measurer with a hashlist of br_maps */
void init(struct br_map *hlist_br_mapping, char *);
