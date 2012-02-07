/* g2gOverlap -  Look at clone clone overlap. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hCommon.h"
#include "psl.h"


int maxTail = 200;	/* Maximum length of tail. */
float maxBad = 0.01;	/* Maximum bad ratio. */
int minUnique = 50;	/* Minimum number of unique matches. */
int minMatch = 100;	/* Minimum number of total matches. */
int minFragSize = 200;	/* Minimum size of fragments. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "g2gOverlap- Look at clone clone overlap. \n"
  "usage:\n"
  "   g2gOverlap source.psl output [maxBad%] [maxTail] [minUniq] [minMatch] [minFragSize]\n"
  "This would put the overlaps in source.psl into out.long\n"
  "in the forms of tab delimited lines:\n"
  "  <clone 1><overlap><clone 2>\n"
  "\n"
  "The optional parameters have the following functions and defaults\n"
  "   maxBad% (1) - Maximum percentage of mismatches and inserts\n"
  "   maxTail (200) - Maximum non-aligning section on end of fragment\n"
  "   minUnique (50) - Minimum number of non-repeat-masked matching bases\n"
  "   minMatch (100) - Minimum number of matching bases\n"
  "   minFragSize(200) - Minimum size of fragments\n");
}

struct clone
/* A clone. */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Name of clone (not allocated here) */
    };

struct cloneOverlap
/* A list of clones and how much they overlap. */
    {
    struct cloneOverlap *next;	/* Next in list. */
    char *clone;			/* Name of clone. */
    int overlap;		/* How much it overlaps. */
    };

struct cloneCenter
/* A clone and a list of all the clones it overlaps with. */
    {
    struct cloneCenter *next;	/* Next in list. */
    struct clone *clone;	/* Clone in center. */
    struct cloneOverlap *overlapList;	/* List of overlaps. */
    };

void freeCloneCenter(struct cloneCenter **pCc)
/* Free up a clone center. */
{
struct cloneCenter *cc;
if ((cc = *pCc) != NULL)
    {
    slFreeList(&cc->overlapList);
    freez(pCc);
    }
}

void dumpCloneCenter(FILE *f, struct cloneCenter *cc)
/* Write one clone center to file. */
{
char *name1 = cc->clone->name;
struct cloneOverlap *co;

for (co = cc->overlapList; co != NULL; co = co->next)
    fprintf(f, "%s\t%d\t%s\n", name1, co->overlap, co->clone);
}

struct clone *storeInCloneHash(struct hash *hash, char *name)
/* Return clone of name.  If none exists create it. */
{
struct hashEl *hel;
struct clone *clone;

if ((hel = hashLookup(hash, name)) != NULL)
    return hel->val;
AllocVar(clone);
hel = hashAdd(hash, name, clone);
clone->name = hel->name;
return clone;
}

float badRatio(struct psl *psl)
/* Return rough ratio of identity. */
{
float ratio = psl->misMatch + psl->tNumInsert + psl->qNumInsert + psl->blockCount;
ratio /= psl->qEnd - psl->qStart;
return ratio;
}

boolean filter(struct psl *psl)
/* Return TRUE if psl looks good enough. */
{
int startTail, endTail;

if (psl->qSize < minFragSize || psl->tSize < minFragSize)
    return FALSE;
if (psl->match < minUnique)
    return FALSE;
if (psl->match + psl->repMatch < minMatch)
    return FALSE;
pslTailSizes(psl, &startTail, &endTail);
if (startTail > maxTail || endTail > maxTail)
    return FALSE;
if (badRatio(psl) > maxBad)
    return FALSE;
return TRUE;
}


struct cloneCenter *nextCloneCenter(struct lineFile *lf, struct hash *cloneHash)
/* Read all alignments that use the same query as the first line.  Process
 * them into a cloneCenter structure. */
{
struct psl *psl;
char otherName[256], ourCloneName[128], cloneName[128];
struct cloneCenter *cc = NULL;
struct clone *clone;
struct cloneOverlap *co;
struct hash *overlapHash = newHash(8);
struct hashEl *hel;
int overlap;

psl = pslNext(lf);
if (psl == NULL)
    return NULL;
fragToCloneName(psl->qName, ourCloneName);
clone = storeInCloneHash(cloneHash, ourCloneName);
AllocVar(cc);
cc->clone = clone;
for (;;)
    {
    fragToCloneName(psl->qName, cloneName);
    if (!sameString(cloneName, ourCloneName))
	{
	lineFileReuse(lf);
	break;
	}
    if (filter(psl))
	{
	fragToCloneName(psl->tName, otherName);
	if ((hel = hashLookup(overlapHash, otherName)) != NULL)
	    co = hel->val;
	else
	    {
	    AllocVar(co);
	    hel = hashAdd(overlapHash, otherName, co);
	    co->clone = hashStoreName(overlapHash, otherName);
	    slAddHead(&cc->overlapList, co);
	    }
	overlap = psl->match + psl->repMatch;
	co->overlap += overlap;
	}
    pslFree(&psl);
    if ((psl = pslNext(lf)) == NULL)
	break;
    }
pslFree(&psl);
freeHash(&overlapHash);
slReverse(&cc->overlapList);
return cc;
}

void g2gOverlap(char *sourceName, char *longName)
/* Look at clone clone overlap. */
{
struct cloneCenter *cc;
struct lineFile *lf = pslFileOpen(sourceName);
FILE *longFile = NULL, *shortFile = NULL;
struct hash *cloneHash = newHash(16);
int i = 0;

longFile = mustOpen(longName, "w");
while ((cc = nextCloneCenter(lf, cloneHash)) != NULL)
    {
    dumpCloneCenter(longFile, cc);
    freeCloneCenter(&cc);
    if ((++i & 0xff) == 0)
	{
	printf(".");
	fflush(stdout);
	}

    }
fclose(longFile);
lineFileClose(&lf);
printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
if (argc >= 4)
   maxBad = atof(argv[3]) * 0.01;
if (argc >= 5)
    maxTail = atoi(argv[4]);
if (argc >= 6)
    minUnique = atoi(argv[5]);
if (argc >= 7)
    minMatch = atoi(argv[6]);
if (argc >= 8)
    minFragSize = atoi(argv[7]);
printf("g2gOverlap maxBad %f, maxTail %d, minUnique %d, minMatch %d, minFragSize %d\n",
	maxBad, maxTail, minUnique, minMatch, minFragSize);
g2gOverlap(argv[1], argv[2]);
return 0;
}
