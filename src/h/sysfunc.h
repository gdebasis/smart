#ifndef SYSFUNCH
#define SYSFUNCH
/*        $Header: /home/smart/release/src/h/sysfunc.h,v 11.2 1993/02/03 16:47:48 smart Exp $ */
/* Declarations of major functions within standard C libraries */
#include <unistd.h>
#include <values.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>

/* For time being, define Berkeley constructs in terms of SVR4 constructs*/
#define bzero(dest,len)      memset(dest,'\0',len)
#define bcopy(source,dest,len)   memcpy(dest,source,len)
#define srandom(seed)        srand(seed)
#define random()             rand()

/* Hack for right now */
#ifdef _SC_PAGESIZE
#define getpagesize()        sysconf(_SC_PAGESIZE)
#endif

/* ANSI should give us an offsetof suitable for the implementation;
 * otherwise, try a non-portable but commonly supported definition
 */
#ifdef __STDC__	
#include <stddef.h>
#endif
#ifndef offsetof
#define offsetof(type, member) ((size_t) \
	((char *)&((type*)0)->member - (char *)(type *)0))
#endif

/* Temporarily add for SunOS 4.1.x systems.  These should have been defined  */
/* in one of the above include files byt weren't */
#ifdef sun_notdef
int fwrite(), fflush(), fprintf(), fputs(), fclose(), fread(), fseek(), 
    fscanf();
FILE *fopen();
int munmap(), getpagesize(), printf(), tolower();
int ioctl(), vfork(), system();
#endif /*  sun */

#endif /* SYSFUNCH */



