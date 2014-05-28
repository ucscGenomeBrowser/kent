/* relPairs - extract relevant pairs that could apply to a .psl
 * file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "psl.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "relPairs - extract pairs from a big pair list file that actually\n"
  "occur in a .psl file\n"
  "usage:\n"
  "    relPairs all.pai some.psl out.pai dir1 dir2 ... dirN\n"
  "This will look in each of the directories for 'some.psl' and\n"
  "create 'out.pai' in the same directory.\n"
  "Example (run from an oo directory)\n"
  "    relPairs ~/hg/mrna/est.pai est.psl ctgEst.pai */ctg*");
}

struct pair
/* Keep track of a pair. */
    {
    struct pair *next;  /* next in list */
    char *a;		/* First in pair. */
    char *b;            /* Second in pair. */
    char *line;		/* Line in file describing pair. */
    };

struct pairRef
/* A reference to a pair, keeps track is we've seen both sides. */
    {
    struct pairRef *next; /* Next in list. */
    struct pair *pair;	  /* Underlying pair. */
    bool gotA, gotB;      /* Seen A?  Seen B?  */
    };

void makePairHash(char *pairName, struct hash **retHash, struct pair **retList)
/* Make up a hash out of the pair file. */
{
struct hash *hash = newHash(20);
struct hashEl *helA, *helB;
struct pair *list = NULL, *pair;
struct lineFile *lf = lineFileOpen(pairName, TRUE);
int lineSize;
char *line;
int wordCount;
char *words[16];
int pairCount = 0;
char *dupeLine;

printf("Processing %s\n", pairName);
while (lineFileNext(lf, &line, &lineSize))
    {
    dupeLine = cloneString(line);
    wordCount = chopLine(line, words); 
    if (wordCount < 2 || wordCount > 8)
	errAbort("Expecting 2-8 words line %d of %s\n", lf->lineIx, lf->fileName);
    AllocVar(pair);
    helA = hashAdd(hash, words[0], pair);
    helB = hashAdd(hash, words[1], pair);
    pair->a = helA->name;
    pair->b = helB->name;
    pair->line = dupeLine;
    slAddHead(&list, pair);
    ++pairCount;
    }
printf("Got %d pairs in %s\n", pairCount, pairName);
lineFileClose(&lf);
slReverse(&list);
*retHash = hash;
*retList = list;
}

void writePairs(char *outName, struct pairRef *refList)
/* Write out pairs in refList that are used on both sides. */
{
int bothCount = 0;
struct pairRef *ref;
struct pair *pair;
FILE *f = mustOpen(outName, "w");

for (ref = refList; ref != NULL; ref = ref->next)
    {
    if (ref->gotA && ref->gotB)
	{
	++bothCount;
	pair = ref->pair;
	fprintf(f, "%s\n", pair->line);
	}
    }
fclose(f);
printf("Wrote %d pairs to %s\n", bothCount, outName);
}

void extractUsedPairs(struct hash *pairHash, char *inPslName, char *outPairName)
/* Extract pairs that are used in inPsl to outPair. */
{
struct hash *refHash = newHash(12);
struct pairRef *refList = NULL, *ref;
struct psl *psl;
struct lineFile *lf;

printf("Processing pairs from %s to %s\n", inPslName, outPairName);
lf = pslFileOpen(inPslName);
while ((psl = pslNext(lf)) != NULL)
    {
    char *name = psl->qName;
    struct hashEl *hel;
    struct pair *pair;
    if ((hel = hashLookup(pairHash, name)) != NULL)
	{
	pair = hel->val;
	if ((hel = hashLookup(refHash, name)) != NULL)
	    {
	    ref = hel->val;
	    }
	else
	    {
	    AllocVar(ref);
	    ref->pair = pair;
	    slAddHead(&refList, ref);
	    hashAdd(refHash, pair->a, ref);
	    hashAdd(refHash, pair->b, ref);
	    }
	if (sameString(name, pair->a))
	    ref->gotA = TRUE;
	else
	    ref->gotB = TRUE;
	}
    pslFree(&psl);
    }
slReverse(&refList);
writePairs(outPairName, refList);
slFreeList(&refList);
freeHash(&refHash);
}

void relPairs(char *allPairName, char *inPslName, char *outPairName, char *dirs[], int dirCount)
/* extract relevant pairs that could apply to a .psl file in each dir. */
{
struct hash *pairHash;
struct pair *pairList;
char inPslPath[512];
char outPairPath[512];
char *dir;
int i;

makePairHash(allPairName, &pairHash, &pairList);
for (i=0; i<dirCount; ++i)
    {
    dir = dirs[i];
    sprintf(inPslPath, "%s/%s", dir, inPslName);
    sprintf(outPairPath, "%s/%s", dir, outPairName);
    if (fileExists(inPslPath))
	extractUsedPairs(pairHash, inPslPath, outPairPath);
    }
freeHash(&pairHash);
slFreeList(&pairList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 5)
    usage();
relPairs(argv[1], argv[2], argv[3], argv+4, argc-4);
return 0;
}

