/* Program to lift tracks that have nearly the same agp, but slightly
   different.  Initially designed for chr21 and chr22 which are starting
   to accumulate ticky tack changes. A fair bit of this code was stolen from
   hgLoadBed. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "agpFrag.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


struct bedStub
/* A line in a bed file with chromosome, start, end position parsed out. */
{
    struct bedStub *next;	/* Next in list. */
    char *chrom;                /* Chromosome . */
    unsigned chromStart;             /* Start position. */
    unsigned chromEnd;		/* End position. */
    char *line;                 /* Line. */
};

struct commonBlock
/* A Region of a chromsome that is the same between two assemblies. */
{
    struct commonBlock *next; /* Next in list. */
    char *chrom;              /* Name of chromosome, 'chr22' */
    unsigned chromStartA;          /* Start of block on version A of .agp */
    unsigned chromEndA;            /* End of block on version A of .agp */
    unsigned chromStartB;          /* Start of block on version B of .agp */
    unsigned chromEndB;            /* End of block on version B of .agp */
};

enum testResult
/* Enumerate test categories. */
{
    trNA,
    trPassed,
    trFailed,
    trMissing,
};

struct testPoint
/* A single point in the genome to use for testing. Idea is to create these
   randomly and see if they end up where they should in the genome via the .agp. */
{
    struct testPoint *next;   /* Next in list. */
    char *chrom;              /* Name of chromosome, 'chr22' */
    unsigned chromStart;          /* Start of block on version B of .agp */
    unsigned chromEnd;            /* End of block on version B of .agp */
    char *clone;              /* Name of clone. */
    unsigned chromStartA;          /* Start of block on version A of .agp */
    unsigned chromEndA;            /* End of block on version A of .agp */
    unsigned cloneStart;           /* Start in clone. */
    unsigned cloneEnd;             /* End in clone. */
    enum testResult result;   /* Result of lifting this point. */
};

FILE *diagnostics = NULL; /* Output file for some diagnostics. */
int cannotConvert = 0;    /* Keep track of how many get dropped. */
int canConvert = 0;       /* Keep track of how many we successfully convert. */
boolean testMode = FALSE; /* Are we in self-test mode or not? */

void usage()
{
errAbort("liftAgp -  Program to lift tracks that have nearly the same .agp file,\n"
         "but slightly different. Initially designed for chr21 and chr22 which\n"
         "are starting to accumulate ticky-tacky changes. Currently works for files\n"
	 "which have a bed like structure, i.e. chr, chrStart, chrEnd.\n"
	 "usage:\n\t"
	 "liftAgp <oldGenomeAgp> <newGenomeAgp> <oldBedFiles...>\n\n"
	 "There is also a special testing mode which is called like so:\n\t"
	 "liftAgp <oldGenomeAgp> <newGenomeAgp> -test\n" );
}

void commonBlockFree(struct commonBlock **pEl)
/* Free a single dynamically allocated commonBlock */
{
struct commonBlock *el;
if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freez(pEl);
}

void commonBlockFreeList(struct commonBlock **pList)
/* Free a list of dynamically allocated commonBlock's */
{
struct commonBlock *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    commonBlockFree(&el);
    }
*pList = NULL;
}

void bedStubFree(struct bedStub **pEl)
/* Free a single dynamically allocated bedStub */

{
struct bedStub *el;
if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->line);
freez(pEl);
}

void bedStubFreeList(struct bedStub **pList)
/* Free a list of dynamically allocated bedStub's */
{
struct bedStub *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bedStubFree(&el);
    }
*pList = NULL;
}


void testPointFree(struct testPoint **pEl)
/* Free a single dynamically allocated testPoint */

{
struct testPoint *el;
if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->clone);
freez(pEl);
}

void testPointFreeList(struct testPoint **pList)
/* Free a list of dynamically allocated testPoint's */
{
struct testPoint *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    testPointFree(&el);
    }
*pList = NULL;
}

int findBedSize(char *fileName)
/* Read first line of file and figure out how many words in it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[32];
int wordCount;
wordCount = lineFileChop(lf, words);
if (wordCount == 0)
    errAbort("%s appears to be empty", fileName);
lineFileClose(&lf);
return wordCount;
}

void loadOneBed(char *fileName, int bedSize, struct bedStub **pList)
/* Load one bed file.  Make sure all lines have bedSize fields.
 * Put results in *pList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line, *dupe;
int wordCount;
struct bedStub *bed;

printf("Reading %s\n", fileName);
while (lineFileNext(lf, &line, NULL))
    {
    dupe = cloneString(line);
    wordCount = chopLine(line, words);
    lineFileExpectWords(lf, bedSize, wordCount);
    AllocVar(bed);
    bed->chrom = cloneString(words[0]);
    bed->chromStart = lineFileNeedNum(lf, words, 1);
    bed->chromEnd = lineFileNeedNum(lf, words, 2);
    bed->line = dupe;
    slAddHead(pList, bed);
    }
lineFileClose(&lf);
}

void writeBedTab(char *fileName, struct bedStub *bedList, int bedSize)
/* Write out bed list to tab-separated file. */
{
struct bedStub *bed;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, wordCount;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    wordCount = chopLine(bed->line, words);
    fprintf(f, "%s\t%d\t%d", bed->chrom, bed->chromStart, bed->chromEnd);
    if (2 == wordCount-1)
	    fputc('\n', f);
	else
	    fputc('\t', f);
    for (i=3; i<wordCount; ++i)
        {
	fputs(words[i], f);
	if (i == wordCount-1)
	    fputc('\n', f);
	else
	    fputc('\t', f);
	}
    }
fclose(f);
}

int agpFragChrCmp(const void *va, const void *vb)
/* Compare to sort based on chrom, chromStart */
{
const struct agpFrag *a = *((struct agpFrag **)va);
const struct agpFrag *b = *((struct agpFrag **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

struct hash *hashAgpByClone(struct agpFrag *afList)
/* Load all of the agp's in a file into a hash indexed by the chromosome. */
{
struct hash *retHash = newHash(10);
struct agpFrag *af= NULL;
for(af = afList; af != NULL; af = af->next)
    {
    hashAddUnique(retHash, af->frag, af);
    }
return retHash;
}

boolean sameSequence(struct agpFrag *a, struct agpFrag *b)
/* same if clone name, start, end, strand are the same. */
{
if(a == NULL || b == NULL)
    return FALSE;
if(a->fragStart != b->fragStart || a->fragEnd != b->fragEnd)
    return FALSE;
if(differentString(a->frag, b->frag))
    return FALSE;
if(differentString(a->strand, b->strand))
    return FALSE;
return TRUE;
}

static boolean findOverlap(struct agpFrag *a, struct agpFrag *b,
		    unsigned int *commonStart, unsigned int *commonEnd)
/* Find where the agpFrags overlap */
{
unsigned int start=0, end=0;
assert(a && b);
assert(sameString(a->frag, b->frag));
assert(sameString(a->strand, b->strand));
/* if we don't overlap abort */
if(rangeIntersection(a->fragStart, a->fragEnd, b->fragStart, b->fragEnd) <= 0)
    return FALSE;

/* find start */
if(a->fragStart < b->fragStart)
    start = b->fragStart;
else
    start = a->fragStart;

/* find end */
if(a->fragEnd > b->fragEnd)
    end = b->fragEnd;
else
    end = a->fragEnd;

*commonStart = start;
*commonEnd = end;
return TRUE;
}

void findSubSeq(struct agpFrag *a, struct agpFrag *b)
/* Find where the common sub-sequence between a and b is. */
{
boolean foundOverlap;
unsigned int commonStart=0, commonEnd=0;
int startDiff = 0;
int endDiff =0;
assert(sameString(a->frag, b->frag));
foundOverlap = findOverlap(a, b, &commonStart, &commonEnd);
assert(foundOverlap);
startDiff = commonStart - a->fragStart;
endDiff = a->fragEnd  - commonEnd;
a->chromStart += startDiff;
a->chromEnd -= endDiff;
a->fragStart = commonStart;
a->fragEnd = commonEnd;
startDiff = commonStart - b->fragStart;
endDiff = b->fragEnd  - commonEnd;
b->chromStart += startDiff;
b->chromEnd -= endDiff;
b->fragStart = commonStart;
b->fragEnd = commonEnd;
}

boolean isSubSeq(struct agpFrag *a, struct agpFrag *b)
/* Is there a common sub-sequence between a and b? */
{
boolean answer;
unsigned int commonStart = 0, commonEnd = 0;
if(a==NULL || b == NULL)
    return FALSE;
if(differentString(a->frag, b->frag))
    return FALSE;
if(differentString(a->strand, b->strand))
    return FALSE;
if(findOverlap(a,b,&commonStart, &commonEnd))
    return TRUE;
return FALSE;
}

boolean noGap(struct agpFrag *a, struct agpFrag *b)
/* Return TRUE if there is no gap, FALSE if there is a gap. */
{
if(b->chromStart == a->chromEnd) // || b->chromStart == a->chromEnd -1 ) // Strange off by one value...
    return TRUE;
return FALSE;
}

void extendCommonBlock( struct agpFrag **oldFrag, struct agpFrag **newFrag,
			struct commonBlock *cb)
/* Extend the common block as long as new and old agps are the same. */
{
struct agpFrag *oldNext = NULL, *oldIt = NULL;
struct agpFrag *newNext = NULL, *newIt = NULL;
oldIt = *oldFrag;
newIt = *newFrag;
while(1)
    {
    oldNext = oldIt->next;
    newNext = newIt->next;
    /* if we're the same extend otherwise cap the common block and return. */
    if(sameSequence(oldNext, newNext) && noGap(oldIt, oldNext) && noGap(newIt, newNext))
	{
	oldIt = oldNext;
	newIt = newNext;
	}
    else
	{
	cb->chromEndA = oldIt->chromEnd;
	cb->chromEndB = newIt->chromEnd;
	break;
	}
    }
*oldFrag = oldNext;
*newFrag = newNext;
}

struct commonBlock *createCommonBlocks(struct agpFrag *oldFrag, struct agpFrag *newFrag)
/* Create a list of common blocks in the chromosomes. */
{
struct agpFrag *newIt, *oldIt;      /* Iterators for the lists. */
struct agpFrag *newMark, *oldMark;  /* Placeholders in the lists. */
struct commonBlock *cbList = NULL, *cb = NULL;
boolean blockSearch = 0;
boolean increment = 0;
newMark = newIt = newFrag;
oldMark = oldIt = oldFrag;

while(newIt != NULL && oldIt != NULL)
    {
    blockSearch = FALSE;
    /* Check to see if we can find a match on the new sequecne for the old. */
    for(newMark = newIt; newMark != NULL && !blockSearch; newMark = newMark->next)
	{
	if(sameSequence(newMark, oldIt))
	    {
	    blockSearch = TRUE;
	    newIt = newMark;
	    break;
	    }
	else if(isSubSeq(newIt, oldIt))
	    {
	    findSubSeq(newIt, oldIt);
	    blockSearch = TRUE;
	    break;
	    }
	}
    /* if we have a match extend it. */
    if(sameSequence(newIt, oldIt))
	{
	AllocVar(cb);
	cb->chrom = cloneString(oldIt->chrom);
	cb->chromStartA = oldIt->chromStart;
	cb->chromStartB = newIt->chromStart;
	extendCommonBlock(&oldIt, &newIt, cb);
	slAddHead(&cbList, cb);
	}

    if(!blockSearch && newIt != NULL && oldIt != NULL)
	{
	/* Couldn't find a match for these guys try the next pair. */
	newIt = newIt->next;
	oldIt = oldIt->next;
	}
    }
return cbList;
}

unsigned int calcAgpSize(struct agpFrag *agpList)
{
struct agpFrag *agp = NULL;
unsigned int size =0;
for(agp = agpList; agp != NULL; agp = agp->next)
    {
    size += agp->chromEnd - agp->chromStart;
    }
return size;
}

unsigned int calcCbSize(struct commonBlock *cbList)
{
struct commonBlock *cb = NULL;
unsigned int size = 0;
for(cb = cbList; cb != NULL; cb = cb->next)
    {
    size += cb->chromEndA - cb->chromStartA;
    }
return size;
}

struct commonBlock *findCommonBlockForBed(struct bedStub *bs, struct commonBlock *cbList)
/* just a linear search for a block which contains the bedStub, return NULL if not
   found. */
{
struct commonBlock *cb = NULL;
for(cb = cbList; cb != NULL; cb = cb->next)
    {
    if(sameString(cb->chrom, bs->chrom) &&
       cb->chromStartA <= bs->chromStart &&
       cb->chromEndA > bs->chromEnd)
	{
	return cb;
	}
    }
return NULL;
}

boolean convertBedStub(void *p, struct commonBlock *cbList)
/* Convert a bedStub to clone coordinates via the old agp and then
   from clone coordinates in the newAgp to new chrom coordinates */
{
struct commonBlock *cb = NULL;
struct bedStub *bs = p;
cb = findCommonBlockForBed(bs, cbList);
if(cb == NULL)
    {
    if(diagnostics != NULL)
	fprintf(diagnostics, "Can't convert %s:%u-%u\n", bs->chrom, bs->chromStart, bs->chromEnd);
    cannotConvert++;
    if(testMode)
	{
	struct testPoint *tp = p;
	testPointFree(&tp);
	}
    else
	bedStubFree(&bs);
    return FALSE;
    }
else
    {
    int diff = cb->chromStartB - cb->chromStartA;
    canConvert++;
    bs->chromStart += diff;
    bs->chromEnd += diff;
    }
return TRUE;
}

void commonBlockTabOut(FILE *out, struct commonBlock *cbList)
{
struct commonBlock *cb = NULL;
for(cb = cbList; cb != NULL; cb = cb->next)
    {
    fprintf(out, "%s\t%u\t%u\t%u\t%u\t%u\t%u\n", cb->chrom, cb->chromStartA, cb->chromEndA,
	    cb->chromEndA - cb->chromStartA,
	    cb->chromStartB, cb->chromEndB,
	    cb->chromEndB - cb->chromStartB);
    }
}

void *convertBeds(void *list, struct commonBlock *cbList)
/* convert a list of bed Stubs using the conversion table created. */
{
struct bedStub *bsList = list;
struct bedStub *bs = NULL, *bsNext;
struct bedStub *convertedList = NULL;
int bedCount = 0;
boolean converted = FALSE;
warn("Converting beds.");
for(bs = bsList; bs != NULL; bs = bsNext)
    {
    /* litte UI stuff to let the user know we're making progress... */
    if(bedCount % 1000 == 0)
	{
	fprintf(stderr, ".");
	fflush(stderr);
	}
    bedCount++;

    /* Actual conversion */
    bsNext = bs->next;
    converted = convertBedStub(bs, cbList);
    if(converted)
	{
	slAddHead(&convertedList, bs);
	}
    else
	{
	bs = NULL;
	}
    }
return convertedList;
}

void liftAgp(char *oldAgpFile, char *newAgpFile, int numFiles, char *oldBedFiles[])
/* Convert oldBedFile to clone space via oldAgpFile then from clone space to new
   assembly via new .agp file. */
{
struct bedStub *bsList = NULL, *bs = NULL, *bsNext = NULL;
struct bedStub *convertedList = NULL;
struct dyString *bedFile = newDyString(1024);
int bedCount = 0;
int convertedBedCount = 0;
int i=0;
unsigned int dnaSize = 0;
unsigned int dnaLifted = 0;
struct agpFrag *newFrag = NULL, *oldFrag = NULL, *tmpFrag = NULL;
struct commonBlock *cbList = NULL;


/* Load the agp fragments. */
warn("Loading oldAgp from %s", oldAgpFile);
oldFrag = agpFragLoadAllNotGaps(oldAgpFile);
for (tmpFrag=oldFrag;  tmpFrag != NULL;  tmpFrag=tmpFrag->next)
    {
    // file is 1-based but agpFragLoad() now assumes 0-based:
    tmpFrag->chromStart -= 1;
    tmpFrag->fragStart  -= 1;
    }
slSort(&oldFrag, agpFragChrCmp);
warn("Loading newAgp from %s", newAgpFile);
newFrag = agpFragLoadAllNotGaps(newAgpFile);
for (tmpFrag=newFrag;  tmpFrag != NULL;  tmpFrag=tmpFrag->next)
    {
    // file is 1-based but agpFragLoad() now assumes 0-based:
    tmpFrag->chromStart -= 1;
    tmpFrag->fragStart  -= 1;
    }
slSort(&newFrag, agpFragChrCmp);
dnaSize = calcAgpSize(newFrag);

/* Find the common blocks between the two agps. */
warn("Creating conversion table.\n");
cbList = createCommonBlocks(oldFrag, newFrag);
dnaLifted = calcCbSize(cbList);
warn("lifted %u of %u bases from old agp (%4.2f%%) in %d blocks.", dnaLifted, dnaSize, (100*(double)dnaLifted/dnaSize), slCount(cbList));
if(diagnostics != NULL)
    commonBlockTabOut(diagnostics, cbList);

/* Do the conversion. */
for(i=0; i<numFiles; i++)
    {
    int bedSize = findBedSize(oldBedFiles[i]);
    dyStringClear(bedFile);
    dyStringPrintf(bedFile, "%s.lft", oldBedFiles[i]);
    /* Load the beds into memory, (I should probably do this one at a time.) */
    warn("Loading beds from %s", oldBedFiles[i]);
    loadOneBed(oldBedFiles[i], bedSize, &bsList);
    bedCount = slCount(bsList);
    convertedList = convertBeds(bsList, cbList);
    convertedBedCount = slCount(convertedList);
    warn(", Converted %d of %d beds. %4.2f%%\n", convertedBedCount, bedCount,
	 (100*(float)convertedBedCount/(float)bedCount));
    warn("Writing converted beds to %s", bedFile->string);

    /* Write out the converted beds. */
    writeBedTab(bedFile->string, convertedList, bedSize);
    bedStubFreeList(&convertedList);
    bsList = NULL;
    }
/* Cleanup. */
agpFragFreeList(&newFrag);
agpFragFreeList(&oldFrag);

commonBlockFreeList(&cbList);
warn("Done.");
}

/* ---------------- Begin code for testing self. ---------------------- */

struct testPoint *createPoint(struct agpFrag *agp, int cloneCoordinate)
/* create a test point in the clone, determined by the cloneCoordinate */
{
struct testPoint *tp = NULL;
AllocVar(tp);
tp->chrom = cloneString(agp->chrom);
tp->clone = cloneString(agp->frag);
assert(cloneCoordinate >= agp->fragStart && cloneCoordinate < agp->fragEnd);
tp->cloneStart = cloneCoordinate;
tp->cloneEnd = cloneCoordinate+1;
tp->chromStart = tp->chromStartA = agp->chromStart - agp->fragStart + tp->cloneStart;
tp->chromEnd = tp->chromEndA = agp->chromStart - agp->fragStart + tp->cloneEnd;
return tp;
}

unsigned getRandCoord(unsigned start, unsigned end)
/* return a random nubmer uniformly distributed between start and end. */
{
unsigned ran = rand();
unsigned size = end - start;
assert(size >= 0);
ran = ran % size;
ran = start + ran;
return ran;
}

struct testPoint *generateRandomTestPoints(struct agpFrag *agpList, int pointsPerClone)
/* create a bunch of random points on each clone. */
{
struct testPoint *tp = NULL, *tpList = NULL;
struct agpFrag *agp = NULL;
int tpCount;
assert(pointsPerClone >= 2);
srand(1);
for(agp = agpList; agp != NULL; agp = agp->next)
    {
    tp = createPoint(agp, agp->fragStart);
    slAddHead(&tpList, tp);
    tp = createPoint(agp, agp->fragEnd-1);
    slAddHead(&tpList, tp);
    for(tpCount = 2; tpCount < pointsPerClone; tpCount++)
	{
	int coordinate = getRandCoord(agp->fragStart, agp->fragEnd);
	if(diagnostics != NULL)
	    fprintf(diagnostics, "%u\n", coordinate - agp->fragStart + agp->chromStart);
	tp = createPoint(agp, coordinate);
	slAddHead(&tpList, tp);
	}
    }
return tpList;
}

void checkConversion(struct testPoint *tpList, struct hash *oldAgpHash, struct hash *newAgpHash,
		     int *correct, int *wrong, struct commonBlock *cbList)
/* Loop through our converted coordinates and see which are correct and which are wrong. */
{
struct testPoint *tp = NULL;
struct agpFrag *newAgp = NULL, *oldAgp = NULL;
unsigned start=0, end=0;
for(tp = tpList; tp != NULL; tp = tp->next)
    {
    newAgp = hashFindVal(newAgpHash, tp->clone);
    oldAgp = hashFindVal(oldAgpHash, tp->clone);
    if(newAgp != NULL && oldAgp != NULL)
	{
	start = newAgp->chromStart - newAgp->fragStart + tp->cloneStart;
	end= newAgp->chromStart - newAgp->fragStart + tp->cloneEnd;
	if(start == tp->chromStart && end == tp->chromEnd)
	    {
	    tp->result = trPassed;
	    (*correct)++;
	    }
	else
	    {
	    tp->result = trFailed;
	    (*wrong)++;
	    }
	}
    else
	{
	errAbort("Can't find %s", tp->clone);
	}
    }
}

void testLiftAgp(char *oldAgpFile, char *newAgpFile)
/* do a test lift of artificially generated track. */
{
struct testPoint *tp=NULL, *tpList = NULL;
struct testPoint *convertedList = NULL;
struct commonBlock *cbList = NULL;
int tpCount = 0;
int passedTpCount = 0;
int numTpPerClone = 100;
unsigned int dnaSize = 0;
unsigned int dnaLifted = 0;
struct hash *oldAgpHash = NULL;
struct hash *newAgpHash = NULL;
struct agpFrag *newFrag = NULL, *oldFrag = NULL, *tmpFrag = NULL;
int correct =0, wrong=0;

/* Loading and hashing agps. */
warn("Loading oldAgp from %s", oldAgpFile);
oldFrag = agpFragLoadAllNotGaps(oldAgpFile);
slSort(&oldFrag, agpFragChrCmp);
warn("Loading newAgp from %s", newAgpFile);
newFrag = agpFragLoadAllNotGaps(newAgpFile);
slSort(&newFrag, agpFragChrCmp);
oldAgpHash = hashAgpByClone(oldFrag);
newAgpHash = hashAgpByClone(newFrag);

/* Create the common blocks. */
warn("Creating conversion table.\n");
dnaSize = calcAgpSize(newFrag);
cbList = createCommonBlocks(oldFrag, newFrag);
dnaLifted = calcCbSize(cbList);
warn("lifted %u of %u bases from old agp (%4.2f%%) in %d blocks.\n", dnaLifted, dnaSize, (100*(double)dnaLifted/dnaSize), slCount(cbList));

/* Generate some random data points. */
tpList = generateRandomTestPoints(oldFrag, numTpPerClone);
tpCount = slCount(tpList);

/* Convert and check lifts. */
convertedList = convertBeds(tpList, cbList);
checkConversion(convertedList, oldAgpHash, newAgpHash, &correct, &wrong, cbList);

/* Do some cleanup and reporting. */
agpFragFreeList(&newFrag);
agpFragFreeList(&oldFrag);
testPointFreeList(&convertedList);
commonBlockFreeList(&cbList);
hashFree(&newAgpHash);
hashFree(&oldAgpHash);
warn("%d converted, %d correct, %d wrong %d total.\n", canConvert, correct, wrong, tpCount);
warn("%4.2f%% correct and %4.2f%% converted.", 100*((float)(correct)/(float)canConvert), 100*((float)canConvert/(float)tpCount));
}

/* ---------------- End code for testing self. ---------------------- */

int main(int argc, char *argv[])
{
if(argc <= 3)
    usage();
cgiSpoof(&argc, argv);
testMode = cgiBoolean("test");
if(cgiBoolean("diagnostics"))
    diagnostics = mustOpen("liftAgp.diagnostics", "w");
if(testMode)
    testLiftAgp(argv[1], argv[2]);
else
    liftAgp(argv[1], argv[2], argc-3, argv+3);
if(cgiBoolean("diagnostics"))
    carefulClose(&diagnostics);
return 0;
}
