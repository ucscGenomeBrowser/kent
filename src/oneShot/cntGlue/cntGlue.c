/* cntGlue - count up gluing and do some stats in general to see
 * how well EST and mrna data is gluing together fragmented BACs. */
#include "common.h"
#include "hash.h"


void usage()
/* Explain how program works and exit. */
{
errAbort(
    "cntGlue - count up gluing data from patSpace runs\n"
    "usage\n"
    "   cntGlue file(s).glu");
}

struct contigTrack 
/* Keep track of a contig. */
    {
    struct contigTrack *next;   /* Next in list. */
    int ix;                     /* Index of contig in bak. */
    char strand;                /* Strand: + or - */
    char dir;                   /* Direction: 5' or 3' */
    int start, end;             /* Start and end of aligning parts of mrna. */
    double cookedScore;         /* Percentage alignment between start and end. */
    };


int cmpContigTrack(const void *va, const void *vb)
/* Compare two contigTracks. */
{
const struct contigTrack *a = *((struct contigTrack **)va);
const struct contigTrack *b = *((struct contigTrack **)vb);
int dif;
dif = b->dir - a->dir;
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

struct contigPair
/* This glues together a pair of contigs. */
    {
    struct contigPair *next;    /* Next in list. */
    int ix1, ix2;
    int useCount;
    };

struct bacTrack
/* Keep track of one BAC. */
    {
    struct bacTrack *next;          /* Next in list. */
    char *name;                     /* Name (allocated in hash table.) */
    struct contigPair *gluePairs;   /* List of gluing pairs. */
    };
struct bacTrack *bacList = NULL;

int cmpBacTrack(const void *va, const void *vb)
/* Compare two bacTracks. */
{
const struct bacTrack *a = *((struct bacTrack **)va);
const struct bacTrack *b = *((struct bacTrack **)vb);
return strcmp(a->name, b->name);
}

struct contigPair *findPair(struct contigPair *pairList, int ix1, int ix2)
/* Find matching pair on list.  Return NULL if not found. */
{
struct contigPair *pair;
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (pair->ix1 == ix1 && pair->ix2 == ix2)
        return pair;
    }
return NULL;
}

void addPairs(struct bacTrack *bt, struct contigTrack *ctList)
/* Add in pairs that are contained in ctList. */
{
struct contigTrack *ct;
int lastIx = ctList->ix;
int ix;
int a, b;
struct contigPair *cp;

for (ct = ctList->next; ct != NULL; ct = ct->next)
    {
    ix = ct->ix;
    if (ix < lastIx)
        {
        a = ix;
        b = lastIx;
        }
    else
        {
        a = lastIx;
        b = ix;
        }
    if ((cp = findPair(bt->gluePairs, a, b)) != NULL)
        ++cp->useCount;
    else
        {
        AllocVar(cp);
        cp->ix1 = a;
        cp->ix2 = b;
        cp->useCount = 1;
        slAddHead(&bt->gluePairs, cp);
        }
    lastIx = ix;
    }
}

void printStats()
/* Print out some statistics about gluing. */
{
int bacCount = 0;       /* Count how many different bacs have gluing. */
int pairCount = 0;      /* Count # of gluing pairs. */
int uniqPairCount = 0;  /* Count # of unique gluing pairs. */

struct bacTrack *bt;
struct contigPair *cp;

for (bt = bacList; bt != NULL; bt = bt->next)
    {
    ++bacCount;
    for (cp = bt->gluePairs; cp != NULL; cp = cp->next)
        {
        ++uniqPairCount;
        pairCount += cp->useCount;
        }
    }
printf("%d glued bacs.  %d gluing pairs.  %d uniq pairs\n",
    bacCount, pairCount, uniqPairCount);
}


int main(int argc, char *argv[])
{
struct hash *bacHash;
char line[1024];
int lineCount;
char *words[256];
int wordCount;
int fileIx;
char *fileName;
FILE *f;

if (argc < 2)
    usage();
bacHash = newHash(16);

for (fileIx = 1; fileIx < argc; ++fileIx)
    {
    fileName = argv[fileIx];
    uglyf("Processing %s\n", fileName);
    f = mustOpen(fileName, "r");
    lineCount = 0;
    while (fgets(line, sizeof(line), f))
        {
        ++lineCount;
        wordCount = chopLine(line, words);
        if (wordCount == ArraySize(words))
            errAbort("Too many words line %d of %s\n", lineCount, fileName);
        if (wordCount != 0)
            {
            char *bacName;
            int cIx;
            struct contigTrack *ctList = NULL, *ct;
            struct bacTrack *bt;
            struct hashEl *hel;

            /* Check line syntax and parse it. */
            if (!sameString(words[1], "glues"))
                errAbort("Bad format line %d of %s\n", lineCount, fileName);
            bacName = words[2];
            for (cIx = 4; cIx < wordCount; cIx += 5)
                {
                char *parts[3];
                int partCount;

                AllocVar(ct);
                ct->ix = atoi(words[cIx]);
                ct->strand = words[cIx+1][0];
                ct->dir = words[cIx+2][0];
                partCount = chopString(words[cIx+3], "(-)", parts, ArraySize(parts));
                if (partCount != 2)
                    errAbort("Bad format line %d of %s\n", lineCount, fileName);
                ct->start = atoi(parts[0]);
                ct->end = atoi(parts[1]);
                ct->cookedScore = atof(words[cIx+4]);
                slAddHead(&ctList, ct);                
                }
            slSort(&ctList, cmpContigTrack);
        
            /* Lookup bacTrack and make it if new. */
            hel = hashLookup(bacHash, bacName);
            if (hel == NULL)
                {
                AllocVar(bt);
                hel = hashAdd(bacHash, bacName, bt);
                bt->name = hel->name;
                slAddHead(&bacList, bt);
                }
            else
                {
                bt = hel->val;
                }
            
            /* Process pairs into bacTrack. */
            addPairs(bt, ctList);
            slFreeList(&ctList);
            }
        }
    fclose(f);
    }
slSort(&bacList, cmpBacTrack);

printStats();
return 0;
}