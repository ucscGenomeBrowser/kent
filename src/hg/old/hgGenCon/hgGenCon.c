/* hgGenCon - Generate constraints for Genie based on
 * human genome data. Currently set for chromosome 22
 * data. */
#include "common.h"
#include "hash.h"
#include "memalloc.h"
#include "errabort.h"
#include "portable.h"
#include "dlist.h"
#include "dnaseq.h"
#include "fa.h"
#include "fof.h"
#include "hgdb.h"
#include "fuzzyFind.h"
#include "cda.h"
#include "keys.h"

boolean enforceGtAgEnds = TRUE;     /* Forces introns to have GT/AG ends. */
boolean showSupport = TRUE;         /* Add supporting DNA to GFF. */

boolean snoop(char *name)
/* Good place to put a break point on particular data. */
{
if (sameString(name, "AI636495"))
    {
    uglyf("Got you %s\n", name);
    return TRUE;
    }
else if (sameString(name, "AW139736"))
    {
    uglyf("Got you %s\n", name);
    return TRUE;
    }
else
    return FALSE;
}

void usage()
/* Print usage instructions and exit */
{
errAbort(
    "hgGenCon - generate Genie constraint files for human genome\n"
    "data.  (Currently set up for chromosome 22.)\n"
    "usage:\n"
    "    hgGenCon input.sf est.pai dnaDir gffDir\n"
    "Will make constraints that correspond to files in dnaDir in\n"
    "gffDir using the alignment data in input.sf and est.pai.");
}


struct sfHit
/* Stores a parsed version of a hit line in .sf (sorted filtered hit)
 * file.  An example line looks like:
 *     91.80% - AA001171:0-413 of 414 at 22_20:495823-498183 of 500500
 */
    {
    struct sfHit *next;         /* Next in list. */
    double score;               /* Psuedo percent score. */
    char *query;                /* Name of query seq (not allocated here). */
    char qStrand;               /* Strand + or - of query seq. */
    int qSize;                  /* Size of query sequence. */
    int qStart, qEnd;           /* Position of match in query. */
    char *target;               /* Name of target seq (not allocated here). */
    int tSize;                  /* Size of target sequence. */
    int tStart, tEnd;           /* Position of match in target. */
    struct cdaAli *cda;         /* Hit as cdaAli. */
    };

struct sfParsedFile
/* Parsed .sf (sorted filtered hit) file. */
    {
    struct sfParsedFile *next;      /* Next in list. */
    char *fileName;                 /* Name of file. */
    struct sfHit *hitList;          /* List of all hits. */
    struct hash *queryNames;        /* Hash of all query names. */
    struct hash *targetNames;       /* Hash of all target names. */
    struct estPair *estPairs;       /* List of all EST pairs. */
     /* The following structures are for interfacing to CDA */
    struct cdaAli *cdaList;         /* List of hits as CDAs. */
    struct targetInfo *targetInfo;  /* Info on each target. */
    char *chromNames[1024];          /* Array of query names. */
    int chromCount;                 /* Number of query names. */
    struct dnaSeq *chromSeq[1024];   /* All query sequences. */
    };

struct hitRef
/* A reference to a hit. */
    {
    struct hitRef *next;    /* Next in list. */
    struct sfHit *hit;      /* Hit this refers to. */
    boolean used;           /* Has hit been used? */
    };

struct estPair
/* Info about an EST pair. This is stored in the val field of
 * the hash sfParsedFile.queryNames. */
    {
    struct estPair *next;       /* Next in list. */
    char *fivePrime;            /* 5' element of pair. Allocated in queryName hash. */
    char *threePrime;           /* 3' element of pair. Allocated in queryName hash. */
    struct hitRef *refList;     /* Hits that refer to pair. */
    };

struct targetInfo
/* Info about target. Stored in val field of the hash
 * sfParsedFile.targetNames. */
    {
    struct targetInfo *next;        /* Next in list. */
    char *name;                     /* Name (allocated in hash). */
    int nameIx;                     /* Index of name in chromNames[] */
    int size;                       /* Size of target. */    
    };

void sfFreeParsedFile(struct sfParsedFile **pSf)
/* Free up file. */
{
struct sfParsedFile *sf;
if ((sf = *pSf) != NULL)
    {
    int i;
    freeMem(sf->fileName);
    slFreeList(&sf->hitList);
    slFreeList(&sf->estPairs);
    slFreeList(&sf->targetInfo);
    cdaFreeAliList(&sf->cdaList);
    freeHash(&sf->queryNames);
    freeHash(&sf->targetNames);
    for (i=0; i<ArraySize(sf->chromSeq); ++i)
        {
        freeDnaSeq(&sf->chromSeq[i]);
        }
    freez(pSf);
    }
}

void malformed(char *fileName, int lineCount)
/* Print malformed line error message and abort. */
{
errAbort("Malformed line %d of %s", lineCount, fileName);
}

void addPairs(struct sfParsedFile *sf, char *fileName)
/* Add in EST pairs to sf. */
{
struct hash *qHash = sf->queryNames;
struct estPair *pairList = NULL, *pair;
FILE *f = mustOpen(fileName, "rb");
char line[512];
int lineCount = 0;
char *words[3];
int wordCount;

printf("Reading pair file %s\n", fileName);
while (fgets(line, sizeof(line), f))
    {
    struct hashEl *hel5, *hel3;
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount == 0) /* Allow blank lines. */
        continue;
    if (wordCount != 2)
        errAbort("Bad line %d of %s", lineCount, fileName);
    hel5 = hashLookup(qHash, words[0]);
    hel3 = hashLookup(qHash, words[1]);
    if (hel3 != NULL && hel5 != NULL)
        {
        AllocVar(pair);
        pair->fivePrime = hel5->name;
        pair->threePrime = hel3->name;
        hel3->val = hel5->val = pair;
        slAddHead(&pairList, pair);
        }
    }
slReverse(&pairList);
sf->estPairs = pairList;
fclose(f);
printf("Have %d active EST pairs\n", slCount(pairList));
}

struct sfParsedFile *sfRead(char *fileName, char *pairFileName)
/* Read in file. */
{
FILE *f = mustOpen(fileName, "rb");
struct sfParsedFile *sf = AllocA(struct sfParsedFile);
struct hash *qHash, *tHash;
char line[512];
int lineCount = 0;
char *words[10];
int wordCount;
char *parts[3];
int partCount;

printf("Reading %s\n", fileName);

sf->fileName = cloneString(fileName);
sf->queryNames = qHash = newHash(18);
sf->targetNames = tHash = newHash(10);

/* Parse lines of the form:
  91.80% - AA001171:0-413 of 414 at 22_20:495823-498183 of 500500
  91.30% - AA001309:34-457 of 462 at 22_44:122434-124477 of 500500
 */
while (fgets(line, sizeof(line), f))
    {
    struct sfHit *hit;
    struct hashEl *hel;

    ++lineCount;
    if (line[0] == '#') /* Allow commented out lines. */
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0) /* Allow blank lines. */
        continue;       
    if (wordCount != 9)
        malformed(fileName, lineCount);

    AllocVar(hit);
    if ((hit->score = atof(words[0])) <= 0)
        malformed(fileName, lineCount);
    hit->qStrand = words[1][0];
    if (hit->qStrand != '-' && hit->qStrand != '+')
        malformed(fileName, lineCount);
    partCount = chopString(words[2], ":-", parts, ArraySize(parts));
    if (partCount != 3)
        malformed(fileName, lineCount);
    hel = hashStore(qHash, parts[0]);
    hit->query = hel->name;
    hit->qStart = atoi(parts[1]);
    hit->qEnd = atoi(parts[2]);
    hit->qSize = atoi(words[4]);
    partCount = chopString(words[6], ":-", parts, ArraySize(parts));
    if (partCount != 3)
        malformed(fileName, lineCount);
    hit->tStart = atoi(parts[1]);
    hit->tEnd = atoi(parts[2]);
    hit->tSize = atoi(words[8]);
    hel = hashLookup(tHash, parts[0]);
    if (hel == NULL)
        {
        struct targetInfo *ti;

        if (sf->chromCount >= ArraySize(sf->chromNames))
            errAbort("Too many targets line %d of %s\n", lineCount, fileName);
        AllocVar(ti);
        hel = hashAdd(tHash, parts[0], ti);
        ti->nameIx = sf->chromCount;
        ti->name = sf->chromNames[sf->chromCount++] = hel->name;
        ti->size = hit->tSize;
        slAddHead(&sf->targetInfo, ti);
        }
    hit->target = hel->name;
    slAddHead(&sf->hitList, hit);
    }
fclose(f);
slReverse(&sf->targetInfo);
slReverse(&sf->hitList);
printf("Have %d hits\n", slCount(sf->hitList));

if (pairFileName != NULL)
    addPairs(sf, pairFileName);
return sf;
}

struct dnaSeq *seqFromBatch(struct fofBatch *bb)
/* Return DNA sequence read from position indicated by bb */
{
FILE *f = bb->f;
int size = bb->size;
char *text = needLargeMem(size+1);
text[size] = 0;
fseek(f, bb->offset, SEEK_SET);
mustRead(f, text, size);
return faFromMemText(text);
}


void makeAlignments(struct sfParsedFile *sf, char *dnaDir)
/* Make alignments in parsed file. Fills in the cda portion
 * of the hitList. */
{
struct sfHit *hit;
struct hash *tHash = sf->targetNames;
int maxMod = 20;
int mod = maxMod;
int maxDotMod = 50;
int dotMod = maxDotMod;
int ix = 0;
int total = slCount(sf->hitList);
struct fofBatch *batchBuf, *batchList, *bb;
int i;
struct fof *rnaFof = hgdbRnaFof(), *keyFof = hgdbKeyFof();

printf("Making alignments\n");

/* Allocate and link up a batch query list. */
batchBuf = needMem(total*sizeof(batchBuf[0]));
for (i=0, hit=sf->hitList; i<total; ++i, hit=hit->next)
    {
    struct fofBatch *b = &batchBuf[i];
    b->next = b+1;
    b->key = hit->query;
    b->data = hit;
    }
batchBuf[total-1].next = NULL;

printf("Making batch query\n");
batchList = fofBatchFind(rnaFof, batchBuf);
for (bb = batchList; bb != NULL; bb = bb->next)
    {
    struct sfHit *hit = bb->data;
    struct dnaSeq *targetSeq, *querySeq;
    struct hashEl *hel;
    struct targetInfo *ti;
    char *target = hit->target;
    char *query = hit->query;
    struct ffAli *ffAli;
    DNA *hay, *needle;

    ++ix;
    if (--mod == 0)
        {
        putc('.', stdout);
        fflush(stdout);
        mod = maxMod;
        if (--dotMod == 0)
            {
            printf("%d of %d\n", ix, total);
            dotMod = maxDotMod;
            }
        }
    hel = hashLookup(tHash, target);
    assert(hel != NULL);
    ti = hel->val;
    if ((targetSeq = sf->chromSeq[ti->nameIx]) == NULL)
        {
        char dnaName[512];
        sprintf(dnaName, "%s/%s.fa", dnaDir, target);
        sf->chromSeq[ti->nameIx] = targetSeq = faReadDna(dnaName);
        }
    
    querySeq = seqFromBatch(bb);
    if (hit->qStrand == '-')
        reverseComplement(querySeq->dna, querySeq->size);
    needle = querySeq->dna;
    hay = targetSeq->dna;
    if (hit->tStart < hit->tEnd)    /* Older versions of PatSpace would sometimes do this... */
        {
        int score;
        ffAli = ffFind(needle + hit->qStart, needle + hit->qEnd, 
                       hay + hit->tStart, hay + hit->tEnd, ffCdna);        
        score = ffScoreCdna(ffAli);
        if (ffAli == NULL)
	    {
            warn("Can't align %s and %s:%d-%d", query, target, hit->tStart, hit->tEnd);
	    }
        else
            {
            int maxBadEnd = 55;
            int fineScore = 170;
            int badLeft, badRight;
            /* Find right end. */
            struct ffAli *right;
            right = ffAli;
            while (right->right != NULL)
                right = right->right;
            badLeft = ffAli->nStart - needle;
            badRight = needle + querySeq->size - right->nEnd;
            if (score >= fineScore || 
                (badRight <= maxBadEnd && badLeft <= maxBadEnd))
                {
                struct cdaAli *cda;
                cda = cdaAliFromFfAli(ffAli, needle, querySeq->size,
                    hay, targetSeq->size, FALSE);
                cda->name = query;
                cda->milliScore = round(10*hit->score);
                cda->strand = hit->qStrand;
                cda->chromIx = ti->nameIx;
                hit->cda = cda;
                slAddHead(&sf->cdaList, cda);
                }
            ffFreeAli(&ffAli);
            }
        }
    freeDnaSeq(&querySeq);
    }
printf("\n");
printf("Looking up directions\n");
printf("Making batch query\n");
batchList = fofBatchFind(keyFof, batchList);
ix = 0;
maxMod *= 2;
mod = maxMod;
dotMod = maxDotMod;
for (bb = batchList; bb != NULL; bb = bb->next)
    {
    struct sfHit *hit = bb->data;
    char *query = hit->query;
    char dirVal[5];
    struct cdaAli *cda = hit->cda;
    ++ix;
    if (--mod == 0)
        {
        putc('.', stdout);
        fflush(stdout);
        mod = maxMod;
        if (--dotMod == 0)
            {
            printf("%d of %d\n", ix, total);
            dotMod = maxDotMod;
            }
        }
    if (cda != NULL)
        {
        char *text = needMem(bb->size+1);
        FILE *f = bb->f;
        fseek(f, bb->offset, SEEK_SET);
        mustRead(f, text, bb->size);
        if (keyTextScan(text, "dir", dirVal, sizeof(dirVal)))
            {
            if (dirVal[0] == '5')
                cda->direction = '+';
            else
                cda->direction = '-';
            }
        else
            cda->direction = '+';
        freeMem(text);
        }
    }
freeMem(batchBuf);
printf("\n");
}


/*-----------From here on down this is pretty much
 *           borrowed from genieCon.c  ---------------------*/

const int noiseLimit = 2;  /* Allow up to two bp of noise */
const int softEndOverlapLimit = 5;  /* For soft ends allow some overlap */

int chromCount;     /* Number of target sequences. */
char **chromNames;  /* Names of target sequences. */
struct dnaSeq **chromSeqs;   /* Target sequences. */

struct clonePair
/* A structure that holds a 5' and a 3' sequence from the same clone. */
    {
    struct clonePair *next;
    struct cdaAli *p5, *p3;
    int chromIx;
    };

struct cdaRef 
/* Holds a reference to a cda */
    {
    struct cdaRef *next;
    struct cdaAli *ali;
    };

struct exon 
/* Info on an exon. */
    {
    struct exon *next;          /* Next in list. */
    int start, end;             /* Start end coordinates (half open) */
    bool isIntron;              /* Always FALSE */
    char hardStart, hardEnd;    /* True if start, end boundary certain. */
    };

struct intron
/* Info on an intron.  First fields must be
 * identical with struct exon.  */
    {
    struct intron *next;        /* Next in list. */
    int start, end;             /* Start end coordinates (half open) */
    bool isIntron;              /* Always TRUE. */
    };

struct clonePair *newClonePair(struct hash *tHash, char *target)
/* Allocate new clone pair and fill in target index. */
{
struct clonePair *cp;
struct targetInfo *ti;
struct hashEl *tHel;
AllocVar(cp);
tHel = hashLookup(tHash, target);
ti = tHel->val;
cp->chromIx = ti->nameIx;
return cp;
}

struct hitRef *findMate(struct hitRef *refList, struct sfHit *hit)
/* Return best mate to hit in refList, or NULL if none. */
{
struct hitRef *ref;
struct hitRef *bestRef = NULL;
char mateDir = (hit->cda->direction == '+' ? '-' : '+');
char mateStrand = (hit->qStrand == '+' ? '-' : '+');
char *target = hit->target;
int maxOkDistance = 200000;
int bestDistance = maxOkDistance;

for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct sfHit *rHit = ref->hit;
    if (rHit->target == target && rHit->cda->direction == mateDir && rHit->qStrand == mateStrand)
        {
        int dist = intAbs(hit->tStart - rHit->tStart);
        if (dist < bestDistance)
            {
            bestDistance = dist;
            bestRef = ref;
            }
        }
    }
return bestRef;
}

struct clonePair *pairClones(struct sfParsedFile *sf)
/* Lump together clones into 3' and 5' pairs whenever possible. */
{
struct clonePair *pairList = NULL;
struct sfHit *hit;
struct hash *qHash = sf->queryNames, *tHash = sf->targetNames;
struct hashEl *hel;
struct estPair *estPair;

/* Loop through and deal with singleton ESTs.  Put
 * hits on pairs onto pairs reference list. */
for (hit = sf->hitList; hit != NULL; hit = hit->next)
    {
    struct cdaAli *cda = hit->cda;
    if (cda != NULL)
        {
        hel = hashLookup(qHash, hit->query);
        if ((estPair = hel->val) != NULL)
            {
            struct hitRef *hr;
            AllocVar(hr);
            hr->hit = hit;
            slAddHead(&estPair->refList, hr);
            }
        else
            {
            struct clonePair *cp = newClonePair(tHash, hit->target);
            if (cda->direction == '-')
                cp->p3 = cda;
            else
                cp->p5 = cda;        
            slAddHead(&pairList, cp);
            }
        }
    }

/* Loop through reference list of estPairs and make clone
 * pairs out of them. */
for (estPair = sf->estPairs; estPair != NULL; estPair = estPair->next)
    {
    struct hitRef *ref;
    for (ref = estPair->refList; ref != NULL; ref = ref->next)
        {
        struct hitRef *mate;
        struct sfHit *hit = ref->hit;
        if (ref->used)
            continue;
        if ((mate = findMate(ref->next, hit)) == NULL)
            {
            struct clonePair *cp = newClonePair(tHash, hit->target);
            struct cdaAli *cda = hit->cda;
            if (cda->direction == '-')
                cp->p3 = cda;
            else
                cp->p5 = cda;        
            slAddHead(&pairList, cp);
            }
        else
            {
            struct clonePair *cp = newClonePair(tHash, hit->target);
            struct cdaAli *cda = hit->cda;
            if (cda->direction == '-')
                {
                cp->p3 = cda;
                cp->p5 = mate->hit->cda;
                }
            else
                {
                cp->p5 = cda;        
                cp->p3 = mate->hit->cda;
                }
            mate->used = TRUE;
            slAddHead(&pairList, cp);
	    uglyf("In %s matching %s and %s\n", chromNames[cp->p3->chromIx],
	    	cp->p5->name, cp->p3->name);
            }
        }
    }
slReverse(&pairList);
return pairList;
}

int cmpExonOrIntron(const void *va, const void *vb)
/* Compare two exons or introns. */
{
struct exon **pA = (struct exon **)va;
struct exon **pB = (struct exon **)vb;
struct exon *a = *pA, *b = *pB;
int diff;

if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

struct entity
/* This holds a gene-like entity. */
    {
    struct entity *next;            /* Entities are kept permenantly on dlLists.  This
                                     * is just link for a temporary list. */
    struct dlNode *node;            /* Pointer to dlList node associated with entity. */
    struct cdaRef *cdaRefList;      /* cDNAs that created this entity. */
    int chromIx;                    /* Chromosome entity is on. */
    char strand;                    /* Chromosome strand. */
    int start, end;                 /* Chromosome position. */
    struct intron *intronList;      /* List of introns (all hard edged) */
    struct exon *exonList;          /* List of exons (some soft some hard edged). */
    struct exon *exonIntronList;    /* Combined intron/exon list (replaces above two if active. */
    int exonTotalCount;              /* Total number of exons unreliable unless you just called
                                     * setExonTotalCount() */
    boolean isLumped;               /* Set when entity is lumped together with others on 
                                     * that it overlaps with. */
    };

void combineExonIntronLists(struct entity *entList)
/* Combine the intron and exon lists of entity int exonIntronList. */
{
struct entity *ent;
for (ent = entList; ent != NULL; ent = ent->next)
    {
    ent->exonIntronList = slCat(ent->exonList, ent->intronList);
    ent->exonList = NULL;
    ent->intronList = NULL;
    slSort(&ent->exonIntronList, cmpExonOrIntron);
    }
}

void separateExonIntronLists(struct entity *entList)
/* Separate exonIntronList into exonList and intronList. */
{
struct entity *ent;

for (ent = entList; ent != NULL; ent = ent->next)
    {
    struct exon *exonList = NULL, *intronList = NULL;
    struct exon *el, *next;

    for (el = ent->exonIntronList; el != NULL; el = next)
        {
        next = el->next;
        if (el->isIntron)
            {
            slAddHead(&intronList, el);
            }
        else
            {
            slAddHead(&exonList, el);
            }
        }
    slReverse(&exonList);
    slReverse(&intronList);
    ent->exonList = exonList;
    ent->intronList = (struct intron*)intronList;
    ent->exonIntronList = NULL;
    }
}

void setExonTotalCounts(struct entity *entList)
/* Set the exonTotalCount field in a list of entities based on exonList */
{
struct entity *ent;

for (ent = entList; ent != NULL; ent = ent->next)
    {
    int total = 0;
    ent->exonTotalCount = slCount(ent->exonList);
#ifdef OLD
    struct exon *exon;
    for (exon = ent->exonList; exon != NULL; exon = exon->next)
        {
        total += exon->end - exon->start;
        }
#endif
    ent->exonTotalCount = total;
    }
}

int cmpEntExonTotalCount(const void *va, const void *vb)
/* Compare two entities total exon size. */
{
struct entity **pA = (struct entity **)va;
struct entity **pB = (struct entity **)vb;
struct entity *a = *pA, *b = *pB;
int diff;

if ((diff = a->start - b->start) != 0)
    return diff;
return b->exonTotalCount - a->exonTotalCount;
}

int cmpEntStart(const void *va, const void *vb)
/* Compare two entities start position. */
{
struct entity **pA = (struct entity **)va;
struct entity **pB = (struct entity **)vb;
struct entity *a = *pA, *b = *pB;
int diff;

if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

boolean isGoodIntron(struct dnaSeq *targetSeq, struct cdaBlock *a, 
    struct cdaBlock *b, char *retStrand)
/* Return true if there's a good intron between a and b. If true will update strand.*/
{
if (a->nEnd == b->nStart && b->hStart - a->hEnd > 40 && a->endGood >= 5 && b->startGood >= 5)
    {
    if (enforceGtAgEnds)
        {
        DNA *iStart = targetSeq->dna + a->hEnd;
        DNA *iEnd = targetSeq->dna + b->hStart;
        if (iStart[0] == 'g' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'g')
            {
            *retStrand = '+';
            return TRUE;
            }
        else if (iStart[0] == 'c' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'c')
            {
            *retStrand = '-';
            return TRUE;
            }
        else
            return FALSE;
        }
    else
        {
        return TRUE;
        }
    }
else
    return FALSE;
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
struct dnaSeq *targetSeq;

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
targetSeq = chromSeqs[ali->chromIx];
if (blockCount > 1)
    {
    for (i=1; i<blockCount; ++i)
        {
        char strand;
        prevBlock = block++;
        if ((goodIntron = isGoodIntron(targetSeq, prevBlock, block, &strand)) != FALSE)
            {
            exon->hardEnd = TRUE;
            AllocVar(intron);
            intron->start = prevBlock->hEnd;
            intron->end = block->hStart;
            intron->isIntron = TRUE;
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
struct dnaSeq *targetSeq = chromSeqs[ali->chromIx];
int i;
boolean hardStart, hardEnd;
char strand;

if (cdaCloneStrand(ali) != entity->strand)
    return FALSE;

/* Check first for compatible exons in ali - can't overlap any introns in
 * entity by more than noiseLimit for hard ends and softEndOverlapLimit
 * for soft ends. */
hardStart = hardEnd = FALSE;
for (block = ali->blocks, i=0; i<blockCount; ++i, ++block)
    {
    struct intron *intron;
    int hStart = block->hStart, hEnd = block->hEnd;
    if (i == blockCount-1)
        hardEnd = FALSE;
    else
        hardEnd = isGoodIntron(targetSeq, block, block+1, &strand);
    for (intron = entity->intronList; intron != NULL; intron = intron->next)
        {
        int iStart, iEnd;
        int start, end;
        int overlap;
        int overlapLimit = softEndOverlapLimit;
        if (hardStart && hardEnd)
            overlapLimit = noiseLimit;
        iStart = intron->start;
        start = max(hStart, iStart);
        iEnd = intron->end;
        end = min(hEnd, iEnd);
        overlap = end - start;
        if (overlap > overlapLimit)
            return FALSE;
        else if (overlap > noiseLimit)
            {
            assert(!hardStart || !hardEnd);
            if (iStart >= hStart && hEnd >= iEnd)
                return FALSE;
            if (iStart <= hStart && hEnd <= iEnd)
                return FALSE;
            if (iStart >= hStart && iStart <= hEnd)
                {
                int o = hEnd - iStart;
                assert(o == overlap);
                if (hardEnd)
                    return FALSE;
                }
            else
                {
                int o = iEnd - hStart;
                assert(o == overlap);
                if (hardStart)
                    return FALSE;
                }
            }
        }
    hardStart = hardEnd;
    }

/* Check that introns in ali are compatible with introns and exons in
 * entity. */
block = ali->blocks;
for (i=1; i<blockCount; ++i)
    {
    lastBlock = block++;
    if (isGoodIntron(targetSeq, lastBlock, block, &strand))
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

struct entity *findOverlappingEntities(struct dlNode *entitiesLeft, int start, int end, char strand)
/* Find all entities on source list that overlap with interval on strand
 * and that haven't already been lumped. Return them as singly-threaded 
 * entity list. */
{
struct dlNode *node;
struct entity *entityList = NULL, *entity;
for (node = entitiesLeft; node->next != NULL; node = node->next)
    {
    int s, e, overlap;
    entity = node->val;
    if (!entity->isLumped && entity->strand == strand)
        {
        s = max(entity->start, start);
        e = min(entity->end, end);
        overlap = e - s;
        if (overlap > 0)
            {
            if (entity->start < start)
                start = entity->start;
            if (entity->end > end)
                end = entity->end;
            slAddHead(&entityList, entity);
            entity->isLumped = TRUE;
            }
        }
    }
setExonTotalCounts(entityList);
slSort(&entityList, cmpEntExonTotalCount);
return entityList;
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

setExonTotalCounts(entityList);
slSort(&entityList, cmpEntExonTotalCount);
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
slSort(&root->exonList, cmpExonOrIntron);
root->exonList = weedDupeExons(root->exonList);
slSort(&root->intronList, cmpExonOrIntron);
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

boolean strandFromIntron(struct cdaAli *ali)
/* Set strand field of ali from introns it has if possible.
 * Returns TRUE if it has any introns. */
{
struct cdaBlock *block, *lastBlock;
int count;
int i;

if (ali == NULL || (count = ali->blockCount) <= 1)
    return FALSE;
block = ali->blocks;
for (i=1; i<count; ++i)
    {
    char strand;
    lastBlock = block++;
    if (isGoodIntron(chromSeqs[ali->chromIx], lastBlock, block, &strand))
        {
        if (strand == ali->direction)
            ali->strand = '+';
        else
            ali->strand = '-';
        return TRUE;
        }
    }
return FALSE;
}

struct clonePair *weedGenomic(struct clonePair *pairList)
/* Weed out clones with no introns (likely genomic contamination. */
{
struct clonePair *newList = NULL, *pair, *next;
boolean p3Intron, p5Intron;
for (pair = pairList; pair != NULL; pair = next)
    {
    next = pair->next;
    p3Intron = strandFromIntron(pair->p3);
    p5Intron = strandFromIntron(pair->p5);
    if (p3Intron || p5Intron)
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
dlSort(easyList, cmpEntStart);
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
return ali->direction == '-';
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

FILE *openEntFile(char *dir, char *prefix, char *chrom)
/* Put together file name and open it. */
{
char fileName[512];
sprintf(fileName, "%s/%s%s.gff", dir, prefix, chrom);
return mustOpen(fileName, "w");
}

void writeOneEntity(FILE *f, char *chrom, struct entity *ent, int entCount)
/* Write one entity to file. */
{
char *source = "genieCon";
struct intron *intron;
int igStart, igEnd, igCount;

fprintf(f, "%s\t%s\tcdnaCluster\t%d\t%d\t%d\t%c\t.\tgc%d",
    chrom, source, ent->start+1, ent->end, slCount(ent->cdaRefList), ent->strand, entCount);
if (showSupport)
    {
    struct cdaRef *ref;
    for (ref = ent->cdaRefList; ref != NULL; ref = ref->next)
        {
        struct cdaAli *ali = ref->ali;
        fprintf(f, " %s", ali->name);
        }
    }
fprintf(f, "\n");
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


#ifdef USELESS
void remergeWithHead(struct entity *entList)
/* Merge entities with first entity in list if possible. */
{
struct entity *head;
struct entity *ent, *next;
struct entity *newTail = NULL;
int hStart, hEnd;

if (entList == NULL)
    return;
head = entList;
ent = head->next;
head->next = NULL;
if (ent == NULL)
    return;
hStart = head->start;
hEnd = head->end;
for (; ent != NULL; ent = next)
    {
    int eStart = ent->start, eEnd = ent->end;
    int start = max(hStart, eStart), end = min(hEnd, eEnd);
    int overlap = end-start;
    next = ent->next;
    if (overlap > 0 && entityCompatableWithList(head, ent))
        {
        slAddTail(&head, ent);
        head = mergeEntities(head);
        head->next = NULL;
        }
    else
        {
        slAddTail(&newTail, ent);
        }
    }
head->next = newTail;
}
#endif /* USELESS */

#ifdef NOT_WORTHWHILE
struct exon *cloneEi(struct exon *exon)
/* Clone an exon or intron */
{
if (exon->isIntron)
    {
    struct intron *intron = AllocA(struct intron);
    intron->start = exon->start;
    intron->end = exon->end;
    intron->isIntron = TRUE;
    return (struct exon *)intron;
    }
else
    {
    struct exon *e = AllocA(struct exon);
    *e = *exon;
    e->next = NULL;
    return e;
    }
}

struct exon *makeMaxList(struct entity *entList)
/* Make a "maximal" list of exons and introns from entity list. */
/* This leaks memory, and I don't care (not bucketloads)... */
{
struct exon *maxList = NULL;
struct exon *a, *b;
struct exon *ei;
struct entity *ent;

for (ent = entList; ent != NULL; ent = ent->next)
    {
    a = maxList;
    maxList = NULL;
    b = ent->exonIntronList;
    for (;;)
        {
        /* Both empty, we're done. */
        if (a == NULL && b == NULL)
            break;
        /* Two non-overlapping cases - just throw in
         * the first one. */
        if (a == NULL || (b != NULL && b->end <= a->start))
            {
            ei = cloneEi(b);
            slAddHead(&maxList, ei);
            b = b->next;
            }
        else if (b == NULL || (a != NULL && a->end <= b->start))
            {
            ei = cloneEi(a);
            slAddHead(&maxList, ei);
            a = a->next;
            }
        /* Completely overlapping case. */
        else if (a->isIntron == b->isIntron &&
            intAbs(a->start - b->start) <= noiseLimit &&
            intAbs(a->end - b->end) <= noiseLimit)
            {
            ei = cloneEi(a);
            slAddHead(&maxList, ei);
            a = a->next;
            b = b->next;
            }
        /* Partially overlapping cases. */
        else
            {
            /* A encompasses B */
            if (a->start <= b->start && a->end >= b->end)
                {
                if (a->isIntron)
                    {
                    if (b->isIntron)
                        {
                        /* Both introns. Toss smaller one
                         * onto result list and advance both. */
                        ei = cloneEi(b);
                        slAddHead(&maxList, ei);
                        a = a->next;
                        b = b->next;
                        }
                    else  /* !b->isIntron */
                        {
                        /* A, an intron, encompasses B an exon.
                         * Put first part of A and all of B on
                         * result list. Put second part of A back on 
                         * source list. */
                        if (b->hardStart && b->hardEnd)
                            {
                            ei = cloneEi(a);
                            ei->end = b->start;
                            slAddHead(&maxList, ei);
                            ei = cloneEi(b);
                            slAddHead(&maxList, ei);
                            ei = cloneEi(a);
                            ei->start = b->end;
                            ei->next = a->next;
                            a = ei;
                            b = b->next;
                            }
                        else
                            {
                            b = b->next;
                            }
                        }
                    }
                else    /* !a->isIntron */
                    {
                    if (b->isIntron)
                        {
                        /* A, an exon, encompasses B an intron.
                         * Put first part of A and all of B on 
                         * result list. Put second part of A back on 
                         * source list. */
                        ei = cloneEi(a);
                        ei->end = b->start;
                        ei->hardEnd = TRUE;
                        slAddHead(&maxList, ei);
                        ei = cloneEi(b);
                        slAddHead(&maxList, ei);
                        ei = cloneEi(a);
                        ei->start = b->end;
                        ei->hardStart = TRUE;
                        ei->next = a->next;
                        a = ei;
                        b = b->next;
                        }
                    else  /* !b->isIntron */
                        {
                        /* If both exons advance over smaller one.
                         * The larger one stays and will get broken
                         * up by subsequent introns. */
                        b = b->next;
                        }
                    }
                }
            /* B encompasses A */
            else if (b->start <= a->start && b->end >= a->end)
                {
                if (a->isIntron)
                    {
                    if (b->isIntron)
                        {
                        /* Both introns. Toss smaller one
                         * onto result list and advance both. */
                        ei = cloneEi(a);
                        slAddHead(&maxList, ei);
                        a = a->next;
                        b = b->next;
                        }
                    else  /* !b->isIntron */
                        {
                        /* B, an exon, encompasses A, an intron.
                         * Put first part of B and all of A onto
                         * result list. Put second part of B back onto
                         * source list. */
                        ei = cloneEi(b);
                        ei->end = a->start;
                        ei->hardEnd = TRUE;
                        slAddHead(&maxList, ei);
                        ei = cloneEi(a);
                        slAddHead(&maxList, ei);
                        ei = cloneEi(b);
                        ei->start = a->end;
                        ei->hardStart = TRUE;
                        ei->next = b->next;
                        b = ei;
                        a = a->next;
                        }
                    }
                else    /* !a->isIntron */
                    {
                    if (b->isIntron)
                        {
                        /* B, an intron, encompasses A, an exon.
                         * Put first part of B and all ov A onto
                         * result list. Put second part of B back onto
                         * source list. */
                        if (a->hardEnd && b->hardEnd)
                            {
                            ei = cloneEi(b);
                            ei->end = a->start;
                            slAddHead(&maxList, ei);
                            ei = cloneEi(a);
                            slAddHead(&maxList, ei);
                            ei = cloneEi(b);
                            ei->start = a->end;
                            ei->next = b->next;
                            b = ei;
                            a = a->next;
                            }
                        else
                            {
                            a = a->next;
                            }
                        }
                    else  /* !b->isIntron */
                        {
                        /* If both exons advance over smaller one.
                         * The larger one stays and will get broken
                         * up by subsequent introns. */
                        a = a->next;
                        }
                    }
                }
            /* A starts before B starts and ends before B ends. */
            else if (a->start <= b->start)                     
                {
                if (a->isIntron)
                    {
                    if (b->isIntron)
                        {
                        /* A and B both introns and A starts before B.
                         * Put intersection of two onto result list. */
                        ei = cloneEi(b);
                        ei->end = a->end;
                        slAddHead(&maxList, ei);
                        a = a->next;
                        b = b->next; 
                        }
                    else
                        {
                        /* A is an intron that starts before B an exon. 
                         * Shrink intron so that it doesn't cover exon
                         * with hard ends. */
                        ei = cloneEi(a);
                        slAddHead(&maxList, ei);
                        a = a->next;
                        if (b->hardStart)
                            {
                            ei->end = b->start;
                            }
                        else
                            {
                            b = b->next;
                            }
                        }
                    }
                else
                    {
                    if (b->isIntron)
                        {
                        /* A is an exon that starts before B an intron.
                         * Shrink intron so that it doesn't cover exon
                         * with hard ends. */
                        if (a->hardEnd)
                            {
                            ei = cloneEi(a);
                            slAddHead(&maxList, a);
                            ei = cloneEi(b);
                            ei->next = b->next;
                            b = ei;
                            b->start = a->end;
                            }
                        else
                            {
                            b = b->next;
                            }
                        a = a->next;
                        }
                    else
                        {
                        /* A and B both exons and A starts before B.
                         * Put union of two onto result list. */
                        ei = cloneEi(a);
                        if (b->hardEnd || !a->hardEnd)
                            {
                            ei->end = b->end;
                            ei->hardEnd = b->hardEnd;
                            }
                        slAddHead(&maxList, ei);
                        a = a->next;
                        b = b->next;
                        }
                    }
                }
            /* B starts before A starts and ends before A ends. */
            else if (b->start <= a->start)
                {
                if (a->isIntron)
                    {
                    if (b->isIntron)
                        {
                        /* A and B both introns and B starts before A.
                         * Put intersection of two onto result list. */
                        ei = cloneEi(a);
                        ei->end = b->end;
                        slAddHead(&maxList, ei);
                        a = a->next;
                        b = b->next; 
                        }
                    else
                        {
                        /* B is an exon that starts before A an intron.
                         * Shrink intron so that it doesn't cover exon
                         * with hard ends. */
                        if (b->hardEnd)
                            {
                            ei = cloneEi(b);
                            slAddHead(&maxList, b);
                            ei = cloneEi(a);
                            ei->next = a->next;
                            a = ei;
                            a->start = b->end;
                            }
                        b = b->next;
                        }                                      
                    }
                else
                    {
                    if (b->isIntron)
                        {
                        /* B is an intron that starts before A an exon. 
                         * Shrink intron so that it doesn't cover exon
                         * with hard ends. */
                        if (a->hardStart)
                            {
                            ei = cloneEi(b);
                            slAddHead(&maxList, ei);
                            b = b->next;
                            ei->end = a->start;
                            }
                        else
                            {
                            b = b->next;
                            }
                        }
                    else
                        {
                        /* A and B both exons and B starts before A.
                         * Put union of two onto result list. */
                        ei = cloneEi(b);
                        if (a->hardEnd || !b->hardEnd)
                            {
                            ei->end = a->end;
                            ei->hardEnd = a->hardEnd;
                            }
                        slAddHead(&maxList, ei);
                        a = a->next;
                        b = b->next;
                        }
                    }
                }
            /* All cases should be covered by now. */
            else
                {
                assert(FALSE);
                }
            }
        }
    slReverse(&maxList);
    }
return maxList;
}

void fixupEntityStartStop(struct entity *ent)
/* Calculate entity start and stop from entities exon list. */
{
struct exon *exon;
for (exon = ent->exonList; exon != NULL; exon = exon->next)
    {
    if (exon->start < ent->start)
        ent->start = exon->start;
    if (exon->end > ent->end)
        ent->end = exon->end;
    }
}

void processOverlapping(struct entity *entList, char *chromName, int entIx, FILE *f, char *outName)
/* Process a list of overlapping entities. */
{
int entCount = slCount(entList);

uglyf("Processing %d overlapping entities into %s\n", entCount, outName);
if (entCount < 2)
    {
    }
else
    {
    struct exon *maxList = NULL;
    struct entity entity;
    combineExonIntronLists(entList);
    maxList = makeMaxList(entList);
    entity = *entList;
    entity.next = NULL;
    separateExonIntronLists(entList);
    entity.exonIntronList = maxList;
    separateExonIntronLists(&entity);
    fixupEntityStartStop(&entity);
    writeOneEntity(f, chromName, &entity, entIx);
    }
return;
}
#endif /* NOT_WORTHWHILE */

void outputEntities(struct dlList *allEnts, char *outDir, char *chromName)
/* Write out enties. */
{
struct dlNode *node;
struct entity *firstEnt;
FILE *ezFile, *oddFile, *bothFile;
int entCount = 0;
int geneCount = 0;
int altCount = 0;
int isoformCount = 0;
char *fooName = "\\temp\\foo.txt";
FILE *fooFile = mustOpen(fooName, "w");

ezFile = openEntFile(outDir, "ez", chromName);
oddFile = openEntFile(outDir, "odd", chromName);
bothFile = openEntFile(outDir, "both", chromName);

dlSort(allEnts, cmpEntStart);
for (node = allEnts->head; node->next != NULL; node = node->next)
    {
    firstEnt = node->val;
    if (!firstEnt->isLumped)
        {
        struct entity *entList, *ent;
        entList = findOverlappingEntities(node, firstEnt->start, firstEnt->end, firstEnt->strand);
        if (entList != NULL)
            {
            ++geneCount;
            ++isoformCount;
            setExonTotalCounts(entList);
            slSort(&entList, cmpEntExonTotalCount);
//            processOverlapping(entList, chromName, entCount, fooFile, fooName);
            writeOneEntity(ezFile, chromName, entList, entCount);
            writeOneEntity(bothFile, chromName, entList, entCount);
            ++entCount;
            if (entList->next != NULL)
                ++altCount;
            for (ent = entList->next; ent != NULL; ent = ent->next)
                {
                ++isoformCount;
                writeOneEntity(oddFile, chromName, ent, entCount);
                writeOneEntity(bothFile, chromName, ent, entCount);
                ++entCount;
                }
            }
        }
    }
printf("Contig %s has %d genes %d alt-spliced genes %d isoforms\n",
    chromName, geneCount, altCount, isoformCount);
fclose(ezFile);
fclose(oddFile);
fclose(bothFile);
}


int main(int argc, char *argv[])
{
char *sfName, *pairFileName, *dnaDir, *outDir;
struct sfParsedFile *sf;
struct cdaAli *cdaList;
struct clonePair *pairList;
struct dlList **goodEntities, **badEntities;  /* Array of lists, one for each chromosome. */
int i;
long startTime, endTime;

pushDebugAbort();
pushCarefulMemHandler(32*1024*1024);
startTime = clock1000();

if (argc != 5)
    usage();
sfName = argv[1];
pairFileName = argv[2];
dnaDir = argv[3];
outDir = argv[4];

#ifdef TESTONLY
sfName = "/d/biodata/human/testSets/ens16/merged/aliGlue/ens16.sf";
pairFileName = "/d/biodata/human/mrna/est.pai";
dnaDir = "/d/biodata/human/testSets/ens16/merged/fa";
outDir = "gff";
#endif /* TESTONLY */

dnaUtilOpen();

sf = sfRead(sfName, pairFileName);
makeAlignments(sf, dnaDir);
chromCount = sf->chromCount;
chromNames = sf->chromNames;
chromSeqs = sf->chromSeq;

goodEntities = needMem(chromCount * sizeof(goodEntities[0]));
badEntities = needMem(chromCount * sizeof(badEntities[0]));
for (i=0; i<chromCount; ++i)
    {
    goodEntities[i] = newDlList();
    badEntities[i] = newDlList();
    }

cdaList = sf->cdaList;
printf("Read in %d alignments\n", slCount(cdaList));
cdaCoalesceBlocks(cdaList);
printf("Coalesced blocks\n");
pairList = pairClones(sf);
printf("Before weeding genomic had %d clones\n", slCount(pairList));
pairList = weedGenomic(pairList);
printf("after weeding genomic had %d clones\n", slCount(pairList)); 
makeEntities(pairList, goodEntities);
for (i=0; i<chromCount; ++i)
    {
    if (dlCount(goodEntities[i]) > 0)
        {
        outputEntities(goodEntities[i], outDir, chromNames[i]);
        }
    }
endTime = clock1000();
printf("Run took %5.1f seconds.\n", 0.001*(endTime-startTime));
return 0;
}
