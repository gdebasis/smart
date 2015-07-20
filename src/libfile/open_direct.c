#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/open_direct.c,v 11.2 1993/02/03 16:50:21 smart Exp $";
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
#include "direct.h"
#include "io.h"
#include "smart_error.h"
#ifdef MMAP
#   include <sys/types.h>
#   include <sys/mman.h>
#endif   /* MMAP */

extern _SDIRECT _Sdirect[];

/* open the relation "file_name", obeying restrictions of "mode" */
/* Following treatment of files takes place, based on mode "bits" being set */
/*  SRDWR and SINCORE - Both fix file and entire var file brought into core */
/*                      with additional space being reserved for writing */
/*  SRDONLY and SINCORE - Both fix file and entire var file brought into */
/*                      core. No additional space */
/*  SWRONLY and SINCORE - Entire fix file brought into core with extra */
/*                      space. Space reserved for var for writing but file */
/*                      will only be appended, therefore not brought in */
/*  SRDWR and ~SINCORE - Neither file brought into core. Space reserved for */
/*                      one fix entry, space allocated for var as needed. */
/*                      Each write of object results in physical var */
/*                      file being written. */
/*  SRDONLY and ~SINCORE - Neither file brought into core. Space reserved */
/*                      for one fix entry. Buffer space reserved for reading*/
/*                      var file as needed (but see SMMAP below) */
/*  SWRONLY and ~SINCORE - Neither file brought into core. No space reserved*/
/*                      for either file at all. Each write */
/*                      results in physical var file being written */
/*  SRDONLY and SMMAP   - If MMAP is defined (param.h), then the files are */
/*                      directly mapped into memory.  The SINCORE flag is */
/*                      set so the behavior of seek and */
/*                      read are the same as for SRDONLY|SINCORE. */
/*  SRDONLY and SIMMAP  - If MMAP is defined (param.h), then the current */
/*                      entry of the var file is directly mapped into */
/*                      memory.  The SMMAP and SINCORE flags are cleared */
/*                      so that the fix file is read an entry at a time. */
/* */

static int get_fix_file(), get_var_files();
extern int get_which_file(), get_one_var_file();

int
open_direct(file_name, mode, dir_type)
char *file_name;
long  mode;
int dir_type;
{
    register _SDIRECT *dir_ptr;
    _SDIRECT *already_open, *tdir_ptr;
    int dir_index;

    if (file_name == (char *) NULL) {
        set_error (EMFILE, "NULL file name", "open_direct");
        return (UNDEF);
    }

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
    if (mode & SIMMAP) {
        /* SIMMAP only allowed on SRDONLY */
        if ((mode & SRDWR) || (mode & SWRONLY))
            mode &= ~SIMMAP;
        else {
	    /* one might eventually think about SMMAP'ing the fix file
	     * while SIMMAP'ing the var file, but for now just read the
	     * fix file entry and mmap the var file entry
	     */
            mode &= ~SINCORE;
	    mode &= ~SMMAP;
        }
    }
#endif  /* MMAP */

    /* Find first open slot in direct table. */
    /* Minor optimization of allowing two or more RDONLY,INCORE invocations */
    /* of the same object to share the incore image */
    already_open = NULL;
    dir_ptr = NULL;
    for ( tdir_ptr = &_Sdirect[0];
          tdir_ptr < &_Sdirect[MAX_NUM_DIRECT];
          tdir_ptr++) {
        if ( mode == tdir_ptr->mode &&
             (mode & SINCORE) &&
             (! (mode & (SWRONLY|SRDWR))) &&
             strncmp (file_name, tdir_ptr->file_name,PATH_LEN) == 0 &&
             tdir_ptr->opened > 0) {
            /* increment count of number relations sharing readonly buffers */
            tdir_ptr->shared++;
            already_open = tdir_ptr;
        }
        else if (tdir_ptr->opened == 0 && dir_ptr == NULL) {
            /* Assign dir-ptr to first open spot */
            dir_ptr = tdir_ptr;
        }
    }

    if (dir_ptr == NULL) {
        set_error (EMFILE, file_name, "open_direct");
        return (UNDEF);
    }
    dir_index = dir_ptr - &_Sdirect[0];

    if (already_open != NULL) {
	int vdir_size = already_open->rh.num_var_files * sizeof(FILE_DIRECT);
	FILE_DIRECT *vdir_ptr;

        (void) memcpy((char *)dir_ptr, (char *)already_open, sizeof(_SDIRECT));
        (void) seek_direct (dir_index, SEEK_FIRST_NODE);

	vdir_ptr = (FILE_DIRECT *)malloc((unsigned)vdir_size);
	if ((FILE_DIRECT *)0 == vdir_ptr) {
	    set_error (errno, dir_ptr->file_name, "open_direct");
	    return (UNDEF);
	}
        (void) memcpy((char *)vdir_ptr, (char *)(already_open->var_dir),
						vdir_size);
	dir_ptr->var_dir = vdir_ptr;
        return (dir_index);
    }

    dir_ptr->mode = mode;

    (void) strncpy(dir_ptr->file_name, file_name, PATH_LEN);

    if (mode & SCREATE) {
	/* need to have taken care of this in open_type, unless we want to
	 * send dir_type into a table to call the right create_type routine
	 */
        set_error (SM_ILLMD_ERR, "SCREATE", "open_direct");
        return (UNDEF);
    }


    /* Bring fix file into memory, or reserve space for an entry */
    if (UNDEF == get_fix_file (dir_index, dir_type)) {
        return (UNDEF);
    }

    /* "which" file may be brought into core, depending on mode */
    if (UNDEF == get_which_file (dir_index)) {
        return (UNDEF);
    }

    /* var file may be brought into core, depending on mode: */
    if (UNDEF == get_var_files (dir_index)) {
        return (UNDEF);
    }

    /* Seek to first object */
    if (UNDEF == seek_direct (dir_index, SEEK_FIRST_NODE)) {
        return (UNDEF);
    }

    dir_ptr->shared = 0;
    dir_ptr->opened = 1;

    return (dir_index);
}

static int
get_fix_file(dir_index, dir_type)
int dir_index, dir_type;
{
    register _SDIRECT *dir_ptr = &(_Sdirect[dir_index]);
    register FILE_DIRECT *fdir_ptr = &(dir_ptr->fix_dir);

    long num_fix_entries;
    long size_fix;
    int fd, new_fd;

    /* Open physical fix file for reading */
    if ( -1 == (fd = open (dir_ptr->file_name,
                           (int) ((dir_ptr->mode & SWRONLY) ? SRDWR
                                                  : dir_ptr->mode & SMASK)))){
        set_error (errno, dir_ptr->file_name, "open_direct");
        return (UNDEF);
    }
    fdir_ptr->size = lseek (fd, 0L, 2);
    (void) lseek (fd, 0L, 0);

    /* Get header at beginning of file */
    if (sizeof (REL_HEADER) != read (fd,
                                     (char *) &dir_ptr->rh,
                                     sizeof (REL_HEADER))) {
        set_error (errno, dir_ptr->file_name, "open_direct");
        return (UNDEF);
    }

    if (dir_ptr->rh.type != dir_type) {
	set_error(SM_INCON_ERR, dir_ptr->file_name, "open_direct");
	return (UNDEF);
    }

    num_fix_entries = dir_ptr->rh.max_primary_value + 1;
    size_fix = num_fix_entries * sizeof(FIX_ENTRY);

    /* Consistency check - Error Flag set but error not returned. With */
    /* concurrent read and write processes, it is possible for the write */
    /* to increase the size of the fixed file before updating the */
    /* rel_header information. A read process will fail the consistency */
    /* check but will actually be fine. */
    if (size_fix != fdir_ptr->size - sizeof (REL_HEADER)) {
        set_error (SM_INCON_ERR, dir_ptr->file_name, "open_direct");
        if (dir_ptr->mode & (SWRONLY|SRDWR)) {
            /* Error for sure if inconsistency detected by write process */
            return (UNDEF);
        }
    }


    if (dir_ptr->mode & SINCORE) {
        /* Read entire fixed table with no extra space */
#ifdef MMAP
        if (dir_ptr->mode & SMMAP) {
            if ((char *) -1 == (fdir_ptr->mem =
                                mmap ((caddr_t) 0,
                                      (size_t) fdir_ptr->size,
                                      PROT_READ,
                                      MAP_SHARED,
                                      fd,
                                      (off_t) 0))) {
                set_error (errno, dir_ptr->file_name, "open_direct");
                return (UNDEF);
            }
            fdir_ptr->mem += sizeof (REL_HEADER);
        }
        else
#endif /* MMAP */
	if (NULL == (fdir_ptr->mem = malloc ( (unsigned) size_fix)) ||
            size_fix != read (fd, fdir_ptr->mem, (int) size_fix)) {
            set_error (errno, dir_ptr->file_name, "open_direct");
            return (UNDEF);
        }

        dir_ptr->fix_pos = fdir_ptr->mem;
        fdir_ptr->end = fdir_ptr->mem + size_fix;
        fdir_ptr->enddata = fdir_ptr->mem + size_fix;
        fdir_ptr->foffset = sizeof (REL_HEADER);
        (void) close (fd);
    }
    else {
        /* Reserve space for one fix file entry */
        if (NULL == (fdir_ptr->mem = malloc ((unsigned) sizeof(FIX_ENTRY)))) {
            set_error (errno, dir_ptr->file_name, "open_direct");
            return (UNDEF);
        }

        /* Must prepare for backup now if writable and SBACKUP. */
        /* Actually, end up writing on new copy of file */
        if ((dir_ptr->mode & (SWRONLY | SRDWR)) && (dir_ptr->mode & SBACKUP)){
            if (UNDEF == (new_fd = prepare_backup (dir_ptr->file_name)) ||
                UNDEF == copy_fd (fd, new_fd)) {
                return (UNDEF);
            }
            (void) close (fd);
            fd = new_fd;
        }

        dir_ptr->fix_pos = NULL;
        fdir_ptr->end = fdir_ptr->mem + sizeof(FIX_ENTRY);
        fdir_ptr->enddata = NULL;
        fdir_ptr->foffset = sizeof (REL_HEADER);
        fdir_ptr->fd = fd;

    }

    return (0);
}

int
get_which_file(dir_index)
int dir_index;
{
    register _SDIRECT *dir_ptr = &(_Sdirect[dir_index]);
    register FILE_DIRECT *wdir_ptr;
    char path_name[PATH_LEN];
    int fd, new_fd;

    if (dir_ptr->rh.num_var_files > 1) {
	wdir_ptr = &(dir_ptr->which_dir);

	(void) sprintf (path_name, "%s.which", dir_ptr->file_name);
	if (-1 == (fd = open (path_name,
			(int) ((dir_ptr->mode & SWRONLY) ? SRDWR
				: dir_ptr->mode & SMASK)))) {
	    set_error (errno, path_name, "open_direct");
	    return (UNDEF);
	}
	wdir_ptr->size = lseek(fd, 0L, 2);
	(void) lseek(fd, 0L, 0);

	/* As in get_fix_file(), the size may temporarily differ from the
	 * "correct" value if another process is writing while this one
	 * is reading.  If this process is a writer, the value needs to
	 * be correct.
	 */
	if (wdir_ptr->size != dir_ptr->rh.max_primary_value + 1) {
	    set_error(SM_INCON_ERR, path_name, "open_direct");
	    if (dir_ptr->mode & (SWRONLY|SRDWR)) {
		return(UNDEF);
	    }
	}

	if (dir_ptr->mode & SINCORE) {
#ifdef MMAP
	    if (dir_ptr->mode & SMMAP) {
		if ((char *)-1 == (wdir_ptr->mem =
				mmap ((caddr_t) 0, (size_t) wdir_ptr->size,
					PROT_READ, MAP_SHARED,
					fd, (off_t) 0))) {
		    set_error(errno, path_name, "open_direct");
		    return(UNDEF);
		}
	    } else
#endif	/* MMAP */
	    if (NULL == (wdir_ptr->mem = malloc((unsigned)wdir_ptr->size)) ||
		wdir_ptr->size != read(fd, wdir_ptr->mem, (int)wdir_ptr->size)){
		set_error(errno, path_name, "open_direct");
		return(UNDEF);
	    }

	    wdir_ptr->end = wdir_ptr->mem + wdir_ptr->size;
	    wdir_ptr->enddata = wdir_ptr->end;
	    wdir_ptr->foffset = 0L;
	    (void) close(fd);

	} else {	/* !SINCORE */
	    /* reserve space for one entry */
	    /* this seems silly, but no obviously better alternatives present
	     * themselves
	     */
	    if (NULL == (wdir_ptr->mem = malloc(1))){
		set_error(errno, path_name, "open_direct");
		return(UNDEF);
	    }

	    /* prepare for backup if writable and SBACKUP */
	    /* actually, write on new copy of file */
	    /* not under VAR_BACKUP, as "which"'s where-is-data is closer to
	     * fix file treatment
	     */
	    if ((dir_ptr->mode & (SWRONLY|SRDWR)) &&
					(dir_ptr->mode & SBACKUP)) {
		if (UNDEF == (new_fd = prepare_backup(path_name)) ||
		    UNDEF == copy_fd(fd, new_fd)) {
		    return(UNDEF);
		}
		(void) close(fd);
		fd = new_fd;
	    }

	    wdir_ptr->end = wdir_ptr->mem + 1;
	    wdir_ptr->enddata = NULL;
	    wdir_ptr->foffset = 0L;
	    wdir_ptr->fd = fd;
	}
    }

    return(0);
}


int
get_one_var_file(dir_ptr, vdir_ptr, i)
register _SDIRECT *dir_ptr;
register FILE_DIRECT *vdir_ptr;
int i;
{
    char path_name[PATH_LEN];
    int fd;
#ifdef VAR_BACKUP
    int new_fd;
#endif

    (void) sprintf (path_name, "%s.v%03d", dir_ptr->file_name, i);
    if (-1 == (fd = open (path_name, (int) dir_ptr->mode & SMASK))) {
	set_error (errno, path_name, "open_direct");
	return (UNDEF);
    }
    vdir_ptr->size = lseek (fd, 0L, 2);
    (void) lseek (fd, 0L, 0);

    if (dir_ptr->mode & SINCORE) {
	if (dir_ptr->mode & SWRONLY) {
	    /* When only writing directs, no need to read in present var */
	    /* file since it will only be appended. The fix file can be */
	    /* changed.*/

	    /* Allocate space for var tuples */
	    if (NULL == (vdir_ptr->mem = malloc (VAR_INCR))) {
		set_error (errno, path_name, "open_direct");
		return (UNDEF);
	    }

	    vdir_ptr->enddata = vdir_ptr->mem;
	    vdir_ptr->foffset = vdir_ptr->size;
	    vdir_ptr->end = vdir_ptr->mem + VAR_INCR;
	    (void) close (fd);
	}
	else if (dir_ptr->mode & SRDWR) {
	     /* Read entire var table into memory plus extra memory */
	    if (NULL == (vdir_ptr->mem = malloc ((unsigned)
			   vdir_ptr->size + VAR_INCR))) {
		set_error (errno, path_name, "open_direct");
		return (UNDEF);
	    }

	    /* read the file into memory */
	    if (vdir_ptr->size != read (fd, vdir_ptr->mem,
					   (int) vdir_ptr->size)) {
		set_error (errno, path_name, "open_direct");
		return (UNDEF);
	    }

	    vdir_ptr->enddata = vdir_ptr->mem + vdir_ptr->size;
	    vdir_ptr->foffset = 0;
	    vdir_ptr->end = vdir_ptr->mem + vdir_ptr->size + VAR_INCR;
	    (void) close (fd);
	}
	else { /* dir_ptr->mode & SRDONLY */
	     /* Read entire var table into memory */
#ifdef MMAP
	    if (dir_ptr->mode & SMMAP) {
		if ((char *) -1 == (vdir_ptr->mem =
				    mmap ((caddr_t) 0,
					  (size_t) vdir_ptr->size,
					  PROT_READ,
					  MAP_SHARED,
					  fd,
					  (off_t) 0))) {
		    set_error (errno, dir_ptr->file_name, "open_direct");
		    return (UNDEF);
		}
	    }
	    else
#endif /* MMAP */
	    if (NULL == (vdir_ptr->mem =
			 malloc ((unsigned) vdir_ptr->size)) ||
		vdir_ptr->size != read (fd, vdir_ptr->mem,
					   (int) vdir_ptr->size)) {
		set_error (errno, path_name, "open_direct");
		return (UNDEF);
	    }

	    vdir_ptr->enddata = vdir_ptr->mem + vdir_ptr->size;
	    vdir_ptr->foffset = 0;
	    vdir_ptr->end = vdir_ptr->enddata;
	    (void) close (fd);
	}

    }
    else { /* ! dir_ptr->mode & SINCORE */
#ifdef MMAP
	if (dir_ptr->mode & SIMMAP) {
	    vdir_ptr->mem = (char *)0;
	    vdir_ptr->enddata = (char *)0;
	    vdir_ptr->end = (char *)0;
	    vdir_ptr->foffset = 0;
	} else
#endif	/* MMAP */
	{
	    /* Reserve reading buffer of size VAR_BUFF */
	    if (NULL == (vdir_ptr->mem = malloc ((unsigned) VAR_BUFF))) {
		set_error (errno, path_name, "open_direct");
		return (UNDEF);
	    }
	    vdir_ptr->enddata = vdir_ptr->mem;
	    vdir_ptr->foffset = 0;
	    vdir_ptr->end = vdir_ptr->mem + VAR_BUFF;
	}

#ifdef VAR_BACKUP
	/* Must prepare for backup now if writable and SBACKUP. */
	/* Actually, end up writing on new copy of file */
	if ((dir_ptr->mode & (SWRONLY | SRDWR)) && (dir_ptr->mode & SBACKUP)){
	    if (UNDEF == (new_fd = prepare_backup (path_name)) ||
		UNDEF == copy_fd (fd, new_fd)) {
		return (UNDEF);
	    }
	    (void) close (fd);
	    fd = new_fd;
	}
#endif

	/* In all cases (if not SINCORE) keep fd around for file access */
	vdir_ptr->fd = fd;
    }

    return(0);
}

static int
get_var_files(dir_index)
int dir_index;
{
    register _SDIRECT *dir_ptr = &(_Sdirect[dir_index]);
    register FILE_DIRECT *vdir_ptr;
    int i, num_var_files;

    num_var_files = dir_ptr->rh.num_var_files;
    vdir_ptr = (FILE_DIRECT *)malloc((unsigned)num_var_files *
						sizeof(FILE_DIRECT));
    if ((FILE_DIRECT *)0 == vdir_ptr) {
	set_error (errno, dir_ptr->file_name, "open_direct");
	return (UNDEF);
    }
    dir_ptr->var_dir = vdir_ptr;

    /* Open var files */
    for (i = 0; i < num_var_files; i++, vdir_ptr++) {
	if (UNDEF == get_one_var_file(dir_ptr, vdir_ptr, i)) {
	    return(UNDEF);
	}
    }

    return (0);
}
