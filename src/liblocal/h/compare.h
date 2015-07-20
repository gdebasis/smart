#ifndef COMPAREH
#define COMPAREH

#include "inter.h"
#include "local_eid.h"
#include "retrieve.h"

typedef struct {
  int num_mods;
  char **modifiers; } PARAM_SETTINGS;

typedef struct {
  PARAM_SETTINGS *params;
  EID_LIST *eids;
  ALT_RESULTS *res; } COMLINE_PARSE_RES;

typedef struct {
    EID did;
    long cross_edges;   /* links from nodes <= this to nodes > this */
    float cross_sim;    /* sum of links from nodes <= this to nodes > this */
    char beg_seg;       /* Does this node begin a segment? */
    char end_seg;       /* ends a segment? */
} SEGINFO;

typedef struct {
    int num_segs, num_nodes;
    SEGINFO *nodes;
    ALT_RESULTS *seg_altres;
} SEG_RESULTS;

typedef struct {
    int num_themes;
    EID_LIST *theme_ids;
} THEME_RESULTS;


int build_eidlist();
int eid_greater(), compare_eids();
int compare_rtups(), compare_rtups_sim();

int init_local_comline_parse _AP((SPEC *, char *)), close_local_comline_parse _AP((int));
int local_comline_parse _AP((INTER_STATE *, COMLINE_PARSE_RES *, int));

int init_compare _AP((SPEC *, char *)), close_compare _AP((int));
int compare _AP((INTER_STATE *, char *, int));
int init_compare_core _AP((SPEC *, char *)), close_compare_core _AP((int));
int compare_core _AP((COMLINE_PARSE_RES *, ALT_RESULTS *, int));

int init_segment _AP((SPEC *, char *)), close_segment _AP((int));
int segment _AP((INTER_STATE *, char *, int));
int init_segment_core _AP((SPEC *, char *)), close_segment_core _AP((int));
int segment_core _AP((COMLINE_PARSE_RES *, SEG_RESULTS *, int));

int init_theme _AP((SPEC *, char *)), close_theme _AP((int));
int theme _AP((INTER_STATE *, char *, int));
int init_theme_core _AP((SPEC *, char *)), close_theme_core _AP((int));
int theme_core _AP((COMLINE_PARSE_RES *, THEME_RESULTS *, int));

int init_path _AP((SPEC *, char *)), close_path _AP((int));
int path _AP((INTER_STATE *, char *, int));

#endif
