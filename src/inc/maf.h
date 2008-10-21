/* maf.h - Multiple alignment format.  */
#ifndef MAF_H
#define MAF_H

#ifndef COMMON_H
#include "common.h"
#endif

#ifndef AXT_H
#include "axt.h"
#endif

struct mafFile
/* A file full of multiple alignments. */
    {
    struct mafFile *next;
    int version;	 /* Required */
    char *scoring;	 /* Optional (may be NULL). Name of  scoring scheme. */
    struct mafAli *alignments;	/* Possibly empty list of alignments. */
    struct lineFile *lf; /* Open line file if any. NULL except while parsing. */
    };

void mafFileFree(struct mafFile **pObj);
/* Free up a maf file including closing file handle if necessary. */

void mafFileFreeList(struct mafFile **pList);
/* Free up a list of maf files. */

struct mafAli
/* A multiple alignment. */
    {
    struct mafAli *next;
    double score;        /* Score.  Meaning depends on mafFile.scoring.  0.0 if no scoring. */
    struct mafComp *components;	/* List of components of alignment */
    int textSize;         /* Size of text in each component. */
    struct mafRegDef *regDef; /* source of region definitions (r line) */
    };

void mafAliFree(struct mafAli **pObj);
/* Free up a maf alignment. */

void mafAliFreeList(struct mafAli **pList);
/* Free up a list of maf alignmentx. */

/* the set of syntenic relationships that the previous and
 * following alignments have with the current one */
#define	MAF_INVERSE_STATUS		'V'
#define	MAF_INSERT_STATUS		'I'
#define	MAF_CONTIG_STATUS		'C'
#define	MAF_CONTIG_NESTED_STATUS	'c'
#define	MAF_NEW_STATUS			'N'
#define	MAF_NEW_NESTED_STATUS		'n'
#define	MAF_MAYBE_NEW_STATUS		'S'
#define	MAF_MAYBE_NEW_NESTED_STATUS	's'
#define	MAF_MISSING_STATUS		'M'
#define	MAF_TANDEM_STATUS		'T'

struct mafComp
/* A component of a multiple alignment. */
    {
    struct mafComp *next;
    char *src;	 /* Name of sequence source.  */
    int srcSize; /* Size of sequence source.  */
    char strand; /* Strand of sequence.  Either + or -*/
    int start;	 /* Start within sequence. Zero based. If strand is - is relative to src end. */
    int size;	 /* Size in sequence (does not include dashes).  */
    char *text;  /* The sequence including dashes. */
    char *quality;  /* The quality data (same length as text, or NULL). */
    char leftStatus; /* the syntenic status of the alignment before us vis a vis ourselves */
    int leftLen;     /* length related information for the previous alignment for the species */
    char rightStatus; /* the syntenic status of the alignment after us vis a vis ourselves */
    int rightLen;     /* length related information for the following alignment for the species */
    };

void mafCompFree(struct mafComp **pObj);
/* Free up a maf component. */

void mafCompFreeList(struct mafComp **pList);
/* Free up a list of maf components. */

struct mafRegDef
/* MAF region definition (r line) */
{
    char *type;   // type of definition, one of constants below (not malloced)
    int size;     // region size
    char *id;     // identifiers
};
extern char *mafRegDefTxUpstream;  // transcription start size upstream region

struct mafRegDef *mafRegDefNew(char *type, int size, char *id);
/* construct a new mafRegDef object */

void mafRegDefFree(struct mafRegDef **mrdPtr);
/* Free a mafRegDef object */

int mafPlusStart(struct mafComp *comp);
/* Return start relative to plus strand of src. */

struct mafFile *mafOpen(char *fileName);
/* Open up a .maf file for reading.  Read header and
 * verify. Prepare for subsequent calls to mafNext().
 * Prints error message and aborts if there's a problem. */

struct mafFile *mafMayOpen(char *fileName);
/* Like mafOpen above, but returns NULL rather than aborting 
 * if file does not exist. */

void mafRewind(struct mafFile *mf);
/* Seek to beginning of open maf file */

struct mafAli *mafNext(struct mafFile *mafFile);
/* Return next alignment in file or NULL if at end. 
 * This will close the open file handle at end as well. */

struct mafAli *mafNextWithPos(struct mafFile *mf, off_t *retOffset);
/* Return next alignment in FILE or NULL if at end.  If retOffset is
 * non-NULL, return start offset of record in file. */

struct mafFile *mafReadAll(char *fileName);
/* Read in full maf file */

void mafWriteStart(FILE *f, char *scoring);
/* Write maf header and scoring scheme name (may be null) */

void mafWrite(FILE *f, struct mafAli *maf);
/* Write next alignment to file. */

void mafWriteEnd(FILE *f);
/* Write end tag of maf file. */

void mafWriteAll(struct mafFile *mf, char *fileName);
/* Write out full mafFile. */

struct mafComp *mafMayFindComponent(struct mafAli *maf, char *src);
/* Find component of given source. Return NULL if not found. */

struct mafComp *mafMayFindComponentDb(struct mafAli *maf, char *db);
/* Find component of given database or source. Return NULL if not found. */

struct mafComp *mafFindComponent(struct mafAli *maf, char *src);
/* Find component of given source or die trying. */

struct mafComp *mafMayFindCompSpecies(struct mafAli *maf, char *species, char sepChar);
/* Find component of given source that starts with species followed by sepChar or '\0'
   Return NULL if not found. */

struct mafComp *mafFindCompSpecies(struct mafAli *maf, char *species, char sepChar);
/* Find component of given source that starts with species followed by sepChar or '\0'
   or die trying. */

struct mafComp *mafMayFindCompPrefix(struct mafAli *maf, char *pre, char *sep);
/* Find component of given source that starts with pre followed by sep.
   Return NULL if not found. */

struct mafComp *mafFindCompPrefix(struct mafAli *maf, char *pre, char *sep);
/* Find component of given source that starts with pre followed by sep
   or die trying. */

boolean mafMayFindAllComponents(struct mafAli *maf, struct hash *cHash);
/* Check to see if all components in hash are in maf block.  Return FALSE if not found. */

struct mafComp *mafMayFindComponentInHash(struct mafAli *maf, struct hash *cHash);
/* Find component of given source that starts matches any string in the cHash.
   Return NULL if not found. */

void mafMoveComponentToTop(struct mafAli *maf, char *componentSource);
/* Move given component to head of component list. */

struct mafAli *mafFromAxt(struct axt *pAxt, int tSize, 
	char *tPrefix, int qSize, char *qPrefix);
/* Make up a maf file from axt.  Slower than mafFromAxtTemp,
 * but the axt and maf are independent afterwards. */

void mafFromAxtTemp(struct axt *axt, int tSize, int qSize,
	struct mafAli *temp);
/* Make a maf out of axt,  parasiting on the memory in axt.
 * Do *not* mafFree this temp.  The memory it has in pointers
 * is still owned by the axt.  Furthermore the next call to
 * this function will invalidate the previous temp value.
 * It's sort of a kludge, but quick to run and easy to implement. */

struct mafAli *mafSubset(struct mafAli *maf, char *componentSource,
	int newStart, int newEnd);
/* see mafSubsetE below  (called with getInitialDases = FALSE */

struct mafAli *mafSubsetE(struct mafAli *maf, char *componentSource,
	int newStart, int newEnd, bool getInitialDashes);
/* Extract subset of maf that intersects a given range
 * in a component sequence.  The newStart and newEnd
 * are given in the forward strand coordinates of the
 * component sequence.  The componentSource is typically
 * something like 'mm3.chr1'.  This will return NULL
 * if maf does not intersect range.  The score field
 * in the returned maf will not be filled in (since
 * we don't know which scoring scheme to use). 
 * If getInitialDashes is TRUE then the initial -'s
 * in the reference sequence are *not* removed*/

boolean mafNeedSubset(struct mafAli *maf, char *componentSource,
	int newStart, int newEnd);
/* Return TRUE if maf only partially fits between newStart/newEnd
 * in given component. */

double mafScoreMultiz(struct mafAli *maf);
/* Return score of a maf (calculated rather than what is
 * stored in the structure. */

double mafScoreRangeMultiz(struct mafAli *maf, int start, int size);
/* Return score of a subset of an alignment.  Parameters are:
 *    maf - the alignment
 *    start - the (zero based) offset to start calculating score
 *    size - the size of the subset
 * The following relationship should hold:
 *   scoreRange(maf,start,size) =
 *	scoreRange(maf,0,start+size) - scoreRange(maf,0,start)
 */

double mafScoreMultizMaxCol(int species);
/* Return maximum possible score for a column. */

void mafColMinMaxScore(struct mafAli *maf, 
	double *retMin, double *retMax);
/* Get min/max maf scores for a column. */

void mafFlipStrand(struct mafAli *maf);
/* Reverse complement maf. */

void mafSrcDb(char *name, char *retDb, int retDbSize);
/* Parse out just database part of name (up to but not including
 * first dot). If dot found, return entire name */

boolean mafColumnEmpty(struct mafAli *maf, int col);
/* Return TRUE if the column is all '-' or '.' */

void mafStripEmptyColumns(struct mafAli *maf);
/* Remove columns that are all '-' or '.' from  maf. */

#endif /* MAF_H */

