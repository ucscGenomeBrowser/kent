/* ggcPic - Generic picture of conservation of features near a gene. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"
#include "genePred.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ggcPic - Generic picture of conservation of features near a gene\n"
  "usage:\n"
  "   ggcPic axtDir chrom.sizes genePred.txt out.gif\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct chromGenes
/* List of all genes on a chromosome. */
    {
    struct chromGenes *next;
    char *name;	/* Allocated in hash */
    struct genePred *geneList;
    int size;	/* Size of chromosome. */
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
    int pixels;		/* Pixel resolution. */
    int *match;		/* Array of match counts. */
    int *count;		/* Array of total counts. */
    };

struct ggcInfo
/* Info necessary to make pic. */
    {
    int baseSplice5, baseSplice3; /* How many bases in splice sites. */
    int baseUp, baseDown;  /* Bases in upstream/downstream regions. */
    struct pcm utr5, utr3;  /* Track UTRs. */
    struct pcm cds; /* Track CDS */
    struct pcm splice5, intron, splice3; /* Track intron. */
    struct pcm up, down;		/* Upstream/downstream regions. */
    };

void initPcm(struct pcm *pcm, int pixels)
/* Allocate and initialize pcm */
{
pcm->pixels = pixels;
AllocArray(pcm->match, pixels);
AllocArray(pcm->count, pixels);
}

struct ggcInfo *newGgcInfo()
/* Initialize ggcInfo */
{
struct ggcInfo *g;
AllocVar(g);
g->baseSplice5 = g->baseSplice3 = 30;
g->baseUp = g->baseDown = 1000;
initPcm(&g->utr3, 1000);
initPcm(&g->utr5, 1000);
initPcm(&g->cds, 1000);
initPcm(&g->splice5, g->baseSplice5);
initPcm(&g->intron, 1000);
initPcm(&g->splice3, g->baseSplice3);
initPcm(&g->up, 1000);
initPcm(&g->down, 1000);
return g;
}

void tallyHits(struct pcm *pcm, bool *hits, int hitSize,
   boolean isRev)
{
int pixels = pcm->pixels;
int *match = pcm->match;
int *count = pcm->count;
int s, e, i, j;

if (isRev)
    reverseBytes(hits, hitSize);
s = 0;
for (i=0; i<pixels; ++i)
    {
    e = ((i+1) * hitSize)/pixels;
    for (j=s; j<e; ++j)
        {
	count[i] += 1;
	if (hits[j])
	    match[i] += 1;
	}
    s = e;
    }
if (isRev)
    reverseBytes(hits, hitSize);
}

void tallyInRange(struct pcm *pcm, bool *hits, int chromSize, 
	int start, int end, boolean isRev)
/* Add hits in range to pcm tally. */
{
int size;
if (start < 0) start = 0;
if (end > chromSize) end = chromSize;
size = end - start;
if (size > 0)
    tallyHits(pcm, hits+start, size, isRev);
}

void ggcChrom(struct chromGenes *chrom, char *axtFile, struct ggcInfo *g)
/* Tabulate matches on chromosome. */
{
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
bool *hits;
int hitCount = 0;
struct axt *axt;
struct genePred *gp;

/* Build up array of booleans - one per base - which are
 * 1's where mouse/human align and bases match, zero 
 * elsewhere. */
AllocArray(hits, chrom->size);
printf("Allocated %d for %s\n", chrom->size, chrom->name);
while ((axt = axtRead(lf)) != NULL)
    {
    int tPos = axt->tStart;
    int symCount = axt->symCount, i;
    char t, *tSym = axt->tSym, *qSym = axt->qSym;

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
	    if (toupper(t) == toupper(qSym[i]))
		{
	        hits[tPos] = TRUE;
		++hitCount;
		}
	    ++tPos;
	    }
	}
    axtFree(&axt);
    }
printf("%d hits (%4.2f%%)\n", hitCount, 100.0*hitCount/chrom->size);

for (gp = chrom->geneList; gp != NULL; gp = gp->next)
    {
    int exonIx;
    int utr3Size = 0, utr5Size = 0, cdsSize = 0;
    int utr3Pos = 0, utr5Pos = 0, cdsPos = 0;
    bool *utr3Hits = NULL;
    bool *utr5Hits = NULL;
    bool *cdsHits = NULL;
    bool isRev = (gp->strand[0] == '-');

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
	     cdsSize += oneCds;
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
	AllocArray(utr5Hits, utr5Size);
    if (utr3Size > 0)
	AllocArray(utr3Hits, utr3Size);
    if (cdsSize > 0)
	AllocArray(cdsHits, cdsSize);
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
	    memcpy(cdsHits + cdsPos, hits + cdsStart, oneCds * sizeof(*hits));
	    cdsPos += oneCds;
	    }
	if (eStart < gp->cdsStart)
	    {
	    int utrStart = eStart;
	    int utrEnd = min(gp->cdsStart, eEnd);
	    int utrSize = utrEnd - utrStart;
	    if (isRev)
		{
		memcpy(utr3Hits + utr3Pos, hits + utrStart, utrSize * sizeof(*hits));
		utr3Pos += utrSize;
		}
	    else
		{
		memcpy(utr5Hits + utr5Pos, hits + utrStart, utrSize * sizeof(*hits));
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
		utr5Pos += utrSize;
		}
	    else
		{
		memcpy(utr3Hits + utr3Pos, hits + utrStart, utrSize * sizeof(*hits));
		utr3Pos += utrSize;
		}
	    }
	}
    assert(utr3Pos == utr3Size);
    assert(utr5Pos == utr5Size);
    assert(cdsPos == cdsSize);

    tallyHits(&g->utr5, utr5Hits, utr5Size, isRev);
    tallyHits(&g->utr3, utr3Hits, utr3Size, isRev);
    tallyHits(&g->cds, cdsHits, cdsSize, isRev);
    if (isRev)
        {
	tallyInRange(&g->down, hits, chrom->size, gp->txStart - g->baseDown,
	    gp->txStart, isRev);
	tallyInRange(&g->up, hits, chrom->size, gp->txEnd, 
	    gp->txEnd + g->baseUp, isRev);
	}
    else
	{
	tallyInRange(&g->up, hits, chrom->size, gp->txStart - g->baseUp,
	    gp->txStart, isRev);
	tallyInRange(&g->down, hits, chrom->size, gp->txEnd, 
	    gp->txEnd + g->baseDown, isRev);
	}
    if (isRev)
    /* Tally hits in introns. */
    for (exonIx=1; exonIx<gp->exonCount; ++exonIx)
        {
	int iStart = gp->exonEnds[exonIx-1];
	int iEnd = gp->exonStarts[exonIx];
	if (isRev)
	    {
	    tallyInRange(&g->splice3, hits, chrom->size, 
		    iStart, iStart + g->baseSplice3, isRev);
	    tallyInRange(&g->splice5, hits, chrom->size, 
		    iEnd - g->baseSplice5, iEnd, isRev);
	    }
	else
	    {
	    tallyInRange(&g->splice5, hits, chrom->size, 
		    iStart, iStart + g->baseSplice3, isRev);
	    tallyInRange(&g->splice3, hits, chrom->size, 
		    iEnd - g->baseSplice5, iEnd, isRev);
	    }
	tallyInRange(&g->intron, hits, chrom->size, iStart, iEnd, isRev);
	}
    freez(&utr5Hits);
    freez(&utr3Hits);
    freez(&cdsHits);
    }
freez(&hits);
lineFileClose(&lf);
}


void dumpPcm(struct pcm *pcm, char *label, FILE *f)
/* Dump out PCM to file. */
{
int i;
fprintf(f, "%s:\n", label);
for (i=0; i<pcm->pixels; ++i)
    {
    double percent;
    int c = pcm->count[i];
    if (c == 0)
        percent = 0;
    else
        percent = 100.0 * pcm->match[i] / c;
    fprintf(f, "%1.2f%%\n", percent);
    }
fprintf(f, "\n");
}

void ggcPic(char *axtDir, char *chromSizes, char *genePred, char *outGif)
/* ggcPic - Generic picture of conservation of features near a gene. */
{
struct hash *chromHash;
struct chromGenes *chromList, *chrom;
struct ggcInfo *g = newGgcInfo();
char axtFile[512];
FILE *f = mustOpen(outGif, "w");

readGenes(genePred, &chromHash, &chromList);
addSizes(chromSizes, chromHash, chromList);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    snprintf(axtFile, sizeof(axtFile), "%s/%s.axt", axtDir, chrom->name);
    ggcChrom(chrom, axtFile, g);
    }
dumpPcm(&g->up, "Up1000", f);
dumpPcm(&g->down, "Down1000", f);
dumpPcm(&g->utr5, "5'UTR", f);
dumpPcm(&g->utr3, "3'UTR", f);
dumpPcm(&g->cds, "CDS", f);
dumpPcm(&g->splice5, "5'Splice", f);
dumpPcm(&g->splice3, "3'Splice", f);
dumpPcm(&g->intron, "Intron", f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
ggcPic(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
