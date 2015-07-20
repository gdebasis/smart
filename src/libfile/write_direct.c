#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/write_direct.c,v 11.2 1993/02/03 16:50:19 smart Exp $";
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

extern _SDIRECT _Sdirect[];

extern int create_var_file(), get_one_var_file();
extern int create_which_file(), get_which_file();
static int write_fix_entry(), select_var_file();

/* The fix file is already positioned for the new record.  If specified,
 * new_entry_num provides a check on this location.  If UNDEF and at end
 * of file, we are adding a new node and will fill in the proper entry_num
 * in new_entry_num.  If UNDEF and not at end of file, we are deleting the
 * record at the current position.
 */
int
write_direct (dir_index, new_entry_num, buf)
int dir_index;
long *new_entry_num;
SM_BUF *buf;
{
    register _SDIRECT *dir_ptr = &(_Sdirect[dir_index]);
    FILE_DIRECT *fdir_ptr, *vdir_ptr;
    FIX_ENTRY node;

    int old_entry;		/* prior contents of desired location */
    long var_offset;            /* Byte offset within var file of start of */
                                /* this nodes variable record */
    register char *ptr;
    unsigned long new_size;

    if (! (dir_ptr->mode & (SRDWR | SWRONLY))) {
        set_error (errno, dir_ptr->file_name, "write_direct");
        return (UNDEF);
    }

    fdir_ptr = &(dir_ptr->fix_dir);

    /* Make sure fix relation (or file) will still be complete after this */
    /* tuple has been added. */
    if (dir_ptr->mode & SINCORE) {
        if (*new_entry_num == UNDEF &&
				dir_ptr->fix_pos >= fdir_ptr->enddata) {
            *new_entry_num = (fdir_ptr->foffset - sizeof (REL_HEADER) +
                                  (fdir_ptr->enddata - fdir_ptr->mem)) /
                                 sizeof(FIX_ENTRY);
        }

        /* Need to possibly "zero" out portions of fixed file if this */
        /* location is beyond end of present initialized portion. */
        while (dir_ptr->fix_pos >= fdir_ptr->end) {
            new_size = 2*(fdir_ptr->end - fdir_ptr->mem) +
                    sizeof(FIX_ENTRY) * FIX_INCR;
            if (NULL == (ptr = realloc (fdir_ptr->mem, (unsigned) new_size))) {
                set_error (errno, dir_ptr->file_name, "write_direct");
                return (UNDEF);
            }
            dir_ptr->fix_pos = ptr + (dir_ptr->fix_pos - fdir_ptr->mem);
            fdir_ptr->end = ptr + new_size;
            fdir_ptr->enddata = ptr + (fdir_ptr->enddata - fdir_ptr->mem);
            fdir_ptr->mem = ptr;

            /* Initialize all new nodes to var_length of UNDEF */
	    node.var_offset = 0;
	    node.var_length = UNDEF;
            for ( ptr = fdir_ptr->enddata;
                  ptr < fdir_ptr->end; 
                  ptr += sizeof(FIX_ENTRY)) {
		(void) memcpy(ptr, (char *)&node, sizeof(FIX_ENTRY));
            }
        }

	node = *((FIX_ENTRY *)dir_ptr->fix_pos);
	old_entry = (node.var_length != UNDEF);
    }
    else {
        if (*new_entry_num == UNDEF && fdir_ptr->foffset >= fdir_ptr->size) {
            *new_entry_num = (fdir_ptr->foffset - sizeof (REL_HEADER)) /
                                 sizeof(FIX_ENTRY);
        }

	/* get old_entry, if at hand */
        if (dir_ptr->fix_pos == fdir_ptr->mem) {
	    node = *((FIX_ENTRY *)dir_ptr->fix_pos);
	    old_entry = (node.var_length != UNDEF);
        } else
            old_entry = FALSE;

        /* fix file is not in core, may have to write out all intermediate */
        /* entries if there are holes.  Seek to desired foffset */
        if (fdir_ptr->foffset >= fdir_ptr->size) {
            (void) lseek (fdir_ptr->fd, 0L, 2);
	    node.var_offset = 0;
	    node.var_length = UNDEF;
            while (fdir_ptr->foffset > fdir_ptr->size) {
                if (sizeof(FIX_ENTRY) != write (fdir_ptr->fd, (char *)&node, 
                                               sizeof(FIX_ENTRY))) {
                    set_error (errno, dir_ptr->file_name, "write_direct");
                    return (UNDEF);
                }
                fdir_ptr->size += sizeof(FIX_ENTRY);
            }
        }
        else {
            (void) lseek (fdir_ptr->fd, fdir_ptr->foffset, 0);
        }

    }        

    /* Update count of number of tuples in relation */
    if (*new_entry_num > dir_ptr->rh.max_primary_value) {
        dir_ptr->rh.max_primary_value = *new_entry_num;
    }
    if (*new_entry_num == UNDEF) {
        if (old_entry)
            dir_ptr->rh.num_entries--;
    } else {
        if (!old_entry)
            dir_ptr->rh.num_entries++;
    }

    if (*new_entry_num == UNDEF) {
	/* Deleting existing node.  Only need to overwrite fixed file */
	node.var_offset = 0;
	node.var_length = UNDEF;
	if (UNDEF == write_fix_entry (dir_ptr, &node)) {
	    return (UNDEF);
	}
        return (0);
    }

    /* determine which var file to write in, and where */
    if (UNDEF == select_var_file(dir_index, *new_entry_num, buf->size,
					&vdir_ptr, &var_offset))
	return(UNDEF);

    /* Fill in fix information.  Do not write out node until after var
     * entries have been written (to ensure reliability of fix information).
     */
    node.var_offset = var_offset;
    node.var_length = buf->size;

    if (dir_ptr->mode & SINCORE) {
	/* make sure there's space for new record in var incore buffer */
	if (vdir_ptr->enddata + buf->size >= vdir_ptr->end) {
	    new_size = 2*(vdir_ptr->end - vdir_ptr->mem) +
						    VAR_INCR + buf->size;
	    ptr = realloc (vdir_ptr->mem, (unsigned) new_size);
	    if (NULL == ptr) {
		set_error (errno, dir_ptr->file_name, "write_direct");
		return (UNDEF);
	    }
	    vdir_ptr->enddata = ptr + (vdir_ptr->enddata - vdir_ptr->mem);
	    vdir_ptr->end = ptr + new_size;
	    vdir_ptr->mem = ptr;
	}

	/* add record to var incore buffer */
	(void) memcpy(vdir_ptr->enddata, buf->buf, buf->size);
	vdir_ptr->enddata += buf->size;
    } else {
	/* Actually write out variable record if not SINCORE */
        if (buf->size) {
            if (buf->size != write (vdir_ptr->fd, buf->buf, (int) buf->size)) {
                set_error (errno, dir_ptr->file_name, "write_direct");
                return (UNDEF);
            }
            vdir_ptr->size += buf->size;
        }
    }

    if (UNDEF == write_fix_entry (dir_ptr, &node)) {
        return (UNDEF);
    }

    return (1);
}


/* Actually add the fix_entry to the existing fixed relation or file */
static int
write_fix_entry (dir_ptr, node)
register _SDIRECT *dir_ptr;
register FIX_ENTRY *node;
{
    FILE_DIRECT *fdir_ptr = &(dir_ptr->fix_dir);

    if (dir_ptr->mode & SINCORE) {
        (void) memcpy(dir_ptr->fix_pos, (char *)node, sizeof(FIX_ENTRY));
        dir_ptr->fix_pos += sizeof(FIX_ENTRY);
        if (dir_ptr->fix_pos > fdir_ptr->enddata) {
            fdir_ptr->enddata = dir_ptr->fix_pos;
        }
    }
    else {
        if (sizeof(FIX_ENTRY) != write (fdir_ptr->fd, (char *)node,
                                       sizeof(FIX_ENTRY))) {
            set_error (errno, dir_ptr->file_name, "write_direct");
            dir_ptr->fix_pos = NULL;
            return (UNDEF);
        }
        fdir_ptr->foffset += sizeof(FIX_ENTRY);
        if (fdir_ptr->foffset > fdir_ptr->size) {
            fdir_ptr->size = fdir_ptr->foffset;
        }
    }
    return (0);
}


/* Determine which var file to write new tuple in, and where.  Create
 * new var files and "which" file as necessary, and update existing
 * "which" file.  Relies on rh.max_primary_value being previously updated.
 */
static int
select_var_file(dir_index, new_entry_num, new_entry_size, vdir_ptr, var_offset)
int dir_index;
long new_entry_num;
int new_entry_size;
FILE_DIRECT **vdir_ptr;
long *var_offset;
{
    register _SDIRECT *dir_ptr = &(_Sdirect[dir_index]);
    FILE_DIRECT *wdir_ptr;
    int prev_var_files;
    long old_which_size, new_which_size, i;
    char *cp;
    long to_write;
    int this_time;
    char buf[PATH_LEN];

    /* until we do garbage collection, we're always appending var data
     * after the existing tuples, so it's just a matter of whether the
     * data fits in the highest var file.
     */
    *vdir_ptr = &(dir_ptr->var_dir[dir_ptr->rh.num_var_files - 1]);

    /* find end of data in current var file */
    if (dir_ptr->mode & SINCORE) {
        *var_offset = (*vdir_ptr)->foffset +
				((*vdir_ptr)->enddata - (*vdir_ptr)->mem);
    } else {
        (void) lseek ((*vdir_ptr)->fd, 0L, 2);
        *var_offset = (*vdir_ptr)->size;
        (*vdir_ptr)->enddata = (*vdir_ptr)->mem;
    }

    if (*var_offset > MAX_VAR_FILE_SIZE - new_entry_size) {
	/* new tuple won't fit, we need to add another var file */
	if (dir_ptr->rh.num_var_files + 1 >= MAX_NUM_VAR_FILES) {
	    set_error(EFBIG, dir_ptr->file_name, "write_direct");
	    return(UNDEF);
	}

	prev_var_files = dir_ptr->rh.num_var_files;
	*vdir_ptr = (FILE_DIRECT *)realloc((char *)dir_ptr->var_dir,
			(unsigned)(prev_var_files + 1) * sizeof(FILE_DIRECT));
	if ((FILE_DIRECT *)0 == *vdir_ptr) {
	    set_error(errno, dir_ptr->file_name, "write_direct");
	    return(UNDEF);
	}
	dir_ptr->var_dir = *vdir_ptr;
	dir_ptr->rh.num_var_files++;

	*vdir_ptr = &(dir_ptr->var_dir[prev_var_files]);
	*var_offset = 0L;

	if (UNDEF == create_var_file(dir_ptr->file_name, prev_var_files) ||
	    UNDEF == get_one_var_file(dir_ptr, *vdir_ptr, prev_var_files)) {
	    return(UNDEF);
	}

	if (prev_var_files == 1) {
	    /* need to create an initial "which" file */
	    if (UNDEF == create_which_file(dir_index) ||
		UNDEF == get_which_file(dir_index)) {
		return(UNDEF);
	    }
	}
    } else if (dir_ptr->rh.num_var_files == 1) {
	/* no which file yet to need updating */
	return(0);
    }

    wdir_ptr = &(dir_ptr->which_dir);
    new_which_size = dir_ptr->rh.max_primary_value + 1;

    if (dir_ptr->mode & SINCORE) {
	old_which_size = wdir_ptr->enddata - wdir_ptr->mem;
	if (old_which_size < new_which_size) {
	    /* make sure there's space */
	    if (wdir_ptr->end - wdir_ptr->mem < new_which_size) {
		cp = realloc(wdir_ptr->mem,
				(unsigned)(2*new_which_size + WHICH_INCR));
		if ((char *)0 == cp) {
		    set_error(errno, "which file", "write_direct");
		    return(UNDEF);
		}
		wdir_ptr->end = cp + 2*new_which_size + WHICH_INCR;
		wdir_ptr->enddata = cp + (wdir_ptr->enddata - wdir_ptr->mem);
		wdir_ptr->mem = cp;
	    }

	    /* extend which data (0 is fine for unused entries) */
	    for (i = old_which_size; i < new_which_size; i++)
		wdir_ptr->mem[i] = 0;
	    wdir_ptr->enddata = wdir_ptr->mem + new_which_size;
	}

	/* update "which" entry */
	wdir_ptr->mem[new_entry_num] = dir_ptr->rh.num_var_files - 1;
    } else {	/* !SINCORE */
	old_which_size = wdir_ptr->size;
	if (old_which_size < new_which_size) {
	    /* make sure there's space */
	    (void) memset(buf, '\0', PATH_LEN);
	    to_write = new_which_size - old_which_size;

	    (void) lseek(wdir_ptr->fd, 0L, 2);
	    while (to_write > 0) {
		if (to_write > PATH_LEN)
		    this_time = PATH_LEN;
		else
		    this_time = to_write;

		if (this_time != write(wdir_ptr->fd, buf, this_time)) {
		    set_error(errno, "which file", "write_direct");
		    return(UNDEF);
		}
		to_write -= this_time;
	    }
	    wdir_ptr->size = new_which_size;
	}

	/* update "which" entry */
	(void) lseek(wdir_ptr->fd, new_entry_num, 0);
	buf[0] = dir_ptr->rh.num_var_files - 1;
	if (1 != write(wdir_ptr->fd, buf, 1)) {
	    set_error(errno, "which file", "write_direct");
	    return(UNDEF);
	}
    }

    return(0);
}
