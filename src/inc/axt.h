/* AXT - A simple alignment format with four lines per
 * alignment.  The first specifies the names of the
 * two sequences that align and the position of the
 * alignment, as well as the strand.  The next two
 * lines contain the alignment itself including dashes
 * for inserts.  The alignment is separated from the
 * next alignment with a blank line. 
 *
 * This file contains routines to read such alignments.
 * Note that though the coordinates are one based and
 * closed on disk, they get converted to our usual half
 * open zero based in memory. 
 *
 * This also contains code for simple DNA alignment scoring
 * schemes. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef AXT_H
#define AXT_H

#ifndef LINEFILE_H
#include "linefile.h"
#endif 

struct axt
/* This contains information about one xeno alignment. */
    {
    struct axt *next;
    char *qName;		/* Name of query sequence. */
    int qStart, qEnd;		/* Half open zero=based coordinates. */
    char qStrand;		/* Is this forward or reverse strand + or - */
    char *tName;		/* Name of target. */
    int tStart, tEnd;		/* Target coordinates. */
    char tStrand;               /* Target strand - currently always +. */
    int score;	                /* Score.  Zero for unknown.  Units arbitrary. */
    int symCount;               /* Size of alignments. */
    char *qSym, *tSym;          /* Alignments. */
    int frame;			/* If non-zero then translation frame. */
    };

void axtFree(struct axt **pEl);
/* Free an axt. */

void axtFreeList(struct axt **pList);
/* Free a list of dynamically allocated axt's */

struct axt *axtRead(struct lineFile *lf);
/* Read in next record from .axt file and return it.
 * Returns NULL at EOF. */

boolean axtCheck(struct axt *axt, struct lineFile *lf);
/* Return FALSE if there's a problem with axt. */

void axtWrite(struct axt *axt, FILE *f);
/* Output axt to axt file. */

int axtCmpQuery(const void *va, const void *vb);
/* Compare to sort based on query position. */

int axtCmpTarget(const void *va, const void *vb);
/* Compare to sort based on target position. */

int axtCmpScore(const void *va, const void *vb);
/* Compare to sort based on score. */

struct axtScoreScheme
/* A scoring scheme or DNA alignment. */
    {
    struct scoreMatrix *next;
    int matrix[256][256];   /* Look up with letters. */
    int gapOpen;	/* Gap open cost. */
    int gapExtend;	/* Gap extension. */
    };

void axtScoreSchemeFree(struct axtScoreScheme **pObj);
/* Free up score scheme. */

struct axtScoreScheme *axtScoreSchemeDefault();
/* Return default scoring scheme (after blastz).  Do NOT axtScoreSchemeFree
 * this. */

struct axtScoreScheme *axtScoreSchemeProteinDefault();
/* Returns default protein scoring scheme.  This is
 * scaled to be compatible with the blastz one.  Don't
 * axtScoreSchemeFree this. */

struct axtScoreScheme *axtScoreSchemeRead(char *fileName);
/* Read in scoring scheme from file. Looks like
    A    C    G    T
    91 -114  -31 -123
    -114  100 -125  -31
    -31 -125  100 -114
    -123  -31 -114   91
    O = 400, E = 30
 * axtScoreSchemeFree this when done. */

int axtScore(struct axt *axt, struct axtScoreScheme *ss);
/* Return calculated score of axt. */

int axtScoreUngapped(struct axtScoreScheme *ss, char *q, char *t, int size);
/* Score ungapped alignment. */

int axtScoreDnaDefault(struct axt *axt);
/* Score DNA-based axt using default scheme. */

int axtScoreProteinDefault(struct axt *axt);
/* Score protein-based axt using default scheme. */

void axtSubsetOnT(struct axt *axt, int newStart, int newEnd, 
	struct axtScoreScheme *ss, FILE *f);
/* Write out subset of axt that goes from newStart to newEnd
 * in target coordinates. */

int axtTransPosToQ(struct axt *axt, int tPos);
/* Convert from t to q coordinates */

struct axtBundle
/* A bunch of axt's on the same query/target sequence. */
    {
    struct axtBundle *next;
    struct axt *axtList;	/* List of alignments. */
    int tSize;			/* Size of target. */
    int qSize;			/* Size of query. */
    };

void axtBundleFree(struct axtBundle **pObj);
/* Free a axtBundle. */

void axtBundleFreeList(struct axtBundle **pList);
/* Free a list of gfAxtBundles. */

void axtBlastOut(struct axtBundle *abList, 
	int queryIx, boolean isProt, FILE *f, 
	char *databaseName, int databaseSeqCount, double databaseLetterCount, 
	boolean isWu, boolean isXml, char *ourId);
/* Output a bundle of axt's on the same query sequence in blast format.
 * The parameters in detail are:
 *   ab - the list of bundles of axt's. 
 *   f  - output file handle
 *   databaseSeqCount - number of sequences in database
 *   databaseLetterCount - number of bases or aa's in database
 *   isWu - TRUE if want wu-blast rather than blastall format
 *   isXml - TRUE if want xml format
 *   ourId - optional (may be NULL) thing to put in header
 */

#endif /* AXT_H */

