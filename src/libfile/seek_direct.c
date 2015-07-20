#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/seek_direct.c,v 11.2 1993/02/03 16:50:32 smart Exp $";
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
#include "direct.h"
#include "io.h"
#include "smart_error.h"

extern _SDIRECT _Sdirect[];


int
seek_direct (dir_index, entry_num)
int dir_index;
register long entry_num;
{   
    char *fix_ptr;
    register _SDIRECT *dir_ptr = &(_Sdirect[dir_index]);
    FILE_DIRECT *fdir_ptr = &(dir_ptr->fix_dir);
    FIX_ENTRY *node;

    if (dir_ptr->mode & SINCORE) {
        if (entry_num == SEEK_FIRST_NODE) {
            /* Seek to beginning of file */
            dir_ptr->fix_pos = fdir_ptr->mem;
            return (1);
        }
        
        if (entry_num == SEEK_NEW_NODE) {
            /* seek to end of file, to prepare for a write of a new node */
            dir_ptr->fix_pos = fdir_ptr->enddata;
            return (0);
        }
    
        /* 'seek' to location in fast-access to read in node */
        fix_ptr = fdir_ptr->mem + entry_num * sizeof(FIX_ENTRY);
        dir_ptr->fix_pos = fix_ptr;
        
        if (fix_ptr >= fdir_ptr->enddata) {
            /* did is too large */
            return (0);
        }
    
        /* get node */
	node = (FIX_ENTRY *)fix_ptr;
    
        if (node->var_length == UNDEF) {
            /* node has been removed or was never there */
            return (0);
        }
    
        /* return successful completion */
        return (1);
    }
    else { /* fix file in not in core */
        if (entry_num == SEEK_FIRST_NODE) {
            /* Seek to beginning of file and read first entry (if exists) */
            fdir_ptr->foffset = sizeof (REL_HEADER);
            (void) lseek (fdir_ptr->fd, fdir_ptr->foffset, 0);
            if (fdir_ptr->size <= sizeof (REL_HEADER)) {
                dir_ptr->fix_pos = NULL;
            }
            else {
                if (sizeof(FIX_ENTRY) != 
                                read (fdir_ptr->fd, fdir_ptr->mem,
                                      sizeof(FIX_ENTRY))) {
                    set_error (errno, dir_ptr->file_name, "seek_direct");
                    dir_ptr->fix_pos = NULL;
                    return (UNDEF);
                }
                dir_ptr->fix_pos = fdir_ptr->mem;
            }
            
            return (1);
        }
        
        if (entry_num == SEEK_NEW_NODE) {
            /* "seek" to end of file, to prepare for a write of a new node */
            fdir_ptr->foffset = fdir_ptr->size;
            dir_ptr->fix_pos = NULL;
            return (0);
        }
    
        fdir_ptr->foffset = entry_num * sizeof(FIX_ENTRY) + sizeof (REL_HEADER);

        if (fdir_ptr->foffset >= fdir_ptr->size) {
            dir_ptr->fix_pos = NULL;
            return (0);
        }
        
        (void) lseek (fdir_ptr->fd, fdir_ptr->foffset, 0);
        if (sizeof(FIX_ENTRY) != 
                        read (fdir_ptr->fd, fdir_ptr->mem, sizeof(FIX_ENTRY))) {
            set_error (errno, dir_ptr->file_name, "seek_direct");
            dir_ptr->fix_pos = NULL;
            return (UNDEF);
        }
        dir_ptr->fix_pos = fdir_ptr->mem;
        
        /* get node */
	node = (FIX_ENTRY *)fdir_ptr->mem;
    
        if (node->var_length == UNDEF) {
            /* node has been removed or was never there */
            return (0);
        }
    
        /* return successful completion */
        return (1);
    }
}
