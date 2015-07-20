#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/hash.c,v 11.2 1993/02/03 16:50:10 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <stdio.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "rel_header.h"
#include "hash.h"
#include "io.h"
#include "smart_error.h"

/* Utility programs for accessing hashtable relations in unix files */
/*    The hashtable relations contain information about each term int the*/
/*    collection that has been indexed.*/
/**/
/**/
/*Procedures include:*/
/*  int seek_hash (file_index, hash_entry);*/
/*          int file_index, HASH_ENTRY *hash_entry;*/
/*      Returns 1 if a hash_entry is found which matches the given information*/
/*      Returns 0 otherwise. If found, position in hashtable set to*/
/*      that entry for future read or write. If not found, position set to*/
/*      entry for insertion (write) of hash_entry with that information.*/
/*      <file_index> must have been returned by open_hash.*/
/*      <hash_entry.con> must be UNDEF or known concept number.*/
/*      <hash_entry.token> must be NULL, "" or known character string */
/*              (max length is MAX_TOKEN_LEN, currently 1024)*/
/*      Either <con> or <token> should be known, If not,*/
/*      UNDEF is returned and position is undefined. */
/*      If hash_entry is NULL, a seek is performed to the beginning of the*/
/*      file and 1 is returned.*/
/**/
/*  int read_hash (file_index, hash_entry);*/
/*          int file_index, HASH_ENTRY *hash_entry;*/
/*      Returns the number of entries actually put into <hash_entry> (1 or 0).*/
/*      Reads previously opened hashtable at current position,*/
/*      putting entries read in <hash_entry>.*/
/*      <file_index> must have been returned by open_hash.*/
/**/
/*  int write_hash(file_index, hash_entry);*/
/*          int file_index, HASH_ENTRY *hash_entry;*/
/*      Returns actual number of hash_entry's written (1 or 0).*/
/*      <file_index> must have been returned by open_hash with mode "a"*/
/*      close_hash must be called to finally commit all written entries.*/
/**/
/*      close_hash(file_index);*/
/*      <file_index> must have been returned by open_hash.*/
/*      Closes all UNIX files, writing out those which were kept in core.*/
/**/
/*Interior forms (from hash.h) :*/
/*       typedef struct {*/
/*           char  *token;       actual string */
/*           long info;          Usage dependent value */
/*           long  con;          unique index for this token ctype pair */
/*       } HASH_ENTRY;*/
/*    */
/**/
/*File format:*/
/*    Hashtable implemented by a hash table. See the actual routines for*/
/*    those implementation details. There are is one+ physical files*/
/*    associated with the present implementation.*/
/*          <file_name>    The hash table implementation of main hashtable.*/
/*          <file_name>    (second part) The actual strings of entries.*/
/*          <file_name>.?? Overflow entries (used only when <file_name>*/
/*                          is full).*/
/*    The present format of a hash entry (from "hash.h")*/
/*       typedef struct hashtab_entry {*/
/*           short collision_ptr;      offset of next word hashed here   */
/*                                     If IN_OVF_TABLE, then need to go to*/
/*                                     (next) overflow hash table */
/*           short prefix;             First two bytes of string */
/*           long str_tab_off;         position of string */
/*           long info;                Usage dependent filed */
/*       } HASHTAB_ENTRY;*/
/*Notes:*/
/*    close_hash must be used in order to write out a file that was opened*/
/*    with mode SWRONLY or SRDWR*/


extern int dict_compare_hash_entry(), dict_get_var_entry(), dict_enter_string_table();
extern long dict_hash_func();
static _SHASH _Shash[MAX_NUM_HASH];
int seek_hash();

int
create_hash (file_name, rh)
char *file_name;
REL_HEADER *rh;
{
    int fd;
    register HASHTAB_ENTRY *hash_tab;
    HASHTAB_ENTRY hashtab_entry;
    register long i;
    long size;
    REL_HEADER real_rh;

    real_rh.max_primary_value = 
                        (rh == (REL_HEADER *) NULL || 
                         rh->max_primary_value == UNDEF) ?
                                    DEFAULT_HASH_SIZE :
                                    rh->max_primary_value;
    real_rh.type = (rh == (REL_HEADER *) NULL) ? S_RH_HASH : rh->type;
    real_rh.num_entries = 0;
    real_rh._entry_size = sizeof (HASHTAB_ENTRY);
    real_rh.num_var_files = 1;
    real_rh._internal = 0;
    real_rh.max_secondary_value = 0;

    size = real_rh.max_primary_value * sizeof (HASHTAB_ENTRY);

    if (-1 == (fd = open (file_name, SWRONLY|SCREATE, 0664))) {
        set_error (errno, file_name, "create_hash");
        return (UNDEF);
    }

    hashtab_entry.info = 0;
    hashtab_entry.collision_ptr = 0;
    hashtab_entry.prefix = 0;
    hashtab_entry.str_tab_off = UNDEF;

    if (NULL == (hash_tab = (HASHTAB_ENTRY *) 
                              malloc ((unsigned) size))) {
        set_error (errno, file_name, "create_hash");
        return (UNDEF);
    }

    for (i = 0; i < real_rh.max_primary_value; i++) {
        bcopy ((char *) &hashtab_entry, 
               (char *) &hash_tab[i], 
               sizeof (HASHTAB_ENTRY));
    }

    if (sizeof (REL_HEADER) != write (fd,
                                      (char *) &real_rh, 
                                      sizeof (REL_HEADER))){
        set_error (errno, file_name, "create_hash");
        return (UNDEF);
    }

    if (size != write (fd, (char *) hash_tab, (int) size)) {
        set_error (errno, file_name, "create_hash");
        return (UNDEF);
    }

    (void) free ((char *) hash_tab);
    (void) close (fd);
    
    return (0);
}
    
#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/open_hash.c,v 11.2 1993/02/03 16:50:15 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "rel_header.h"
#include "hash.h"
#include "io.h"
#include "smart_error.h"
#ifdef MMAP
#   include <sys/types.h>
#   include <sys/mman.h>
#endif   /* MMAP */

extern _SHASH _Shash[];

/*  int open_hash (file_name, mode); */
/*          char *file_name, int mode; */
/*      Returns <file_index> to be used by other routines to access this */
/*      hashtable. Returns UNDEF if cannot access files. */
/*      Prepares access to all UNIX files associated with hashtable */
/*      <file_name>. */
/*      Depending on <mode>, files may be read entirely into core */
/*      <file_name> has maximum length (PATH_LEN - 4) currently 1020 */
/*      <mode> has values as given in "io.h" */

static int get_file_header(), get_file(), get_next_ovfl_file();
static void get_hash_ovfl_file();

#ifdef MMAP
static int get_file_mmap();
#endif // MMAP


int
open_hash(file_name, mode)
char *file_name;
long  mode;
{
    int hash_index;
    register _SHASH *hash_ptr;
    _SHASH *already_open, *t_hash_ptr;
    REL_HEADER temp_rel_entry;

#ifdef MMAP
    if (mode & SMMAP) {
        /* SMMAP only allowed on SRDONLY */
        if ((mode & SRDWR) || (mode & SWRONLY))
            mode &= ~SMMAP;
        else {
            /* Turn on SINCORE bit in addition */
            mode |= SINCORE;
        }
    }
#endif  /* MMAP */

    /* Find first open slot in hash table. */
    /* Minor optimization of allowing two or more RDONLY,INCORE invocations */
    /* of the same hashtable to share the incore image */
    already_open = NULL;
    hash_ptr = NULL;
    for ( t_hash_ptr = &_Shash[0]; 
          t_hash_ptr < &_Shash[MAX_NUM_HASH];
          t_hash_ptr++) {
        if ( mode == t_hash_ptr->mode &&
             (mode & SINCORE) &&
             (! (mode & (SWRONLY|SRDWR))) &&
             strncmp (file_name, t_hash_ptr->file_name,PATH_LEN) == 0 &&
             t_hash_ptr->opened > 0) {
            /* increment count of number relations sharing readonly buffers */
            t_hash_ptr->shared++;
            already_open = t_hash_ptr;
        }
        else if (t_hash_ptr->opened == 0 && hash_ptr == NULL) {
            /* Assign hash-ptr to first open spot */
            hash_ptr = t_hash_ptr;
        }
    }

    if (hash_ptr == NULL) {
        set_error (EMFILE, file_name, "open_hash");
        return (UNDEF);
    }
    hash_index = hash_ptr - &_Shash[0];
    
    if (already_open != NULL) {
        bcopy ((char *) already_open, (char *) hash_ptr, sizeof (_SHASH));
        (void) seek_hash (hash_index, (HASH_ENTRY *) NULL);
        if (hash_ptr->next_ovfl_index != UNDEF) {
            if (UNDEF == (hash_ptr->next_ovfl_index =
                          open_hash (hash_ptr->next_ovfl_file_name,
                                     hash_ptr->mode)))
                return (UNDEF);
        }
        return (hash_index);
    }

    /* Set default modes.  Presently must have hashtable in core to write. */
    /* Also, must have read access to file at all times. */
    if (mode & SWRONLY) {
        mode = (mode & ~SWRONLY) | SRDWR;
    }
    if ((! (mode & SINCORE)) && (mode & SRDWR)) {
        mode = mode | SINCORE;
    }
    
    hash_ptr->mode = mode;
    (void) strncpy (hash_ptr->file_name, file_name, PATH_LEN);

    if (mode & SCREATE) {
        temp_rel_entry.type = S_RH_DICT;
        temp_rel_entry.max_primary_value = DEFAULT_HASH_SIZE;
        if (UNDEF == create_hash (hash_ptr->file_name, &temp_rel_entry)) {
            return (UNDEF);
        }
        hash_ptr->mode = mode & (~SCREATE);
    }
    
    if (-1 == (hash_ptr->fd = open (file_name, (int) (mode & SMASK)))) {
        set_error (errno, file_name, "open_hash");
        return (UNDEF);
    }
    
    hash_ptr->file_size = lseek (hash_ptr->fd, 0L, 2);
    (void) lseek (hash_ptr->fd, 0L, 0);

    if (mode & SINCORE) {
#ifdef MMAP
        if (mode & SMMAP) {
            if (UNDEF == get_file_mmap (hash_index))
                return (UNDEF);
        }
        else if (UNDEF == get_file (hash_index))
            return (UNDEF);
#else
        if (UNDEF == get_file (hash_index))
            return (UNDEF);
#endif /* MMAP */
    }
    else if (mode & (SWRONLY | SRDWR)) {
        /* Ignore advice and read in_core, set the SINCORE bit in mode */
        if (UNDEF == get_file (hash_index)) {
            return (UNDEF);
        }
        mode = mode | SINCORE;
    }
    else {
        if (UNDEF == get_file_header (hash_index)) {
            return (UNDEF);
        }
    }

    hash_ptr->opened = 1;
    if (UNDEF == (hash_ptr->next_ovfl_index = 
                            get_next_ovfl_file (hash_index))) {
        clr_err();
    }

    /* Position at beginning of file */
    (void) seek_hash (hash_index, (HASH_ENTRY *) NULL);

    return (hash_index);
}


static int
get_file (hash_index)
int hash_index;
{
    long hash_size, string_size;
    long reserved_size;
    register _SHASH *hash_ptr = &_Shash[hash_index];
    
    /* Get header at beginning of file */
    if (sizeof (REL_HEADER) != read (hash_ptr->fd,
                                     (char *) &hash_ptr->rh,
                                     sizeof (REL_HEADER))) {
        set_error (errno, hash_ptr->file_name, "open_hash");
        return (UNDEF);
    }
    
    /* Read hash table portion of file */
    hash_ptr->hsh_tab_size = hash_ptr->rh.max_primary_value;
    hash_size = hash_ptr->hsh_tab_size * sizeof (HASHTAB_ENTRY);
    
    if (NULL == (hash_ptr->hsh_table = 
                     (HASHTAB_ENTRY *) malloc ((unsigned) hash_size))) {
        set_error (errno, hash_ptr->file_name, "open_hash");
        return (UNDEF);
    }
    
    if (hash_size != read (hash_ptr->fd,
                      (char *) hash_ptr->hsh_table, 
                      (int) hash_size)) {
        set_error (errno, hash_ptr->file_name, "open_hash");
        return (UNDEF);
    }


    /* Now read string table portion of file */

    string_size = hash_ptr->file_size - sizeof (REL_HEADER) - hash_size;

    if ((hash_ptr->mode & SRDWR) || (hash_ptr->mode & SWRONLY))
        reserved_size = (2 * string_size + HASH_MALLOC_SIZE);
    else
        reserved_size = string_size;

    if (NULL == (hash_ptr->str_table = 
                     malloc ((unsigned) reserved_size))) {
        set_error (errno, hash_ptr->file_name, "open_hash");
        return (UNDEF);
    }
    
    if (string_size != read (hash_ptr->fd, 
                             hash_ptr->str_table, 
                             (int) string_size)) {
        set_error (errno, hash_ptr->file_name, "open_hash");
        return (UNDEF);
    }
    
    
    hash_ptr->str_tab_size = reserved_size;
    hash_ptr->str_next_loc = hash_ptr->str_table + string_size;

    (void) close (hash_ptr->fd);
    return (0);
}

#ifdef MMAP
static int
get_file_mmap (hash_index)
int hash_index;
{
    char *hash_map;
    register _SHASH *hash_ptr = &_Shash[hash_index];
    
    /* Get header at beginning of file */
    if (sizeof (REL_HEADER) != read (hash_ptr->fd,
                                     (char *) &hash_ptr->rh,
                                     sizeof (REL_HEADER))) {
        set_error (errno, hash_ptr->file_name, "open_hash");
        return (UNDEF);
    }
    
    /* Map entire file into memory (must be SRDONLY else SMMAP
       wouldn't have been set) */
    if ((char *) -1 == (hash_map =
                        mmap ((caddr_t) 0,
                              (size_t) hash_ptr->file_size,
                              PROT_READ,
                              MAP_SHARED,
                              hash_ptr->fd,
                              (off_t) 0))) {
        set_error (errno, hash_ptr->file_name, "open_hash");
        return (UNDEF);
    }
    /* Get hash table portion of file */
    hash_ptr->hsh_tab_size = hash_ptr->rh.max_primary_value;
    hash_ptr->hsh_table = (HASHTAB_ENTRY *) (hash_map + sizeof (REL_HEADER));

    /* Now get string table portion of file */
    hash_ptr->str_table = (char *) hash_ptr->hsh_table +
        hash_ptr->hsh_tab_size * sizeof (HASHTAB_ENTRY);
    hash_ptr->str_tab_size = hash_ptr->file_size - sizeof (REL_HEADER) -
        hash_ptr->hsh_tab_size * sizeof (HASHTAB_ENTRY);
    hash_ptr->str_next_loc = NULL;

    (void) close (hash_ptr->fd);
    return (0);
}
#endif  /* MMAP */


static int
get_file_header (hash_index)
int hash_index;
{
    register _SHASH *hash_ptr = &_Shash[hash_index];

    /* Get header at beginning of file */
    if (sizeof (REL_HEADER) != read (hash_ptr->fd,
                                     (char *) &hash_ptr->rh,
                                     sizeof (REL_HEADER))) {
        set_error (errno, hash_ptr->file_name, "open_hash");
        return (UNDEF);
    }

    hash_ptr->hsh_tab_size = hash_ptr->rh.max_primary_value;

    return (0);
}


static int
get_next_ovfl_file (current_index)
int current_index;
{
    register _SHASH *hash_ptr = &_Shash[current_index];

    (void) get_hash_ovfl_file (hash_ptr->file_name,
                               hash_ptr->next_ovfl_file_name);

    return (open_hash (hash_ptr->next_ovfl_file_name, (long) hash_ptr->mode));
}


static void
get_hash_ovfl_file (old_path, new_path)
char *old_path;
char *new_path;
{
    int len;
    register char *ptr;

    (void) strncpy (new_path, old_path, PATH_LEN - 6);
    
    len = strlen(new_path);
    if (len < 2 || new_path[len-2] != '.') {
        /* Just append ".1" to current file name */
        (void) strcat (new_path, ".1");
    }
    else {
        /* Increase current suffix */
        ptr = &new_path[len-1];
        if (*ptr >= '0' && *ptr <= '8') {
            (*ptr)++;
        }
        else if (*ptr== '9') {
            *ptr = 'a';
        }
        else if (*ptr >= 'a' && *ptr < 'z') {
            (*ptr)++;
        }
        else {
            /* Just append ".1" to current file name */
            (void) strcat (new_path, ".1");
        }
    }
}


int 
destroy_hash (file_name)
char *file_name;
{
    char buf[PATH_LEN];

    if (-1 == unlink (file_name)) {
        set_error (errno, file_name, "destroy_hash");
        return (UNDEF);
    }

    /* Unlink any overflow files */
    get_hash_ovfl_file (file_name, buf);
    if (UNDEF == destroy_hash (buf)) {
        clr_err ();
    }

    return (1);
}
    
/* Rename an instance of a hashtable relational object. */
/* Bug at moment: can't distinguish real error in renaming an
  overflow file from overflow file not existing. */
int
rename_hash (in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    char in_buf[PATH_LEN+4];
    char out_buf[PATH_LEN+4];

    /* Name the next overflow file */
    get_hash_ovfl_file (in_file_name, in_buf);
    get_hash_ovfl_file (out_file_name, out_buf);

    /* Link out_file_name to the existing in_file_name relation */
    /* Maintain invariant that if the main file_name of a relation
       exists, it is a complete relation */
    if (-1 == link (in_file_name, out_file_name)) {
        set_error (errno, in_file_name, "rename_hash");
        return (UNDEF);
    }

    if (UNDEF == rename_hash (in_buf, out_buf)) {
        clr_err();
    }

    /* Unlink the old filename */
    if (-1 == unlink (in_file_name)) {
        set_error (errno, in_file_name, "rename_hash");
        return (UNDEF);
    }
    return (1);
}

int
seek_hash (hash_index, entry)
int hash_index;
HASH_ENTRY *entry;
{
    HASHTAB_ENTRY *hash_node;      /* current node of hash_table    */
    
    int concept;
    int found;
    HASH_ENTRY temp_entry;

    register _SHASH *hash_ptr = &_Shash[hash_index];

    hash_ptr->in_ovfl_file = 0;

    if (entry == NULL) {
        GET_HASH_NODE (0, hash_ptr, hash_ptr->current_hashtab_entry);
        hash_ptr->current_concept = 0;
        hash_ptr->in_ovfl_file = 0;
        hash_ptr->current_position = BEGIN_HASH_ENTRY;
        return (1);
    }

    if (entry->con == UNDEF) {
        if (entry->token == NULL || entry->token[0] == '\0') {
            /* call to seek with insufficient information */
            hash_ptr->current_concept = -1;
            hash_ptr->current_hashtab_entry = NULL;
            hash_ptr->current_position = END_HASH_ENTRY;
            hash_ptr->in_ovfl_file = 0;

            set_error (SM_ILLSK_ERR, hash_ptr->file_name, "seek_hash");
            return (UNDEF);
        }

        /* search this concepts crash list */
        concept = dict_hash_func (entry->token,
                                  hash_ptr->hsh_tab_size);
        found = FALSE;
        while (!found) {
            /* get proper node (possibly from secondary store) */
            GET_HASH_NODE (concept, hash_ptr, hash_node);
            if (hash_node == NULL) {
                /* Interior consistency error */
                set_error (SM_INCON_ERR, hash_ptr->file_name, "seek_hash");
                return (UNDEF);
            }
            
            found = dict_compare_hash_entry (hash_ptr, hash_node, entry);

            if (! found) {
                if (hash_node->collision_ptr == 0) {
                    /* entry not in hashtable */
                    break;
                }
                if (hash_node->collision_ptr == IN_OVFL_TABLE) {
                    /* Next entry in overflow file. Recursively seek there */
                    if (hash_ptr->next_ovfl_index != UNDEF) {
                        found = seek_hash (hash_ptr->next_ovfl_index, entry);
                        hash_ptr->in_ovfl_file = 1;
                        return (found);
                    }
                    else {
                       set_error (SM_ILLSK_ERR, 
                                  hash_ptr->file_name,
                                  "seek_hash");
                        return (UNDEF);
                    }
                }
                else {  
                    /* Check next collision bucket in main hash_table */
                    concept = concept + hash_node->collision_ptr;
                    if (concept >= hash_ptr->hsh_tab_size)  
                        concept -= hash_ptr->hsh_tab_size;
                }
            }
        }

        hash_ptr->current_hashtab_entry = hash_node;
        hash_ptr->current_concept = concept;

        if (found) {
            hash_ptr->current_position = BEGIN_HASH_ENTRY;
            return (1);
        }
        else {
            if (EMPTY_NODE (hash_node)) {
                hash_ptr->current_position = BEGIN_HASH_ENTRY;
            }
            else {
                hash_ptr->current_position = END_HASH_ENTRY;
            }
            return (0);
        }
    }
    else {

        /* Check and see if the current_hashtab_entry is the one desired */
        if ( entry->con == hash_ptr->current_concept) {
            hash_ptr->current_position = BEGIN_HASH_ENTRY;
            if (EMPTY_NODE (hash_ptr->current_hashtab_entry)) {
                return (0);
            }
            return (1);
        }

        if (entry->con >= hash_ptr->hsh_tab_size) {
            /* Next entry in overflow file. Recursively seek there */
            if (hash_ptr->next_ovfl_index != UNDEF) {
                temp_entry.con = entry->con - hash_ptr->hsh_tab_size;
                temp_entry.info = entry->info;
                temp_entry.token = entry->token;
                found = seek_hash (hash_ptr->next_ovfl_index, &temp_entry);
                hash_ptr->in_ovfl_file = 1;
                return (found);
            }
            else {
                /* Bug: No way to currently seek to larger con then now */
                /* exists and then write. (I do not see any reason for doing*/
		/* that, but it is inconsistent). */
                hash_ptr->current_concept = UNDEF;
                return (0);
            }
        }
            
        GET_HASH_NODE (entry->con, hash_ptr, hash_node);
        if (NULL == hash_node) {
            /* Interior consistency error */
            set_error (SM_ILLSK_ERR, hash_ptr->file_name, "seek_hash");
            return (UNDEF);
        }

        hash_ptr->current_hashtab_entry = hash_node;
        hash_ptr->current_concept = entry->con;
        hash_ptr->current_position = BEGIN_HASH_ENTRY;

        if (EMPTY_NODE (hash_node)) {
            return (0);
        }
        return (1);
    }
}


int
read_hash (hash_index, hash_entry)
int hash_index;
HASH_ENTRY *hash_entry;
{
    HASHTAB_ENTRY *hash_node;
    register _SHASH *hash_ptr = &_Shash[hash_index];
    long concept;
    long number_read = 0;

    if (hash_ptr->in_ovfl_file) {
        if (hash_ptr->next_ovfl_index != UNDEF) {
            number_read = read_hash (hash_ptr->next_ovfl_index, hash_entry);
        }
        if (number_read == 1) {
            hash_entry->con += hash_ptr->hsh_tab_size;
        }
        return (number_read);
    }

    hash_node = hash_ptr->current_hashtab_entry;

    if (hash_ptr->current_position == END_HASH_ENTRY ||
        EMPTY_NODE (hash_node)) {

        /* Find next hash_node which has a valid entry */
        concept = hash_ptr->current_concept + 1;
        GET_HASH_NODE (concept, hash_ptr, hash_node);
        while (NULL != hash_node && EMPTY_NODE(hash_node)) {
            concept ++;
            GET_HASH_NODE (concept, hash_ptr, hash_node);
        }
        if (NULL == hash_node) {
            /* End of hashtable, try to read more from overflow file */
            
            if (hash_ptr->next_ovfl_index != UNDEF) {
                hash_ptr->in_ovfl_file = 1;

                /* Position at beginning of overflow file */
                (void) seek_hash (hash_ptr->next_ovfl_index, 
                                  (HASH_ENTRY *) NULL);

                /* Read from overflow file */
                number_read = read_hash(hash_ptr->next_ovfl_index, hash_entry);
                if (number_read == 1) {
                    hash_entry->con += hash_ptr->hsh_tab_size;
                }
            }
            return (number_read);
        }
        else {
            hash_ptr->current_hashtab_entry = hash_node;
            hash_ptr->current_concept = concept;
        }
    }

    if (UNDEF == dict_get_var_entry (hash_node->str_tab_off,
				hash_ptr,
				hash_entry)) {
        set_error (SM_ILLSK_ERR, hash_ptr->file_name, "read_hash");
        return (UNDEF);
    }
    hash_entry->info  = hash_node->info;
    hash_entry->con   = hash_ptr->current_concept;
        
    hash_ptr->current_position = END_HASH_ENTRY;

    return (1);
}

static int get_new_ovfl_file();
                
/* Enter a hashtable entry into the hashtable. If the concept is undefined,*/
/* then this will possibly be a new entry. If the concept is defined, then*/
/* the entry redefines the old entry. If this is a new entry, the token, */
/* and info are entered into the hash_table and string_table, */
/* BEWARE: if the concept is given, nothing is done to check that the token */
/* of the hsh_table entry match the hashtable entry. */

int
write_hash (hash_index, hash_entry)
int hash_index;
HASH_ENTRY * hash_entry;
{
    register HASHTAB_ENTRY *hash_node;  /* current node of hash_table   */
    register long concept;
    int end_search;
    long offset;
    register _SHASH *hash_ptr = &_Shash[hash_index];
    HASH_ENTRY temp_entry;
    int ovfl_index;
    int status;

    if (! (hash_ptr->mode & (SWRONLY | SRDWR))) {
        set_error (SM_ILLMD_ERR, hash_ptr->file_name, "write_hash");
        return (UNDEF);
    }

    if (hash_ptr->in_ovfl_file) {
        if (hash_entry->con >= hash_ptr->hsh_tab_size) {
            temp_entry.con = hash_entry->con - hash_ptr->hsh_tab_size;
            temp_entry.info = hash_entry->info;
            temp_entry.token = hash_entry->token;
            return ( write_hash(get_new_ovfl_file (hash_index), &temp_entry) );
        }
        else {
            /* Includes case where con is UNDEF.   Is that only case? */
            /* Establish correct con for this entry */
            /* Bug fix: 11-17-90 ChrisB */
            status = write_hash (get_new_ovfl_file (hash_index), hash_entry);
            hash_entry->con += hash_ptr->hsh_tab_size;
            return (status);
        }
    }


    if (END_HASH_ENTRY == hash_ptr->current_position) {
        /* Find location to add to current_hashtab_entrys collision chain */
        /* Search for MAX_HASH_OFFSET places within hash_table */
        concept = hash_ptr->current_concept + 1;
        if (concept >= hash_ptr->hsh_tab_size)
            concept -= hash_ptr->hsh_tab_size;
        end_search = concept - 2 + 
                      MIN(hash_ptr->hsh_tab_size, MAX_HASH_OFFSET);
        if (end_search >= hash_ptr->hsh_tab_size)
            end_search -= hash_ptr->hsh_tab_size;
        do {
            GET_HASH_NODE (concept, hash_ptr, hash_node);
            if (EMPTY_NODE (hash_node))
                break;
            concept++;
            if (concept >= hash_ptr->hsh_tab_size)
                concept -= hash_ptr->hsh_tab_size;
        } while (concept != end_search);

        if (concept == end_search) {
            /* Need to enter in overflow file */
            hash_ptr->current_hashtab_entry->collision_ptr = IN_OVFL_TABLE;
            if (UNDEF == (ovfl_index = get_new_ovfl_file (hash_index))) {
                return (UNDEF);
            }
            if (hash_entry->con != UNDEF) {
                temp_entry.con = hash_entry->con - hash_ptr->hsh_tab_size;
                temp_entry.info = hash_entry->info;
                temp_entry.token = hash_entry->token;
                if (UNDEF == seek_hash (ovfl_index, &temp_entry)) {
                    return (UNDEF);
                }
                return (write_hash (ovfl_index, &temp_entry));
            }
            else {
                if (UNDEF == seek_hash (ovfl_index, hash_entry)) {
                    return (UNDEF);
                }
                /* Establish correct con for this entry */
                /* Bug fix: 11-17-90 ChrisB */
                status = write_hash(ovfl_index, hash_entry);
                hash_entry->con += hash_ptr->hsh_tab_size;
                return (status);
            }
        }

        offset = concept - hash_ptr->current_concept;
        if (offset < 0) {
            offset += hash_ptr->hsh_tab_size;
        }

        /* Write new hash_entry information to hash_node and fix collision */
        /* chain so that current_hashtab_entrys collision pointer points to */
        /* hash_node. */
        hash_node->info = hash_entry->info;
        if (UNDEF == (hash_node->str_tab_off =
                      dict_enter_string_table (hash_node, hash_entry, hash_ptr)))
            return (UNDEF);

        /* Side note: collision_ptr may still be valid from an earlier */
        /* entry at this concept */
        hash_ptr->current_hashtab_entry->collision_ptr = offset;
        
        /* Update idea of current_entry */
        hash_ptr->current_hashtab_entry = hash_node;
        hash_ptr->current_concept = concept;

        /* Update hash_entry ideas of con, so it agrees with value actually */
        /* "written" */
        hash_entry->con = concept;

        /* Have added new entry to relation */
        hash_ptr->rh.num_entries++;
    }
    else {
        hash_node = hash_ptr->current_hashtab_entry;
        
        if (EMPTY_NODE (hash_node)) {
            hash_node->info = hash_entry->info;
            if (UNDEF == (hash_node->str_tab_off =
                          dict_enter_string_table (hash_node, hash_entry, hash_ptr)))
                return (UNDEF);

            /* Update hash_entry ideas of con, so agrees with value actually */
            /* "written" */
            hash_entry->con = hash_ptr->current_concept;

            /* Have added new entry to relation */
            hash_ptr->rh.num_entries++;
        }
        else {      /* Overwriting existing entry */
            /* FEATURE (bug?) Assume that token remains the same */
            hash_node->info = hash_entry->info;
        
            /* Delete existing tuple if the con and token fields are empty */
            /* This is done by setting str_tab_off */
            /* to UNDEF. Note that nothing is done to collision_ptr, so */
            /* collision chains are not affected */
            if ((hash_entry->token == NULL ||
                 *hash_entry->token == '\0') &&
                hash_entry->con == UNDEF) {
                /* Have deleted entry to relation */
                hash_ptr->rh.num_entries--;

                hash_node->str_tab_off = UNDEF;
            }
        }
    }

    hash_ptr->current_position = END_HASH_ENTRY;
    return (1);
}


static int
get_new_ovfl_file (current_index)
int current_index;
{
    register _SHASH *hash_ptr = &_Shash[current_index];
    char *new_path;
    REL_HEADER rh;

    if (hash_ptr->next_ovfl_index != UNDEF) {
        return (hash_ptr->next_ovfl_index);
    }

    new_path = &(hash_ptr->next_ovfl_file_name[0]);

    rh.type = S_RH_DICT;
    rh.max_primary_value =  hash_ptr->hsh_tab_size;
    if (UNDEF == create_hash (new_path, &rh)) {
        return (UNDEF);
    }

    hash_ptr->next_ovfl_index = open_hash (new_path, (long) hash_ptr->mode);
    if (hash_ptr->next_ovfl_index == UNDEF) {
        return (UNDEF);
    }

    return (hash_ptr->next_ovfl_index);
}

int
close_hash (hash_index)
int hash_index;
{
    int fd;
    register _SHASH *hash_ptr = &_Shash[hash_index];
    _SHASH *t_hash_ptr;

    if (hash_ptr->opened == 0) {
        set_error (SM_ILLMD_ERR, hash_ptr->file_name, "close_hash");
        return (UNDEF);
    }

    if (hash_ptr->next_ovfl_index != UNDEF) {
        if (UNDEF == close_hash (hash_ptr->next_ovfl_index))
            return (UNDEF);
    }

    if ((hash_ptr->mode & SINCORE) && (hash_ptr->mode & (SWRONLY | SRDWR))) {
        if (hash_ptr->mode & SBACKUP) {
            if (UNDEF == (fd = prepare_backup (hash_ptr->file_name))) {
                return (UNDEF);
            }
        }
        else if (-1 == (fd = open (hash_ptr->file_name, SWRONLY))) {
            set_error (errno, hash_ptr->file_name, "close_hash");
            return (UNDEF);
        }

        if (-1 == write (fd,
                         (char *) &hash_ptr->rh, 
                         (int) (sizeof (REL_HEADER))) ||
            -1 == write (fd,
                         (char *) hash_ptr->hsh_table, 
                       (int) (sizeof (HASHTAB_ENTRY) * hash_ptr->hsh_tab_size))||
            -1 == write (fd,
                         hash_ptr->str_table, 
                         (int) (hash_ptr->str_next_loc - 
                                hash_ptr->str_table)) ||
            -1 ==  close (fd)) {

            set_error (errno, hash_ptr->file_name, "close_hash");
            return (UNDEF);
        }
        if (hash_ptr->mode & SBACKUP) {
            if (UNDEF == make_backup (hash_ptr->file_name)) {
                return (UNDEF);
            }
        }
    }
    else {
	if (!(hash_ptr->mode & SINCORE) &&
	    -1 == close (hash_ptr->fd)) {
            set_error (errno, hash_ptr->file_name, "close_hash");
            return (UNDEF);
        }
    }
 
    /* Free up any memory alloced (except if still in use by another rel) */
    /* BUG: never frees buffers if they were shared at some point */
    if ((hash_ptr->mode & SINCORE) && hash_ptr->shared == 0) {
#ifdef MMAP
        if (hash_ptr->mode & SMMAP) {
            if (-1 == munmap (((char *) hash_ptr->hsh_table) -
                                  sizeof (REL_HEADER),
                              (int) hash_ptr->file_size)) {
                set_error (errno, hash_ptr->file_name, "close_hash");
                return (UNDEF);
            }
        }
        else {
            (void) free ((char *) hash_ptr->str_table);
            (void) free ((char *) hash_ptr->hsh_table);
        }
#else
        (void) free ((char *) hash_ptr->str_table);
        (void) free ((char *) hash_ptr->hsh_table);
#endif // MMAP
    }
    hash_ptr->opened --;
    /* Handle shared files.  All descriptors with shared set for a file
       should have the same dir_ptr->shared value (1 less than the number of
       shared descriptors) */
    if (hash_ptr->shared) {
        for (t_hash_ptr = &_Shash[0];
             t_hash_ptr < &_Shash[MAX_NUM_HASH];
             t_hash_ptr++) {
            if (hash_ptr->mode == t_hash_ptr->mode &&
                0 == strcmp (hash_ptr->file_name, t_hash_ptr->file_name)) {
                /* decrement count of relations sharing readonly buffers */
                t_hash_ptr->shared--;
            }
        }
    }

    return (0);
}
