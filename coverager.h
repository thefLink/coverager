#include "uthash.h"

struct br_map{
   void *lsb;
   short orig_byte;
   UT_hash_handle hh; 
};

void init(struct br_map *mapping, char *);
