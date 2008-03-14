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

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif

#ifndef CHAIN_H
#include "chain.h"
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

struct axt *axtReadWithPos(struct lineFile *lf, off_t *retOffset);
/* Read next axt, and if retOffset is not-NULL, fill it with
 * offset of start of axt. */

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

int axtCmpTargetScoreDesc(const void *va, const void *vb);
/* Compare to sort based on target name and score descending. */

struct axtScoreScheme
/* A scoring scheme or DNA alignment. */
    {
    struct scoreMatrix *next;
    int matrix[256][256];   /* Look up with letters. */
    int gapOpen;	/* Gap open cost. */
    int gapExtend;	/* Gap extension. */
    char *extra;        /* extra parameters */
    };

void axtScoreSchemeFree(struct axtScoreScheme **pObj);
/* Free up score scheme. */

struct axtScoreScheme *axtScoreSchemeDefault();
/* Return default scoring scheme (after blastz).  Do NOT axtScoreSchemeFree
 * this. */

struct axtScoreScheme *axtScoreSchemeSimpleDna(int match, int misMatch, int gapOpen, int gapExtend);
/* Return a relatively simple scoring scheme for DNA. */

struct axtScoreScheme *axtScoreSchemeRnaDefault();
/* Return default scoring scheme for RNA/DNA alignments
 * within the same species.  Do NOT axtScoreSchemeFree
 * this. */

struct axtScoreScheme *axtScoreSchemeFromBlastzMatrix(char *text, int gapOpen, int gapExtend);
/* return scoring schema from a string in the following format */
/* 91,-90,-25,-100,-90,100,-100,-25,-25,-100,100,-90,-100,-25,-90,91 */

struct axtScoreScheme *axtScoreSchemeRnaFill();
/* Return scoreing scheme a little more relaxed than 
 * RNA/DNA defaults for filling in gaps. */

struct axtScoreScheme *axtScoreSchemeProteinDefault();
/* Returns default protein scoring scheme.  This is
 * scaled to be compatible with the blastz one.  Don't
 * axtScoreSchemeFree this. */

struct axtScoreScheme *axtScoreSchemeProteinRead(char *fileName);
/* read in blosum-like matrix */

struct axtScoreScheme *axtScoreSchemeRead(char *fileName);
/* Read in scoring scheme from file. Looks like
    A    C    G    T
    91 -114  -31 -123
    -114  100 -125  -31
    -31 -125  100 -114
    -123  -31 -114   91
    O = 400, E = 30
 * axtScoreSchemeFree this when done. */

struct axtScoreScheme *axtScoreSchemeReadLf(struct lineFile *lf );
/* Read in scoring scheme from file. Looks like
    A    C    G    T
    91 -114  -31 -123
    -114  100 -125  -31
    -31 -125  100 -114
    -123  -31 -114   91
    O = 400, E = 30
 * axtScoreSchemeFree this when done. */

void axtScoreSchemeDnaWrite(struct axtScoreScheme *ss, FILE *f, char *name);
/* output the score dna based score matrix in meta Data format to File f,
name should be set to the name of the program that is using the matrix */

int axtScoreSym(struct axtScoreScheme *ss, int symCount, char *qSym, char *tSym);
/* Return score without setting up an axt structure. */

int axtScore(struct axt *axt, struct axtScoreScheme *ss);
/* Return calculated score of axt. */

int axtScoreFilterRepeats(struct axt *axt, struct axtScoreScheme *ss);
/* Return calculated score of axt. do not score gaps if they are repeat masked*/

int axtScoreUngapped(struct axtScoreScheme *ss, char *q, char *t, int size);
/* Score ungapped alignment. */

int axtScoreDnaDefault(struct axt *axt);
/* Score DNA-based axt using default scheme. */

int axtScoreProteinDefault(struct axt *axt);
/* Score protein-based axt using default scheme. */

boolean axtGetSubsetOnT(struct axt *axt, struct axt *axtOut,
			int newStart, int newEnd, struct axtScoreScheme *ss,
			boolean includeEdgeGaps);
/* Return FALSE if axt is not in the new range.  Otherwise, set axtOut to
 * a subset that goes from newStart to newEnd in target coordinates. 
 * If includeEdgeGaps, don't trim target gaps before or after the range. */

void axtSubsetOnT(struct axt *axt, int newStart, int newEnd, 
	struct axtScoreScheme *ss, FILE *f);
/* Write out subset of axt that goes from newStart to newEnd
 * in target coordinates. */

int axtTransPosToQ(struct axt *axt, int tPos);
/* Convert from t to q coordinates */

void axtSwap(struct axt *axt, int tSize, int qSize);
/* Flip target and query on one axt. */

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
	char *blastType, char *ourId, double minIdentity);
/* Output a bundle of axt's on the same query sequence in blast format.
 * The parameters in detail are:
 *   ab - the list of bundles of axt's. 
 *   f  - output file handle
 *   databaseSeqCount - number of sequences in database
 *   databaseLetterCount - number of bases or aa's in database
 *   blastType - blast/wublast/blast8/blast9/xml
 *   ourId - optional (may be NULL) thing to put in header
 */

struct axt *axtAffine(bioSeq *query, bioSeq *target, struct axtScoreScheme *ss);
/* Return alignment if any of query and target using scoring scheme.  This does
 * dynamic program affine-gap based alignment.  It's not suitable for very large
 * sequences. */

boolean axtAffineSmallEnough(double querySize, double targetSize);
/* Return TRUE if it is reasonable to align sequences of given sizes
 * with axtAffine. */


struct axt *axtAffine2Level(bioSeq *query, bioSeq *target, struct axtScoreScheme *ss);
/* 

   Return alignment if any of query and target using scoring scheme. 
   
   2Level uses an economical amount of ram and should work for large target sequences.
   
   If Q is query size and T is target size and M is memory size, then
   Total memory used M = 30*Q*sqrt(T).  When the target is much larger than the query
   this method saves ram, and average runtime is only 50% greater, or 1.5 QT.  
   If Q=5000 and T=245,522,847 for hg17 chr1, then M = 2.2 GB ram.  
   axtAffine would need M=3QT = 3.4 TB.
   Of course massive alignments will be painfully slow anyway.

   Works for protein as well as DNA given the correct scoreScheme.
  
   NOTES:
   Double-gap cost is equal to gap-extend cost, but gap-open would also work.
   On very large target, score integer may overflow.
   Input sequences not checked for invalid chars.
   Input not checked but query should be shorter than target.
   
*/

void axtAddBlocksToBoxInList(struct cBlock **pList, struct axt *axt);
/* Add blocks (gapless subalignments) from (non-NULL!) axt to block list. 
 * Note: list will be in reverse order of axt blocks. */

void axtPrintTraditional(struct axt *axt, int maxLine, struct axtScoreScheme *ss, 
	FILE *f);
/* Print out an alignment with line-breaks. */

void axtPrintTraditionalExtra(struct axt *axt, int maxLine,
			      struct axtScoreScheme *ss, FILE *f,
			      boolean reverseTPos, boolean reverseQPos);
/* Print out an alignment with line-breaks.  If reverseTPos is true, then
 * the sequence has been reverse complemented, so show the coords starting
 * at tEnd and decrementing down to tStart; likewise for reverseQPos. */

double axtIdWithGaps(struct axt *axt);
/* Return ratio of matching bases to total symbols in alignment. */

double axtCoverage(struct axt *axt, int qSize, int tSize);
/* Return % of q and t covered. */

void axtOutPretty(struct axt *axt, int lineSize, FILE *f);
/* Output axt in pretty format. */

#endif /* AXT_H */

