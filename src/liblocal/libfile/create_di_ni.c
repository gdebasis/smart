#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfile/create_di_ni.c,v 11.2 1993/02/03 16:53:02 smart Exp $";
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
#include "dict_noinfo.h"
#include "io.h"
#include "smart_error.h"

int
create_dict_noinfo (file_name, rh)
char *file_name;
REL_HEADER *rh;
{
    int fd;
    register HASH_NOINFO *hash_tab;
    HASH_NOINFO hash_entry;
    register long i;
    int size;
    REL_HEADER real_rh;

    real_rh.max_primary_value = 
                        (rh == (REL_HEADER *) NULL || 
                         rh->max_primary_value == UNDEF) ?
                                    DEFAULT_DICT_SIZE :
                                    rh->max_primary_value;
    real_rh.num_entries = 0;
    real_rh._entry_size = sizeof (HASH_NOINFO);
    real_rh.type = S_RH_DICT_NOINFO;
    real_rh.num_var_files = 1;
    real_rh._internal = 0;
    real_rh.max_secondary_value = 0;

    size = real_rh.max_primary_value * sizeof (HASH_NOINFO);

    if (-1 == (fd = open (file_name, SWRONLY|SCREATE, 0664))) {
        set_error (errno, file_name, "create_dict_noinfo");
        return (UNDEF);
    }

    hash_entry.collision_ptr = 0;
    hash_entry.prefix[0] = '\0';
    hash_entry.prefix[1] = '\0';
    hash_entry.str_tab_off = UNDEF;
    

    if (NULL == (hash_tab = (HASH_NOINFO *) 
                              malloc ((unsigned) size))) {
        set_error (errno, file_name, "create_dict_noinfo");
        return (UNDEF);
    }

    for (i = 0; i < real_rh.max_primary_value; i++) {
        bcopy ((char *) &hash_entry, 
               (char *) &hash_tab[i], 
               sizeof (HASH_NOINFO));
    }

    if (sizeof (REL_HEADER) != write (fd,
                                      (char *) &real_rh, 
                                      sizeof (REL_HEADER))){
        set_error (errno, file_name, "create_dict_noinfo");
        return (UNDEF);
    }

    if (size != write (fd, (char *) hash_tab, size)) {
        set_error (errno, file_name, "create_dict_noinfo");
        return (UNDEF);
    }

    (void) free ((char *) hash_tab);
    (void) close (fd);
    
    return (0);
}
    
