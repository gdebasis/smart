#ifndef FIGH
#define FIGH

#define WIDTH 10.0		/* in inches */
#define HEIGHT 7.5		/* in inches */
#define CIRCLE_WIDTH 3.0	/* radius of circle, in inches */
#define PIXS_PER_INCH 80
#define STRETCH (CIRCLE_WIDTH * PIXS_PER_INCH)
#define X_STRETCH (STRETCH * (WIDTH / HEIGHT))
#define Y_STRETCH (STRETCH)
#define YSHIFT (0.3*PIXS_PER_INCH)	/* pixels to shift picture down */
#define XSHIFT 0		/* pixels to shift picture right */

#define SEG_X_STRETCH (X_STRETCH * (CIRCLE_WIDTH-0.5) / CIRCLE_WIDTH)
#define SEG_Y_STRETCH (Y_STRETCH * (CIRCLE_WIDTH-0.5) / CIRCLE_WIDTH)
#define SLINK_X_STRETCH (X_STRETCH * (CIRCLE_WIDTH-1.25) / CIRCLE_WIDTH)
#define SLINK_Y_STRETCH (Y_STRETCH * (CIRCLE_WIDTH-1.25) / CIRCLE_WIDTH)

#define LINETYPE 0
#define LINEWIDTH 1

#define CLR_DEFAULT -1
#define CLR_BLACK 0
#define CLR_BLUE 1
#define CLR_GREEN 2
#define CLR_CYAN 3
#define CLR_RED 4
#define CLR_MAGENTA 5
#define CLR_YELLOW 6
#define CLR_WHITE 7

#define TITLE_X_POS (20+XSHIFT)
#define INFO_X_POS ((WIDTH*PIXS_PER_INCH) - 20 + XSHIFT)
#define TEXT_HEIGHT 14    /* really it's 9, but this gives some nice space */

/* Params: justification, rotation, xpos, ypos, text */
/* NOTE: Assumes fontsize=12, height=9, width=6*length
 *           and fontsize=9,  height=7, width=5*length */
#define SIMTEXT_FMT "4 %d 0  9 0 -1 0 %6.4f 4 7 20 %d %d %s\001\n"
#define TEXT_FMT    "4 %d 0 12 0 %d 0 %6.4f 4 9 %d %d %d %s\001\n"
#define TITLE_FMT   "4 0  1 12 0 %d 0 0.000 4 9 %d %d %d %s\001\n"
#define LABEL_FMT   "4 0 31 12 0 -1 0 0.000 4 9 %d %d %d %s\001\n"
#define INFO_FMT    "4 2  1 12 0 -1 0 0.000 4 9 %d %d %d %s\001\n"
#define BIG_INFO_FMT    "4 2  1 18 0 -1 0 0.000 4 21 %d %d %d %s\001\n"
#define LINE_FMT "2 1 0 1 %d 0 0 0 0.000 -1 0 0\n     %d %d %d %d 9999 9999\n"
/* Like LINE, but first two args are line style (0=norm,1=dash,2=dots)
 * and line thickness (in pixels), 3=dots/inch(or something) */
#define LINEV_FMT "2 1 %d %d %d 0 0 0 3.000 -1 0 0\n     %d %d %d %d 9999 9999\n"

#define PIXWIDTH(len) ((len)*6)
#define JUSTIFY(rot) ((rot) <= M_PI_2 ? 0 : (rot) > (3*M_PI_2) ? 0 : 2)
#define ROTATE(rot) JUSTIFY(rot)==0 ? (rot) : \
    (rot)>=M_PI ? ((rot)-M_PI) : ((rot)+M_PI)


typedef struct {
    EID did;
    char beg_seg, end_seg;
    double rot;		/* rotation of text (fig output) */
    int x, y;		/* where put (fig output) */
} POINT;

#endif
