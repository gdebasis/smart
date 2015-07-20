#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/triestem.c,v 11.2 1993/02/03 16:51:08 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Determine stem of word by looking up stems in trie
 *1 index.stem.triestem
 *2 triestem (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_triestem (spec, param_prefix)
 *5    "index.stem.trace"
 *4 close_triestem (inst)

 *7 Determine longest matching legal stem of word by looking up possible
 *7 stems in a trie, and then remove stem, returning resulting recoded root in
 *7 output_word.
 *7 Return UNDEF if error, 0 otherwise.

 *8 Basic algorithm from Lovins' (CACM) article, but has been much distorted
 *8 over the years.  Initial trie contains list of stems plus conditions under
 *8 which stem can NOT be applied.  Search that trie for the longest matching
 *8 stem that can be applied.  Remove the stem, and then recode the root to
 *8 normalize that form (eg want "believes" -> "belief").  Recoding done by
 *8 looking up end of root in a separate recoding trie.

 *9 Bug: Since tries are not yet full objects in the smart system, the tries
 *9 searched are compiled into the code.  This is bad.
 *9 Warning: assumes input word is already all lower case.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "trie.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.stem.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *tstemmer();

int
init_triestem (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_triestem");

return (0);
}

/* tstemmer uses a trie based approach */
int
triestem (word, output_word, inst)
char *word;                     /* word to be stemmed */
char **output_word;             /* the stemmed word (with tstemmer, always
                                   the same as word ) */
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering triestem");
    PRINT_TRACE (6, print_string, word);

    *output_word = tstemmer (word);

    PRINT_TRACE (4, print_string, *output_word);
    PRINT_TRACE (2, print_string, "Trace: leaving triestem");

    return (0);
}

int
close_triestem (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_triestem");
    return (0);
}


/* Modification of look_trie().
   Return -1 if string not found.
*/
static int t_bad_cc(), cmpstrn();
static char *t_recode();
static char *trie_stem();

extern TRIE suffix_trie[];
extern TRIE recode_trie[];

static char is_consonant[26] = {  /* consonants except x,y,w */
        0,1,1,1,0,1,1,1,0,1,1,1,1,
        1,0,1,1,1,1,1,0,1,0,0,0,1};
static char is_vowel[26] = {
        1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,1,0,0,0,0,0,1,0,0,0,0,0};
static char double_letter[26] = { /* array indicating if double */
        0,1,0,1,0,0,1,0,0,0,0,0,1,     /* letter should be removed */
        1,0,1,0,1,0,1,0,0,0,0,0,0};

#define isvowel(c) (islower (c) && is_vowel[c-'a'])

static char *orig_word;

static char *
tstemmer (word)
register char *word;
{
    char *endword;
    int word_len;
    char *suffix;
    long cc;

    /* word_len gives length */
    word_len = strlen(word);
    
    /* Return if word is less than three characters long */
    if (word_len <= 2) {
        return (word);
    }

    endword = &word[word_len-1];
    /* Remove final ', or 'd or 's or n't  before further processing */
    if (*endword == '\'') {
        *endword = '\0';
        endword--;
        word_len--;
    }
    /* Remove final 'd, 's, or n't and return */
    else if (*(endword-1) == '\'') {
        if (*endword == 's' || *endword == 'd') {
            *(endword-1) = '\0';
            endword -= 2;
            word_len -= 2;
        }
        else if (*endword == 't' && *(endword-2) == 'n' && word_len > 3) {
            *(endword-2) = '\0';
            endword -= 3;
            word_len -= 3;
        }
    }

    if (word_len <= 2) {
        return (word);
    }

    orig_word = word;

    /* Find the longest matching suffix 
       which satisfies the constraints imposed by 
       bad_cc.  Suffix_trie is a trie containing all suffixes
       along with the condition code to be checked by bad_cc.
       Trie_stem returns the potential suffix, placing the
       condition code in cc. (cc == 0 means no stemming)
    */
    suffix = trie_stem (endword, word+3, suffix_trie, &cc);
    while (suffix <= endword && t_bad_cc (suffix-1, cc)) {
        suffix = trie_stem (endword, suffix+1, suffix_trie, &cc);
    }

    *suffix = '\0';
    return (t_recode (word, suffix - 1, cc));
}

/* Returns the last valid suffix seen, setting cond_code to he
   value found in the trie for that suffix 
 */
static char *
trie_stem (suffix, max_suffix, trie, cond_code)
char *suffix;                   /* next suffix letter candidate */
                                /* (matched suffix is in suffix+1) */
char *max_suffix;               /* position of longest allowed suffix */
TRIE *trie;                     /* position within suffix_trie corresponding
                                   to matched suffix so far */
long *cond_code;                /* condition code to be returned */
{
    register TRIE *ptr;         /* position within suffix_trie corresponding
                                   to next potential match */
    char *save_suffix;           /* longest suffix match which is a suffix
                                    in suffix_trie, as opposed to being
                                    a part of a suffix in the trie */
    *cond_code = -1;
    save_suffix = suffix + 1;
    while (suffix >= max_suffix && has_children (trie)) {
        /* Check to see if we can increase the suffix match */
        ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
        while (! last_child (ptr) && ascii_char (ptr) < *suffix)
            ptr++;
        if (! (ascii_char (ptr) == *suffix)) {
            /* No further match, return best so far */
            break;
        }
        if (has_value (ptr)) {
            /* Save this match of suffix */
            save_suffix = suffix;
            *cond_code = (long) get_value (ptr + children_ptr (ptr));
        }
        trie = ptr;
        suffix--;
    }

    return (save_suffix);
}

/* Check to see if rule number "cond_code" prohibits the removal of the suffix
     from word. Return 1 if prohibited, 0 otherwise.
*/
static int
t_bad_cc (lastchar, cond_code)
register char *lastchar;         /* character preceding candidate suffix */
long cond_code;            /* rule number to check */
{
    register int  len = lastchar - orig_word + 1;

    switch(cond_code){
      case 0:  /*  min stem length=3 */
      case 1:  /* Seperate entry for suffixes starting with consonant so
                   won't later add an 'e' back on */
        return(0);
      case 2: /*  min stem length=4 */
        return (len < 4);
      case 3: /*  min stem length=5 */
        return (len < 5);
      case 4: /*  don't remove after "e" */
        /* ed */
        return (*lastchar == 'e');
      case 5:  /* e */
        /* after consonant or 'u' */
        return (*lastchar == 'a' || *lastchar == 'e' ||
                *lastchar == 'i' || *lastchar == 'o');
      case 6:  /* Suffix abl*, an, anhood*, ans, ar*(see 23),
                  edly, eli*, ement*, ories, ory */
        return (len < 4 || *lastchar == 'a' ||
                *lastchar == 'e' || *lastchar == 'i' ||
                *lastchar == 'o');
      case 7:  /* Suffix a,i,is,o,y */
        /* after consonant and len >= 4 */
        return (len < 4 || isvowel (*lastchar));
      case 8:  /* Suffix, at* log[iy]* */
        return (len < 6 || *lastchar == 'a' ||
                *lastchar == 'e' || *lastchar == 'i' ||
                *lastchar == 'o');
      case 9: /* y */
        return (len < 5 || isvowel (*lastchar));

      case 10: /* m.s.l.=4 & don't remove after e, i, u, met or yst */
        /* Suffix al* (Note ial separate ) */
        if (len < 4 || *lastchar == 'i' ||
            *lastchar == 'e' || *lastchar == 'u' ||
            cmpstrn (lastchar-2, "met") == 0 ||
            cmpstrn (lastchar-2, "yst") == 0)
            return (1);
        return (0);
      case 11: /* like s but len == 3 and don't want {d,tr,rt}ices
                  which are later recoded to end in ix or ex, and want
                  to remove after i (ies not removed because too short). */
        /* es */
        if (*lastchar == 'a' || *lastchar == 'e' || *lastchar == 'o' ||
            (len > 6 &&(*lastchar == 'c' &&
                        *(lastchar-1) == 'i' &&
                        (*(lastchar-2) == 'd' ||
                         (*(lastchar-2)=='t' && *(lastchar-3)=='r') ||
                         (*(lastchar-2)=='r' && *(lastchar-3)=='t')))))
            return (1);
        return (0);
      case 12: /* don't remove after "adv", "bal"; m.s.l.=4) */
        /* Suffix ance*, ancing */
        if ( len < 4 ||
            cmpstrn (lastchar-2, "adv") == 0 ||
            cmpstrn (lastchar-2, "bal") == 0)
            return (1);
        return (0);
      case 13: /* remove only after l */
        /* ar, ars, arly */
        if (len > 4 && *lastchar == 'l')
            return (0);
        return (1);
      case 14: /* don't remove after "e", "g", or "eav"; m.s.l. = 4 */
        /* Suffix en, ens, enly, ening */
        if ( len < 4 ||
            *lastchar == 'e' ||
            *lastchar == 'g' ||
            (len >= 4 && cmpstrn (lastchar-2, "eav") == 0))
            return (1);
        return(0);
      case 15: /* do not remove after "th", or "e", m.s.l.= 4 */
        /* er*  */
        if ( len < 4 ||
            *lastchar == 'e' ||
            (*lastchar == 'h' && *(lastchar-1) == 't'))
            return (1);
        return (0);
      case 16: /* same as 36 except len = 3 */
        /* er, ers */
        if (*lastchar == 'e' ||
            (*lastchar == 'h' && *(lastchar-1) == 't'))
            return (1);
        return (0);
      case 17: /* do not remove after "th", or "e", m.s.l.= 6 */
        /* eri[sz]*  */
        if ( len < 6 ||
            *lastchar == 'e' ||
            (*lastchar == 'h' && *(lastchar-1) == 't'))
            return (1);
        return (0);
      case 18: /* "est" is a major problem. Can't do anything except
                  list bad words */
        if ((len == 3 && (cmpstrn (lastchar-2, "arr") == 0 ||
                          cmpstrn (lastchar-2, "att") == 0 ||
                          cmpstrn (lastchar-2, "det") == 0 ||
                          cmpstrn (lastchar-2, "dig") == 0 ||
                          cmpstrn (lastchar-2, "div") == 0 ||
                          cmpstrn (lastchar-2, "for") == 0 ||
                          cmpstrn (lastchar-2, "hon") == 0 ||
                          cmpstrn (lastchar-2, "inc") == 0 ||
                          cmpstrn (lastchar-2, "inf") == 0 ||
                          cmpstrn (lastchar-2, "inv") == 0 ||
                          cmpstrn (lastchar-2, "mod") == 0 ||
                          cmpstrn (lastchar-2, "mol") == 0 ||
                          cmpstrn (lastchar-2, "pri") == 0 ||
                          cmpstrn (lastchar-2, "unr") == 0 )) ||
            (len == 4 && (cmpstrn (lastchar-3, "armr") == 0 ||
                          cmpstrn (lastchar-3, "bequ") == 0 ||
                          cmpstrn (lastchar-3, "cong") == 0 ||
                          cmpstrn (lastchar-3, "cont") == 0 ||
                          cmpstrn (lastchar-3, "harv") == 0 ||
                          cmpstrn (lastchar-3, "prot") == 0 ||
                          cmpstrn (lastchar-3, "requ") == 0 ||
                          cmpstrn (lastchar-3, "sugg") == 0)) ||
            (len == 5 && (cmpstrn (lastchar-4, "conqu") == 0 ||
                          cmpstrn (lastchar-4, "defor") == 0 ||
                          cmpstrn (lastchar-4, "immod") == 0 ||
                          cmpstrn (lastchar-4, "refor") == 0 ||
                          cmpstrn (lastchar-4, "inter") == 0 ||
                          cmpstrn (lastchar-4, "manif") == 0))) 
            return (1);
        return (0);
      case 19: /* don't remove after a,o,u,v,x, or s unless os; m.s.l.=3 */
        /* ide, ides */
        if (len < 4 ||
            *lastchar == 'u' || *lastchar == 'v' ||
            *lastchar == 'x' || *lastchar == 'o' ||
            *lastchar == 'a' ||
            (*lastchar == 's' && *(lastchar-1) != 'o'))
            return (1);
        return (0);
      case 21: /* not after s[tp]r, else m.s.l.=3 */
        /* ing, ings */
        if (len == 3 && *orig_word == 's' && *lastchar == 'r')
            return (1);
        return (0);
      case 22:  /* ise if len > 5 and follows n,m,r,t,l */
        if (len >= 5 && (*lastchar == 'l' ||
                         *lastchar == 'r' ||
                         *lastchar == 't' ||
                         *lastchar == 'n' ||
                         *lastchar == 'm'))
            return (0);
        return (1);
      case 23: /* don't remove after  "cons" */
        /* ist, ists */
        if (len < 5 ||
            cmpstrn (lastchar-3, "cons") == 0)
            return (1);
        return (0);
      case 24: /* remove only after l,eor */
        /* ite* */
        if (len > 3 && (*lastchar == 'l' || 
                        (*lastchar == 'r' && *(lastchar-1) == 'o' &&
                         *(lastchar-2) == 'e')))
            return(0);
        return (1);
      case 25: /* don't remove after "ce", "pr", or "g" */
        /* iv* */
        if (len < 5 ||
            (*lastchar == 'g') ||
            (*lastchar == 'e' && *(lastchar-1) == 'c') ||
            (*lastchar == 'r' && *(lastchar-1) == 'p'))
            return (1);
        return (0);
      case 26: /* don't remove after b or p */
        /* ly, lies  */
        if (len < 4 || *lastchar == 'p' ||
            *lastchar == 'b')
            return (1);
        return (0);
      case 27: /* don't remove after "wit", har */
        /* ness, nesses */
        if (len > 3 ||
            cmpstrn (lastchar-2, "wit") != 0 ||
            cmpstrn (lastchar-2, "har") != 0)
            return (0);
        return (1);
      case 28: /* remove only after s or t, unless ot */
        /* or, ors */
        if (*lastchar == 's' ||
            (*lastchar == 't' && *(lastchar-1) != 'o'))
            return (0);
        return (1);
      case 29: /* don't remove after s or u */
        /* s */
        return (*lastchar == 's' || *lastchar == 'u');
      case 30: /* remove only after l,m,n,r */
        /* um */
        if (len >= 5 && (*lastchar == 'l' || *lastchar == 'm' ||
                         *lastchar == 'n' || *lastchar == 'r'))
            return (0);
        return (1);
      default:    /* unknown condition code */
        (void)fprintf (stderr,
                       "stemmer: exit- bad_cc: %ld in stemmed word %s\n",
                       cond_code, orig_word);
        exit(1);
    }
    return (0);
}

/* Compare the two equal length strings word1 and word2. */
/* Return 0 if equal, nonzero otherwise */
static int
cmpstrn (word1, word2)
register char *word1;
register char *word2;
{
    while (*word2 && *word1 == *word2) {
        word1++; word2++;
    }
    return (*word2);
}

static
char *
t_recode(stem, lastchar, cc)
char *stem;
char *lastchar;
long cc;
{
    int word_len;
    long cond_code;
    char *trie_stem(), *match_start;

    word_len = lastchar - stem + 1;
    
    if (word_len < 3) {
        return (stem);
    }
    
    if ((! islower (*lastchar)) || (! islower (*(lastchar-1)))) {
        return (stem);
    }

    /* Add an 'e' to short stems (when suffix didn't start with
       a consonant) of the form   consonant, vowel, consonant. */
    if (word_len == 3 && cc != -1 && cc != 29 && cc != 26 && cc != 1 &&
        is_consonant [*lastchar - 'a'] &&
        is_vowel [*(lastchar-1) - 'a'] &&
        is_consonant [*(lastchar-2) - 'a']) {
        lastchar++;
        word_len++;
        *lastchar = 'e';
        *(lastchar+1) = '\0';
    }

    /* Change final i to y on short stems if est, er, ers, es, ed removed */
    if (word_len <=5 && *lastchar == 'i' &&
        (cc == 4 || cc == 11 || cc == 16 || cc == 18)) {
        *lastchar = 'y';
    }

    /* Note: Rest of recode is independent of incoming cc */

    /* check for double consonant; remove if found */
    if (word_len > 3 && *lastchar == *(lastchar-1) &&
        double_letter[*lastchar - 'a'] == 1) {
        *lastchar-- = '\0';
        word_len--;
    }

    match_start = trie_stem (lastchar, stem, recode_trie, &cond_code);
/*    cond_code = trie_recode (lastchar, word_len, recode_trie); */

    if (match_start > lastchar)
        /* No changes */
        return (stem);

    switch (cond_code) {
        
      case 0:   /* iev -> ief */
        *lastchar = 'f';
        break;
      case 1:   /* remove final letter */
        *lastchar = '\0';
        break;
      case 2:   /* umpt -> um */
        *(lastchar-1) = '\0';
        break;
      case 3:   /* rpt -> rb */
        *(lastchar-1) = 'b';
        *lastchar = '\0';
        break;
      case 4:   /* insert 'e' before final 'r' */
        *lastchar++ = 'e';
        *lastchar++ = 'r';
        *lastchar   = '\0';
        break;
      case 5:   /* olv -> olut  except wolves, absolve */
        if ((word_len > 3 && *(lastchar-3) == 'w') ||
            (word_len > 5 && *(lastchar-4) == 'b' &&
             *(lastchar-3) == 's')) {
            *lastchar = 'f';    /* same as case 14 */
            break;
        }        
        *lastchar++ = 'u';
        *lastchar++ = 't';
        *lastchar   = '\0';
        break;
      case 6:   /* ul -> l except after a, i, o */
        if ( word_len > 3 &&
            *(lastchar-2) != 'a' &&
            *(lastchar-2) != 'i' &&
            *(lastchar-2) != 'o') {
            *(lastchar-1) = 'l';
            *lastchar = '\0';
        }
        break;
      case 7:   /* change final 'trice' to end in 'ix' */
        *(lastchar-2) = 'i';
        *(lastchar-1) = 'x';
        *lastchar = '\0';
        break;
      case 8:   /* change final 'dice' 'rtice' to end in 'ex' */
        *(lastchar-2) = 'e';
        *(lastchar-1) = 'x';
        *lastchar = '\0';
        break;
      case 9:   /* change final consonant to 's' */
        *lastchar = 's';
        break;
      case 10:  /* {h,sc,exp,ext,off,pret,susp,def}ens to .end */
                /* vas,uas,trus,expans,allus,clus,delus, collis */
                /* ivis, invers, ascent, descent to final d */
        if (word_len > 3) 
            *lastchar = 'd';
        break;
      case 11:  /* her -> hes except following 'p','t', or 'c'*/
        if ( word_len > 3 && 
            *(lastchar-3) != 'p' &&
            *(lastchar-3) != 't' &&
            *(lastchar-3) != 'c')
            *lastchar = 's';
        break;
      case 12:  /* urs -> ur except after 'o' */
        if (word_len > 3 && *(lastchar-3) != 'o')
            *lastchar = '\0';
        break;
      case 13:  /* if word_len>4, {th,i,r}es -> .et ( hypthesis, synthesis) */
        if ( word_len > 4)
            *lastchar = 't';
        break;
      case 14:   /* lv -> lf */
        *lastchar = 'f';
        break;
      case 15:   /* if word_len>4, ond -> ons, but not after c*/
        if (word_len > 4 && *(lastchar-3) != 'c')
            *lastchar = 's';
        break;
      case 16:   /* came -> come */
        *(lastchar-2) = 'o';
        break;
      case 17:   /* abil -> abl, ibil -> ibl*/
        if (word_len > 4) {
            *(lastchar-1) = 'l';
            *lastchar = '\0';
        }
        break;
      case 18:   /* drop the last two letters, does -> do, pathet -> path,
                    addit -> add */
        *(lastchar-1) = '\0';
        break;
      case 19:   /* salv -> sav */
        *(lastchar-1) = 'v';
        *lastchar = '\0';
        break;
      case 20:   /* obedi -> obey */
        *(lastchar-1) = 'y';
        *lastchar = '\0';
        break;
      case 21:   /* gav -> giv, began,begun -> begin*/
        *(lastchar-1) = 'i';
        break;
      case 22:   /* ew -> ow  (flew,grew,threw,knew,blew) */
        *(lastchar-1) = 'o';
        break;
      case 23:   /* ew -> aw  (drew) */
        *(lastchar-1) = 'a';
        break;
      case 24:   /* ship - remove and then restem */
        if (word_len > 6) {
            *(lastchar-3) = '\0';
            stem = tstemmer (stem);
        }
        break;
      case 25:   /* remove double l if sufficient length (cancelled) */
        if (word_len > 5)
            *(lastchar) = '\0';
        break;
      case 26:   /* change  on short words */
        break;
      case 27:   /* Change British our to or */
        if (word_len > 5 &&
            *(lastchar-3) != 'h' &&
            *(lastchar-3) != 't' &&
            *(lastchar-3) != 'c' &&
            *(lastchar-3) != 'f') {
            *(lastchar-1) = 'r';
            *lastchar = '\0';
        }
        break;
      case 28:   /* galact, syntact to end in ax */
        *lastchar = '\0';
        *(lastchar-1) = 'x';
        break;
      case 29:   /* enc to ent */
        if (word_len >= 5)
            *lastchar = 't';
        break;
      case 30:   /* miss to mit */
        if (word_len > 4) {
            *lastchar = '\0';
            *(lastchar-1) = 't';
        }
        break;
      case 31:   /* quis to quir */
        *lastchar = 'r';
        break;
      case 32:   /* final d,r in four letter words dropped sometimes */
        if (word_len == 4)
            *lastchar = '\0';
        break;
      case 33:   /* Nothing - used for exceptions to above rules */
        break;
      default:   /* error: some case should match */
        (void) fprintf(stderr,"error: non-matching case in recode\n");
        exit(1);
        break;
    }
    return (stem);
}

    

