#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/close_direct.c,v 11.2 1993/02/03 16:50:11 smart Exp $";
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

static int close_fix_file(), close_which_file(), close_var_file();

/* buffers are only shared on SRDONLY/SINCORE, although there are
 * provisions here for sharing in other cases.
 */

int
close_direct (dir_index)
int dir_index;
{
    register _SDIRECT *dir_ptr = &_Sdirect[dir_index];
    _SDIRECT *tdir_ptr;

    if (dir_ptr->opened == 0) {
        set_error (EBADF, dir_ptr->file_name, "close_direct");
        return (UNDEF);
    }

    if (UNDEF == close_var_file(dir_index))
	return(UNDEF);

    if (UNDEF == close_which_file(dir_index))
	return(UNDEF);

    if (UNDEF == close_fix_file(dir_index))
	return(UNDEF);

    dir_ptr->opened--;
    /* Handle shared files.  All descriptors with shared set for a file
       should have the same dir_ptr->shared value (1 less than the number of
       shared descriptors) */
    if (dir_ptr->shared) {
        for (tdir_ptr = &_Sdirect[0];
             tdir_ptr < &_Sdirect[MAX_NUM_DIRECT];
             tdir_ptr++) {
            if (dir_ptr->mode == tdir_ptr->mode &&
                0 == strcmp (dir_ptr->file_name, tdir_ptr->file_name)) {
                /* decrement count of relations sharing readonly buffers */
                tdir_ptr->shared--;
            }
        }
    }
    return (0);
}

static int
close_fix_file(dir_index)
int dir_index;
{
    int fd;
    register _SDIRECT *dir_ptr = &_Sdirect[dir_index];
    FILE_DIRECT *fdir_ptr;
    long fix_size;

    fdir_ptr = &(dir_ptr->fix_dir);
    if (dir_ptr->mode & (SWRONLY | SRDWR)) {
        /* Write out fix file */
        if (dir_ptr->mode & SINCORE) {
            if (dir_ptr->mode & SBACKUP) {
                if (UNDEF == (fd = prepare_backup (dir_ptr->file_name))) {
                    return (UNDEF);
                }
            }
            else if (-1 == (fd = open (dir_ptr->file_name, SWRONLY))) {
                set_error (errno, dir_ptr->file_name, "close_direct");
                return (UNDEF);
            }

            fix_size = fdir_ptr->enddata - fdir_ptr->mem;

            /* Consistency check */
            if (fix_size != (dir_ptr->rh.max_primary_value+1) *
                            sizeof(FIX_ENTRY)) {
                set_error (SM_INCON_ERR, dir_ptr->file_name, "close_direct");
                return (UNDEF);
            }

            if (sizeof (REL_HEADER) != write (fd,
                                              (char *) &dir_ptr->rh,
                                              (int) sizeof (REL_HEADER))) {
                set_error (errno, dir_ptr->file_name, "close_direct");
                return (UNDEF);
            }

            if (fix_size != write (fd, (char *) fdir_ptr->mem,
                                   (int) fix_size)) {
                set_error (errno, dir_ptr->file_name, "close_direct");
                return (UNDEF);
            }

            if (dir_ptr->shared == 0) {
                (void) free (fdir_ptr->mem);
            }
            (void) close (fd);
        }
        else {
            /* update rel_header information */

            /* Consistency check */
            if (fdir_ptr->size != (dir_ptr->rh.max_primary_value + 1) *
                              sizeof(FIX_ENTRY) + sizeof (REL_HEADER)) {
                set_error (SM_INCON_ERR, dir_ptr->file_name, "close_direct");
                return (UNDEF);
            }

            (void) lseek (fdir_ptr->fd, 0L, 0);
            if (sizeof (REL_HEADER) != write (fdir_ptr->fd,
                                              (char *) &dir_ptr->rh,
                                              sizeof (REL_HEADER))) {
                set_error (errno, dir_ptr->file_name, "close_direct");
                return (UNDEF);
            }
            if (dir_ptr->shared == 0) {
                (void) free (fdir_ptr->mem);
            }
            (void) close (fdir_ptr->fd);
        }

        if (dir_ptr->mode & SBACKUP) {
            if (UNDEF == make_backup (dir_ptr->file_name)) {
                return (UNDEF);
            }
        }
    }
    else {  /* SRDONLY is set */
        /* Free up any memory alloced (except if still used by another rel) */
        if (dir_ptr->shared == 0) {
#ifdef MMAP
            if (dir_ptr->mode & SMMAP) {
                if (-1 == munmap (fdir_ptr->mem - sizeof (REL_HEADER),
                                  (int) fdir_ptr->size))
                    return (UNDEF);

	    } else
#endif  /* MMAP */
	    {
		(void) free (fdir_ptr->mem);
	    }
        }
        if (! (dir_ptr->mode & SINCORE)) {
            (void) close (fdir_ptr->fd);
        }
    }

    return(0);
}

static int
close_which_file(dir_index)
int dir_index;
{
    int fd;
    register _SDIRECT *dir_ptr = &_Sdirect[dir_index];
    FILE_DIRECT *wdir_ptr;
    char tmp_filename[PATH_LEN];
    long which_size;

    if (dir_ptr->rh.num_var_files <= 1)
	return(0);

    wdir_ptr = &(dir_ptr->which_dir);
    (void) sprintf(tmp_filename, "%s.which", dir_ptr->file_name);

    if (dir_ptr->mode & (SWRONLY | SRDWR)) {
	/* write out "which" file */
	if (dir_ptr->mode & SINCORE) {
	    if (dir_ptr->mode & SBACKUP) {
		if (UNDEF == (fd = prepare_backup(tmp_filename))) {
		    return(UNDEF);
		}
	    }
	    else if (-1 == (fd = open(tmp_filename, SWRONLY))) {
		set_error(errno, tmp_filename, "close_direct");
		return(UNDEF);
	    }

	    which_size = wdir_ptr->enddata - wdir_ptr->mem;

	    /* consistency check */
	    if (which_size != dir_ptr->rh.max_primary_value + 1) {
		set_error(SM_INCON_ERR, tmp_filename, "close_direct");
		return(UNDEF);
	    }

	    if (which_size != write(fd, (char *)wdir_ptr->mem,
				    (int) which_size)) {
		set_error(errno, tmp_filename, "close_direct");
		return(UNDEF);
	    }

	    (void) close(fd);
	}
	/* if !SINCORE, written as we went */

	if (dir_ptr->shared == 0) {
	    (void) free(wdir_ptr->mem);
	}

	if (dir_ptr->mode & SBACKUP) {
	    if (UNDEF == make_backup(tmp_filename)) {
		return(UNDEF);
	    }
	}
    }
    else {	/* SRDONLY is set */
	/* free up any memory alloced (except if still used by another rel */
	if (dir_ptr->shared == 0) {
#ifdef MMAP
	    if (dir_ptr->mode & SMMAP) {
		if (-1 == munmap(wdir_ptr->mem, (int) wdir_ptr->size))
		    return(UNDEF);
	    } else
#endif	/* MMAP */
	    {
		(void) free(wdir_ptr->mem);
	    }
	}
	if (!(dir_ptr->mode & SINCORE)) {
	    (void) close(wdir_ptr->fd);
	}
    }

    (void) close(wdir_ptr->fd);

    return(0);
}

static int
close_var_file(dir_index)
int dir_index;
{
    int fd;
    register _SDIRECT *dir_ptr = &_Sdirect[dir_index];
    FILE_DIRECT *vdir_ptr;
    char tmp_filename[PATH_LEN];
    long var_size;
    int i, num_var_files;

    num_var_files = dir_ptr->rh.num_var_files;
    vdir_ptr = dir_ptr->var_dir;

    for (i = 0; i < num_var_files; i++, vdir_ptr++) {

	/* Construct var file name */
	(void) sprintf (tmp_filename, "%s.v%03d", dir_ptr->file_name, i);

	if (dir_ptr->mode & (SWRONLY | SRDWR)) {
	    /* Write out var file if needed */
	    if ((dir_ptr->mode & SINCORE) && (dir_ptr->mode & SRDWR)) {
		/* Entire var file is in core. Backup and write entire file */

#ifdef VAR_BACKUP
		if (dir_ptr->mode & SBACKUP) {
		    if (UNDEF == (fd = prepare_backup (tmp_filename))) {
			return (UNDEF);
		    }
		}
		else
#endif
		if (-1 == (fd = open (tmp_filename, SWRONLY))) {
		    set_error (errno, tmp_filename, "close_direct");
		    return (UNDEF);
		}

		var_size = vdir_ptr->enddata - vdir_ptr->mem;

		if (var_size != write (fd, (char *) vdir_ptr->mem,
				       (int) var_size)) {
		    set_error (errno, tmp_filename, "close_direct");
		    return (UNDEF);
		}
		if (dir_ptr->shared == 0) {
		    (void) free (vdir_ptr->mem);
		}
		(void) close (fd);
	    }
	    else if ((dir_ptr->mode & SINCORE) && (dir_ptr->mode & SWRONLY)) {
		/* Only newly written var tuples are in core. Copy the */
		/* existing file for backup, and append the new tuples */

		if (-1 == (fd = open (tmp_filename, SRDWR))) {
		    set_error (errno, tmp_filename, "close_direct");
		    return (UNDEF);
		}

#ifdef VAR_BACKUP
		if (dir_ptr->mode & SBACKUP) {
		    if (UNDEF == (new_fd = prepare_backup (tmp_filename)) ||
			UNDEF == copy_fd (fd, new_fd)) {
			return (UNDEF);
		    }
		    (void) close (fd);
		    fd = new_fd;
		}
#endif
		var_size = vdir_ptr->enddata - vdir_ptr->mem;

		(void) lseek (fd, vdir_ptr->foffset, 0);
		if (var_size != write (fd, (char *) vdir_ptr->mem,
				       (int) var_size)) {
		    set_error (errno, tmp_filename, "close_direct");
		    return (UNDEF);
		}
		if (dir_ptr->shared == 0) {
		    (void) free (vdir_ptr->mem);
		}
		(void) close (fd);
	    }
	    else { /* SINCORE is not set */
		/* var file is all done. Just close file */
		(void) close (vdir_ptr->fd);
		if (dir_ptr->shared == 0) {
		    (void) free (vdir_ptr->mem);
		}
	    }

#ifdef VAR_BACKUP
	    if (dir_ptr->mode & SBACKUP) {
		if (UNDEF == make_backup (tmp_filename)) {
		    return (UNDEF);
		}
	    }
#endif
	}
	else {  /* SRDONLY is set */
	    /* Free up any memory alloced (except if still used by another
	     * rel)
	     */
	    if (dir_ptr->shared == 0) {
#ifdef MMAP
		if (dir_ptr->mode & SMMAP) {
		    if (-1 == munmap(vdir_ptr->mem, (int)vdir_ptr->size))
			return (UNDEF);

		} else if (dir_ptr->mode & SIMMAP) {
		    if (vdir_ptr->mem) {
			if (-1 == munmap (vdir_ptr->mem,
					vdir_ptr->enddata - vdir_ptr->mem))
			    return (UNDEF);
		    }
		} else
#endif  /* MMAP */
		{
		    (void) free (vdir_ptr->mem);
		}
	    }
	    if (! (dir_ptr->mode & SINCORE)) {
		(void) close (vdir_ptr->fd);
	    }
	}
    }

    /* even if the buffers are shared, the FILE_DIRECTs pointing to them
     * are not.
     */
    (void) free(dir_ptr->var_dir);

    return(0);
}
