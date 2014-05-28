/* Reach in cage data files from Riken and process them into two top level tracks.
   One track for + strand and another for the - strand. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "bed.h"
#include "dlist.h"
#include "genePred.h"
#include "linefile.h"
#include "hash.h"
#include "verbose.h"
#include "options.h"

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"input", OPTION_STRING},
    {"forward", OPTION_STRING},
    {"reverse", OPTION_STRING},
    {"stats-only", OPTION_BOOLEAN},
    {"alpha", OPTION_FLOAT},
    {"xmax", OPTION_INT},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Bed file with read alignments on genome.",
    "Output file for bedgraph on forward (+) strand",
    "Output file for bedgraph on reverse (-) strand",
    "Just generate counts and output for pulling data into R and fitting alpha",
    "Alpha coefficient in power law for calculating posterior probability",
    "Maximum value of read counts that fits the power law",
};


static char *chroms[] = {"chr1","chr2", "chr3","chr4","chr5","chr6","chr7","chr8","chr9",
			 "chr10","chr11","chr12","chr13","chr14","chr15","chr16",
			 "chr17","chr18","chr19","chr20","chr21","chr22",
			 "chrM","chrX","chrY"};

double alpha = 0;
int xmax = 0;
boolean statsOnly = FALSE;
//static int * sample = NULL;
//static int numSample = 0;
//static double probCach[5] = {-1.0, -1.0, -1.0, -1.0, -1.0};

struct bedFloat
/* Browser extensible data */
    {
    struct bedFloat *next;  /* Next in singly linked list. */
    char *chrom;	/* Human chromosome or FPC contig */
    unsigned chromStart;	/* Start position in chromosome */
    unsigned chromEnd;	/* End position in chromosome */
    char *name;	/* Name of item */
    char strand;
    /* The following items are not loaded by   the bedLoad routines. */
    float score; /* Score - 0-1000 */
    int itemRgb;
};

void printPointSummary(struct hash *hash, FILE *forward, FILE *reverse) 
{
struct hashEl *hel, *helList = hashElListHash(hash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct bed *bedList = hel->val;
    int max = 0; 
    struct bed *bed = NULL;
    for(bed = bedList; bed != NULL; bed = bed->next) 
	{
	max = max(bed->score, max);
	}
    double s = max;
    s = logBase2(s+1) * 10;
    bedList->score = round(s) - 9;
    bedList->score = max(0,bedList->score);
    if(bedList->strand[0] == '+') 
	{
	fprintf(forward, "%s\t%d\t%d\t%d\n", bedList->chrom, bedList->chromStart, bedList->chromEnd, bedList->score);
	}
    else if(bedList->strand[0] == '-') 
	{
	fprintf(reverse, "%s\t%d\t%d\t%d\n", bedList->chrom, bedList->chromStart, bedList->chromEnd, bedList->score);
	}
    else 
	errAbort("*Error* - Don't recongize strand: %c\n", bed->strand[0]);
    }
hashElFreeList(&helList);
}

/* static int intCmp(const void *va, const void *vb) */
/* /\* Compare function to sort array of ints. *\/ */
/* { */
/* const int *a = va; */
/* const int *b = vb; */
/* int diff = *a - *b; */
/* if (diff < 0) */
/*     return -1; */
/* else if (diff > 0) */
/*     return 1; */
/* else */
/*     return 0; */
/* } */

/* void intSort(int count, int *array) */
/* /\* Sort an array of ints. *\/ */
/* { */
/* if (count > 1) */
/* qsort(array, count, sizeof(array[0]), intCmp); */
/* } */

/* void readDist(char *bedFile) { */
/* struct bed *bedList = bedLoadAll(bedFile); */
/* numSample = slCount(bedList); */
/* sample = needMem(sizeof(int) * numSample); */
/* struct bed *bed = NULL; */
/* int count = 0; */
/* for(bed = bedList; bed != NULL; bed = bed->next)  */
/*     { */
/*     sample[count++] = bed->score; */
/*     } */
/* intSort(numSample, sample); */
/* } */

double probVal(int val) 
{
if(val> xmax) 
    val = xmax;
double p = pow((val*1.0)/xmax, alpha);
return p;
}

/* double probVal(int val) { */

/* int startIx = numSample/2; */
/* double maxIterations = 1000000; */
/* double prob = -1.0; */
/* int lastStart = 0; */
/* if(val < 5 && probCach[val] >= 0)  */
/*     return probCach[val]; */
/* while(1)  */
/*     { */
/*     if(val == sample[startIx] || startIx == 0 || startIx == numSample - 1 || (val > sample[startIx] && val < sample[lastStart]))  */
/* 	{ */
/* 	while(startIx > 0 && sample[startIx] >= val)  */
/* 	    startIx--; */
/* 	prob = startIx * 1.0 / numSample; */
/* 	break; */
/* 	} */
/*     else if(val < sample[startIx] && startIx > 0)  */
/* 	{ */
/* 	int newStart = startIx / 2; */
/* 	if(maxIterations-- < 0)  */
/* 	    { */
/* 	    warn("Shouldn't get 0 iterations!"); */
/* 	    } */
/* 	lastStart = startIx; */
/* 	startIx = newStart; */
/* 	} */
/*     else if(val > sample[startIx] && startIx < numSample - 1) */
/* 	{ */
/* 	int newStart = (numSample - startIx) / 2 + startIx; */
/* 	if(maxIterations-- < 0)  */
/* 	    { */
/* 	    warn("Shouldn't get 0 iterations!"); */
/* 	    } */
/* 	lastStart = startIx; */
/* 	startIx = newStart; */
/* 	} */

/*     else  */
/* 	{ */
/* 	errAbort("Shouldn't have gotten here...."); */
/* 	} */
/*     } */
/* if(val < 5)  */
/*     probCach[val] = prob; */
/* return prob; */
/* } */



void doChromosome(char *bedFile, char *chrom, FILE *forward, FILE *reverse) 
{
struct hash *hash = newHash(12);
char keyBuf[256];
char *line = NULL;
/* Load the hashes from the bed file */
struct lineFile *bedLf = lineFileOpen(bedFile, TRUE);
while(lineFileNextReal(bedLf, &line)) 
    {
    char *row[12];
    int numCols = chopByWhite(line, row, ArraySize(row));
    if (numCols < 4)
	errAbort("bed must have at least 4 columns");
    if(differentString(row[0],chrom)) 
	continue;
    struct bed *bed = bedLoadN(row, numCols);
    safef(keyBuf, ArraySize(keyBuf), "%s:%c:%d-%d", bed->chrom, bed->strand[0], bed->chromStart, bed->chromEnd);
    struct hashEl *element = hashLookup(hash, keyBuf);
    if(element == NULL) 
	hashAdd(hash, keyBuf, bed);
    else 
	slSafeAddHead(&element->val, bed);        
    }
lineFileClose(&bedLf);

printPointSummary(hash, forward, reverse);
hashFreeWithVals(&hash, bedFreeList);
}

boolean differentCoord(struct bed *b1, struct bed *b2) 
{
if((b1 == NULL || b2 == NULL) && (b1 != b2))
    return FALSE;
boolean same = sameString(b1->chrom, b2->chrom);
same &= b1->chromStart == b2->chromStart;
same &= b1->chromEnd == b2->chromEnd;
same &= b1->strand[0] == b2->strand[0];
return !same;
}

struct bed *readBedList(char *bedFile, char *chrom) 
{
char *line = NULL;
/* Load the hashes from the bed file */
struct lineFile *bedLf = lineFileOpen(bedFile, TRUE);
struct bed *bedList = NULL;
while(lineFileNextReal(bedLf, &line)) 
    {
    char *row[12];
    int numCols = chopByWhite(line, row, ArraySize(row));
    if (numCols < 4)
	errAbort("bed must have at least 4 columns");
    if(differentString(row[0],chrom)) 
	continue;
    struct bed *bed = bedLoadN(row, numCols);
    slAddHead(&bedList, bed);
    }
lineFileClose(&bedLf);
slSort(&bedList, bedCmpChromStrandStart);
return bedList;
}


struct dlList *makeDlList(struct bed *bedList) 
{
struct dlList *dlList = newDlList();
struct slRef * currentList = NULL;
struct dlNode *currentNode = NULL;
struct bed *head = bedList;
while(head != NULL) 
    {
    if(currentList == NULL || differentCoord(head, currentList->val)) 
	{
	struct dlNode *node = needMem(sizeof(struct dlNode));
	// Save our node and list
	if(currentNode != NULL) 
	    {
	    currentNode->val = currentList;
	    dlAddTail(dlList, currentNode);
	    }
	currentList = needMem(sizeof(struct slRef));
	currentList->val = head;
	currentNode = node;
	}
    else 
	{
	struct slRef *newRef = needMem(sizeof(struct slRef));
	newRef->val = head;
	slAddHead(&currentList, newRef);
	}
    head = head->next;
    }
currentNode->val = currentList;
dlAddTail(dlList, currentNode);
return dlList;
}

void sumListCount(struct slRef *refList, double *sum, int *count) 
{
struct slRef *ref = NULL;
double localSum = 0;
int localCount = 0;
for(ref = refList; ref != NULL; ref = ref->next) 
    {
    struct bed *b = ref->val;
    localSum += b->score;
    localCount++;
    }
//localSum = localSum / localCount;
(*sum) += localSum;
(*count) += localCount;
}

void sumAveList(struct slRef *refList, double *sum, int *count) 
{
struct slRef *ref = NULL;
double localSum = 0;
int localCount = 0;
for(ref = refList; ref != NULL; ref = ref->next) 
    {
    struct bed *b = ref->val;
    localSum += b->score;
    localCount++;
    }
localSum = localSum / localCount;
(*sum) += localSum;
(*count)++;
}

struct bedFloat *calculateWindow(struct dlList *headList, int window) 
{
struct bedFloat *bfList = NULL;
struct dlNode *current = NULL;
for(current = headList->head; !dlEnd(current); current= current->next) 
    {
    double sum = 0;
    int count = 0;
    sumAveList((struct slRef*)current->val, &sum, &count);
    struct bed *targetBed = ((struct bed *)((struct slRef *)current->val)->val);
    int targetStart = targetBed->chromStart;
    int targetEnd = targetBed->chromEnd;
    struct dlNode *prev = current->prev;
    int nonZero = 0;
    while(prev != NULL && !dlStart(prev)) 
	{
	struct bed *bList = ((struct bed *)((struct slRef *)prev->val)->val);
	if(abs(bList->chromStart - targetStart) > window)
	    {
	    break;
	    }
	nonZero++;
	sumAveList(prev->val, &sum, &count);
	prev = prev->prev;
	}
    struct dlNode *next = current->next;
    while(next != NULL && !dlEnd(next)) 
	{
	struct bed *bList = ((struct bed *)((struct slRef *)next->val)->val);
	if(abs(bList->chromEnd - targetEnd) > window)
	    {
	    break;
	    }
	nonZero++;
	sumAveList(next->val, &sum, &count);
	next = next->next;
	}
    int expected = 2*window+1;
    count += expected - nonZero;
    struct bedFloat *bf = needMem(sizeof(struct bedFloat));
    bf->chrom = cloneString(targetBed->chrom);
    bf->chromStart = targetBed->chromStart;
    bf->chromEnd = targetBed->chromEnd;
    bf->strand = targetBed->strand[0];
    bf->score = sum/count;
    bf->name = cloneString(targetBed->name);
    slAddHead(&bfList, bf);
    }
return bfList;
}

struct bedFloat *calculateWindowCounts(struct dlList *headList, int window) 
{
struct bedFloat *bfList = NULL;
struct dlNode *current = NULL;
for(current = headList->head; !dlEnd(current); current= current->next) 
    {
    double sum = 0;
    int count = 0;
    sumListCount((struct slRef*)current->val, &sum, &count);
    struct bed *targetBed = ((struct bed *)((struct slRef *)current->val)->val);
    int targetStart = targetBed->chromStart;
    int targetEnd = targetBed->chromEnd;
    struct dlNode *prev = current->prev;
    int nonZero = 0;
    while(prev != NULL && !dlStart(prev)) 
	{
	struct bed *bList = ((struct bed *)((struct slRef *)prev->val)->val);
	if(abs(bList->chromStart - targetStart) > window)
	    {
	    break;
	    }
	nonZero++;
	sumListCount(prev->val, &sum, &count);
	prev = prev->prev;
	}
    struct dlNode *next = current->next;
    while(next != NULL && !dlEnd(next)) 
	{
	struct bed *bList = ((struct bed *)((struct slRef *)next->val)->val);
	if(abs(bList->chromEnd - targetEnd) > window)
	    {
	    break;
	    }
	nonZero++;
	sumListCount(next->val, &sum, &count);
	next = next->next;
	}
    struct bedFloat *bf = needMem(sizeof(struct bedFloat));
    bf->chrom = cloneString(targetBed->chrom);
    bf->chromStart = targetBed->chromStart;
    bf->chromEnd = targetBed->chromEnd;
    bf->strand = targetBed->strand[0];
    bf->score = round(sum);
    char buff[256];
    bf->itemRgb = count;
    safef(buff, sizeof(buff), "%d", count);
    bf->name = cloneString(buff);
    slAddHead(&bfList, bf);
    }
return bfList;
}

void doChromosomeWindow(char *bedFile, char *chrom, FILE *forward, FILE *reverse) 
{
struct bed *bedList = readBedList(bedFile, chrom);
struct dlList *dlList = makeDlList(bedList);
struct bedFloat *bFloatList = calculateWindowCounts(dlList, 10);
struct bedFloat *bFloat = NULL;

for(bFloat = bFloatList; bFloat != NULL; bFloat= bFloat->next) 
    {
    double d = bFloat->score;
    if(!statsOnly) 
	d = probVal(bFloat->score);
    if(bFloat->strand == '+') 
	{
	fprintf(forward, "%s\t%d\t%d\t%.6f\n", bFloat->chrom, bFloat->chromStart, bFloat->chromEnd, d);
//	fprintf(forward, "%s\t%d\t%d\t%d\t%.4f\t%d\t%.6f\n", bFloat->chrom, bFloat->chromStart, bFloat->chromEnd, bFloat->itemRgb, bFloat->score,round(d*1000), d);
	}
    else if(bFloat->strand == '-') 
	{
	fprintf(reverse, "%s\t%d\t%d\t%.6f\n", bFloat->chrom, bFloat->chromStart, bFloat->chromEnd,  d);
//	fprintf(reverse, "%s\t%d\t%d\t%d\t%.4f\t%d\t%.6f\n", bFloat->chrom, bFloat->chromStart, bFloat->chromEnd, bFloat->itemRgb, bFloat->score,round(d*1000), d);
//	fprintf(reverse, "%s\t%d\t%d\t%d\n", bFloat->chrom, bFloat->chromStart, bFloat->chromEnd, round(d*1000));
//	fprintf(reverse, "%s\t%d\t%d\t%d\t%d\n", bFloat->chrom, bFloat->chromStart, bFloat->chromEnd, round(bFloat->score), round(d*100));
	}
    else 
	errAbort("Don't recognize strand for bed: %s:%d-%d\n", bFloat->chrom, bFloat->chromStart, bFloat->chromEnd);
    }
}

void cageToSingleTrack(char *infile, char *forwardOut, char *reverseOut) 
{

FILE *forward = mustOpen(forwardOut, "w");
FILE *reverse = mustOpen(reverseOut, "w");
int chrIx = 0;
for(chrIx = 0; chrIx < ArraySize(chroms); chrIx++) 
    {
    verbose(1, "Doing chromosome: %s\n", chroms[chrIx]);
    doChromosomeWindow(infile, chroms[chrIx], forward, reverse);
    }
verbose(1, "Finished\n");
}

void usage()
/* Introduce ourselves. */
{
int i = 0;
warn("cageSingleTrack - Generate bedgraph statistics and posterior probability\n"
     "under an assumption of power law distribution."
     "usage:\n"
     "   cageSingleTrack --input all.bed --forward forward.bgraph --reverse reverse.bgraph --alpha 1.067001 --xmax 198\n"
     "options:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "   -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort(" ");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (optionExists("help"))
    usage();

//readDist(argv[4]);
if(optionExists("alpha")) 
    alpha = optionFloat("alpha", 1.067001) + 1.0;
if(optionExists("xmax")) 
    xmax = optionInt("xmax",198);
if(optionExists("stats-only")) 
    statsOnly = TRUE;
char * input = optionVal("input", "");
char * forward = optionVal("forward","");
char * reverse = optionVal("reverse","");
verbose(1,"Alpha is: %.6f Xmax is: %d\n", alpha, xmax);
if(!fileExists(input)) 
    usage();
cageToSingleTrack(input, forward, reverse);
return 0;
}
