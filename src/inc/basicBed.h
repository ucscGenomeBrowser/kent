/* basicBed.h contains the basic interface to Browser Extensible Data (bed) files and tables.
 * The idea behind bed is that the first three fields are defined and required.
 * A total of 15 fields are defined, and the file can contain any number of these.
 * In addition after any number of defined fields there can be custom fields that
 * are not defined in the bed spec.
 *
 * There's additional bed-related code in src/hg/inc/bed.h.  This module contains the
 * stuff that's independent of the database and other genomic structures. */

#ifndef BASICBED_H
#define BASICBED_H

#ifndef PSL_H
#include "psl.h"
#endif

struct bed
/* Browser extensible data */
    {
    struct bed *next;  /* Next in singly linked list. */
    char *chrom;	/* Human chromosome or FPC contig */
    unsigned chromStart;	/* Start position in chromosome */
    unsigned chromEnd;	/* End position in chromosome */
    char *name;	/* Name of item */

    /* The following items are not loaded by   the bedLoad routines. */
    int score; /* Score - 0-1000 */
    char strand[2];  /* + or -.  */
    unsigned thickStart; /* Start of where display should be thick (start codon for genes) */
    unsigned thickEnd;   /* End of where display should be thick (stop codon for genes) */
    unsigned itemRgb;    /* RGB 8 bits each */
    unsigned blockCount; /* Number of blocks. */
    int *blockSizes;     /* Comma separated list of block sizes.  */
    int *chromStarts;    /* Start positions inside chromosome.  Relative to chromStart*/


    int expCount;	/* Experiment count */
    int *expIds;		/* Comma separated list of Experiment ids */
    float *expScores;	/* Comma separated list of Experiment scores. */
    };

#define bedKnownFields 15	/* Maximum known fields in bed */

struct bed3
/* Browser extensible data - first three fields */
    {
    struct bed3 *next;  /* Next in singly linked list. */
    char *chrom;	/* Human chromosome or FPC contig */
    unsigned chromStart;	/* Start position in chromosome */
    unsigned chromEnd;	/* End position in chromosome */
    };

struct bed3 *bed3New(char *chrom, int start, int end);
/* Make new bed3. */

struct bed4
/* Browser extensible data - first four fields */
    {
    struct bed4 *next;  /* Next in singly linked list. */
    char *chrom;	/* Human chromosome or FPC contig */
    unsigned chromStart;	/* Start position in chromosome */
    unsigned chromEnd;	/* End position in chromosome */
    char *name;	/* Name of item */
    };


void bedStaticLoad(char **row, struct bed *ret);
/* Load a row from bed table into ret.  The contents of ret will
 * be replaced at the next call to this function. */

struct bed *bedLoad(char **row);
/* Load a bed from row fetched with select * from bed
 * from database.  Dispose of this with bedFree(). 
 * This loads first four fields. */

struct bed *bedCommaIn(char **pS, struct bed *ret);
/* Create a bed out of a comma separated string. 
 * This will fill in ret if non-null, otherwise will
 * return a new bed */

void bedFree(struct bed **pEl);
/* Free a single dynamically allocated bed such as created
 * with bedLoad(). */

void bedFreeList(struct bed **pList);
/* Free a list of dynamically allocated bed's */

void bedOutput(struct bed *el, FILE *f, char sep, char lastSep);
/* Print out bed.  Separate fields with sep. Follow last field with lastSep. */

#define bedTabOut(el,f) bedOutput(el,f,'\t','\n');
/* Print out bed as a line in a tab-separated file. */

#define bedCommaOut(el,f) bedOutput(el,f,',',',');
/* Print out bed as a comma separated list including final comma. */

/* --------------- End of AutoSQL generated code. --------------- */

int bedCmp(const void *va, const void *vb);
/* Compare to sort based on chrom,chromStart. */

int bedCmpEnd(const void *va, const void *vb);
/* Compare to sort based on chrom,chromEnd. */

int bedCmpScore(const void *va, const void *vb);
/* Compare to sort based on score - lowest first. */

int bedCmpPlusScore(const void *va, const void *vb);
/* Compare to sort based on chrom, chromStart and score - lowest first. */

int bedCmpSize(const void *va, const void *vb);
/* Compare to sort based on size of element (end-start == size) */

int bedCmpChromStrandStart(const void *va, const void *vb);
/* Compare to sort based on chrom,strand,chromStart. */

struct bedLine
/* A line in a bed file with chromosome, start position parsed out. */
    {
    struct bedLine *next;	/* Next in list. */
    char *chrom;                /* Chromosome parsed out. */
    int chromStart;             /* Start position (still in rest of line). */
    char *line;                 /* Rest of line. */
    };

struct bedLine *bedLineNew(char *line);
/* Create a new bedLine based on tab-separated string s. */

void bedLineFree(struct bedLine **pBl);
/* Free up memory associated with bedLine. */

void bedLineFreeList(struct bedLine **pList);
/* Free a list of dynamically allocated bedLine's */

int bedLineCmp(const void *va, const void *vb);
/* Compare to sort based on chrom,chromStart. */

void bedSortFile(char *inFile, char *outFile);
/* Sort a bed file (in place, overwrites old file. */

struct bed *bedLoad3(char **row);
/* Load first three fields of bed. */

struct bed *bedLoad5(char **row);
/* Load first five fields of bed. */

struct bed *bedLoad6(char **row);
/* Load first six fields of bed. */

struct bed *bedLoad12(char **row);
/* Load all 12 fields of bed. */

struct bed *bedLoadN(char *row[], int wordCount);
/* Convert a row of strings to a bed. */

struct bed *bedLoadNAllChrom(char *fileName, int numFields, char* chrom);
/* Load bed entries from a tab-separated file that have the given chrom.
 * Dispose of this with bedFreeList(). */

struct bed *bedLoadNAll(char *fileName, int numFields);
/* Load all bed from a tab-separated file.
 * Dispose of this with bedFreeList(). */

struct bed *bedLoadAll(char *fileName);
/* Determines how many fields are in a bedFile and load all beds from
 * a tab-separated file.  Dispose of this with bedFreeList(). */

void bedLoadAllReturnFieldCount(char *fileName, struct bed **retList, int *retFieldCount);
/* Load bed of unknown size and return number of fields as well as list of bed items.
 * Ensures that all lines in bed file have same field count. */

void bedOutputN(struct bed *el, int wordCount, FILE *f, char sep, char lastSep);
/* Write a bed of wordCount fields. */

void bedOutputNitemRgb(struct bed *el, int wordCount, FILE *f,
	char sep, char lastSep);
/* Write a bed of wordCount fields, interpret column 9 as RGB. */

#define bedTabOutNitemRgb(el,wordCount, f) bedOutputNitemRgb(el,wordCount,f,'\t','\n')
/* Print out bed as a line in a tab-separated file. Interpret
   column 9 as RGB */

#define bedTabOutN(el,wordCount, f) bedOutputN(el,wordCount,f,'\t','\n')
/* Print out bed as a line in a tab-separated file. */

#define bedCommaOutN(el,wordCount, f) bedOutputN(el,wordCount,f,',',',')
/* Print out bed as a comma separated list including final comma. */

int bedTotalBlockSize(struct bed *bed);
/* Return total size of all blocks. */

int bedTotalThickBlockSize(struct bed *bed);
/* Return total size of all thick blocks. */

int bedStartThinSize(struct bed *bed);
/* Return total size of all blocks before thick part. */

int bedEndThinSize(struct bed *bed);
/* Return total size of all blocks after thick part. */

int bedBlockSizeInRange(struct bed *bed, int rangeStart, int rangeEnd);
/* Get size of all parts of all exons between rangeStart and rangeEnd. */

struct bed *bedFromPsl(struct psl *psl);
/* Convert a single psl to a bed structure */

void makeItBed12(struct bed *bedList, int numFields);
/* If it's less than bed 12, make it bed 12. The numFields */
/* param is for how many fields the bed *currently* has. */

struct bed *cloneBed(struct bed *bed);
/* Make an all-newly-allocated copy of a single bed record. */

struct bed *cloneBedList(struct bed *bed);
/* Make an all-newly-allocated list copied from bed. */

struct bed *bedListNextDifferentChrom(struct bed *bedList);
/* Return next bed in list that is from a different chrom than the start of the list. */

struct bed *lmCloneBed(struct bed *bed, struct lm *lm);
/* Make a copy of bed in local memory. */

struct bed *bedCommaInN(char **pS, struct bed *ret, int fieldCount);
/* Create a bed out of a comma separated string looking for fieldCount
 * fields. This will fill in ret if non-null, otherwise will return a
 * new bed */

struct hash *readBedToBinKeeper(char *sizeFileName, char *bedFileName, int wordCount);
/* Read a list of beds and return results in hash of binKeeper structure for fast query
 * See also bedsIntoKeeperHash, which takes the beds read into a list already, but
 * dispenses with the need for the sizeFile. */

int bedParseRgb(char *itemRgb);
/*	parse a string: "r,g,b" into three unsigned char values
	returned as 24 bit number, or -1 for failure */

long long bedTotalSize(struct bed *bedList);
/* Add together sizes of all beds in list. */

int bedSameStrandOverlap(struct bed *a, struct bed *b);
/* Return amount of block-level overlap on same strand between a and b */

boolean bedExactMatch(struct bed *oldBed, struct bed *newBed);
/* Return TRUE if it's an exact match. */

boolean bedCompatibleExtension(struct bed *oldBed, struct bed *newBed);
/* Return TRUE if newBed is a compatible extension of oldBed, meaning
 * all internal exons and all introns of old bed are contained, in the 
 * same order in the new bed. */

struct rbTree *bedToRangeTree(struct bed *bed);
/* Convert bed into a range tree. */

void bedIntoRangeTree(struct bed *bed, struct rbTree *rangeTree);
/* Add all blocks in bed to range tree.  For beds without blocks,
 * add entire bed. */

int bedRangeTreeOverlap(struct bed *bed, struct rbTree *rangeTree);
/* Return number of bases bed overlaps with rangeTree. */

struct bed *bedThickOnly(struct bed *in);
/* Return a bed that only has the thick part. (Which is usually the CDS). */

struct bed *bedThickOnlyList(struct bed *inList);
/* Return a list of beds that only are the thick part of input. */

char *bedAsDef(int bedFieldCount, int totalFieldCount);
/* Return an autoSql definition for a bed of given number of fields. 
 * Normally totalFieldCount is equal to bedFieldCount.  If there are extra
 * fields they are just given the names field16, field17, etc and type string. */


#endif /* BASICBED_H */
