#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_account.c,v 11.2 1993/02/03 16:53:46 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "buf.h"
#include "accounting.h"

#ifdef ACCOUNTING
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <utime.h>

time_t time();

static SM_BUF internal_output = {0, 0, (char *) 0};

static long init_tv_sec = 0;        /* Values of absolute time when
                                       print_accounting first called */
static long init_tv_usec = 0;

#endif

void
print_accounting (message, output)
char *message;
SM_BUF *output;
{
#ifdef ACCOUNTING
    char temp_buf[PATH_LEN];
    struct timeval tp;

    SM_BUF *out_p;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    if (message != NULL && 
        UNDEF == add_buf_string (message, out_p))
        return;

    /* Print current time (in seconds/microseconds past Jan 1, 1970) */
    if (-1 != gettimeofday (&tp, (struct timezone *) NULL)) {
        if (init_tv_sec == 0) {
            init_tv_sec = tp.tv_sec; init_tv_usec = tp.tv_usec;
        }
        (void) sprintf (temp_buf, "Elapsed Time:\t%ld.%6.6ld\n",
                        (tp.tv_usec >= init_tv_usec) ? tp.tv_sec - init_tv_sec
                        : tp.tv_sec - init_tv_sec -1,
                        (tp.tv_usec >= init_tv_usec) ? tp.tv_usec- init_tv_usec
                        : tp.tv_usec + 1000000 - init_tv_usec);
        if (UNDEF == add_buf_string (temp_buf, out_p)) return;
    }

#ifdef RUSAGE
    if (global_accounting > 1) {
	struct rusage rusage;

        /* Get current max memory  */
        (void) sprintf (temp_buf, "Max Mem:\t%d\n", (int) sbrk(0));
        if (UNDEF == add_buf_string (temp_buf, out_p)) return;

        /* Get resource utilization info */
        if (-1 != getrusage (RUSAGE_SELF, &rusage)) {
            (void) sprintf (temp_buf,
                            "Comp time:\tUser %ld.%6.6ld\tSystem %ld.%6.6ld\tMem RSS:\t%d\n",
                            rusage.ru_utime.tv_sec, rusage.ru_utime.tv_usec,
                            rusage.ru_stime.tv_sec, rusage.ru_stime.tv_usec,
                            rusage.ru_maxrss);
            if (UNDEF == add_buf_string (temp_buf, out_p)) return;

            if (global_accounting > 2) {
                (void) sprintf (temp_buf,
                                "Fault:\tMinor %d\tMajor %d\t\tSwaps %d\n",
                                rusage.ru_minflt, rusage.ru_majflt,
                                rusage.ru_nswap);
                if (UNDEF == add_buf_string (temp_buf, out_p)) return;
                (void) sprintf (temp_buf,
                              "Blocks:\tIn\t%d\tOut\t%d\t\tMsgs: \tIn\t%d\tOut %d\n",
                                rusage.ru_inblock, rusage.ru_oublock,
                                rusage.ru_msgrcv,  rusage.ru_msgsnd);
                if (UNDEF == add_buf_string (temp_buf, out_p)) return;
                (void) sprintf (temp_buf,
                                "Signals\t%d\t\tConSwitch:\tVol %d\tInvol %d\n",
                                rusage.ru_nsignals,
                                rusage.ru_nvcsw,  rusage.ru_nivcsw);
                if (UNDEF == add_buf_string (temp_buf, out_p)) return;
            }
        }
    }
#endif

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
#endif

}

