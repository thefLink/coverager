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

struct struct_init{
    struct br_map *hlist;
    char *p_binary;
    char *callstr;
    char **callargv;
    char **callenvp;
    int timeout;
};

/* Initialize the measurer with a hashlist of br_maps */
void init(struct struct_init *s_init);
