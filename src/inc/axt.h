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

struct axtScoreScheme
/* A scoring scheme or DNA alignment. */
    {
    struct scoreMatrix *next;
    int matrix[5][5];   /* Look up with A_BASE_VAL etc. */
    int gapOpen;	/* Gap open cost. */
    int gapExtend;	/* Gap extension. */
    };

struct axtScoreScheme *axtScoreSchemeDefault();
/* Return default scoring scheme (after blastz). */

struct axtScoreScheme *axtScoreSchemeRead(char *fileName);
/* Read in scoring scheme from file. Looks like
    A    C    G    T
    91 -114  -31 -123
    -114  100 -125  -31
    -31 -125  100 -114
    -123  -31 -114   91
    O = 400, E = 30
*/

int axtScore(struct axt *axt, struct axtScoreScheme *ss);
/* Return calculated score of axt. */

void axtSubsetOnT(struct axt *axt, int newStart, int newEnd, 
	struct axtScoreScheme *ss, FILE *f);
/* Write out subset of axt that goes from newStart to newEnd
 * in target coordinates. */

#endif /* AXT_H */

