/* genieCon - generate constraint GFF files for Genie the gene finder from cDNA 
 * alignments. */
#include "common.h"
#include "hash.h"
#include "dlist.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "cda.h"
#include "wormdna.h"
#include "flydna.h"

char **chromNames;
int chromCount;
boolean isFly = FALSE;

const int noiseLimit = 2;  /* Allow up to two bp of noise */


struct clonePair
/* A structure that holds a 5' and a 3' sequence from the same clone. */
    {
    struct clonePair *next;
    struct cdaAli *p5, *p3;
    int chromIx;
    };

char *nameOfClone(char *estName, char *retEnd)
/* Chop off the .3 or .5 from a clone name and return result.  Return 
 * unchanged if doesn't end in .3 or .5. */
{
static char buf[64];
int pPos;
char *s = strrchr(estName, '.');
char c;

*retEnd = 0;
if (s == NULL)
    return estName;
pPos = s - estName;
assert(pPos < sizeof(buf));
c = s[1];
if (c != '3' && c != '5')
    {
    *retEnd = '5';
    return estName;
    }
memcpy(buf, estName, pPos);
buf[pPos] = 0;
*retEnd = c;
return buf;
}

struct clonePair *pairClones(struct cdaAli *aliList)
/* Lump together clones into 3' and 5' pairs whenever possible. */
{
struct clonePair *pairList = NULL, *pair;
struct hash *hash = newHash(14);
struct cdaAli *ali, *mate;
struct hashEl *hel;
boolean gotMate;
char *name;
char threeFive;
int matedCount = 0;
int pairCount;

for (ali = aliList; ali != NULL; ali = ali->next)
    {
    gotMate = FALSE;
    name = nameOfClone(ali->name, &threeFive);
    if (threeFive && (hel = hashLookup(hash, name)) != NULL)
        {
        struct cdaAli *ali3, *ali5;
        boolean duped = FALSE;
        pair = hel->val;
        if (pair->p5 != NULL)
            {
            ali5 = mate = pair->p5;
            ali3 = ali;
            if (threeFive != '3')
                {
                duped = TRUE;
                }
            }
        else if (pair->p3 != NULL)
            {
            ali3 = mate = pair->p3;
            ali5 = ali;
            if (threeFive != '5')
                {
                duped = TRUE;
                }
            }
        else
            {
            assert(FALSE);
            }
        /* Strands of 5' and 3' reads of same clone should be opposite. */
        if (!duped && mate->chromIx == ali->chromIx && mate->strand != ali->strand)
            {
            boolean goodRelPos = FALSE;
            if (cdaCloneIsReverse(mate))
                {
                gotMate = (ali5->chromStart > ali3->chromEnd
                    && ali5->chromStart - ali3->chromEnd < 50000);
                }
            else
                {
                gotMate =  (ali5->chromStart < ali3->chromEnd
                    && ali3->chromStart - ali5->chromEnd < 50000);
                }
            if (gotMate)
                {
                if (threeFive == '5')
                    pair->p5 = ali;
                else
                    pair->p3 = ali;
                assert(pair->p5 != NULL && pair->p3 != NULL);
                ++matedCount;
                }
            }
        }
    if (!gotMate)
        {
        AllocVar(pair);
        if (threeFive == '5')
            pair->p5 = ali;
        else
            pair->p3 = ali;
        pair->chromIx = ali->chromIx;
        slAddHead(&pairList, pair);
        hashAdd(hash, name, pair);
        ++pairCount;
        }
    }
printf("Got %d pairs in %d cDNAs\n", matedCount, slCount(aliList));
freeHash(&hash);
slReverse(&pairList);
return pairList;
}

FILE *anyOpenGoodAli()
/* Open up binary alignment file for fly or worm. */
{
if (isFly)
	{
	return flyOpenGoodAli();
	}
else
	{
	return wormOpenGoodAli();
	}
}

void anyChromNames(char ***retChromNames, int *retChromCount)
/* Get chromosome list for fly or worm. */
{
if (isFly)
	flyChromNames(retChromNames, retChromCount);
else
	wormChromNames(retChromNames, retChromCount);
}

struct cdaAli *readAllCda()
/* Read in all worm Cda (cDNA alignments medium detail level) and return them. */
{
struct cdaAli *list = NULL, *el;
FILE *f = anyOpenGoodAli();
while ((el = cdaLoadOne(f)) != NULL)
    {
//    if (el->chromIx == 0 && el->chromStart < 1000000)   /* uglyf */
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

struct cdaRef 
/* Holds a reference to a cda */
    {
    struct cdaRef *next;
    struct cdaAli *ali;
    };

struct exon 
/* Info on an exon. */
    {
    struct exon *next;
    int start, end;
    char hardStart, hardEnd;
    };

int cmpExons(const void *va, const void *vb)
/* Compare two exons. */
{
struct exon **pA = (struct exon **)va;
struct exon **pB = (struct exon **)vb;
struct exon *a = *pA, *b = *pB;
int diff;

if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

struct intron
/* Info on an intron. */
    {
    struct intron *next;
    int start, end;
    };

int cmpIntrons(const void *va, const void *vb)
/* Compare two exons. */
{
struct intron **pA = (struct intron **)va;
struct intron **pB = (struct intron **)vb;
struct intron *a = *pA, *b = *pB;
int diff;

if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

struct entity
/* This holds a gene-like entity. */
    {
    struct entity *next;
    struct dlNode *node;
    struct cdaRef *cdaRefList;
    int chromIx;
    char strand;
    int start, end;
    struct exon *exonList;
    struct intron *intronList;
    };

int cmpEntities(const void *va, const void *vb)
/* Compare two entities. */
{
struct entity **pA = (struct entity **)va;
struct entity **pB = (struct entity **)vb;
struct entity *a = *pA, *b = *pB;
int diff;

if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

boolean isGoodIntron(struct cdaBlock *a, struct cdaBlock *b)
/* Return true if there's a good intron between a and b. */
{
return (a->nEnd == b->nStart && b->hStart - a->hEnd > 16 && a->endGood >= 5 && b->startGood >= 5);
}

struct entity *newEntity(struct cdaAli *ali)
/* Create a new entity based initially on ali. */
{
struct entity *ent;
struct cdaRef *ref;
int i;
int blockCount;
struct cdaBlock *prevBlock, *block;
struct exon *exon;
struct intron *intron;
boolean goodIntron;

AllocVar(ent);
AllocVar(ref);
ref->ali = ali;
ent->cdaRefList = ref;
ent->chromIx = ali->chromIx;
ent->strand = cdaCloneStrand(ali);
ent->start = ali->chromStart;
ent->end = ali->chromEnd;
block = ali->blocks;
blockCount = ali->blockCount;
AllocVar(exon);
exon->start = block->hStart;
exon->end = block->hEnd;
ent->exonList = exon;
if (blockCount > 1)
    {
    for (i=1; i<blockCount; ++i)
        {
        prevBlock = block++;
        if ((goodIntron = isGoodIntron(prevBlock, block)) != FALSE)
            {
            exon->hardEnd = TRUE;
            AllocVar(intron);
            intron->start = prevBlock->hEnd;
            intron->end = block->hStart;
            slAddHead(&ent->intronList, intron);
            }
        AllocVar(exon);
        exon->start = block->hStart;
        exon->end = block->hEnd;
        exon->hardStart = goodIntron;
        slAddHead(&ent->exonList, exon);
        }
    slReverse(&ent->exonList);
    slReverse(&ent->intronList);
    }
return ent;
}

boolean slOnList(void *vList, void *vEl)
/* Return TRUE if el is somewhere on list. */
{
struct slList *list = vList, *el = vEl;
for (;list != NULL; list = list->next)
    {
    if (el == list)
        return TRUE;
    }
return FALSE;
}

boolean isCompatable(struct entity *entity, struct cdaAli *ali)
/* Returns TRUE if ali could get folded into entity. */
{
struct cdaBlock *block, *lastBlock;
int blockCount = ali->blockCount;
int i;

if (cdaCloneStrand(ali) != entity->strand)
    return FALSE;

/* Check first for compatible exons in ali - can't overlap any introns in
 * entity by more than noiseLimit.  */
for (block = ali->blocks, i=0; i<blockCount; ++i, ++block)
    {
    struct intron *intron;
    int hStart = block->hStart, hEnd = block->hEnd;
    for (intron = entity->intronList; intron != NULL; intron = intron->next)
        {
        int iStart, iEnd;
        int start, end;
        iStart = intron->start;
        start = max(hStart, iStart);
        iEnd = intron->end;
        end = min(hEnd, iEnd);
        if (end - start > noiseLimit)
            return FALSE;
        }
    }

/* Check that introns in ali are compatible with introns and exons in
 * entity. */
block = ali->blocks;
for (i=1; i<blockCount; ++i)
    {
    lastBlock = block++;
    if (isGoodIntron(lastBlock, block))
        {
        int iStart = lastBlock->hEnd;
        int iEnd = block->hStart;
        int iSizeMinusNoise = iEnd - iStart - noiseLimit;
        struct intron *intron;
        struct exon *exon;
        int exonCount = 0;

        for (exon = entity->exonList; exon != NULL; exon = exon->next)
            {
            int eStart = exon->start, eEnd = exon->end;
            int start = max(iStart, eStart);
            int end = min(iEnd, eEnd);
            if (end - start > noiseLimit)
                return FALSE;
            if (++exonCount > 1000)
                assert(FALSE);
            }
        for (intron = entity->intronList; intron != NULL; intron = intron->next)
            {
            int iiStart = intron->start, iiEnd = intron->end;
            int start = max(iStart, iiStart);
            int end = min(iEnd, iiEnd);
            int overlap = end - start;
            if (overlap > 0)
                {
                int iiSizeMinusNoise = iiEnd - iiStart - noiseLimit;
                if (overlap < iSizeMinusNoise || overlap < iiSizeMinusNoise)
                    return FALSE;
                }
            }
        }
    }
return TRUE;
}

boolean entityCompatableWithList(struct entity *entList, struct entity *entity)
/* Figure out if can add entity to list ok. */
{
struct entity *listEnt;
struct cdaRef *ref;
struct cdaAli *ali;

for (listEnt = entList; listEnt != NULL; listEnt = listEnt->next)
    {
    for (ref = entity->cdaRefList; ref != NULL; ref = ref->next)
        {
        ali = ref->ali;
        if (!isCompatable(listEnt, ali))
            return FALSE;
        }
    }
return TRUE;
}

struct entity *findCompatableEntities(struct dlList *entitySourceList, struct clonePair *pair)
/* Find list of entities compatible with and overlapping pair. */
{
struct entity *entityList = NULL, *entity;
struct dlNode *node;
struct cdaAli *alis[2], *ali;
int aliCount = 0;
int i;
if (pair->p3 != NULL)
    alis[aliCount++] = pair->p3;
if (pair->p5 != NULL)
    alis[aliCount++] = pair->p5;
for (i=0; i<aliCount; ++i)
    {
    ali = alis[i];
    for (node = entitySourceList->head; node->next != NULL; node = node->next)
        {
        entity = node->val;
        if (entity->start < ali->chromEnd && entity->end >  ali->chromStart &&
            isCompatable(entity, ali) && !slOnList(entityList, entity))
            {
            if (aliCount == 1 || isCompatable(entity, alis[1-i]) )
                if (entityCompatableWithList(entityList, entity))
                    slAddHead(&entityList, entity);
            }
        }
    }
slReverse(&entityList);
return entityList;
}

struct intron *weedDupeIntrons(struct intron *intronList)
/* Return a list of introns with duplicates removed. */
{
struct intron *newList = NULL, *intron;
if (intronList == NULL || (intron = intronList->next) == NULL)
    return intronList;
/* Move first element of old list onto new list. */
slAddHead(&newList, intronList);
/* Loop through remaining elements of list. */
while (intron != NULL)
    {
    struct intron *next = intron->next;
    int start = max(newList->start, intron->start);
    int end = min(newList->end, intron->end);
    int overlap = end-start;
    
    if (overlap > 0)        /* If they overlap at all we know they overlap fully. */
        {
        assert(overlap >= intron->end - intron->start - noiseLimit);
        }
    else
        slAddHead(&newList, intron);
    intron = next;
    }
slReverse(&newList);
return newList;
}

struct exon *weedDupeExons(struct exon *exonList)
/* Return a list of exons with duplicates removed and overlaps merged. */
{
struct exon *newList = NULL, *exon;
if (exonList == NULL || (exon = exonList->next) == NULL)
    return exonList;
/* Move first element of old list onto new list. */
slAddHead(&newList, exonList);
/* Loop through remaining elements of list. */
while (exon != NULL)
    {
    struct exon *next = exon->next;
    int start = max(newList->start, exon->start);
    int end = min(newList->end, exon->end);
    int overlap = end - start;

    /* Expand exon to cover union of both, except if there's 
     * a hardStart respect that limit. */
    if (overlap > 0)
        {
        start = min(newList->start, exon->start);
        end = max(newList->end, exon->end);
        if (exon->hardStart)
            {
            newList->hardStart = TRUE;
            newList->start = start;
            }
        else if (newList->hardStart)
            {
            }
        else
            {
            newList->start = start;
            }
        if (exon->hardEnd)
            {
            newList->hardEnd = TRUE;
            newList->end = end;
            }
        else if (newList->hardStart)
            {
            }
        else
            {
            newList->end = end;
            }
        }
    else
        slAddHead(&newList, exon);
    exon = next;
    }
slReverse(&newList);
return newList;
}

struct entity *mergeEntities(struct entity *entityList)
/* Merge two entities. */
{
struct entity *root = entityList, *ent;
struct dlNode *node;

if (root == NULL || root->next == NULL)
    return root;
for (ent = root->next; ent != NULL; ent = ent->next)
    {
    node = ent->node;
    if (node != NULL)
        {
        dlRemove(node);
        ent->node = NULL;
        }
    root->exonList = slCat(root->exonList, ent->exonList);
    root->intronList = slCat(root->intronList, ent->intronList);
    root->cdaRefList = slCat(root->cdaRefList, ent->cdaRefList);
    if (root->start > ent->start)
        root->start = ent->start;
    if (root->end < ent->end)
        root->end = ent->end;
    }
slSort(&root->exonList, cmpExons);
root->exonList = weedDupeExons(root->exonList);
slSort(&root->intronList, cmpIntrons);
root->intronList = weedDupeIntrons(root->intronList);
return root;
}

struct entity *addToEntityList(struct entity *entityList, struct cdaAli *ali)
/* If ali is non-null, make a new entity around it and add it to list. */
{
struct entity *entity;
if (ali == NULL)
    return entityList;
entity = newEntity(ali);
slAddTail(&entityList, entity);
return entityList;
}

void makeEntities(struct clonePair *pairList, struct dlList **entLists)
/* Lump pairs of cDNAs into entities based on them having overlapping
 * and compatable cDNAs. */
{
struct dlList *chromEntList;
struct entity *compatableList, *entity;
struct clonePair *pair;
struct dlNode *node;
int pairCount = 0;

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (++pairCount % 1000 == 0)
        printf("Processing pair %d\n", pairCount);
    chromEntList = entLists[pair->chromIx];
    if ((compatableList = findCompatableEntities(chromEntList, pair)) != NULL)
        {
        compatableList = addToEntityList(compatableList, pair->p3);
        compatableList = addToEntityList(compatableList, pair->p5);
        mergeEntities(compatableList);
        }
    else
        {
        if (pair->p5)
            {
            entity = newEntity(pair->p5);
            if (pair->p3)
                {
                if (isCompatable(entity, pair->p3)) /* There are a few rare cases
                                                     * where this isn't true. */
                    {
                    entity = addToEntityList(entity, pair->p3);
                    entity = mergeEntities(entity);
                    }
                }
            }
        else
            {
            entity = newEntity(pair->p3);
            }
        node = dlAddValTail(chromEntList, entity);
        entity->node = node;
        }
    }
}

boolean hasIntron(struct cdaAli *ali)
{
struct cdaBlock *block, *lastBlock;
int count;
int i;

if (ali == NULL || (count = ali->blockCount) <= 1)
    return FALSE;
block = ali->blocks;
for (i=0; i<count; ++i)
    {
    lastBlock = block++;
    if (isGoodIntron(lastBlock, block))
        return TRUE;
    }
return FALSE;
}

struct clonePair *weedGenomic(struct clonePair *pairList)
/* Weed out clones with no introns (likely genomic contamination. */
{
struct clonePair *newList = NULL, *pair, *next;
for (pair = pairList; pair != NULL; pair = next)
    {
    next = pair->next;
    if (hasIntron(pair->p3) || hasIntron(pair->p5))
        {
        slAddHead(&newList, pair);
        }
    }
slReverse(&newList);
return newList;
}

void separateEntities(struct dlList *easyList, struct dlList *hardList)
/* Separate out hard (overlapping) entities onto hard list. */
{
struct dlNode *node, *next, *listNode;
int sepCount = 0;
dlSort(easyList, cmpEntities);
for (node = easyList->head; node->next != NULL; node = next)
    {
    struct entity *ent = node->val;
    int eStart = ent->start, eEnd = ent->end;
    next = node->next;
    for (listNode = easyList->head; listNode->next != NULL; listNode = listNode->next)
        {
        struct entity *listEnt = listNode->val;
        int lStart = listEnt->start, lEnd = listEnt->end;
        int start = max(eStart, lStart);
        int end = min(eEnd, lEnd);
        int overlap = end - start;
        if (listEnt != ent && overlap > 0)
            {
            int eRefCount = slCount(ent->cdaRefList);
            int lRefCount = slCount(listEnt->cdaRefList);
            /* Move the one with less cDNA to the hard list. */
            if (eRefCount > lRefCount)
                {
                dlRemove(listNode);
                dlAddTail(hardList, listNode);
                if (listNode == next)
                    next = node;
                }
            else
                {
                dlRemove(node);
                dlAddTail(hardList, node);
                }
            ++sepCount;
            break;
            }
        }
    }
}

boolean isThreePrime(struct cdaAli *ali)
/* Returns true if cdaAli is from 3' end of clone. */
{
char *dotPos = strrchr(ali->name, '.');
if (dotPos == NULL)
    return FALSE;       /* Make this better maybe later. */
return (dotPos[1] == '3');
}

double paranoidSqrt(double x)
/* There seems to be a bug in MS's square-root that enclosing it in
 * a subroutine avoids. */
{
double y = sqrt(x);
if (fabs(y*y - x) > 0.001)
    uglyf("sqrt(%f) = %f !?? \n", x, y);
return y;
}

boolean findIgRegion(struct entity *ent, int *retStart, int *retEnd, int *retCount)
/* Find intergenic region compute mean and variance of 3' ends, then 
 * return mean after discarding outlyers.  Returns false if data looks funky. */
{
int totalCount = 0;
int totalPos = 0;
int insideCount = 0;
int insidePos = 0;
double mean;
double insideMean;
double dif;
double varience = 0;
double std;
struct cdaRef *ref;
struct cdaAli *ali;
int end;
boolean revStrand = (ent->strand == '-');

/* Calculate mean. */
for (ref = ent->cdaRefList; ref != NULL; ref = ref->next)
    {
    ali = ref->ali;
    if (isThreePrime(ali))
        {
        ++totalCount;
        if (revStrand)
            totalPos += ali->chromStart-1;
        else
            totalPos += ali->chromEnd;
        }
    }
if (totalCount <= 0)
    return FALSE;

mean = (double)totalPos/totalCount;

/* Calculate square root of varience to estimate standard deviation. */
for (ref = ent->cdaRefList; ref != NULL; ref = ref->next)
    {
    ali = ref->ali;
    if (isThreePrime(ali))
        {
        if (revStrand)
            dif = ali->chromStart-1;
        else
            dif = ali->chromEnd;
        dif -= mean;
        varience += dif*dif;
        }
    }
varience /= totalCount;
std = paranoidSqrt(varience);

/* If varience too large, or curve not very bell shaped return FALSE. 
 * Figure out insideMean (mean of stuff within one standard deviation of outer mean.) */
if (std > 200)
    {
    return FALSE;
    }
totalPos = 0;
for (ref = ent->cdaRefList; ref != NULL; ref = ref->next)
    {
    ali = ref->ali;
    if (isThreePrime(ali))
        {
        ++totalCount;
        if (revStrand)
            end = ali->chromStart-1;
        else
            end = ali->chromEnd;
        dif = end - mean;
        if (fabs(dif) <= 200)
            {
            ++insideCount;
            insidePos += end;
            }
        }
    }
if (insideCount*2 < totalCount)
    {
    return FALSE;
    }
insideMean = (double)insidePos/insideCount;

if (revStrand)
    {
    end = round(insideMean - std*0.5);
    }
else
    {
    end = round(insideMean + std*0.5);
    }
*retStart = end-3;
*retEnd = end+3;
*retCount = totalCount;
return TRUE;
}

void saveEntities(struct dlList *entList, char *dir, char *prefix, char *chrom)
/* Write out list of entities to a file. */
{
char fileName[512];
FILE *f;
struct dlNode *node;
struct entity *ent;
static int entCount = 0;
struct intron *intron;
int igStart, igEnd, igCount;
char *source = "genieCon";
char upcChrom[16];

strcpy(upcChrom, chrom);
touppers(upcChrom);

sprintf(fileName, "%s/%s%s.gff", dir, prefix, upcChrom);
f = mustOpen(fileName, "w");
for (node = entList->head; node->next != NULL; node = node->next)
    {
    ent = node->val;
    ++entCount;
    fprintf(f, "%s\t%s\tcdnaCluster\t%d\t%d\t%d\t%c\t.\tgc%d\n",
        chrom, source, ent->start+1, ent->end, slCount(ent->cdaRefList), ent->strand, entCount);
    for (intron = ent->intronList; intron != NULL; intron = intron->next)
        {
        char *startType, *endType;
        if (ent->strand == '+')
            {
            startType = "splice5";
            endType = "splice3";
            }
        else
            {
            startType = "splice3";
            endType = "splice5";
            }
        fprintf(f, "%s\t%s\t%s\t%d\t%d\t.\t%c\t.\tgc%d\n",
            chrom, source, startType, intron->start, intron->start+1, ent->strand, entCount);
        fprintf(f, "%s\t%s\tintron\t%d\t%d\t.\t%c\t.\tgc%d\n",
            chrom, source, intron->start+1, intron->end, ent->strand, entCount);
        fprintf(f, "%s\t%s\t%s\t%d\t%d\t.\t%c\t.\tgc%d\n",
            chrom, source, endType, intron->end, intron->end+1, ent->strand, entCount);
        }
    if (findIgRegion(ent, &igStart, &igEnd, &igCount))
        {
        fprintf(f, "%s\t%s\tIG\t%d\t%d\t%d\t%c\t.\tafter_gc%d\n",
            chrom, source, igStart+1, igEnd, igCount, ent->strand, entCount);
        }
    }        
fclose(f);
}

int main(int argc, char *argv[])
{
char *outDir;
struct cdaAli *cdaList;
struct clonePair *pairList;
struct dlList **goodEntities, **badEntities;  /* Array of lists, one for each chromosome. */
int i;

if (argc != 2)
    {
    errAbort("genieCon - generates constraint GFF files for Genie from cDNA alignments\n"
             "usage\n"
             "      genieCon outputDir\n"
             "genieCon will create one file of form conXX.gff for each chromosome, where\n"
             "the XX is replaced by the chromosome name.");
    }
outDir = argv[1];

dnaUtilOpen();
anyChromNames(&chromNames, &chromCount);

goodEntities = needMem(chromCount * sizeof(goodEntities[0]));
badEntities = needMem(chromCount * sizeof(badEntities[0]));
for (i=0; i<chromCount; ++i)
    {
    goodEntities[i] = newDlList();
    badEntities[i] = newDlList();
    }

cdaList = readAllCda();
printf("Read in %d alignments\n", slCount(cdaList));
cdaCoalesceBlocks(cdaList);
printf("Coalesced blocks\n");
pairList = pairClones(cdaList);
printf("Before weeding genomic had %d clones\n", slCount(pairList));
pairList = weedGenomic(pairList);
printf("after weeding genomic had %d clones\n", slCount(pairList)); 
makeEntities(pairList, goodEntities);
for (i=0; i<chromCount; ++i)
    {
    printf("Made %d gene-like entities on chromosome %s\n",
        dlCount(goodEntities[i]), chromNames[i]);
    }
for (i=0; i<chromCount; ++i)
    {
    if (dlCount(goodEntities[i]) > 0)
        {
        separateEntities(goodEntities[i], badEntities[i]);
        printf("%d good %d bad entities on chromosome %s\n",
            dlCount(goodEntities[i]), dlCount(badEntities[i]), chromNames[i]);
        saveEntities(goodEntities[i], outDir, "ez", chromNames[i]);
        saveEntities(badEntities[i], outDir, "odd", chromNames[i]);
        }
    }
return 0;
}
