/* pbTracks.h - stuff private to pbTracks, but shared with
 * individual functions. */

#ifndef PBTRACKS_H
#define PBTRACKS_H

#define CHARGE_POS 1
#define CHARGE_NEG 2
#define NEUTRAL    3
#define POLAR      4

#ifndef VGFX_H
#include "vGfx.h"
#endif

extern struct cart *cart; /* The cart where we keep persistent variables. */
extern char *database;	  /* Name of database we're using. */
extern char *organism;	  /* Name of organism we're working on. */
extern int gfxBorder;		/* Width of graphics border. */
extern int insideX;			/* Start of area to draw track in in pixels. */

extern boolean hgDebug;      /* Activate debugging code. Set to true by hgDebug=on in command line*/
extern int imagePixelHeight;

extern struct cart *cart;	/* The cart where we keep persistent variables. */

/* These variables persist from one incarnation of this program to the
 * next - living mostly in the cart. */
extern char *database;			/* Name of database we're using. */
extern char *organism;			/* Name of organism we're working on. */

extern int gfxBorder;		/* Width of graphics border. */
extern int insideWidth;		/* Width of area to draw tracks in in pixels. */

extern char *protDbName;               /* Name of proteome database for this genome. */
extern boolean suppressHtml;	/* If doing PostScript output we'll suppress most
         			 * of HTML output. */
extern char *protDbName;
extern char *proteinID;
extern char *protDisplayID;

extern char *protSeq;
extern int   protSeqLen;
extern char aaAlphabet[];

extern char aaChar[];
extern float aa_hydro[];
extern int aa_attrib[];
extern char aa[];
extern int exCount;
extern int exStart[], exEnd[];
extern int aaStart[], aaEnd[];

extern struct vGfx *g_vg;
extern MgFont *g_font;
extern int currentYoffset;
extern int pbScale;

extern int *yOffp;
extern double tx[], ty[];

extern double xScale;
extern double yScale;

extern struct pbStampPict *stampPictPtr;

extern struct sqlConnection *spConn;  
extern double avg[];
extern double stddev[];

extern int exStart[], exEnd[];
extern int exCount;

extern int aaStart[], aaEnd[];

void hPrintf(char *format, ...);

char *getAA(char *pepAccession);
int chkAnomaly(double currentAvg, double avg, double stddev);

void calxy(int xin, int yin, int *outxp, int *outyp);

void get_exons(char *proteinID, char *mrnaID);
void printExonAA(char *proteinID, char *aa, int exonNum);
void doGenomeBrowserLink(char *proteinID, char *mrnaID);

void doTracks(char *proteinID, char *mrnaID, char *aa, struct vGfx *vg, int *yOffp);
void doStamps(char *proteinID, char *mrnaID, char *aa, struct vGfx *vg, int *yOffp);

void domainsPrint(struct sqlConnection *conn, char *swissProtAcc);
void aaPropertyInit();

#endif
