#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/theme.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate themes for a document.
 *2 theme (is, unused)
 *3   INTER_STATE *is;
 *3   char *unused;
 *4 init_theme (spec, unused)
 *4 close_theme (inst)

 *7 Call compare_core to generate the text relationship map and apply the
 *7 theme generation algorithm to the results.

***********************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "compare.h"
#include "context.h"
#include "functions.h"
#include "inst.h"
#include "inter.h"
#include "local_eid.h"
#include "local_functions.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static double tsim;
static PROC_TAB *vecs_vecs;
static SPEC_PARAM spec_args[] = {
    {"local.inter.theme.theme_sim", getspec_double, (char *)&tsim},
    {"inter.theme.vecs_vecs", getspec_func, (char *)&vecs_vecs},
  TRACE_PARAM ("local.inter.theme.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


typedef EID TRIANGLE[3];

typedef struct {
    int valid_info;	/* bookkeeping */

    int compare_inst, centroid_inst; 

    int max_eids;
    EID_LIST elist;
    int max_triangles, num_triangles;
    TRIANGLE *triangle;

    int max_themes, num_themes;
    EID_LIST *theme_ids;
    VEC *theme_vecs;

    PROC_TAB *vecs_vecs;
    int vv_inst;

    double tsim;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int find_triangles(), find_themes();


int
init_theme_core (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_theme_core");
    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF==(ip->compare_inst = init_compare_core(spec, "local.inter.theme.")) ||
	UNDEF==(ip->centroid_inst = init_create_cent(spec, "theme.")))
	return(UNDEF);

    ip->max_eids = ip->max_triangles = ip->max_themes = 0;
    ip->num_themes = ip->num_triangles = UNDEF;

    ip->tsim = tsim;
    ip->vecs_vecs = vecs_vecs;
    if (UNDEF == (ip->vv_inst = ip->vecs_vecs->init_proc(spec, NULL)))
	return(UNDEF);
	

    PRINT_TRACE (2, print_string, "Trace: leaving init_theme_core");
    return new_inst;
}


int
close_theme_core (inst)
int inst;
{
    int i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_theme_core");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_theme_core");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_compare_core(ip->compare_inst) ||
	UNDEF == close_create_cent(ip->centroid_inst) ||	
	UNDEF == ip->vecs_vecs->close_proc(ip->vv_inst))
	return(UNDEF);

    if (ip->max_eids) Free(ip->elist.eid);
    if (ip->max_triangles) Free(ip->triangle);
    if (ip->max_themes) {
	for (i = 0; i < ip->num_themes; i++) {
	    Free(ip->theme_ids[i].eid);
	    free_vec(&ip->theme_vecs[i]);
	}
	Free(ip->theme_ids);
	Free(ip->theme_vecs);
    }
	     
    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_theme_core");
    return (0);
}


int
theme_core(parse_results, themes, inst)
COMLINE_PARSE_RES *parse_results;
THEME_RESULTS *themes;
int inst;
{
    int i;
    STATIC_INFO *ip;
    ALT_RESULTS altres;

    PRINT_TRACE (2, print_string, "Trace: entering theme_core");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "theme_core");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check if the tsim parameter has been set on the command line. 
     * All other options are ignored. 
     */
    for (i = 0; i < parse_results->params->num_mods; i++) { 
	if (0 == strncmp(parse_results->params->modifiers[i], "tsim", 4 )) {
	    float tsim = atof(parse_results->params->modifiers[++i]);
	    if (tsim > 0)
		printf("Theme merge threshold set to %f\n", ip->tsim = tsim);
	    else
		printf("Theme merge threshold unchanged (still %f)\n", 
		       ip->tsim);
	}
    }

    /* Get the map. */
    if (UNDEF == compare_core(parse_results, &altres, ip->compare_inst) ||
	UNDEF == build_eidlist(&altres, &ip->elist, &ip->max_eids)) 
        return(UNDEF);

    if (UNDEF == find_triangles(ip, &altres) ||
	UNDEF == find_themes(ip))
	return(UNDEF);

    themes->num_themes = ip->num_themes;
    themes->theme_ids = ip->theme_ids;

    PRINT_TRACE (2, print_string, "Trace: leaving theme_core");
    return(1);
}


static int
find_triangles( ip, altres )
STATIC_INFO *ip;
ALT_RESULTS *altres;
{
    long max_poss_tri;
    EID node1, node2;
    RESULT_TUPLE *res, *edge, *tmp;

    /*
     * If there are n nodes, there can be at most (n) = n*(n-1)*(n-2)/6
     *                                            (3)
     * triangles in the graph.
     */
    max_poss_tri = ip->elist.num_eids*(ip->elist.num_eids-1)*(ip->elist.num_eids-2)/6;
    if ( max_poss_tri > ip->max_triangles ) {
	if (ip->max_triangles) Free(ip->triangle);
	ip->max_triangles = max_poss_tri;
	if (NULL == (ip->triangle = Malloc(ip->max_triangles, TRIANGLE)))
	    return UNDEF;
    }

    /*
     * All inks were made forward links by build_didlist.
     * Just sort them by source of the edge.
     */
    qsort((char *)altres->results, altres->num_results,
	  sizeof(RESULT_TUPLE), compare_rtups); /* CHECK */

    ip->num_triangles = 0;
    for (res = altres->results;
	 res < altres->results+altres->num_results-2;
	 res++){
        edge = res+1;
        while (edge < altres->results+altres->num_results-1 &&
	       EID_EQUAL(res->qid, edge->qid)) {
            tmp = edge+1;
            if (eid_greater(&edge->did, &res->did)) {
                node1 = res->did;
                node2 = edge->did;
            }
            else {
                node2 = res->did;
                node1 = edge->did;
            }

            /* Check if the two nodes linked to res->qid are linked */
            while (tmp<altres->results+altres->num_results &&
		   !(EID_EQUAL(tmp->qid, node1)))
                tmp++;
            while (tmp<altres->results+altres->num_results &&
		   EID_EQUAL(tmp->qid, node1) && 
		   !(EID_EQUAL(tmp->did, node2)))
		tmp++;

            if (tmp<altres->results+altres->num_results && 
		EID_EQUAL(tmp->qid, node1) && EID_EQUAL(tmp->did, node2)) {
                ip->triangle[ip->num_triangles][0] = res->qid;
                ip->triangle[ip->num_triangles][1] = node1;
                ip->triangle[ip->num_triangles++][2] = node2;
            }
            edge++;
        }
    }

    return 1;
}

static int
find_themes( ip )
STATIC_INFO *ip;
{
    ALT_RESULTS altres, good_altres;
    VEC_LIST q_list, d_list;
    VEC_LIST_PAIR pair;
    float max_sim;
    long i, j, max_sim_index = 0;

    if (ip->num_triangles)
	printf("Starting processing with %d Theme%s\n", ip->num_triangles,
	       ip->num_triangles==1 ? "" : "s" );
    else {
	printf("No triangles on map\n");
	ip->num_themes = 0;
	return(0);
    }

    /* Cleanup to prevent creating unreachable memory. num_themes is 
     * initialised to UNDEF, so that this code works in all cases. */
    for (i = 0; i < ip->num_themes; i++) {
	Free(ip->theme_ids[i].eid);
	free_vec(&ip->theme_vecs[i]);
    }

    /* Start with each triangle as a theme. */
    if (ip->num_triangles > ip->max_themes) {
	if (ip->max_themes) {
	    Free(ip->theme_ids);
	    Free(ip->theme_vecs);
	}
	ip->max_themes = ip->num_triangles;
	if (NULL == (ip->theme_ids = Malloc(ip->max_themes, EID_LIST)) ||
	    NULL == (ip->theme_vecs = Malloc(ip->max_themes, VEC)))
	    return UNDEF;
    }

    for ( i = 0; i < ip->num_triangles; i++ ) {
        if (NULL == (ip->theme_ids[i].eid = Malloc(3, EID)))
            return UNDEF;
        for (j = 0; j < 3; j++)
	    ip->theme_ids[i].eid[j] = ip->triangle[i][j];
        ip->theme_ids[i].num_eids = 3;
    }
    ip->num_themes = ip->num_triangles;

    for (i = 0; i < ip->num_themes; i++) {
        if (UNDEF == create_cent(&ip->theme_ids[i], &ip->theme_vecs[i],
				 ip->centroid_inst) ||
	    UNDEF == save_vec(&ip->theme_vecs[i]))
            return UNDEF;
	ip->theme_vecs[i].id_num.id = i;
	EXT_NONE(ip->theme_vecs[i].id_num.ext);
    }

    /* Compare themes to find which ones could be merged. */
    max_sim = 0;
    good_altres.num_results = 0;
    if (NULL == 
	(good_altres.results = Malloc(ip->num_themes*(ip->num_themes-1)/2, 
				      RESULT_TUPLE)))
        return UNDEF;
    for (i = 0; i < ip->num_themes - 1; i++) {
	q_list.num_vec = 1;
	/* don't compare a theme to itself */
	d_list.num_vec = ip->num_themes-(i+1);
	q_list.vec = &ip->theme_vecs[i];
	d_list.vec = &ip->theme_vecs[i+1];
	pair.vec_list1 = &q_list;
	pair.vec_list2 = &d_list;
	if (UNDEF==ip->vecs_vecs->proc(&pair, &altres, ip->vv_inst))
	    return UNDEF;

	/* Copy only results that could qualify for a merger
	 * and gather max_sim value at the same time.	*/
	for ( j = 0; j < altres.num_results; j++ ) {
	    if (altres.results[j].sim > ip->tsim) {
		Bcopy(&altres.results[j],
		      &good_altres.results[good_altres.num_results++],
		      sizeof(RESULT_TUPLE));
		if (altres.results[j].sim > max_sim) {
		    max_sim = altres.results[j].sim;
		    max_sim_index = good_altres.num_results-1;
		}
	    }
	}
    }

    /* Merge sufficiently similar themes. */
    while ( max_sim > ip->tsim ) {
        EID_LIST tmp_theme;
        VEC tmp_centroid;
	long theme1 = good_altres.results[max_sim_index].qid.id;
	long theme2 = good_altres.results[max_sim_index].did.id;
        long t1 = 0, t2 = 0;

	if (ip->num_themes % 10 == 0)
	    printf("Processing %d Themes\n", ip->num_themes);

        if ( theme2 < theme1 ) {
            long tmp = theme1;
            theme1 = theme2;
            theme2 = tmp;
        }

        /* Now merge the themes 1 and 2 */
	tmp_theme.num_eids = 0;
        /* The theme can have at most all nodes of themes 1 and 2 */
	if (NULL == (tmp_theme.eid =
		      Malloc(ip->theme_ids[theme1].num_eids +
			     ip->theme_ids[theme2].num_eids, EID)))
	    return UNDEF;

        while (t1 < ip->theme_ids[theme1].num_eids &&
	       t2 < ip->theme_ids[theme2].num_eids) {
            if (eid_greater(&ip->theme_ids[theme2].eid[t2], 
			    &ip->theme_ids[theme1].eid[t1])) {
                tmp_theme.eid[tmp_theme.num_eids++] =
                    ip->theme_ids[theme1].eid[t1++];
            }
            else {
                if (eid_greater(&ip->theme_ids[theme1].eid[t1], 
				&ip->theme_ids[theme2].eid[t2])) {
                    tmp_theme.eid[tmp_theme.num_eids++] =
                        ip->theme_ids[theme2].eid[t2++];
                }
                else {
                    tmp_theme.eid[tmp_theme.num_eids++] =
                        ip->theme_ids[theme1].eid[t1++];
                    t2++;
                }
            }
        }
        while ( t1 < ip->theme_ids[theme1].num_eids )
            tmp_theme.eid[tmp_theme.num_eids++] =
                ip->theme_ids[theme1].eid[t1++];   
        while ( t2 < ip->theme_ids[theme2].num_eids )
            tmp_theme.eid[tmp_theme.num_eids++] =
                ip->theme_ids[theme2].eid[t2++];

        if (UNDEF == create_cent(&tmp_theme, &tmp_centroid, ip->centroid_inst))
            return UNDEF;
        if (UNDEF == save_vec(&tmp_centroid))
            return UNDEF;
	tmp_centroid.id_num.id = theme1;
	EXT_NONE(tmp_centroid.id_num.ext);

        /* Now change theme1 to this new theme and delete theme2 */
	Free(ip->theme_ids[theme1].eid);
        free_vec(&ip->theme_vecs[theme1]);
        ip->theme_ids[theme1].num_eids = tmp_theme.num_eids;
        ip->theme_ids[theme1].eid = tmp_theme.eid;
        Bcopy(&tmp_centroid, &ip->theme_vecs[theme1], sizeof(VEC));
        qsort((char *)ip->theme_ids[theme1].eid, ip->theme_ids[theme1].num_eids,
              sizeof(EID), compare_eids);

        Free(ip->theme_ids[theme2].eid);
        free_vec(&ip->theme_vecs[theme2]);
        ip->theme_ids[theme2].num_eids = ip->theme_ids[ip->num_themes-1].num_eids;
        ip->theme_ids[theme2].eid = ip->theme_ids[ip->num_themes-1].eid;
        Bcopy(&ip->theme_vecs[ip->num_themes-1], &ip->theme_vecs[theme2],
	      sizeof(VEC));
	ip->theme_vecs[theme2].id_num.id = theme2;
	EXT_NONE(ip->theme_vecs[theme2].id_num.ext);
        ip->num_themes--;

	/* Delete similarities involving theme2 and change similarities
	 * for the last theme, since that has become theme2.
	 * We use ip->num_themes (instead of ip->num_themes - 1) here
	 * since we have already decremented ip->num_themes above.	*/
	max_sim = 0;
        for ( i = 0; i < good_altres.num_results; i++ ) {
            if (good_altres.results[i].qid.id == theme1 ||
		good_altres.results[i].did.id == theme1 ||
                good_altres.results[i].qid.id == theme2 ||
                good_altres.results[i].did.id == theme2) {
		good_altres.results[i] =
		    good_altres.results[--good_altres.num_results];
		i--;
            }
	    else {
		if (good_altres.results[i].qid.id == ip->num_themes) {
		    good_altres.results[i].qid.id = theme2;
		    EXT_NONE(good_altres.results[i].qid.ext);
		}
		if (good_altres.results[i].did.id == ip->num_themes) {
		    good_altres.results[i].did.id = theme2;
		    EXT_NONE(good_altres.results[i].did.ext);
		}
		if ( good_altres.results[i].sim > max_sim ) {
		    max_sim = good_altres.results[i].sim;
		    max_sim_index = i;
		}
	    }
        }

	/* Recompute similarities for the new theme. */
        q_list.num_vec = 1;
        d_list.num_vec = ip->num_themes;
        q_list.vec = &ip->theme_vecs[theme1];
        d_list.vec = ip->theme_vecs;
        pair.vec_list1 = &q_list;
        pair.vec_list2 = &d_list;
        if (UNDEF==ip->vecs_vecs->proc(&pair, &altres, ip->vv_inst))
            return UNDEF;
	for ( i = 0; i < altres.num_results; i++ ) {
	    /* Copy only results that could qualify for a merger
	     * and gather max_sim value at the same time.	*/
	    if (!(EID_EQUAL(altres.results[i].qid, altres.results[i].did)) &&
		altres.results[i].sim > ip->tsim) {
		Bcopy(&altres.results[i],
		      &good_altres.results[good_altres.num_results++],
		      sizeof(RESULT_TUPLE));
		if ( altres.results[i].sim > max_sim ) {
		    max_sim = altres.results[i].sim;
		    max_sim_index = good_altres.num_results-1;
		}
	    }
	}
    }
    printf("Processing ended with %d Theme%s\n", ip->num_themes,
	   ip->num_themes==1 ? "" : "s" );

    Free(good_altres.results);

    return 1;
}
