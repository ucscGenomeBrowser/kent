/* ggcPic - Generic picture of conservation of features near a gene. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
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
  "   -xxx=XXX\n"
  );
}

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
    int *count;		/* Array of total counts. */
    int totalSize;	/* Sum of all sizes of features */
    int totalFeatures;  /* Number of features. */
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

void initPcm(struct ggcInfo *g, struct pcm *pcm, char *name, int pixels)
/* Allocate and initialize pcm */
{
ZeroVar(pcm);
pcm->name = cloneString(name);
pcm->pixels = pixels;
AllocArray(pcm->match, pixels);
AllocArray(pcm->count, pixels);
if (g != NULL)
    slAddTail(&g->pcmList, pcm);
}

struct ggcInfo *newGgcInfo()
/* Initialize ggcInfo */
{
struct ggcInfo *g;
AllocVar(g);
g->closeSize = 30;
g->baseUp = g->baseDown = 3000;
initPcm(g, &g->up, "up", g->baseUp);
initPcm(g, &g->down, "down", g->baseDown);
initPcm(g, &g->utr3, "utr3", 400);
initPcm(g, &g->utr5, "utr5", 100);
initPcm(g, &g->cdsFirst, "cdsFirst", 100);
initPcm(g, &g->cdsMiddle, "cdsMiddle", 100);
initPcm(g, &g->cdsLast, "cdsLast", 100);
initPcm(g, &g->cdsSingle, "cdsSingle", 200);
initPcm(g, &g->cdsAll, "cdsAll", 200);
initPcm(g, &g->singleExon, "singleExon", 400);
initPcm(g, &g->intron, "intron", 1000);
initPcm(g, &g->splice5, "splice5", g->closeSize);
initPcm(g, &g->splice3, "splice3", g->closeSize);
initPcm(g, &g->txStart, "txStart", g->closeSize);
initPcm(g, &g->txEnd, "txEnd", g->closeSize);
initPcm(g, &g->tlStart, "tlStart", g->closeSize);
initPcm(g, &g->tlEnd, "tlEnd", g->closeSize);
return g;
}

#ifdef NEVER
void oldTallyHits(struct pcm *pcm, bool *hits, int hitSize,
   boolean isRev)
/* Buggy sampler. */
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

void debugTallyHits(struct pcm *pcm, bool *hits, int hitSize,
   boolean isRev)
/* Put samples from hits into pcm. */
{
int pixels = pcm->pixels;
int *match = pcm->match;
int *count = pcm->count;
int end = (min(hitSize, pixels));
int i, x;

if (hitSize <= 0)
    return;
pcm->totalSize += hitSize;
pcm->totalFeatures += 1;
if (isRev)
    reverseBytes(hits, hitSize);
for (i=0; i<end; ++i)
    {
    count[i] += 1;
    if (hits[i])
        match[i] += 1;
    }
if (isRev)
    reverseBytes(hits, hitSize);
}
#endif /* NEVER */

void tallyHits(struct pcm *pcm, bool *hits, int hitSize,
   boolean isRev)
/* Put samples from hits into pcm. */
{
int pixels = pcm->pixels;
int *match = pcm->match;
int *count = pcm->count;
int i, x;
double scale = (double)hitSize/(double)pixels;

if (hitSize <= 0)
    return;
pcm->totalSize += hitSize;
pcm->totalFeatures += 1;
if (isRev)
    reverseBytes(hits, hitSize);
for (i=0; i<pixels; ++i)
    {
    count[i] += 1;
    x = floor(i * scale);
    if (x >= hitSize)
         {
	 assert(x < pixels);
	 }
    if (hits[x])
        match[i] += 1;
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
int closeSize = g->closeSize;
int closeHalf = closeSize/2;

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
    int utr3Size = 0, utr5Size = 0, cdsAllSize = 0;
    int utr3Pos = 0, utr5Pos = 0, cdsAllPos = 0;
    bool *utr3Hits = NULL;
    bool *utr5Hits = NULL;
    bool *cdsAllHits = NULL;
    bool isRev = (gp->strand[0] == '-');

    if (gp->cdsStart - gp->txStart < g->closeSize/2 || 
    	gp->txEnd - gp->cdsEnd < g->closeSize/2)
        continue;
    // uglyf("%s %s tx %d-%d cds %d-%d\n", gp->name, gp->strand, gp->txStart, gp->txEnd, gp->cdsStart, gp->cdsEnd);
    /* Total up UTR and CDS sizes. */
    for (exonIx=0; exonIx<gp->exonCount; ++exonIx)
	 {
	 int eStart = gp->exonStarts[exonIx];
	 int eEnd = gp->exonEnds[exonIx];
	 int eSize = eEnd - eStart;
	 int oneUtr, oneCds;
	 oneCds = rangeIntersection(gp->cdsStart, gp->cdsEnd, eStart, eEnd);
	 // uglyf(" exon %d-%d, cdsIntersection %d\n", eStart, eEnd, oneCds);
	 if (oneCds > 0)
	     {
	     cdsAllSize += oneCds;
	     }
	 if (eStart < gp->cdsStart)
	     {
	     int utrStart = eStart;
	     int utrEnd = min(gp->cdsStart, eEnd);
	     int utrSize = utrEnd - utrStart;
	     // uglyf(" start utrSize %d\n", utrSize);
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
	     // uglyf(" end utrSize %d\n", utrSize);
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
    if (cdsAllSize > 0)
	AllocArray(cdsAllHits, cdsAllSize);
    // uglyf(" utr5size %d, utr3Size %d, cdsAllSize %d\n", utr5Size, utr3Size, cdsAllSize);
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
    assert(cdsAllPos == cdsAllSize);

    tallyHits(&g->utr5, utr5Hits, utr5Size, isRev);
    tallyHits(&g->utr3, utr3Hits, utr3Size, isRev);
    tallyHits(&g->cdsAll, cdsAllHits, cdsAllSize, isRev);
#ifdef OLD
    /* Uglyf - tally hits at UTR start/end */
        {
	if (isRev)
	    {
	    tallyInRange(&g->utr3, hits, chrom->size, gp->txStart, gp->txStart + g->utr3.pixels, isRev);
	    tallyInRange(&g->utr5, hits, chrom->size, gp->txEnd - g->utr5.pixels, gp->txEnd, isRev);
	    }
	else
	    {
	    tallyInRange(&g->utr5, hits, chrom->size, gp->txStart, gp->txStart + g->utr5.pixels, isRev);
	    tallyInRange(&g->utr3, hits, chrom->size, gp->txEnd - g->utr3.pixels, gp->txEnd, isRev);
	    }
	}
#endif /* OLD */

    /* Tally upstream/downstream hits. */
	{
	int s1 = gp->txStart - closeHalf;
	int e1 = s1 + closeSize;
	int s2 = gp->txEnd - closeHalf;
	int e2 = s2 + closeSize;
	if (isRev)
	    {
	    tallyInRange(&g->down, hits, chrom->size, gp->txStart - g->baseDown,
		gp->txStart, isRev);
	    tallyInRange(&g->up, hits, chrom->size, gp->txEnd, 
		gp->txEnd + g->baseUp, isRev);
	    tallyInRange(&g->txEnd, hits, chrom->size, s1, e1, isRev);
	    tallyInRange(&g->txStart, hits, chrom->size, s2, e2, isRev);
	    }
	else
	    {
	    tallyInRange(&g->up, hits, chrom->size, gp->txStart - g->baseUp,
		gp->txStart, isRev);
	    tallyInRange(&g->down, hits, chrom->size, gp->txEnd, 
		gp->txEnd + g->baseDown, isRev);
	    tallyInRange(&g->txStart, hits, chrom->size, s1, e1, isRev);
	    tallyInRange(&g->txEnd, hits, chrom->size, s2, e2, isRev);
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
	   tallyInRange(&g->cdsSingle, hits, chrom->size,
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
		tallyInRange(&g->tlEnd, hits, chrom->size, cs, ce, isRev);
		tallyInRange(&g->cdsLast, hits, chrom->size, 
			eStart, eEnd, isRev);
		}
	    else
	        {
		tallyInRange(&g->tlStart, hits, chrom->size, cs, ce, isRev);
		tallyInRange(&g->cdsFirst, hits, chrom->size, 
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
		tallyInRange(&g->tlStart, hits, chrom->size, cs, ce, isRev);
		tallyInRange(&g->cdsFirst, hits, chrom->size, 
			eStart, eEnd, isRev);
		}
	    else
	        {
		tallyInRange(&g->tlEnd, hits, chrom->size, cs, ce, isRev);
		tallyInRange(&g->cdsLast, hits, chrom->size, 
			eStart, eEnd, isRev);
		}
	    }
	/* Middle (but not only) coding exon */
	else if (eStart >= gp->cdsStart && eEnd <= gp->cdsEnd)
	    {
	    tallyInRange(&g->cdsMiddle, hits, chrom->size, eStart, eEnd, isRev);
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
	    tallyInRange(&g->splice5, hits, chrom->size, 
		    s1, e1, isRev);
	    tallyInRange(&g->splice3, hits, chrom->size, 
		    s2, e2, isRev);
	    }
	else
	    {
	    tallyInRange(&g->splice3, hits, chrom->size, 
		    s1, e1, isRev);
	    tallyInRange(&g->splice5, hits, chrom->size, 
		    s2, e2, isRev);
	    }
	tallyInRange(&g->intron, hits, chrom->size, iStart, iEnd, isRev);
	}
    freez(&utr5Hits);
    freez(&utr3Hits);
    freez(&cdsAllHits);
    }
freez(&hits);
lineFileClose(&lf);
}


void dumpPcm(struct pcm *pcm, FILE *f)
/* Dump out PCM to file. */
{
int i;
fprintf(f, "#%s: aveSize %2.1f\n", pcm->name, (double)pcm->totalSize/pcm->totalFeatures);
for (i=0; i<pcm->pixels; ++i)
    {
    double percent;
    int c = pcm->count[i];
    if (c == 0)
        percent = 0;
    else
        percent = 100.0 * pcm->match[i] / c;
    // fprintf(f, "%1.2f%%\n", percent);
    fprintf(f, "%1.2f%% %d of %d\n", percent, pcm->match[i], pcm->count[i]); //uglyf
    }
}

void ggcPic(char *axtDir, char *chromSizes, char *genePred, char *outDir)
/* ggcPic - Generic picture of conservation of features near a gene. */
{
struct hash *chromHash;
struct chromGenes *chromList, *chrom;
struct ggcInfo *g = newGgcInfo();
char axtFile[512];
char fileName[512];
FILE *f = NULL;
struct pcm *pcm;

makeDir(outDir);
readGenes(genePred, &chromHash, &chromList);
addSizes(chromSizes, chromHash, chromList);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    snprintf(axtFile, sizeof(axtFile), "%s/%s.axt", axtDir, chrom->name);
    ggcChrom(chrom, axtFile, g);
    }
snprintf(fileName, sizeof(fileName), "%s/%s", outDir, "genericGene.tab");
f = mustOpen(fileName, "w");
dumpPcm(&g->up, f);
dumpPcm(&g->utr5, f);
dumpPcm(&g->cdsFirst, f);
dumpPcm(&g->intron, f);
dumpPcm(&g->cdsMiddle, f);
dumpPcm(&g->intron, f);
dumpPcm(&g->cdsLast, f);
dumpPcm(&g->utr3, f);
dumpPcm(&g->down, f);
carefulClose(&f);

for (pcm = g->pcmList; pcm != NULL; pcm = pcm->next)
    {
    snprintf(fileName, sizeof(fileName), "%s/%s.tab", outDir, pcm->name);
    f = mustOpen(fileName, "w");
    dumpPcm(pcm, f);
    carefulClose(&f);
    }
}

void test(int oldSize, int newSize)
/* Test stretching... */
{
struct pcm pcm;
bool *hits;
int i;
initPcm(NULL, &pcm, "test", newSize);
AllocArray(hits, oldSize);
for (i=0; i<oldSize; ++i)
    {
    if (i&1)
        hits[i] = 1;
    }
tallyHits(&pcm, hits, oldSize, FALSE);
tallyHits(&pcm, hits, oldSize, FALSE);
for (i=0; i<newSize; ++i)
    {
    uglyf("%2d %2d %2d\n", i, pcm.match[i], pcm.count[i]);
    }
uglyAbort("All for now");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
// test(10, 10);
ggcPic(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
