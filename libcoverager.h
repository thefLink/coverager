#include "uthash.h"

/* Initialise with content of a description file*/
void init_coveraging(char *, char *, char *, int);
/* Create bbs bitmap for arguments*/
char *generate_bitmap(char *);
/* Destroy a given bitmap */
void destroy_bitmap(char *);

/* Hashmap that maps the offset of an breakpoint to the 
 * original byte */
struct struct_br_map{
   uint32_t offset;
   int orig_byte;

   /* Each breakpoint has an identifier */
   uint32_t id;
   UT_hash_handle hh; 
};

void destroy_pointer(char *);
