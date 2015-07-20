#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/read_direct.c,v 11.2 1993/02/03 16:50:18 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "buf.h"
#include "functions.h"
#include "rel_header.h"
#include "direct.h"
#include "io.h"
#include "smart_error.h"
#ifdef MMAP
#   include <sys/types.h>
#   include <sys/mman.h>
#endif	/* MMAP */

extern _SDIRECT _Sdirect[];

int
read_direct(dir_index, entry_num, buf)
int dir_index;              /* index to fast-direct direct */
long *entry_num;
SM_BUF *buf;
{
    register _SDIRECT *dir_ptr = &(_Sdirect[dir_index]);
    FILE_DIRECT *fdir_ptr, *vdir_ptr;
    FIX_ENTRY node;
    long num_read;
    char *var_ptr;              /* pointer to current entry in var buffer */

    if (dir_ptr->mode & SWRONLY) {
        set_error (SM_ILLMD_ERR, dir_ptr->file_name, "read_direct");
        return (UNDEF);
    }

    fdir_ptr = &(dir_ptr->fix_dir);

    /* Get next non_empty record in fast access file */
    if (dir_ptr->mode & SINCORE) {
        if (dir_ptr->fix_pos >= fdir_ptr->enddata) {
            /* if EOF, return 0 to caller */
            return(0);
        }

	node = *((FIX_ENTRY *)dir_ptr->fix_pos);
        dir_ptr->fix_pos += sizeof(FIX_ENTRY);

        while (node.var_length == UNDEF) {  /* if node is UNDEF */
            /* Disregard this node, advance to the next one */
            if (dir_ptr->fix_pos >= fdir_ptr->enddata) {
                /* if EOF, return 0 to caller */
                return(0);
            }
	    node = *((FIX_ENTRY *)dir_ptr->fix_pos);
            dir_ptr->fix_pos += sizeof(FIX_ENTRY);
        }
	*entry_num = (dir_ptr->fix_pos - fdir_ptr->mem) / sizeof(FIX_ENTRY)
				- 1;
    }
    else {
        /* fix file is not in core, bring entries in (if needed) */
        if (fdir_ptr->foffset >= fdir_ptr->size) {
            /* if EOF return 0 to caller */
            return (0);
        }
        
        if (dir_ptr->fix_pos == fdir_ptr->mem) {
            /* Entry read during last seek may be valid */
            /* NOTE: depend on last action being seek to avoid physical */
            /* seek of file */
	    node = *((FIX_ENTRY *)dir_ptr->fix_pos);
            fdir_ptr->foffset += sizeof(FIX_ENTRY);
        }
        else {
            node.var_length = UNDEF;
            (void) lseek (fdir_ptr->fd, fdir_ptr->foffset, 0);
        }
        while (node.var_length == UNDEF) {
            /* Disregard this direct node, advance to the next one */
            if (fdir_ptr->foffset >= fdir_ptr->size) {
                /* if EOF, return 0 to caller */
                dir_ptr->fix_pos = NULL;
                return(0);
            }
            fdir_ptr->foffset += sizeof(FIX_ENTRY);
            if (sizeof(FIX_ENTRY) != 
                            read (fdir_ptr->fd, (char *)&node,
                                  sizeof(FIX_ENTRY))) {
                set_error (errno, dir_ptr->file_name, "read_direct");
                dir_ptr->fix_pos = NULL;
                return (UNDEF);
            }
        }
        dir_ptr->fix_pos = NULL;

	*entry_num = (fdir_ptr->foffset - sizeof(REL_HEADER))
				/ sizeof(FIX_ENTRY) - 1;
    }

    /* determine which var file to look in */
    if (dir_ptr->rh.num_var_files == 1) {
	vdir_ptr = dir_ptr->var_dir;
    } else {
	FILE_DIRECT *wdir_ptr = &(dir_ptr->which_dir);
	int i;

	if (dir_ptr->mode & SINCORE) {
	    if (*entry_num >= wdir_ptr->enddata - wdir_ptr->mem) {
		set_error(SM_INCON_ERR, "which size", "read_direct");
		return(UNDEF);
	    }

	    i = wdir_ptr->mem[*entry_num];
	} else {
	    if (*entry_num >= wdir_ptr->size) {
		set_error(SM_INCON_ERR, "which size", "read_direct");
		return(UNDEF);
	    }

	    (void) lseek(wdir_ptr->fd, *entry_num, 0);
	    if (1 != read(wdir_ptr->fd, wdir_ptr->mem, 1)) {
		set_error(errno, "which read", "read_direct");
		return(UNDEF);
	    }
	    wdir_ptr->foffset = *entry_num;
	    i = wdir_ptr->mem[0];
	}
	vdir_ptr = &(dir_ptr->var_dir[i]);
    }

    /* Determine where in memory the variable records for this tuple start */
    /* If not in core, they need to be brought into core */
    if (dir_ptr->mode & SINCORE) {
        var_ptr = vdir_ptr->mem + node.var_offset;

#ifdef MMAP
    } else if (dir_ptr->mode & SIMMAP) {
	size_t mmap_len;

	if (vdir_ptr->mem) {
		if (-1 == munmap(vdir_ptr->mem,
				vdir_ptr->enddata - vdir_ptr->mem))
			return (UNDEF);
	}
	/* find beginning of first page for tuple */
	/* (assuming page size must be a power of 2) */
	vdir_ptr->foffset = node.var_offset & ~(getpagesize() - 1L);
	mmap_len = node.var_length + (node.var_offset - vdir_ptr->foffset);

	vdir_ptr->mem = mmap((caddr_t)0, mmap_len,
				PROT_READ, MAP_SHARED, vdir_ptr->fd,
				(off_t) vdir_ptr->foffset);
	if ((char *)-1 == vdir_ptr->mem) {
	    set_error (errno, dir_ptr->file_name, "read_direct");
	    vdir_ptr->mem = (char *)0;
	    return (UNDEF);
	}
	vdir_ptr->enddata = vdir_ptr->mem + mmap_len;
	vdir_ptr->end = vdir_ptr->enddata;

	var_ptr = vdir_ptr->mem + (node.var_offset - vdir_ptr->foffset);
#endif	/* MMAP */

    } else {
        if (node.var_length > vdir_ptr->end - vdir_ptr->mem) {
            if (NULL == (vdir_ptr->mem = realloc (vdir_ptr->mem,
                                            (unsigned)node.var_length))) {
                set_error (errno, dir_ptr->file_name, "read_direct");
                vdir_ptr->end = NULL;
                return (UNDEF);
            }
            vdir_ptr->end = vdir_ptr->mem + node.var_length;
        }
        
        if (node.var_length > 0) {
            (void) lseek (vdir_ptr->fd, node.var_offset, 0);
            if (0 >= (num_read = read (vdir_ptr->fd, vdir_ptr->mem,
                                       (int) node.var_length))) {
                set_error (errno, dir_ptr->file_name, "read_direct");
                return (UNDEF);
            }
            vdir_ptr->enddata = vdir_ptr->mem + num_read;
        }
        else 
            vdir_ptr->enddata = vdir_ptr->mem;
        vdir_ptr->foffset = node.var_offset;

        var_ptr = vdir_ptr->mem;
    }

    buf->buf = var_ptr;
    buf->size = node.var_length;

    return (1);
}
