#include "common.h"
#include "hash.h"
#include "portable.h"
#include "dnautil.h"
#include "nt4.h"
#include "wormdna.h"
#include "flydna.h"
#include "localmem.h"
#include "cdnaAli.h"


struct lm *memPool;
boolean isFly = FALSE;

char **chromNames;
int chromCount;

void loadGenome(char *nt4Dir, struct nt4Seq ***retNt4Seq, char ***retChromNames, int *retNt4Count)
/* Load up entire packed  genome into memory. */
{
struct slName *nameList, *name;
int count;
int i;
char **chromNames;
struct nt4Seq **chroms;
char fileName[512];
char chromName[256];

nameList = listDir(nt4Dir, "*.nt4");
count = slCount(nameList);
chroms = needMem(count*sizeof(chroms[0]));
chromNames = needMem(count*sizeof(chromNames[0]));

for (name = nameList, i=0; name != NULL; name = name->next, i+=1)
    {
    char *end;

    sprintf(fileName, "%s/%s", nt4Dir, name->name);
    
    /* Cut off .nt4 suffix. */
    strcpy(chromName, name->name);
    end = strrchr(chromName, '.');
    assert(end != NULL);
    *end = 0;
    
    chroms[i] = loadNt4(fileName, chromName);
    chromNames[i] = chroms[i]->name;
    }
slFreeList(&nameList);
*retNt4Seq = chroms;
*retChromNames = chromNames;
*retNt4Count = count;
}

void freeGenome(struct nt4Seq ***pNt4Seq, char ***pChromNames, int chromCount)
/* Free up genome. */
{
int i;
struct nt4Seq **nt4s = *pNt4Seq;
for (i=0; i<chromCount; ++i)
    {
    freeNt4(&nt4s[i]);
    }
freez(pNt4Seq);
freez(pChromNames);
}


void cdnaAliInit()
/* Call this before most anything else - sets up
 * local memory pool, chromosomes, etc. */
{
dnaUtilOpen();
memPool = lmInit(256*1024);
if (isFly)
    flyChromNames(&chromNames, &chromCount);
else
    wormChromNames(&chromNames, &chromCount);
}

char *findInStrings(char *name, char *strings[], int stringCount)
/* Return string in array thats same as name. */
{
int i;
for (i=0; i<stringCount; ++i)
    {
    if (!differentWord(name, strings[i]))
        {
        return strings[i];
        }
    }
errAbort("Couldn't find %s in %s %s %s...", name, strings[0],
    strings[1], strings[2]);
return NULL;
}

int ixInStrings(char *name, char *strings[], int stringCount)
/* Return index of string in array. */
{
int i;
for (i=0; i<stringCount; ++i)
    {
    if (!differentWord(name, strings[i]))
        {
        return i;
        }
    }
errAbort("Couldn't find %s in %s %s %s...", name, strings[0],
    strings[1], strings[2]);
return -1;
}

void blockEnds(struct block *block, int *retStart, int *retEnd)
/* Return start and end of block list. */
{
int start = block->hStart;
int end = block->hEnd;
while ((block = block->next) != NULL)
    {
    if (block->hStart < start)
        start = block->hStart;
    if (block->hEnd > end)
        end = block->hEnd;
    }
*retStart = start;
*retEnd = end;
}

struct cdna *loadCdna(char *fileName)
/* Load up good.txt file into a cdna list. */
{
FILE *f;
char lineBuf[256];
char *words[64];
int wordCount;
struct cdna *cdnaList = NULL;
struct cdna *cdna;
struct ali *ali;
int lineCount = 0;
int lineMod = 0;


f = mustOpen(fileName, "r");
while (fgets(lineBuf, sizeof(lineBuf), f) != NULL)
    {
    ++lineCount;
    if (++lineMod >= 50000)
        {
        printf("%s line %d\n", fileName, lineCount);
        lineMod = 0;
        }
    wordCount = chopLine(lineBuf, words);
    if (strcmp(words[0], "cDNA") == 0)
        {
        cdna = allocA(struct cdna);
        cdna->name = cloneString(words[1]);
        cdna->size = atoi(words[3]);
        cdna->isEmbryonic = sameWord(words[4], "embryo");
        slAddHead(&cdnaList, cdna);
        }
    else if (strcmp(words[0], "score") == 0)
        {
        ali = allocA(struct ali);
        ali->cdna = cdna;
        ali->score = atof(words[1]);
        ali->chrom = findInStrings(words[3], chromNames, chromCount);
        ali->strand = words[5][0];
        ali->direction = words[6][0];
        ali->gene = cloneString(words[8]);
        if (wormIsChromRange(words[8]))
            {
            uglyf("Too weird %s\n", words[8]);
            }
        slAddHead(&cdna->aliList, ali);
        }
    else if (strcmp(words[7], "goodEnds") == 0)
        {
        struct block *b = allocA(struct block);
        b->nStart = atoi(words[0]);
        b->nEnd = atoi(words[1]);
        b->hStart = atoi(words[3]);
        b->hEnd = atoi(words[4]);
        b->startGood = atoi(words[8]);
        b->endGood = atoi(words[9]);
        b->midGood = b->nEnd - b->nStart;
        slAddHead(&ali->blockList, b);
        }
    else
        {
        errAbort("Unexpected line beginning with %s line %d of %s\n",
            words[0],  lineCount, fileName);
        }
    }
slReverse(&cdnaList);
for (cdna = cdnaList; cdna != NULL; cdna = cdna->next)
    {
    slReverse(&cdna->aliList);
    for (ali = cdna->aliList; ali != NULL; ali = ali->next)
        slReverse(&ali->blockList);
    }
return cdnaList;
}

struct cdnaRef *inRefList(struct cdnaRef *refList, struct ali *cdnaAli)
/* Returns cdnaRef associated with cdna in list, or NULL if it isn't
 * in list. */
{
struct cdnaRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    if (ref->ali == cdnaAli) return ref;
return NULL;
}

struct cdnaRef *addUniqRefs(struct cdnaRef *list, struct cdnaRef *newRefs)
/* Make list that contains union of two reference lists. */
{
struct cdnaRef *newList;
struct cdnaRef *b, *ref;

slReverse(&list);
newList = list;
for (b = newRefs; b != NULL; b = b->next)
    {
    if (!inRefList(list, b->ali))
        {
        allocV(ref);
        ref->ali = b->ali;
        slAddHead(&newList, ref);
        }
    }
slReverse(&newList);
return newList;
}

int cmpFeatures(const void *va, const void *vb)
/* Compare two introns. */
{
struct feature **pA = (struct feature **)va;
struct feature **pB = (struct feature **)vb;
struct feature *a = *pA, *b = *pB;
int diff;

if ((diff = strcmp(a->chrom, b->chrom)) != 0)
    return diff;
if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}


void collapseFeatures(struct feature *featureList)
/* Merge duplicate introns. */
{
struct feature *feat, *uniq;
int dupeCount = 1;

if ((uniq = featureList) == NULL)
    return;
feat = uniq->next;
for (;;)
    {
    if (feat == NULL || cmpFeatures(&feat, &uniq) != 0)
        {
        uniq->next = feat;
        uniq = feat;
        if (feat == NULL)
            break;
        }
    else
        {
        struct cdnaRef *ref = feat->cdnaRefs;
        ref->next = uniq->cdnaRefs;
        uniq->cdnaRefs = ref;
        ++uniq->cdnaCount;
        }
    feat = feat->next;
    }
}

struct feature *skipIrrelevant(struct feature *listSeg, struct feature *feat,
    int extraSpace)
/* Skip parts of listSeg that couldn't matter to feat. Assumes
 * listSeg is sorted on start. Returns first possibly relevant
 * item of listSeg. */
{
struct feature *seg = listSeg;
char *chrom = feat->chrom;
int start = feat->start - extraSpace;
int skipCount = 0;

/* skip past wrong chromosome. */
while (seg != NULL && strcmp(seg->chrom, chrom) < 0)
    {  
    seg = seg->next;
    ++skipCount;
    }
/* Skip past stuff that we've passed on this chromosome. */
while (seg != NULL && seg->chrom == chrom && seg->end < start)
    {
    seg = seg->next;
    ++skipCount;
    }
return seg;
}

struct feature *sortedSearchOverlap(struct feature *listSeg, struct feature *feature, int extra)
/* Assuming that listSeg is sorted on start, search through list
 * until find something that overlaps with feature, or have gone
 * past where feature could be. Returns overlapping feature or NULL. */
{
struct feature *seg = listSeg;
char *chrom = feature->chrom;
int start = feature->start;
int end = feature->end;

while (seg != NULL && seg->chrom == chrom && seg->start <= end + extra)
    {
    if (!(seg->start - extra >= end || seg->end + extra <= start))
        return seg;
    seg = seg->next;
    }
return NULL;
}


struct feature *makeIntrons(struct cdna *cdnaList)
/* Create introns from cdnaList. */
{
struct cdna *cdna;
struct ali *ali;
struct block *block, *nextBlock;
struct feature *intron, *intronList = NULL;
struct cdnaRef *ref;
enum {minIntronSize = 24, minGoodEnd = 5, minExonSize = 12};
printf("Making introns\n");
for (cdna = cdnaList; cdna != NULL; cdna = cdna->next)
    {
    for (ali = cdna->aliList; ali != NULL; ali = ali->next)
        {
        nextBlock = ali->blockList;
        for (;;)
            {
            int thisSize, nextSize;
            int nGap, hGap;
            block = nextBlock;
            if ((nextBlock = nextBlock->next) == NULL)
                break;
            thisSize = block->nEnd - block->nStart;
            nextSize = nextBlock->nEnd - nextBlock->nStart;            
            nGap = nextBlock->nStart - block->nEnd;
            hGap = nextBlock->hStart - block->hEnd;
            if (nGap == 0 && hGap >= minIntronSize && thisSize >= minExonSize && nextSize >= minExonSize
                && block->endGood > minGoodEnd && nextBlock->startGood > minGoodEnd)
                {
                ref = allocA(struct cdnaRef);
                ref->ali = ali;
                intron = allocA(struct feature);
                intron->chrom = ali->chrom;
                intron->start = block->hEnd;
                intron->end = nextBlock->hStart;
                intron->gene = ali->gene;
                intron->cdnaRefs = ref;
                intron->cdnaCount = 1;
                slAddHead(&intronList, intron);
                }
            }
        }
    }
slSort(&intronList, cmpFeatures);
collapseFeatures(intronList);
printf("Done making introns\n");
return intronList;
}

