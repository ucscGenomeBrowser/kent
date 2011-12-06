/* ppAnalyse - Analyse paired plasmid reads. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ppAnalyse - Analyse paired plasmid reads.\n"
  "usage:\n"
  "   ppAnalyse headers insertSizes pairs.psl finDir pairedReads.pair mispaired.lst\n");
}

struct readInfo
/* Information on a single read. */
    {
    struct readInfo *next;	/* Next in list. */
    char *name;                 /* Read name - allocated in hash. */
    struct pairInfo *pair;      /* Associated pair info. */
    struct shortAli *aliList;   /* List of alignments this is in. */
    char readDir;	        /* Direction of read - F or R. */
    UBYTE version;              /* Version, usually zero. */
    };

struct pairInfo
/* Info on a read pair. */
    {
    struct pairInfo *next;	/* Next in list. */
    char *name;                 /* Pair name - allocated in hash. */
    char project[8];            /* Project - determines insert size. */
    int plate;                  /* Plate within project. */
    char well[4];               /* Well - something like 'E10' */
    struct readInfo *fRead;     /* Forward read. */
    struct readInfo *rRead;     /* Reverse read. */
    };

struct shortAli
/* Short alignment info. */
    {
    struct shortAli *next;
    char *tName;           /* Target name - allocated in hash */
    int tStart, tEnd;      /* Target start and end position. */
    int tSize;             /* Target size. */
    char strand;           /* Relative oriention. */
    };

struct groupSizeInfo
/* Info about size distribution of a group of reads. */
    {
#define gsiSlotCount 100
#define gsiSlotSize 100
    struct groupSizeInfo *next;	/* Next in list. */
    char *name;			/* Allocated in hash. */
    int guessedMin;             /* Size it's supposed to be. */
    int guessedMax;             /* Size it's supposed to be. */
    int guessedCount;    	/* Number measured that are in guessed range. */
    int measuredCount;		/* Count of measured ones. */
    int measuredMin;            /* Minimum measured size. */
    int measuredMax;            /* Max measured size. */
    double measuredTotal;       /* Cumulative total measured. */
    double measuredMean;        /* Average. */
    double variance;            /* Sum of squares of differences from mean. */
    int histogram[gsiSlotCount];  /* Grouped by += 100. */
    };

boolean parseWhiteheadReadName(char *readName, char retProject[8], int *retPlate, 
	char *retReadDir, char retWell[4], UBYTE *retVersion)
/* Parse a Whitehead read name into components.  Return FALSE if
 * an error. */
{
char *s, *e, c;
int version;
s = readName;
e = strchr(s+1, 'P');
if (e == NULL)
    return FALSE;
if (e - s >= 8)
    return FALSE;
*e++ = 0;
strcpy(retProject, s);
s = e;
while (isdigit(*e))
   ++e;
c = *e;
if (c != 'F' && c != 'R')
     return FALSE;
*retReadDir = c;
*e++ = 0;
*retPlate = atoi(s);
s = e;
e = strchr(s, '.');
if (e == NULL)
    return FALSE;
*e++ = 0;
if (strlen(s) > 3)
    return FALSE;
strcpy(retWell, s);
s = e;
if (*s++ != 'T')
    return FALSE;
if (!isdigit(*s))
    return FALSE;
version = atoi(s);
if (version > 255)
    return FALSE;
*retVersion = version;
return TRUE;
}

void readHeaders(char *headersName, struct hash *readHash, struct hash *pairHash,
    struct readInfo **pReadList, struct pairInfo **pPairList)
/* Read in header file and make reads and pairs out of it. */
{
struct lineFile *lf = lineFileOpen(headersName, TRUE);
int lineSize;
char *line;
struct readInfo *rd;
struct pairInfo *pair;
char pairName[64];
int plate;
UBYTE version;
char well[4], project[8], readDir;
char *readName;
struct hashEl *hel;
int dotEvery = 20*1024;
int dotty = dotEvery;

printf("Reading and processing %s\n", headersName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (--dotty <= 0)
	{
	dotty = dotEvery;
        printf(".");
	fflush(stdout);
	}

    /* Make read and fill in name part of it. Insure name is unique. */
    readName = line+1;
    if (hashLookup(readHash, readName) != NULL)
        {
	warn("Duplicate read name %s line %d of %s", 
		readName, lf->lineIx, lf->fileName);
	continue;
	}
    AllocVar(rd);
    hel = hashAdd(readHash, readName, rd);
    rd->name = hel->name;

    /* Parse read name into components and construct pair name. */
    if (!parseWhiteheadReadName(readName, project, &plate, &readDir, well, &version))
        errAbort("Bad Whitehead read name %s line %d of %s", 
		line, lf->lineIx, lf->fileName);
    sprintf(pairName, "%sP%d%s", project, plate, well);

    /* Make up a new pair if it doesn't exist. */
    if ((pair = hashFindVal(pairHash, pairName)) == NULL)
        {
	AllocVar(pair);
	hel = hashAdd(pairHash, pairName, pair);
	pair->name = hel->name;
	strncpy(pair->project, project, sizeof(pair->project));
	pair->plate = plate;
	strncpy(pair->well, well, sizeof(pair->well));
	slAddHead(pPairList, pair);
	}

    /* Fill in the rest of read and attatch it to pair. */
    rd->pair = pair;
    rd->readDir = readDir;
    rd->version = version;
    if (rd->readDir == 'F')
        {
	if (pair->fRead == NULL || pair->fRead->version < version)
	    pair->fRead = rd;
	}
    else 
        {
	if (pair->rRead == NULL || pair->rRead->version < version)
	    pair->rRead = rd;
	}
    slAddHead(pReadList, rd);
    }
printf("\n");
lineFileClose(&lf);
}


boolean filter(struct psl *psl)
/* Return TRUE if psl passes filter. */
{
int startTail, endTail;
int maxTail = 30;
pslTailSizes(psl, &startTail, &endTail);
if (startTail > maxTail || endTail > maxTail)
    return FALSE;
return TRUE;
}

int aliSizes[20];

int readAlignments(char *pairsPsl, struct hash *readHash, struct hash *fragHash)
/* Read in alignments and process them into the read->aliList. 
 * Returns number of alignments altogether. */
{
struct lineFile *lf = pslFileOpen(pairsPsl);
struct shortAli *ali;
struct psl *psl;
struct readInfo *rd;
int aliCount = 0;
int dotEvery = 20*1024;
int dotty = dotEvery;
int aliSize;

printf("Reading and processing %s\n", pairsPsl);
for (;;)
    {
    AllocVar(ali);     /* Allocate this first to reduce memory fragmentation. */
    if ((psl = pslNext(lf)) == NULL)
        {
	freeMem(ali);
	break;
	}
    if (filter(psl))
	{
	rd = hashMustFindVal(readHash, psl->qName);
	aliSize = psl->match + psl->repMatch;
	aliSize /= 100;
	if (aliSize < 0) aliSize = 0;
	if (aliSize >= ArraySize(aliSizes)) aliSize = ArraySize(aliSizes)-1;
	aliSizes[aliSize] += 1;
	ali->tName = hashStoreName(fragHash, psl->tName);
	ali->tStart = psl->tStart;
	ali->tEnd = psl->tEnd;
	ali->tSize = psl->tSize;
	ali->strand = psl->strand[0];
	slAddHead(&rd->aliList, ali);
	pslFree(&psl);
	++aliCount;
	}
    else
        {
	pslFree(&psl);
	freeMem(ali);
	}
    if (--dotty <= 0)
	{
	dotty = dotEvery;
        printf(".");
	fflush(stdout);
	}
    }
printf("\n");
return aliCount;
}

void doReadStats(struct readInfo *readList, int aliCount)
/* Calculate and display some stats on the reads and alignments. */
{
int hist[21];
int hitCount;
int i;
struct readInfo *rd;
int readCount = 0;
double scale;

for (i=0; i<ArraySize(hist); ++i)
   hist[i] = 0;
printf("calculating statistics\n");
for (rd = readList; rd != NULL; rd = rd->next)
    {
    ++readCount;
    hitCount = slCount(rd->aliList);
    if (hitCount >= ArraySize(hist))
        hitCount = ArraySize(hist)-1;
    hist[hitCount] += 1;
    }
printf("\n");
printf("How many reads hit how many times\n");
scale = 1.0/(double)aliCount;
for (i=0; i<ArraySize(hist); ++i)
    {
    printf("%2d %7d (%5.2f)\n", i, hist[i], 100.0 * hist[i] * scale);
    }
printf("%d reads, %d hits total\n", readCount, aliCount);
}

char *pairInFrag(struct pairInfo *pair, 
    struct shortAli **retFali, struct shortAli **retRali)
/* If both sides of pair align to same frag return frag name from 
 * frag hash table.  Otherwise return NULL. */
{
struct shortAli *rAli, *fAli;
char *frag;

for (rAli = pair->rRead->aliList; rAli != NULL; rAli = rAli->next)
    {
    frag = rAli->tName;
    for (fAli = pair->fRead->aliList; fAli != NULL; fAli = fAli->next)
        if (fAli->tName == frag)
	    {
	    *retFali = fAli;
	    *retRali = rAli;
	    return frag;
	    }
    }
return NULL;
}


void checkLargeFragHits(struct pairInfo *pair, struct hash *finHash,
    struct hash *gsiHash, boolean *retMidLarge,
    boolean *retBothLarge, int *retSize, FILE *misses)
/* Check if fRead of pair hits in middle of a large fragment.
 * If so see if other end also hits. */
{
struct shortAli *fAli, *rAli;
char *midFragName = NULL;
char cloneName[128];


/* For the moment only process ones where have exactly one
 * alignment for each side of read. */
if (pair->fRead->aliList == NULL || pair->fRead->aliList->next != NULL 
   || pair->rRead->aliList == NULL || pair->rRead->aliList->next != NULL)
   {
   *retMidLarge = *retBothLarge = FALSE;
   return;
   }

*retMidLarge = *retBothLarge = FALSE;
for (fAli = pair->fRead->aliList; fAli != NULL; fAli = fAli->next)
    {
    char *frag = fAli->tName;
    if (fAli->tStart > 15000 && fAli->tEnd + 15000 <= fAli->tSize)
	{
	strcpy(cloneName, frag);
	chopSuffix(cloneName);
	if (finHash == NULL ||  hashLookup(finHash, cloneName))
	    {
	    *retMidLarge = TRUE;
	    midFragName = frag;
	    for (rAli = pair->rRead->aliList; rAli != NULL; rAli = rAli->next)
		{
		if (rAli->tName == frag)
		    {
		    int minPos, maxPos, pairSize;
		    struct groupSizeInfo *gsi;
		    int slot;

		    minPos = min(fAli->tStart, rAli->tStart);
		    maxPos = max(fAli->tEnd, rAli->tEnd);
		    pairSize = maxPos - minPos;
		    assert(pairSize > 0);

		    if (gsiHash != NULL)
			{
			gsi = hashMustFindVal(gsiHash, pair->project);
			if (gsi->measuredCount == 0)
			    gsi->measuredMin = gsi->measuredMax = pairSize;
			else
			    {
			    if (gsi->measuredMin > pairSize)
				gsi->measuredMin = pairSize;
			    if (gsi->measuredMax < pairSize)
				gsi->measuredMax = pairSize;
			    }
			gsi->measuredCount += 1;
			gsi->measuredTotal += pairSize;
			slot = pairSize/gsiSlotSize;
			if (slot >= gsiSlotCount)
			   slot = gsiSlotCount - 1;
			gsi->histogram[slot] += 1;
			if (gsi->guessedMin <= pairSize && pairSize <= gsi->guessedMax)
			    ++gsi->guessedCount;
			}

		    *retBothLarge = TRUE;
		    *retSize = pairSize;
		    }
		}
	    }
	}
    }
if (misses != NULL && midFragName != NULL && *retBothLarge == FALSE)
    {
    strcpy(cloneName, midFragName);
    chopSuffix(cloneName);
    fprintf(misses, "%s\t%s\t%s\n", 
        pair->fRead->name, pair->rRead->name, cloneName);
    }
}


void doPairStats(struct pairInfo *pairList, struct hash *finHash, 
    struct hash *gsiHash, char *mispairFile, 
    struct pairInfo **retMeasured, struct pairInfo **retOther)
/* Calculate and display some stats on the pairs and alignments. */
{
struct pairInfo *pair, *nextPair;
int pairCount = 0;
int isoCount = 0;
int noAliCount = 0;
int oneAliCount = 0;
int inFragCount = 0;
int joiningCount = 0;
int midCount = 0;
int midBoth = 0;
struct pairInfo *measured = NULL, *other = NULL;
FILE *misPairs = mustOpen(mispairFile, "w");

for (pair = pairList; pair != NULL; pair = nextPair)
    {
    nextPair = pair->next;
    ++pairCount;
    if (pair->fRead == NULL || pair->rRead == NULL)
        ++isoCount;
    else
        {
	if (pair->fRead->aliList == NULL && pair->rRead->aliList == NULL)
	    ++noAliCount;
	else if (pair->fRead->aliList == NULL || pair->rRead->aliList == NULL)
	    {
	    ++oneAliCount;
	    }
	else
	    {
	    struct shortAli *rAli, *fAli;
	    boolean largeHit, largeHitBoth;
	    int size;

	    checkLargeFragHits(pair, finHash, gsiHash,
		&largeHit, &largeHitBoth, &size, misPairs);
	    midCount += largeHit;
	    midBoth += largeHitBoth;
	    if (pairInFrag(pair, &fAli, &rAli))
		{
	        ++inFragCount;
		slAddHead(&measured, pair);
		pair = NULL;
		}
	    else
	       ++joiningCount;
	    }
	}
    if (pair != NULL)
        {
	slAddHead(&other, pair);
	}
    }
printf("Unpaired: %d\n", isoCount);
printf("Neither aligns: %d\n", noAliCount);
printf("Only one aligns: %d\n", oneAliCount);
printf("Both align to same frag: %d\n", inFragCount);
printf("Join fragments: %d\n", joiningCount);
printf("\n");
printf("Pairs where one end hits near middle of large frag: %d\n", midCount);
printf("Pairs where other end hits same frag: %d %5.2f%%\n",
     midBoth, (100.0)*(double)midBoth/(double)midCount);
fclose(misPairs);
*retMeasured = measured;
*retOther = other;
}

void gsiMeanAndVariance(struct pairInfo *pairList, struct groupSizeInfo *gsiList,
   struct hash *gsiHash, struct hash *finHash)
/* Calculate mean gsi position, and then scan through pairs list 
 * again to calculate variance. */
{
struct groupSizeInfo *gsi;
struct pairInfo *pair;
boolean largeHit, largeHitBoth;
int size;
int hitCount = 0, checkCount = 0;

/* Calculate means. */
for (gsi = gsiList; gsi != NULL; gsi = gsi->next)
    {
    if (gsi->measuredCount > 0)
	gsi->measuredMean = gsi->measuredTotal/gsi->measuredCount;
    gsi->variance = 0;
    }

/* Sum up variance. */
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (pair->fRead != NULL && pair->rRead != NULL)
	{
	++checkCount;
	checkLargeFragHits(pair, finHash, NULL, &largeHit, &largeHitBoth,
	   &size, NULL);
	if (largeHitBoth)
	   {
	   double dif;
	   gsi = hashMustFindVal(gsiHash, pair->project);
	   dif = (size - gsi->measuredMean);
	   gsi->variance += dif*dif;
	   ++hitCount;
	   }
	}
    }

/*  Finish calculating variance. */
for (gsi = gsiList; gsi != NULL; gsi = gsi->next)
    {
    if (gsi->measuredCount > 1)
	gsi->variance /= (gsi->measuredCount-1);
    }
}

struct shortAli *findShortAli(struct shortAli *list, char *tName)
/* Return matching short ali from list. */
{
struct shortAli *el;

for (el = list; el != NULL; el = el->next)
    {
    if (el->tName == tName)
        return el;
    }
errAbort("Couldn't find %s on list, aborting", tName);
return NULL;
}

void doMeasuredStats(struct pairInfo *list)
/* Measure distance between pairs on a common frag. */
{
int opCount = 0, sameCount = 0, totalCount = 0;
struct pairInfo *pair;
char *fragName;
struct shortAli *fAli, *rAli;

for (pair = list; pair != NULL; pair = pair->next)
    {
    pairInFrag(pair, &fAli, &rAli);
    if (fAli->strand == rAli->strand)
        ++sameCount;
    else
        ++opCount;
    ++totalCount;
    }
printf("Of pairs that hit same fragment:\n");
printf("Same strand %d (%5.2f%%)\n",
	sameCount, 100.0 * (double)sameCount/(double)totalCount);
printf("Opposite strands %d (%5.2f%%)\n",
	opCount, 100.0 * (double)opCount/(double)totalCount);
}

void savePairs(char *fileName, struct pairInfo *pairList, struct hash *gsiHash)
/* Save pairs to file. */
{
struct pairInfo *pair;
FILE *f = mustOpen(fileName, "w");
struct groupSizeInfo *gsi;

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (pair->fRead && pair->rRead)
        {
	gsi = hashMustFindVal(gsiHash, pair->project);
	fprintf(f, "%s\t%s\t%s\t%d\t%d\n",
	   pair->fRead->name, pair->rRead->name, pair->name,
	   gsi->guessedMin, gsi->guessedMax);
	}
    }
fclose(f);
}

struct groupSizeInfo *readSizes(char *fileName, struct hash *gsiHash)
/* Read in file of format:
 *     groupName guessedMin guessedMax
 * and save in hash and as list. */
{
struct groupSizeInfo *gsiList = NULL, *gsi;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[8];
struct hashEl *hel;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 3, wordCount);
    AllocVar(gsi);
    hel = hashAddUnique(gsiHash, words[0], gsi);
    gsi->name = hel->name;
    gsi->guessedMin = atoi(words[1]);
    gsi->guessedMax = atoi(words[2]);
    slAddHead(&gsiList, gsi);
    }

lineFileClose(&lf);
slReverse(&gsiList);
return gsiList;
}

double  gsiTight(struct groupSizeInfo *gsi, int maxDistance)
/* Return percentage w/in maxDistance size of ideal. */
{
int idealSlot = (gsi->measuredMean+gsiSlotSize/2)/gsiSlotSize;
int minSlot = idealSlot - maxDistance/gsiSlotSize;
int maxSlot = idealSlot + maxDistance/gsiSlotSize;
double total = 0;
int slot;

if (gsi->measuredCount == 0)
   return 0.0;
for (slot = minSlot; slot < maxSlot; ++slot)
    total += gsi->histogram[slot];
return 100.0 * total / gsi->measuredCount;
}

void showHistogram(struct groupSizeInfo *gsi)
/* Show size histogram. */
{
int maxCount = 0;
int slot, i;
int itemsPerX;
int firstSlot = -1, lastSlot = -1;
int count;
int xCount;

printf("Histogram of %s with %d samples\n", gsi->name, gsi->measuredCount);
for (slot = 0; slot < gsiSlotCount; ++slot)
    {
    if ((count = gsi->histogram[slot]) > maxCount)
        maxCount = gsi->histogram[slot];
    }
itemsPerX = maxCount/50 + 1;
for (slot = 0; slot < gsiSlotCount; ++slot)
    {
    if ((count = gsi->histogram[slot]) >= itemsPerX)
        {
	if (firstSlot < 0)
	   firstSlot = slot;
	lastSlot = slot;
	}
    }
printf("   %d samples per X\n", itemsPerX);
for (slot = firstSlot; slot <= lastSlot; ++slot)
    {
    if (slot == gsiSlotCount-1)
	printf("%4d +     : ", slot*gsiSlotSize);
    else
	printf("%4d - %4d: ", slot*gsiSlotSize, (slot+1)*gsiSlotSize);
    xCount = gsi->histogram[slot]/itemsPerX;
    for (i=0; i<xCount; ++i)
       printf("X");
    printf("\n");
    }
printf("\n");
}

void ppAnalyse(char *headersName, char *sizesName, char *pairsPsl, 
    char *finDir, char *pairOut, char *mispairOut)
/* ppAnalyse - Analyse paired plasmid reads. */
{
struct hash *readHash = newHash(21);
struct hash *pairHash = newHash(20);
struct hash *fragHash = newHash(17);
struct hash *finHash = NULL;
struct hash *gsiHash = newHash(8);
struct pairInfo *pairList = NULL, *measuredList, *pair;
struct readInfo *readList = NULL, *rd;
struct groupSizeInfo *gsiList = NULL, *gsi;
int aliCount;
int finCount;
int i;
struct slName *finList, *name;

finHash = newHash(14);
finList = listDir(finDir, "*.fa");
if ((finCount = slCount(finList)) == 0)
    errAbort("No fa files in %s\n", finDir);
printf("Got %d (finished) .fa files in %s\n", finCount, finDir);
for (name = finList; name != NULL; name = name->next)
    {
    chopSuffix(name->name);
    hashStore(finHash, name->name);
    }
#ifdef SOMETIMES
#endif /* SOMETIMES */

gsiList = readSizes(sizesName, gsiHash);
printf("Got %d groups\n", slCount(gsiList));

readHeaders(headersName, readHash, pairHash, &readList, &pairList);
slReverse(&readList);
slReverse(&pairList);
printf("Got %d reads in %d pairs\n", slCount(readList), slCount(pairList));


savePairs(pairOut, pairList, gsiHash);
printf("Saved pairs to %s\n", pairOut);

aliCount = readAlignments(pairsPsl, readHash, fragHash);
printf("Got %d alignments in %s\n", aliCount, pairsPsl);


doReadStats(readList, aliCount);
doPairStats(pairList, finHash, gsiHash, mispairOut, &measuredList, &pairList);
gsiMeanAndVariance(measuredList, gsiList, gsiHash, finHash);
printf("Alignment length stats:\n");
for (i=0; i<10; ++i)
    {
    printf("%3d - %4d :  %6d  %4.2f%%\n",
    	i*100, (i+1)*100, aliSizes[i], 100.0 * (double)aliSizes[i]/(double)aliCount);
    }
doMeasuredStats(measuredList);

for (gsi = gsiList; gsi != NULL; gsi = gsi->next)
    {
    if (gsi->measuredCount > 0)
	{
	printf("%s: mean %f, std %f, min %d, max %d, samples %d\n",
	    gsi->name, gsi->measuredMean, sqrt(gsi->variance),
	    gsi->measuredMin, gsi->measuredMax,
	    gsi->measuredCount);
	printf("   %4.2f%% within guessed range (%d-%d)\n", 
		100.0 * (double)gsi->guessedCount/(double)gsi->measuredCount,
		gsi->guessedMin, gsi->guessedMax);
	printf("   w/in 200 %4.2f%%, w/in 400 %4.2f%%,  w/in 800 %4.2f%%,  w/in 1600 %4.3f%%, w/in 3200 %4.2f%%\n\n",
	    gsiTight(gsi, 200), gsiTight(gsi, 400), gsiTight(gsi, 800), gsiTight(gsi, 1600), gsiTight(gsi, 3200) );
	showHistogram(gsi);
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 7)
    usage();
ppAnalyse(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
