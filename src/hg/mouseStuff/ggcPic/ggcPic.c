/* ggcPic - Generic picture of conservation of features near a gene. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memalloc.h"
#include "portable.h"
#include "axt.h"
#include "genePred.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ggcPic - Generic gene conservation picture\n"
  "usage:\n"
  "   ggcPic axtDir chrom.sizes genePred.txt outDir\n"
  "options:\n"
  "   -generic - Scale things for generic gene\n"
  "   -restrict=file  Restrict to only accessions listed in file\n"
  "          File should have one accession per line\n"
  "   -geneParts=parts.txt - Include info on parts of each gene in file\n"
  );
}

char *restrictFile = NULL;

struct chromGenes
/* List of all genes on a chromosome. */
    {
    struct chromGenes *next;
    char *name;			/* Allocated in hash */
    struct genePred *geneList;	/* List of genes in this chromosome. */
    int size;			/* Size of chromosome. */
    };

int chromGenesCmpName(const void *va, const void *vb)
/* Compare to sort based on name. */
{
const struct chromGenes *a = *((struct chromGenes **)va);
const struct chromGenes *b = *((struct chromGenes **)vb);
return strcmp(a->name, b->name);
}

void readGenes(char *fileName, 
	struct hash **retHash, struct chromGenes **retList)
/* Read genes into a hash of chromGenes. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(8);
struct chromGenes *chrom, *chromList = NULL;
struct genePred *gp;
char *row[10];
int count = 0;

while (lineFileRow(lf, row))
    {
    gp = genePredLoad(row);
    if ((chrom = hashFindVal(hash, gp->chrom)) == NULL)
        {
	AllocVar(chrom);
	hashAddSaveName(hash, gp->chrom, chrom, &chrom->name);
	slAddHead(&chromList, chrom);
	}
    slAddHead(&chrom->geneList, gp);
    ++count;
    }
printf("Read %d genes in %d chromosomes in %s\n", count, 
	slCount(chromList), fileName);
lineFileClose(&lf);
slSort(&chromList, chromGenesCmpName);
*retHash = hash;
*retList = chromList;
}

void addSizes(char *fileName, struct hash *chromHash, 
	struct chromGenes *chromList)
/* Add size of chromosome to all chromosomes in hash */
{
char *row[2];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chromGenes *chrom;

while (lineFileRow(lf, row))
    {
    if ((chrom = hashFindVal(chromHash, row[0])) != NULL)
	chrom->size = lineFileNeedNum(lf, row, 1);
    }
lineFileClose(&lf);

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (chrom->size == 0)
        errAbort("No size for %s in %s", chrom->name, fileName);
    }
}

struct pcm
/* Keep track of matches bases vs. total bases at
 * a certain pixel resolution. */
    {
    struct pcm *next;	/* Next in list. */
    char *name;		/* Name. */
    int pixels;		/* Pixel resolution. */
    int *match;		/* Array of match counts. */
    int *cover;		/* Array of covered counts. */
    int *coverNondash;  /* Array of covered-by-non-dash counts. */
    int *count;		/* Array of total counts. */
    int totalSize;	/* Sum of all sizes of features */
    int totalFeatures;  /* Number of features. */
    boolean detailed;	/* True for detailed output. */
    };

struct ggcInfo
/* Info necessary to make pic. */
    {
    int closeSize;	/* How big is close-up */
    int baseUp, baseDown;  /* Bases in upstream/downstream regions. */
    struct pcm *pcmList;   /* List of all enclosed pcm's. */
    struct pcm utr5, utr3; /* Track UTRs. */
    struct pcm cdsAll;     /* Track overall CDS */
    struct pcm cdsFirst;   /* Track first CDS exon */
    struct pcm cdsMiddle;  /* Track middle CDS exons */
    struct pcm cdsLast;    /* Track last CDS exon */
    struct pcm cdsSingle;  /* Track only one CDS exon genes. */
    struct pcm singleExon; /* Track single exon genes. */
    struct pcm intron;     /* Track intron. */
    struct pcm up, down;   /* Upstream/downstream regions. */
    struct pcm splice5, splice3; /* Track splice sites */
    struct pcm txStart, txEnd;	/* Transcription start/end */
    struct pcm tlStart, tlEnd;	/* Translation start/end */
    };

void initPcm(struct ggcInfo *g, struct pcm *pcm, char *name, int pixels, bool detailed)
/* Allocate and initialize pcm */
{
ZeroVar(pcm);
pcm->name = cloneString(name);
pcm->pixels = pixels;
pcm->detailed = detailed;
AllocArray(pcm->match, pixels);
AllocArray(pcm->cover, pixels);
AllocArray(pcm->coverNondash, pixels);
AllocArray(pcm->count, pixels);
if (g != NULL)
    slAddTail(&g->pcmList, pcm);
}

struct ggcInfo *usualGgcInfo()
/* Initialize ggcInfo */
{
struct ggcInfo *g;
AllocVar(g);
g->closeSize = 80;
g->baseUp = g->baseDown = 5000;
initPcm(g, &g->up, "up", 5000, FALSE);
initPcm(g, &g->down, "down", 5000, FALSE);
initPcm(g, &g->utr3, "utr3", 800, FALSE);
initPcm(g, &g->utr5, "utr5", 200, FALSE);
initPcm(g, &g->cdsFirst, "cdsFirst", 170, FALSE);
initPcm(g, &g->cdsMiddle, "cdsMiddle", 140, FALSE);
initPcm(g, &g->cdsLast, "cdsLast", 220, FALSE);
initPcm(g, &g->cdsSingle, "cdsSingle", 200, FALSE);
initPcm(g, &g->cdsAll, "cdsAll", 1400, FALSE);
initPcm(g, &g->singleExon, "singleExon", 400, FALSE);
initPcm(g, &g->intron, "intron", 5000, FALSE);
initPcm(g, &g->splice5, "splice5", g->closeSize, TRUE);
initPcm(g, &g->splice3, "splice3", g->closeSize, TRUE);
initPcm(g, &g->txStart, "txStart", g->closeSize, TRUE);
initPcm(g, &g->txEnd, "txEnd", g->closeSize, TRUE);
initPcm(g, &g->tlStart, "tlStart", g->closeSize, TRUE);
initPcm(g, &g->tlEnd, "tlEnd", g->closeSize, TRUE);
return g;
}

struct ggcInfo *genericGgcInfo()
/* Initialize ggcInfo scaled right for generic gene*/
{
struct ggcInfo *g;
AllocVar(g);
g->closeSize = 80;
g->baseUp = g->baseDown = 500;
initPcm(g, &g->up, "up", 200, FALSE);
initPcm(g, &g->down, "down", 200, FALSE);
initPcm(g, &g->utr3, "utr3", 200, FALSE);
initPcm(g, &g->utr5, "utr5", 200, FALSE);
initPcm(g, &g->cdsFirst, "cdsFirst", 200, FALSE);
initPcm(g, &g->cdsMiddle, "cdsMiddle", 200, FALSE);
initPcm(g, &g->cdsLast, "cdsLast", 200, FALSE);
initPcm(g, &g->cdsSingle, "cdsSingle", 200, FALSE);
initPcm(g, &g->cdsAll, "cdsAll", 200, FALSE);
initPcm(g, &g->singleExon, "singleExon", 200, FALSE);
initPcm(g, &g->intron, "intron", 200, FALSE);
initPcm(g, &g->splice5, "splice5", g->closeSize, TRUE);
initPcm(g, &g->splice3, "splice3", g->closeSize, TRUE);
initPcm(g, &g->txStart, "txStart", g->closeSize, TRUE);
initPcm(g, &g->txEnd, "txEnd", g->closeSize, TRUE);
initPcm(g, &g->tlStart, "tlStart", g->closeSize, TRUE);
initPcm(g, &g->tlEnd, "tlEnd", g->closeSize, TRUE);
return g;
}

struct ggcInfo *newGgcInfo()
/* Initialize ggcInfo */
{
if (optionExists("generic"))
    {
    return genericGgcInfo();
    }
else
    {
    return usualGgcInfo();
    }
}

void tallyHits(struct pcm *pcm, bool *hits, bool *covers, int hitSize,
   boolean isRev)
/* Put samples from hits into pcm. */
{
int pixels = pcm->pixels;
int *match = pcm->match;
int *count = pcm->count;
int *cover = pcm->cover;
int *coverNondash = pcm->coverNondash;
int i, x;
double scale = (double)hitSize/(double)pixels;
bool c;

if (hitSize <= 0)
    return;
pcm->totalSize += hitSize;
pcm->totalFeatures += 1;
if (isRev)
    {
    reverseBytes(hits, hitSize);
    reverseBytes(covers, hitSize);
    }
for (i=0; i<pixels; ++i)
    {
    x = floor(i * scale);
    if (x >= hitSize)
         {
	 assert(x < pixels);
	 }
    count[i] += 1;
    if (hits[x])
        match[i] += 1;
    c = covers[x];
    if (c)
	{
        cover[i] += 1;
	if (c == 2)
	    coverNondash[i] += 1;
	}
    if (hits[x] && !covers[x])
        errAbort("%s: i=%d, x=%d, hits[x] = %d, covers[x] = %d", pcm->name, i, x, hits[x], covers[i]);
    }
if (isRev)
    {
    reverseBytes(hits, hitSize);
    reverseBytes(covers, hitSize);
    }
}



void tallyInRange(struct pcm *pcm, bool *hits, bool *covers, int chromSize, 
	int start, int end, boolean isRev)
/* Add hits in range to pcm tally. */
{
int size;
if (start < 0) start = 0;
if (end > chromSize) end = chromSize;
size = end - start;
if (size > 0)
    tallyHits(pcm, hits+start, covers+start, size, isRev);
}

int countBools(bool *b, int size)
/* Return count of booleans in array that are TRUE. */
{
int i, count = 0;
for (i=0; i<size; ++i)
    if (b[i])
       ++count;
return count;
}

int totalGenes = 0, reviewedGenes = 0, genesUsed = 0;

void ggcChrom(struct chromGenes *chrom, char *axtFile, 
	struct ggcInfo *g, struct hash *restrictHash, 
	FILE *fParts)
/* Tabulate matches on chromosome. */
{
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
bool *hits, *covers;
int hitCount = 0, coverCount = 0;
struct axt *axt;
struct genePred *gp;
int closeSize = g->closeSize;
int closeHalf = closeSize/2;

/* Build up array of booleans - one per base - which are
 * 1's where mouse/human align and bases match, zero 
 * elsewhere. */
AllocArray(hits, chrom->size);
AllocArray(covers, chrom->size);
printf("%s (%d bases)\n", chrom->name, chrom->size);
while ((axt = axtRead(lf)) != NULL)
    {
    int tPos = axt->tStart;
    int symCount = axt->symCount, i;
    char t, q, *tSym = axt->tSym, *qSym = axt->qSym;

    if (axt->tEnd > chrom->size)
        errAbort("tEnd %d, chrom size %d in %s", 
		axt->tEnd, chrom->size, axtFile);
    if (axt->tStrand == '-')
        errAbort("Can't handle minus strand on target in %s", axtFile);
    for (i=0; i<symCount; ++i)
        {
	t = tSym[i];
	if (t != '-')
	    {
	    q = qSym[i];
	    if (toupper(t) == toupper(q))
		{
	        hits[tPos] = TRUE;
		++hitCount;
		}
	    if (q == '-')
	       covers[tPos] = 1;
	    else
	       covers[tPos] = 2;
	    ++tPos;
	    }
	}
    axtFree(&axt);
    }

for (gp = chrom->geneList; gp != NULL; gp = gp->next)
    {
    int exonIx;
    int utr3Size = 0, utr5Size = 0, cdsAllSize = 0;
    int utr3Pos = 0, utr5Pos = 0, cdsAllPos = 0;
    bool *utr3Hits = NULL, *utr3Covers = NULL;
    bool *utr5Hits = NULL, *utr5Covers = NULL;
    bool *cdsAllHits = NULL, *cdsAllCovers = NULL;
    bool isRev = (gp->strand[0] == '-');


    /* Filter out genes not in restrict hash if any. */
    ++totalGenes;
    if (restrictHash != NULL)
        if (!hashLookup(restrictHash, gp->name))
	    continue;
    ++reviewedGenes;

    /* Filter out genes without meaningful UTRs */
    if (gp->cdsStart - gp->txStart < g->closeSize/2 || 
    	gp->txEnd - gp->cdsEnd < g->closeSize/2)
        continue;
    ++genesUsed;

    /* Total up UTR and CDS sizes. */
    for (exonIx=0; exonIx<gp->exonCount; ++exonIx)
	 {
	 int eStart = gp->exonStarts[exonIx];
	 int eEnd = gp->exonEnds[exonIx];
	 int eSize = eEnd - eStart;
	 int oneUtr, oneCds;
	 oneCds = rangeIntersection(gp->cdsStart, gp->cdsEnd, eStart, eEnd);
	 if (oneCds > 0)
	     {
	     cdsAllSize += oneCds;
	     }
	 if (eStart < gp->cdsStart)
	     {
	     int utrStart = eStart;
	     int utrEnd = min(gp->cdsStart, eEnd);
	     int utrSize = utrEnd - utrStart;
	     if (isRev)
		 utr3Size += utrSize;
	     else
		 utr5Size += utrSize;
	     }
	 if (eEnd > gp->cdsEnd)
	     {
	     int utrStart = max(gp->cdsEnd, eStart);
	     int utrEnd = eEnd;
	     int utrSize = utrEnd - utrStart;
	     if (isRev)
		 utr5Size += utrSize;
	     else
		 utr3Size += utrSize;
	     }
	 }

    /* Condense hits from UTRs and CDSs */
    if (utr5Size > 0)
	{
	AllocArray(utr5Hits, utr5Size);
	AllocArray(utr5Covers, utr5Size);
	}
    if (utr3Size > 0)
	{
	AllocArray(utr3Hits, utr3Size);
	AllocArray(utr3Covers, utr3Size);
	}
    if (cdsAllSize > 0)
	{
	AllocArray(cdsAllHits, cdsAllSize);
	AllocArray(cdsAllCovers, cdsAllSize);
	}
    for (exonIx=0; exonIx<gp->exonCount; ++exonIx)
	{
	int eStart = gp->exonStarts[exonIx];
	int eEnd = gp->exonEnds[exonIx];
	int eSize = eEnd - eStart;
	int oneUtr, oneCds;
	oneCds = rangeIntersection(gp->cdsStart, gp->cdsEnd, eStart, eEnd);
	if (oneCds > 0)
	    {
	    int cdsStart = eStart;
	    int cdsEnd = gp->cdsEnd;

	    if (cdsStart < gp->cdsStart)
		cdsStart = gp->cdsStart;
	    memcpy(cdsAllHits + cdsAllPos, hits + cdsStart, oneCds * sizeof(*hits));
	    memcpy(cdsAllCovers + cdsAllPos, covers + cdsStart, oneCds * sizeof(*covers));
	    cdsAllPos += oneCds;
	    }
	if (eStart < gp->cdsStart)
	    {
	    int utrStart = eStart;
	    int utrEnd = min(gp->cdsStart, eEnd);
	    int utrSize = utrEnd - utrStart;
	    if (isRev)
		{
		memcpy(utr3Hits + utr3Pos, hits + utrStart, utrSize * sizeof(*hits));
		memcpy(utr3Covers + utr3Pos, covers + utrStart, utrSize * sizeof(*covers));
		utr3Pos += utrSize;
		}
	    else
		{
		memcpy(utr5Hits + utr5Pos, hits + utrStart, utrSize * sizeof(*hits));
		memcpy(utr5Covers + utr5Pos, covers + utrStart, utrSize * sizeof(*covers));
		utr5Pos += utrSize;
		}
	    }
	if (eEnd > gp->cdsEnd)
	    {
	    int utrStart = max(gp->cdsEnd, eStart);
	    int utrEnd = eEnd;
	    int utrSize = utrEnd - utrStart;
	    if (isRev)
		{
		memcpy(utr5Hits + utr5Pos, hits + utrStart, utrSize * sizeof(*hits));
		memcpy(utr5Covers + utr5Pos, covers + utrStart, utrSize * sizeof(*covers));
		utr5Pos += utrSize;
		}
	    else
		{
		memcpy(utr3Hits + utr3Pos, hits + utrStart, utrSize * sizeof(*hits));
		memcpy(utr3Covers + utr3Pos, covers + utrStart, utrSize * sizeof(*covers));
		utr3Pos += utrSize;
		}
	    }
	}
    assert(utr3Pos == utr3Size);
    assert(utr5Pos == utr5Size);
    assert(cdsAllPos == cdsAllSize);

    tallyHits(&g->utr5, utr5Hits, utr5Covers, utr5Size, isRev);
    tallyHits(&g->utr3, utr3Hits, utr3Covers, utr3Size, isRev);
    tallyHits(&g->cdsAll, cdsAllHits, cdsAllCovers, cdsAllSize, isRev);

    /* Optionally write out file with gene by gene info. */
    if (fParts != NULL)
        {
	/* Write header line first time through. */
	static boolean firstTime = TRUE;
	if (firstTime)
	    {
	    firstTime = FALSE;
	    fprintf(fParts, "#accession\tsize_5\tali_5\tmatch_5\tsize_c\tali_c\tmatch_c\tsize_3\tali_3\tmatch_3\n");
	    }
	fprintf(fParts, "%s\t", gp->name);
	fprintf(fParts, "%d\t%d\t%d\t", utr5Size, 
		countBools(utr5Covers, utr5Size),
		countBools(utr5Hits, utr5Size));
	fprintf(fParts, "%d\t%d\t%d\t", cdsAllSize, 
		countBools(cdsAllCovers, cdsAllSize),
		countBools(cdsAllHits, cdsAllSize));
	fprintf(fParts, "%d\t%d\t%d\n", utr3Size, 
		countBools(utr3Covers, utr3Size),
		countBools(utr3Hits, utr3Size));
	}

    /* Tally upstream/downstream hits. */
	{
	int s1 = gp->txStart - closeHalf;
	int e1 = s1 + closeSize;
	int s2 = gp->txEnd - closeHalf;
	int e2 = s2 + closeSize;
	if (isRev)
	    {
	    tallyInRange(&g->down, hits, covers, chrom->size, gp->txStart - g->baseDown,
		gp->txStart, isRev);
	    tallyInRange(&g->up, hits, covers, chrom->size, gp->txEnd, 
		gp->txEnd + g->baseUp, isRev);
	    tallyInRange(&g->txEnd, hits, covers, chrom->size, s1, e1, isRev);
	    tallyInRange(&g->txStart, hits, covers, chrom->size, s2, e2, isRev);
	    }
	else
	    {
	    tallyInRange(&g->up, hits, covers, chrom->size, gp->txStart - g->baseUp,
		gp->txStart, isRev);
	    tallyInRange(&g->down, hits, covers, chrom->size, gp->txEnd, 
		gp->txEnd + g->baseDown, isRev);
	    tallyInRange(&g->txStart, hits, covers, chrom->size, s1, e1, isRev);
	    tallyInRange(&g->txEnd, hits, covers, chrom->size, s2, e2, isRev);
	    }
	}

    /* Tally hits in coding exons */
    for (exonIx=0; exonIx < gp->exonCount; ++exonIx)
        {
	int eStart = gp->exonStarts[exonIx];
	int eEnd = gp->exonEnds[exonIx];
	/* Single coding exon. */
	if (eStart <= gp->cdsStart && eEnd >= gp->cdsEnd)
	   {
	   eStart = gp->cdsStart;
	   eEnd = gp->cdsEnd;
	   tallyInRange(&g->cdsSingle, hits, covers, chrom->size,
	   		eStart, eEnd, isRev);
	   }
	/* Initial coding exon */
	else if (eStart < gp->cdsStart && eEnd > gp->cdsStart)
	    {
	    int cs = gp->cdsStart - closeHalf;
	    int ce = cs + closeSize;
	    eStart = gp->cdsStart;
	    if (isRev)
	        {
		tallyInRange(&g->tlEnd, hits, covers, chrom->size, cs, ce, isRev);
		tallyInRange(&g->cdsLast, hits, covers, chrom->size, 
			eStart, eEnd, isRev);
		}
	    else
	        {
		tallyInRange(&g->tlStart, hits, covers, chrom->size, cs, ce, isRev);
		tallyInRange(&g->cdsFirst, hits, covers, chrom->size, 
			eStart, eEnd, isRev);
		}
	    }
	/* Final coding exon */
	else if (eStart < gp->cdsEnd && eEnd > gp->cdsEnd)
	    {
	    int cs = gp->cdsEnd - closeHalf;
	    int ce = cs + closeSize;
	    eEnd = gp->cdsEnd;
	    if (isRev)
	        {
		tallyInRange(&g->tlStart, hits, covers, chrom->size, cs, ce, isRev);
		tallyInRange(&g->cdsFirst, hits, covers, chrom->size, 
			eStart, eEnd, isRev);
		}
	    else
	        {
		tallyInRange(&g->tlEnd, hits, covers, chrom->size, cs, ce, isRev);
		tallyInRange(&g->cdsLast, hits, covers, chrom->size, 
			eStart, eEnd, isRev);
		}
	    }
	/* Middle (but not only) coding exon */
	else if (eStart >= gp->cdsStart && eEnd <= gp->cdsEnd)
	    {
	    tallyInRange(&g->cdsMiddle, hits, covers, chrom->size, eStart, eEnd, isRev);
	    }
	else
	    {
	    }
	}
	

    /* Tally hits in introns and splice sites. */
    for (exonIx=1; exonIx<gp->exonCount; ++exonIx)
        {
	int iStart = gp->exonEnds[exonIx-1];
	int iEnd = gp->exonStarts[exonIx];
	int s1 = iStart - closeHalf;
	int e1 = s1 + closeSize;
	int s2 = iEnd - closeHalf;
	int e2 = s2 + closeSize;
	if (isRev)
	    {
	    tallyInRange(&g->splice3, hits, covers, chrom->size, 
		    s1, e1, isRev);
	    tallyInRange(&g->splice5, hits, covers, chrom->size, 
		    s2, e2, isRev);
	    }
	else
	    {
	    tallyInRange(&g->splice5, hits, covers, chrom->size, 
		    s1, e1, isRev);
	    tallyInRange(&g->splice3, hits, covers, chrom->size, 
		    s2, e2, isRev);
	    }
	tallyInRange(&g->intron, hits, covers, chrom->size, iStart, iEnd, isRev);
	}
    freez(&utr5Hits);
    freez(&utr3Hits);
    freez(&cdsAllHits);
    freez(&utr5Covers);
    freez(&utr3Covers);
    freez(&cdsAllCovers);
    }
freez(&hits);
freez(&covers);
lineFileClose(&lf);
}


void dumpPcm(struct pcm *pcm, FILE *f, boolean header)
/* Dump out PCM to file. */
{
int i;
if (header)
    {
    /* fprintf(f, "#%s: aveSize %2.1f\n", pcm->name, (double)pcm->totalSize/pcm->totalFeatures); */
    if (pcm->detailed)
	fprintf(f, "position\t");
    fprintf(f, "aligning\tidentity\n");
    }
for (i=0; i<pcm->pixels; ++i)
    {
    double combined, pid, ali;
    int count = pcm->count[i];
    int cover = pcm->cover[i];
    int coverNondash = pcm->coverNondash[i];
    int match = pcm->match[i];
    if (count == 0)
        combined = pid = ali = 0;
    else
	{
	ali = 100.0 * cover / count;
        combined = 100.0 * match / count;
	if (cover == 0)
	    pid = 0;
	else
	    pid = 100.0 * match / coverNondash;
	   
	}
    if (pcm->detailed)
        {
	int half = pcm->pixels/2;
	int pos;
	if (i < half)
	    {
	    pos = -(pcm->pixels/2 - i);
	    }
	else
	    {
	    fprintf(f, "+");
	    pos = i+1 - pcm->pixels/2;
	    }
	fprintf(f, "%d\t", pos);
	}
    fprintf(f, "%5.2f%% %5.2f%%\n", 
    	ali, pid);
    }
}

struct hash *readRestrictHash()
/* Read restricted file into hash */
{
if (restrictFile == NULL)
    return NULL;
else
    {
    struct hash *hash = newHash(0);
    struct lineFile *lf = lineFileOpen(restrictFile, TRUE);
    char *row[1];
    while (lineFileRow(lf, row))
	hashAdd(hash, row[0], NULL);
    lineFileClose(&lf);
    return hash;
    }
}

void ggcPic(char *axtDir, char *chromSizes, char *genePred, char *outDir,
	char *outParts)
/* ggcPic - Generic picture of conservation of features near a gene. */
{
struct hash *chromHash;
struct chromGenes *chromList, *chrom;
struct ggcInfo *g = newGgcInfo();
char axtFile[512];
char fileName[512];
FILE *f = NULL;
FILE *fParts = NULL;
struct pcm *pcm;
struct hash *restrictHash = readRestrictHash();

if (outParts != NULL)
    fParts = mustOpen(outParts, "w");
makeDir(outDir);
readGenes(genePred, &chromHash, &chromList);
addSizes(chromSizes, chromHash, chromList);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    snprintf(axtFile, sizeof(axtFile), "%s/%s.axt", axtDir, chrom->name);
    ggcChrom(chrom, axtFile, g, restrictHash, fParts);
    }
snprintf(fileName, sizeof(fileName), "%s/%s", outDir, "genericGene.tab");
f = mustOpen(fileName, "w");
dumpPcm(&g->up, f, TRUE);
dumpPcm(&g->utr5, f, FALSE);
dumpPcm(&g->cdsFirst, f, FALSE);
dumpPcm(&g->intron, f, FALSE);
dumpPcm(&g->cdsMiddle, f, FALSE);
dumpPcm(&g->intron, f, FALSE);
dumpPcm(&g->cdsLast, f, FALSE);
dumpPcm(&g->utr3, f, FALSE);
dumpPcm(&g->down, f, FALSE);
carefulClose(&f);

for (pcm = g->pcmList; pcm != NULL; pcm = pcm->next)
    {
    snprintf(fileName, sizeof(fileName), "%s/%s.tab", outDir, pcm->name);
    f = mustOpen(fileName, "w");
    dumpPcm(pcm, f, TRUE);
    carefulClose(&f);
    }
printf("\n");
printf("%d total genes, %d reviewed genes, %d with UTRs\n", 
	totalGenes, reviewedGenes, genesUsed);
}

void test(int oldSize, int newSize)
/* Test stretching... */
{
struct pcm pcm;
bool *hits, *covers;
int i;
initPcm(NULL, &pcm, "test", newSize, FALSE);
AllocArray(hits, oldSize);
AllocArray(covers, oldSize);
for (i=0; i<oldSize; ++i)
    {
    covers[i] = 1;
    if (i&1)
        hits[i] = 1;
    }
tallyHits(&pcm, hits, covers, oldSize, FALSE);
tallyHits(&pcm, hits, covers, oldSize, FALSE);
for (i=0; i<newSize; ++i)
    {
    printf("%2d %2d %2d\n", i, pcm.match[i], pcm.count[i]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
restrictFile = optionVal("restrict", NULL);
if (argc != 5)
    usage();
pushCarefulMemHandler(1000*1000*1000);
ggcPic(argv[1], argv[2], argv[3], argv[4], optionVal("geneParts", NULL));
return 0;
}
