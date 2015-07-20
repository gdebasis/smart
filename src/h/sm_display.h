#ifndef SM_DISPLAYH
#define SM_DISPLAYH
/*        $Header: /home/smart/release/src/h/sm_display.h,v 11.2 1993/02/03 16:47:44 smart Exp $*/

#include "eid.h"

/* Internal format of a preparsed document */
typedef struct sm_disp_sec {
    char section_id;
    long begin_section;
    long end_section;
    long num_subsects;
    struct sm_disp_sec *subsect;
} SM_DISP_SEC;

typedef struct  {
    EID   id_num;                   /* Id of doc */
    long  num_sections;             /* Number of sections in this did */
    SM_DISP_SEC *sections;          /* id/start/end of each section. */
    char  *file_name;               /* File to find text of doc in */
    char  *title;                   /* Title of doc */
} SM_DISPLAY;

#endif /* SM_DISPLAYH */
