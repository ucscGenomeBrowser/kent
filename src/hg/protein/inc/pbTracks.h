/* pbTracks.h - stuff private to pbTracks, but shared with
 * individual functions. */

#ifndef PBTRACKS_H
#define PBTRACKS_H

#define CHARGE_POS 1
#define CHARGE_NEG 2
#define NEUTRAL    3
#define POLAR      4

#define MAX_PB_PIXWIDTH 30000

#define MAX_PB_ORG 50		/* max number of oganisms that support PB */

#define GLOBAL_PB_DB "proteome"

#ifndef VGFX_H
#include "vGfx.h"
#endif

#include "spDb.h"

extern boolean IAmPbTracks;	/* global flag indicating the program running is pbTracks or not */

extern Color pbRed, pbBlue;

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
extern char *hgsid;

extern int gfxBorder;		/* Width of graphics border. */
extern int insideWidth;		/* Width of area to draw tracks in in pixels. */

extern char *protDbName;               /* Name of proteome database for this genome. */

extern struct tempName gifTn, gifTn2;  /* gifTn for tracks image and gifTn2 for stamps image */
extern boolean hideControls;    /* Hide all controls? */
extern boolean suppressHtml;	/* If doing PostScript output we'll suppress most
         			 * of HTML output. */
extern char *protDbName;
extern char *proteinID;
extern char *protDisplayID;
extern char *description;

extern char *positionStr;
extern char *prevGBChrom;              /* chrom          previously chosen by Genome Browser */
extern int  prevGBStartPos;            /* start position previously chosen by Genome Browser */
extern int  prevGBEndPos;              /* end position previously chosen by Genome Browser */
extern int  prevExonStartPos;          /* start pos chosen by GB, converted to exon pos */
extern int  prevExonEndPos;            /* end   pos chosen by GB, converted to exon pos */

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

extern int sfCount;
extern char *ensPepName;
extern struct vGfx *g_vg;
extern MgFont *g_font;
extern int currentYoffset;
extern int pbScale;
extern char pbScaleStr[5];
extern boolean initialWindow;

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

extern char kgProtMapTableName[];

extern int blockSize[], blockSizePositive[];
extern int blockStart[], blockStartPositive[];
extern int blockEnd[], blockEndPositive[];
extern int blockGenomeStart[], blockGenomeStartPositive[];
extern int blockGenomeEnd[], blockGenomeEndPositive[];
extern int trackOrigOffset;        /* current track display origin offset */
extern int aaOrigOffset;           /* current track AA base origin offset */
extern boolean scaleButtonPushed;

extern struct vGfx *vg, *vg2;
extern Color bkgColor;
extern Color abnormalColor;
extern Color normalColor;
extern char hgsidStr[];
extern boolean proteinInSupportedGenome;
extern int protCntInSupportedGenomeDb; /* The protein count in supported genome DBs */
extern int protCntInSwissByGene;       /* The protein count from gene search in Swiss-Prot */

void hWrites(char *string);
void hButton(char *name, char *label);
void hPrintf(char *format, ...);

char *getAA(char *pepAccession);
int chkAnomaly(double currentAvg, double pctLow, double pctHi);

void calxy(int xin, int yin, int *outxp, int *outyp);

int getSuperfamilies(char *proteinID);

void getExonInfo(char *proteinID, int *exonCount, char **chrom, char *strand);
void printExonAA(char *proteinID, char *aa, int exonNum);
void doPathwayLinks(char *proteinID, char *mrnaID);
void doGenomeBrowserLink(char *spAcc, char *mrnaID, char *hgsidStr);
void doGeneDetailsLink(char *spAcc, char *mrnaID, char *hgsidStr);
void doGeneSorterLink(char *spAcc, char *mrnaID, char *hgsidStr);
void doBlatLink(char *db, char *sciName, char *commonName, char *userSeq);

void doTracks(char *proteinID, char *mrnaID, char *aa, int *yOffp, char *psOutput);
void doStamps(char *proteinID, char *mrnaID, char *aa, struct vGfx *vg, int *yOffp);

void domainsPrint(struct sqlConnection *conn, char *swissProtAcc);
void aaPropertyInit(int *hasResFreq);
void printFASTA(char *proteinID, char *aa);
int  searchProteinsInSupportedGenomes(char *proteinID, char **gDatabase);
int  searchProteinsInSwissProtByGene(char *queryGeneID);
void presentProteinSelections(char *proteinID, int protCntInSwissByGene, int protCntInSupportedGenomeDb);
char *hDbOrganism(char *database);
void doSamT02(char *proteinId, char *database);

#endif
