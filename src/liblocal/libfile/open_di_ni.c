#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfile/open_di_ni.c,v 11.2 1993/02/03 16:53:04 smart Exp $";
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
#include "dict_noinfo.h"
#include "io.h"
#include "smart_error.h"
#ifdef MMAP
#   include <sys/types.h>
#   include <sys/mman.h>
#endif   /* MMAP */

int create_dict_noinfo(), seek_dict_noinfo();

extern _SDICT_NOINFO _Sdict_noinfo[];

/*  int open_dict_noinfo (file_name, mode); */
/*          char *file_name, int mode; */
/*      Returns <file_index> to be used by other routines to access this */
/*      dictionary. Returns UNDEF if cannot access files. */
/*      Prepares access to all UNIX files associated with dictionary */
/*      <file_name>. */
/*      Depending on <mode>, files may be read entirely into core */
/*      <file_name> has maximum length (PATH_LEN - 4) currently 1020 */
/*      <mode> has values as given in "io.h" */

static int get_file_header(), get_file(), get_next_ovfl_file();
static void get_dict_ovfl_file();

#ifdef MMAP
static int get_file_mmap();
#endif // MMAP


int
open_dict_noinfo(file_name, mode)
char *file_name;
long  mode;
{
    int dict_index;
    register _SDICT_NOINFO *dict_ptr;
    _SDICT_NOINFO *already_open, *tdict_ptr;
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

    /* Find first open slot in dict table. */
    /* Minor optimization of allowing two or more RDONLY,INCORE invocations */
    /* of the same dictionary to share the incore image */
    already_open = NULL;
    dict_ptr = NULL;
    for ( tdict_ptr = &_Sdict_noinfo[0]; 
          tdict_ptr < &_Sdict_noinfo[MAX_NUM_DICT];
          tdict_ptr++) {
        if ( mode == tdict_ptr->mode &&
             (mode & SINCORE) &&
             (! (mode & (SWRONLY|SRDWR))) &&
             strncmp (file_name, tdict_ptr->file_name,PATH_LEN) == 0 &&
             tdict_ptr->opened > 0) {
            /* increment count of number relations sharing readonly buffers */
            tdict_ptr->shared++;
            already_open = tdict_ptr;
        }
        else if (tdict_ptr->opened == 0 && dict_ptr == NULL) {
            /* Assign dict-ptr to first open spot */
            dict_ptr = tdict_ptr;
        }
    }

    if (dict_ptr == NULL) {
        set_error (EMFILE, file_name, "open_dict_noinfo");
        return (UNDEF);
    }
    dict_index = dict_ptr - &_Sdict_noinfo[0];
    
    if (already_open != NULL) {
        bcopy ((char *) already_open, (char *) dict_ptr, sizeof (_SDICT_NOINFO));
        (void) seek_dict_noinfo (dict_index, (DICT_NOINFO *) NULL);
        if (dict_ptr->next_ovfl_index != UNDEF) {
            if (UNDEF == (dict_ptr->next_ovfl_index =
                          open_dict_noinfo (dict_ptr->next_ovfl_file_name,
                                     dict_ptr->mode)))
                return (UNDEF);
        }
        return (dict_index);
    }

    /* Set default modes.  Presently must have dictionary in core to write. */
    /* Also, must have read access to file at all times. */
    if (mode & SWRONLY) {
        mode = (mode & ~SWRONLY) | SRDWR;
    }
    if ((! (mode & SINCORE)) && (mode & SRDWR)) {
        mode = mode | SINCORE;
    }
    
    dict_ptr->mode = mode;
    (void) strncpy (dict_ptr->file_name, file_name, PATH_LEN);

    if (mode & SCREATE) {
        temp_rel_entry.max_primary_value = DEFAULT_DICT_SIZE;
        if (UNDEF == create_dict_noinfo (dict_ptr->file_name, &temp_rel_entry)) {
            return (UNDEF);
        }
        dict_ptr->mode = mode & (~SCREATE);
    }
    
    if (-1 == (dict_ptr->fd = open (file_name, (int) (mode & SMASK)))) {
        set_error (errno, file_name, "open_dict_noinfo");
        return (UNDEF);
    }
    
    dict_ptr->file_size = lseek (dict_ptr->fd, 0L, 2);
    (void) lseek (dict_ptr->fd, 0L, 0);

    if (mode & SINCORE) {
#ifdef MMAP
        if (mode & SMMAP) {
            if (UNDEF == get_file_mmap (dict_index))
                return (UNDEF);
        }
        else if (UNDEF == get_file (dict_index))
            return (UNDEF);
#else
        if (UNDEF == get_file (dict_index))
            return (UNDEF);
#endif /* MMAP */
    }
    else if (mode & (SWRONLY | SRDWR)) {
        /* Ignore advice and read in_core, set the SINCORE bit in mode */
        if (UNDEF == get_file (dict_index)) {
            return (UNDEF);
        }
        mode = mode | SINCORE;
    }
    else {
        if (UNDEF == get_file_header (dict_index)) {
            return (UNDEF);
        }
    }

    dict_ptr->opened = 1;
    if (UNDEF == (dict_ptr->next_ovfl_index = 
                            get_next_ovfl_file (dict_index))) {
        clr_err();
    }

    /* Position at beginning of file */
    (void) seek_dict_noinfo (dict_index, (DICT_NOINFO *) NULL);

    return (dict_index);
}


static int
get_file (dict_index)
int dict_index;
{
    int hash_size, string_size;
    int reserved_size;
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];
    
    /* Get header at beginning of file */
    if (sizeof (REL_HEADER) != read (dict_ptr->fd,
                                     (char *) &dict_ptr->rh,
                                     sizeof (REL_HEADER))) {
        set_error (errno, dict_ptr->file_name, "open_dict_noinfo");
        return (UNDEF);
    }
    
    /* Read hash table portion of file */
    dict_ptr->hsh_tab_size = dict_ptr->rh.max_primary_value;
    hash_size = dict_ptr->hsh_tab_size * sizeof (HASH_NOINFO);
    
    if (NULL == (dict_ptr->hsh_table = 
                     (HASH_NOINFO *) malloc ((unsigned) hash_size))) {
        set_error (errno, dict_ptr->file_name, "open_dict_noinfo");
        return (UNDEF);
    }
    
    if (hash_size != read (dict_ptr->fd,
                      (char *) dict_ptr->hsh_table, 
                      (int) hash_size)) {
        set_error (errno, dict_ptr->file_name, "open_dict_noinfo");
        return (UNDEF);
    }


    /* Now read string table portion of file */

    string_size = dict_ptr->file_size - sizeof (REL_HEADER) - hash_size;

    reserved_size = (2 * string_size + DICT_MALLOC_SIZE);

    if (NULL == (dict_ptr->str_table = 
                     malloc ((unsigned) reserved_size))) {
        set_error (errno, dict_ptr->file_name, "open_dict_noinfo");
        return (UNDEF);
    }
    
    if (string_size != read (dict_ptr->fd, 
                             dict_ptr->str_table, 
                             string_size)) {
        set_error (errno, dict_ptr->file_name, "open_dict_noinfo");
        return (UNDEF);
    }
    
    
    dict_ptr->str_tab_size = reserved_size;
    dict_ptr->str_next_loc = dict_ptr->str_table + string_size;

    (void) close (dict_ptr->fd);
    return (0);
}

#ifdef MMAP
static int
get_file_mmap (dict_index)
int dict_index;
{
    char *dict_map;
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];
    
    /* Get header at beginning of file */
    if (sizeof (REL_HEADER) != read (dict_ptr->fd,
                                     (char *) &dict_ptr->rh,
                                     sizeof (REL_HEADER))) {
        set_error (errno, dict_ptr->file_name, "open_dict_noinfo");
        return (UNDEF);
    }
    
    /* Map entire file into memory (must be SRDONLY else SMMAP
       wouldn't have been set) */
    if ((char *) -1 == (dict_map =
                        mmap ((caddr_t) 0,
                              (size_t) dict_ptr->file_size,
                              PROT_READ,
                              MAP_SHARED,
                              dict_ptr->fd,
                              (off_t) 0))) {
        set_error (errno, dict_ptr->file_name, "open_direct");
        return (UNDEF);
    }
    /* Get hash table portion of file */
    dict_ptr->hsh_tab_size = dict_ptr->rh.max_primary_value;
    dict_ptr->hsh_table = (HASH_NOINFO *) (dict_map + sizeof (REL_HEADER));

    /* Now get string table portion of file */
    dict_ptr->str_table = (char *) dict_ptr->hsh_table +
        dict_ptr->hsh_tab_size * sizeof (HASH_NOINFO);
    dict_ptr->str_tab_size = dict_ptr->file_size - sizeof (REL_HEADER) -
        dict_ptr->hsh_tab_size * sizeof (HASH_NOINFO);
    dict_ptr->str_next_loc = NULL;

    (void) close (dict_ptr->fd);
    return (0);
}
#endif  /* MMAP */


static int
get_file_header (dict_index)
int dict_index;
{
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];

    /* Get header at beginning of file */
    if (sizeof (REL_HEADER) != read (dict_ptr->fd,
                                     (char *) &dict_ptr->rh,
                                     sizeof (REL_HEADER))) {
        set_error (errno, dict_ptr->file_name, "open_dict_noinfo");
        return (UNDEF);
    }

    dict_ptr->hsh_tab_size = dict_ptr->rh.max_primary_value;

    return (0);
}


static int
get_next_ovfl_file (current_index)
int current_index;
{
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[current_index];

    (void) get_dict_ovfl_file (dict_ptr->file_name,
                               dict_ptr->next_ovfl_file_name);

    return (open_dict_noinfo (dict_ptr->next_ovfl_file_name, (long) dict_ptr->mode));
}


static void
get_dict_ovfl_file (old_path, new_path)
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
destroy_dict_noinfo (file_name)
char *file_name;
{
    char buf[PATH_LEN];

    if (-1 == unlink (file_name)) {
        set_error (errno, file_name, "destroy_dict_noinfo");
        return (UNDEF);
    }

    /* Unlink any overflow files */
    get_dict_ovfl_file (file_name, buf);
    if (UNDEF == destroy_dict_noinfo (buf)) {
        clr_err ();
    }

    return (1);
}
    
/* Rename an instance of a dictionary relational object. */
/* Bug at moment: can't distinguish real error in renaming an
  overflow file from overflow file not existing. */
int
rename_dict_noinfo (in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    char in_buf[PATH_LEN+4];
    char out_buf[PATH_LEN+4];

    /* Name the next overflow file */
    get_dict_ovfl_file (in_file_name, in_buf);
    get_dict_ovfl_file (out_file_name, out_buf);

    /* Link out_file_name to the existing in_file_name relation */
    /* Maintain invariant that if the main file_name of a relation
       exists, it is a complete relation */
    if (-1 == link (in_file_name, out_file_name)) {
        set_error (errno, in_file_name, "rename_dict_noinfo");
        return (UNDEF);
    }

    if (UNDEF == rename_dict_noinfo (in_buf, out_buf)) {
        clr_err();
    }

    /* Unlink the old filename */
    if (-1 == unlink (in_file_name)) {
        set_error (errno, in_file_name, "rename_dict_noinfo");
        return (UNDEF);
    }
    return (1);
}
