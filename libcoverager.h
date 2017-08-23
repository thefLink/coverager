#include "uthash.h"

/* Hashmap that maps the offset of an breakpoint to the 
 * original byte */
typedef struct {
   uint32_t offset;
   int orig_byte;

   /* Each breakpoint has an identifier */
   uint32_t id;
   UT_hash_handle hh; 
}struct_br_map;

/* Struct for metainfos */
typedef struct {
    int timeout; /* Auto kill child  */
    char *run_path; /* Path to be executed as child */
    char *patched_path; /* Path to the binaries with breakpoints */
    char *mapping; /* Mapping between br-offset and original byte */
} meta_infos_run_binary;

/* Struct for starting and giving feedback on an execution */
typedef struct {
    char *bitmap;
    char *arguments;
    int bitmap_size;
    int bbs_counter;
} execution_infos;

/* Initialise with content of a description file*/
void init_coveraging(meta_infos_run_binary *);
/* Create bbs bitmap for arguments*/
void generate_bitmap(execution_infos *);

