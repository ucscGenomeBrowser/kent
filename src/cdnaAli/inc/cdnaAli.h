#ifndef LOCALMEM_H
#include "localmem.h"
#endif 

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef NT4_H
#include "nt4.h"
#endif



extern struct lm *memPool;
extern char **chromNames;
extern int chromCount;
extern boolean isFly;

void cdnaAliInit();
/* Call this before most anything else - sets up
 * local memory pool, chromosome names, etc. */

#define alloc(size) lmAlloc(memPool, size)
#define allocA(type) alloc(sizeof(type))
#define allocV(pvar) pvar = alloc(sizeof(*(pvar)))


struct cdna
    {
    struct cdna *next;
    char *name;
    int size;
    struct ali *aliList;
    boolean isUsed;
    boolean isEmbryonic;
    };

struct ali
    {
    struct ali *next;
    struct cdna *cdna;
    double score;
    char *chrom;
    char strand;
    char direction;
    char *gene;
    struct block *blockList;
    };

struct block
    {
    struct block *next;
    int nStart, nEnd, hStart, hEnd;
    int startGood, endGood, midGood;
    };

void blockEnds(struct block *block, int *retStart, int *retEnd);
/* Return start and end of block list. */

#define GENE_SPACE 1000
/* The minimum number of bases that we say separates genes. */

char *findInStrings(char *name, char *strings[], int stringCount);
/* Return string in array thats same as name. */

int ixInStrings(char *name, char *strings[], int stringCount);
/* Return index of string in array. */

struct cdna *loadCdna(char *fileName);
/* Load up good.txt file into a cdna list. */


struct cdnaRef
/* Simple reference to cDNA that can put on a list. */
    {
    struct cdnaRef *next;
    struct ali *ali;
    };

struct cdnaRef *inRefList(struct cdnaRef *refList, struct ali *cdnaAli);
/* Returns cdnaRef associated with cdna in list, or NULL if it isn't
 * in list. */

struct cdnaRef *addUniqRefs(struct cdnaRef *list, struct cdnaRef *newRefs);
/* Make list that contains union of two reference lists. */

struct feature
/* Something associated with a chromosome range.  May also
 * be associated with a gene and cdna.  Sort of a sloppy,
 * multi-use structure, not all of whose fields are used by
 * everyone. */
    {
    struct feature *next;
    char *chrom;
    int start, end;
    int startGood, endGood;
    char *gene;
    int cdnaCount;
    struct cdnaRef *cdnaRefs;
    char strand;
    };

int cmpFeatures(const void *va, const void *vb);
/* Help sort features by chromosome, start, end. */


void collapseFeatures(struct feature *featureList);
/* Merge duplicate features. */

struct feature *skipIrrelevant(struct feature *listSeg, struct feature *feat,
    int extraSpace);
/* Skip parts of listSeg that couldn't matter to feat. Assumes
 * listSeg is sorted on start. Returns first possibly relevant
 * item of listSeg. */

struct feature *sortedSearchOverlap(struct feature *listSeg, 
    struct feature *feature, int extra);
/* Assuming that listSeg is sorted on start, search through list
 * until find something that overlaps with feature, or have gone
 * past where feature could be. Returns overlapping feature or NULL. */

void freeGenome(struct nt4Seq ***pNt4Seq, char ***pChromNames, int chromCount);
/* Free up genome. */

void loadGenome(char *nt4Dir, struct nt4Seq ***retNt4Seq, char ***retChromNames, int *retNt4Count);
/* Load up entire packed  genome into memory. */

struct feature *makeIntrons(struct cdna *cdnaList);
/* Create introns from cdnaList. */

