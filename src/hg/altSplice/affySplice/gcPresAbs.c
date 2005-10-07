/* gcPresAbs.c - Calculate probability of expression in an mrna
   sample given a cel file of results. Use empiracal rank within
   a gc population to estimate probability and fisher's method
   of combining probabilities to calculate probe set probability.
*/
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "options.h"
#include "dystring.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_cdf.h>


struct row1lq
/* A single row from a affymetrix 1lq file. Also used for 
   storing values and probability of expression. */
{
    struct row1lq *next;    /* Next in list. */
    char *psName;           /* Name of probe set.*/
    char *seq;              /* Sequence of probe, in weird affy format. */
    int x;                  /* X coordinate from affy cel file. */
    int y;                  /* Y coordinate from affy cel file. */
    int gcCount;            /* Number of g's and c's in sequence. */
    int repCount;           /* Number of experiments we're looking at. */
    double *intensity;      /* Intensities for each experiment. */
    double *pVal;           /* Pvals for each experiment. */
};

struct psProb 
/* Probability that a probe set is expressed. */
{
    struct psProb *next; /* Next in list. */
    char psName[128];    /* Name of probe set. */
    double pVal;         /* P-Value of expression. */
};

struct replicateMap 
/* Map the replicates to eachother. */
{
    struct replicateMap *next;  /* Next in list. */
    char *rootName;             /* Root name of replicates. */
    int repCount;               /* Number of replicates. */
    int *repIndexes;            /* Indexes into matrix columns. */
};

struct row1lq **gcBins = NULL;     /* Bins of lists of row1lqs for different amounts of GCs. */
struct row1lq *badBin = NULL;
struct hash *probeSetHash = NULL;  /* Hash of slRefs for each probe set where slRef->val is a row1lq. */
struct hash *killHash = NULL;      /* Hash of probes to be ignored. */
struct hash *outputValsHash= NULL; /* Hash of probe sets to output values for. */
FILE *outputPsSetSeqs = NULL;      /* Output probe sets and sequences used here. */
FILE *outputIntenVals = NULL;       /* File to output intensity for individual values to. */
FILE *outputPvals = NULL;          /* File to output pvals for individual to. */
int minGcBin = 0;                  /* Minimum GC bin. */
int maxGcBin = 0;                  /* Maximum GC bin. */
struct psProb *probePVals = NULL;  /* List of psProbe structs with pVals filled in. */
int minProbeNum = 3;               /* Ignore probe sets that have less than this many probes. */
char *placeHolder = "dummy";       /* Hold a place in a hash. */
boolean combineCols = FALSE;       /* Combine columns that are the same samples? */
int repToSort = 0;                 /* What replicate to sort the gc bins by. */
int maxReplicates = 0;             /* What are the maximum number of replicates. */
int sampleCount = 0;               /* Number of samples. */
int numBadProbes = 0;              /* Number of bad probes counted. */
FILE *gCountsFile = NULL;              /* File to output 'G' counts per probe to. */
FILE *gcCountFile = NULL;             /* File to output 'G' + 'C' counts per probe to. */
FILE *cCountsFile = NULL;              /* File to output 'C' counts per probe. */
boolean doMedianComb = FALSE;          /* Do a median rather than Fisher's combination. */
boolean doCombo = FALSE; /* Do a combination of median and fisher. */

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"1lqFile", OPTION_STRING},
    {"outFile", OPTION_STRING},
    {"badProbes", OPTION_STRING},
    {"combineReplicates", OPTION_BOOLEAN},
    {"outputDuplicates", OPTION_BOOLEAN},
    {"testVals", OPTION_BOOLEAN},
    {"countPrefix", OPTION_STRING},
    {"doMedianComb", OPTION_BOOLEAN},
    {"testMedian", OPTION_BOOLEAN},
    {"doCombo", OPTION_BOOLEAN},
    {"outputVals", OPTION_STRING},
    {"outputValsList", OPTION_STRING},
    {"outputPsSetSeqs", OPTION_STRING},
    {NULL,0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "1lq file from affymetrix specifying probe layout on chip",
    "File to output matrix of probabilities to.",
    "File with sequence of probes that are known to be bad and should be ignored.",
    "Combine experiments with same name prefix.",
    "When used with combineReplicates, outputs a column for each tissue that has same prefix.",
    "Skip everything else and just run inverse chisq on each value degreeFreedom pair on command line.",
    "Output G/C probe countent counts to prefixCounts.tab",
    "Use median rather than Fisher's combination",
    "Test the median code.",
    "Do a combination of medians and fischer's combination.",
    "Output the intensity and probability to file with this prefix.",
    "Only output probe sets in this list.",
    "Output probe sets and sequences in 5'->3' orientation"
};

void usage()
/** Print usage and quit. */
{
int i=0;
warn("gcPresAbs - Calculate probability of expression in an mrna\n"
     "sample given a cel file of results. Use empiracal rank within\n"
     "a gc population to estimate probability and fisher's method\n"
     "of combining probabilities to calculate probe set probability.");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   "
	 "gcPresAbs -1lqFile=chipmousea.1lq -outFile=probeSet.matrix.probs file1.cel file2.cel ... fileN.cel");
}

int psCmp(const void *va, const void *vb)
/* Compare based on probeSet name. */
{
const struct psProb *a = *((struct psProb **)va);
const struct psProb *b = *((struct psProb **)vb);
return strcmp(a->psName, b->psName);
}

int row1lqCmp(const void *va, const void *vb)
/* Compare to sort based on intensity for the
   given repliate. */
{
const struct row1lq *a = *((struct row1lq **)va);
const struct row1lq *b = *((struct row1lq **)vb);
double bI = b->intensity[repToSort];
double aI = a->intensity[repToSort];
if(aI < bI)
    return 1;
if(aI > bI)
    return -1;
return 0;
}

int countGc(char *sequence)
/* Count the numbers of G's+C's in the sequence. */
{
int gCount = 0, cCount =0;
tolowers(sequence);
gCount = countChars(sequence, 'g');
if(gCountsFile)
    fprintf(gCountsFile, "%d\n", gCount);
cCount = countChars(sequence, 'c');
if(cCountsFile)
    fprintf(cCountsFile, "%d\n", cCount);
if(gcCountFile)
    fprintf(gcCountFile, "%d\n", cCount + gCount);
return cCount + gCount;
}

void initGcBins(int minGc, int maxGc)
/* Intialize the gcBins structure for maxGc-minGc bins. */
{
int numBins = maxGc - minGc + 1;
minGcBin = minGc;
maxGcBin = maxGc;
AllocArray(gcBins, numBins);
}

int binForGc(int gcCount)
/* Return the correct bin for this gc count. */
{
int count = 0;
if(gcCount < minGcBin)
    gcCount = minGcBin;
if(gcCount > maxGcBin)
    gcCount = maxGcBin;
return gcCount - minGcBin;
}

void readBadProbes(char *fileName)
/* Read in a list that contains bad probes sequence should be in first row. */
{
struct lineFile *lf = NULL;
char *line = NULL;
killHash = newHash(15);
lf = lineFileOpen(fileName, TRUE);
while(lineFileNextReal(lf, &line))
    {
    tolowers(line);
    hashAdd(killHash, line, placeHolder);
    }
lineFileClose(&lf);
}


void read1lqFile(char *name, struct row1lq ***pRowArray, int *pRowCount)
/* Read in the 1lq popluating the gcBins and hasing by probe set name.
   Need to index rows by a few different ways:
   1) Index in file, for filling in intensity vals from cel files.
   2) Gc Bin for calculating the rank within a set of probes that have same gc content.
   3) Probe set for calculating the probability that the probe set as a whole is expressed.
*/
{
struct lineFile *lf = lineFileOpen(name, TRUE);
struct row1lq **array1lq = NULL;
struct row1lq *rowList = NULL, *row=NULL, *rowNext = NULL;
int i;
char *words[50];
char *line = NULL;
int count =0;
boolean found = FALSE;
struct slRef *refList = NULL, *ref = NULL;
while(lineFileNextReal(lf, &line))
    {
    if(stringIn("X\tY\t", line))
	{
	found = TRUE;
	break;
	}
    }
if(!found)
    errAbort("Couldn't find \"X\tY\t\" in 1lq file: %s", name);

while(lineFileChopNextTab(lf, words, sizeof(words))) 
    {
    AllocVar(row);
    row->psName = cloneString(words[5]);
    row->seq = cloneString(words[2]);
    row->x = atoi(words[0]);
    row->y = atoi(words[1]);
    slAddHead(&rowList, row);
    (*pRowCount)++;
    }
slReverse(&rowList);
AllocArray(array1lq, (*pRowCount));

probeSetHash = newHash(15);

for(row = rowList; row != NULL; row = rowNext)
    {
    int bin = -1;
    struct slRef *ref = NULL, *reflist = NULL;

    rowNext = row->next;
    
    /* Keep track of the row by its index in the file. */
    array1lq[count++] = row;

    /* Skip things that are on the kill list. */
    tolowers(row->seq);
    if(sameString(row->seq, "!") || (killHash != NULL && hashFindVal(killHash, row->seq) != NULL))
	{
	slAddHead(&badBin, row);
	numBadProbes++;
	continue;
	}

    /* Add it to the correct gc bin. */
    row->gcCount = countGc(row->seq);
    bin = binForGc(row->gcCount);
    slAddHead(&gcBins[bin], row);

    /* Create a list of references indexed by the probe sets. */
    AllocVar(ref);
    ref->val = row;
    refList = hashFindVal(probeSetHash, row->psName);
    if(refList == NULL)
	hashAddUnique(probeSetHash, row->psName, ref);
    else
	slAddTail(&refList, ref);
    }

for(i = minGcBin; i < maxGcBin; i++)
    slReverse(&gcBins[i-minGcBin]);

//lineFileClose(&lf);
*pRowArray = array1lq;
}

void fillInMatrix(double **matrix, int celIx, int rowCount, char *celFile)
/* Read in the values of the cel file into matrix column celIx. */
{
struct lineFile *lf = lineFileOpen(celFile, TRUE);
char *words[50];
char *line = NULL;
boolean found = FALSE;
int count =0;
/* Read out the header. */
while(lineFileNextReal(lf, &line))
    {
    if(stringIn("[INTENSITY]", line))
	{
	found = TRUE;
	break;
	}
    }
if(!found)
    errAbort("Couldn't find \"[INTENSITY]\" in cel file: %s", celFile);
count =0;
lineFileNextReal(lf, &line); /* NumberCells... */
lineFileNextReal(lf, &line); /* CellHeader...*/
/* Read in the data. */
while(lineFileChopNext(lf, words, sizeof(words)))
    {
    if(*(words[0]) == '[')
	break;
    assert(count+1 <= rowCount);
    matrix[count][celIx] = atof(words[2]);
    count++;
    if(lf->bytesInBuf == 0)
	warn("Something weird is happening...");
    }
//warn("Closing line file.");
lineFileClose(&lf);
}

int xy2i(int x, int y)
/* Take in the x,y coordinates on the array and 
   return the index into the array. */
{
return (712 * y) + x;
}

int doubleComp(const void *a, const void *b)
/* Return difference to sort on doubles. */
{
double x = (*(double *)a);
double y = (*(double *)b);
if(x > y)
    return 1;
else if(y > x)
    return -1;
else 
    return 0;
}

/* double quantileArray(double *array, int count, double quantile) */
/* /\* give the quantile at a given position of the array. *\/ */
/* { */
/* double *results = NULL; */
/* boolean isEven = FALSE; */
/* int i = 0; */
/* double quant = 0; */

/* assert(count); */
/* assert(quantile >=0 && quantile <=1); */
/* AllocArray(results, count); */
/* if(floor(count * quantile) == ceil(count *quantile)) */
/*     isEven = TRUE; */

/* if(isEven && (count % 2 != 0)) */
/*     errAbort("Wrong even for quantile %.2f with count %d", quantile, count); */
/* if(!isEven && (count % 2 == 0)) */
/*     errAbort("Wrong odd for quantile %.2f with count %d", quantile, count); */

/* for(i = 0; i < count; i++) */
/*     results[i] = array[i]; */
/* qsort(results, count, sizeof(results[0]), doubleComp); */
/* if(isEven) */
/*     quant = (results[(int)floor(count * quantile)-1] + results[(int)floor(count*quantile)])/2; */
/* else */
/*     quant = results[(int)floor(count * quantile)]; */
/* freez(&results); */
/* return quant; */
/* } */

/* double medianArray(double *array, int count)  */
/* /\* Give the median, or a quantile of an array. *\/ */
/* { */
/* return quantileArray(array, count, quantile); */
/* } */

double medianArray(double *array, int count)
/* give the median of the array. */
{
double *results = NULL;
boolean isEven = FALSE;
int i = 0;
double median = 0;

assert(count);
AllocArray(results, count);
if(count % 2 == 0)
    isEven = TRUE;

for(i = 0; i < count; i++)
    results[i] = array[i];
qsort(results, count, sizeof(results[0]), doubleComp);
if(isEven)
    median = (results[count/2-1] + results[count/2])/2;
else
    median = results[count/2];
freez(&results);
return median;
}

void printDoubleArray(double *array, int count)
/* Print the elements of a double array. */
{
int i = 0;
for(i = 0; i<count; i++) 
    printf("%.3f, ", array[i]);
printf("\n");
}

double medianRefList(struct slRef *refList)
/** Return the median value of the row list. */
{
struct slRef *ref = NULL;
struct row1lq *row = NULL;
int count = 0, currentIx = 0;
boolean isEven = FALSE;
double median = 0;
double *results = NULL;
int repIx = 0;

/* How much memory do we need? */
for(ref = refList; ref != NULL; ref = ref->next)
    {
    row = ref->val;
    count += row->repCount;
    }

assert(count > 0);
AllocArray(results, count);
/* Fill in the values. */
for(ref = refList; ref != NULL; ref = ref->next)
    {
    row = ref->val;
    for(repIx = 0; repIx < row->repCount; repIx++)
	{
	results[currentIx++] = row->pVal[repIx];
	}
    }

/* Get the median. */
median = medianArray(results, count);

freez(&results);
return 1 - median;
}

void probeSetCalcPval(void *val)
/* Loop through the slRef list calculating pVal based
   on row1lq->pval using fishers method for combining probabilities.
   Create a psProbe val and add it to the probePVals list. */
{
struct slRef *ref = NULL, *refList = NULL;
struct psProb *ps = NULL;
struct row1lq *row = NULL;
boolean debug = FALSE;
double probProduct = 0;
int probeCount = 0;
int probRepCount = 0;
int i = 0, j = 0;
refList = val;
probeCount = slCount(refList);
if(outputPsSetSeqs != NULL)
    {
    assert(refList);
    row = refList->val;
    fprintf(outputPsSetSeqs, "%s\t", row->psName);
    for(ref = refList; ref != NULL; ref = ref->next) 
	{
	row = ref->val;
	reverseBytes(row->seq, strlen(row->seq));
	if(ref->next != NULL)
	    fprintf(outputPsSetSeqs, "%s,", row->seq);
	else
	    fprintf(outputPsSetSeqs, "%s", row->seq);

	reverseBytes(row->seq, strlen(row->seq));
	}
    fprintf(outputPsSetSeqs, "\n");
    }
/* Allocate some memory. */
AllocVar(ps);
row = refList->val;
ps->pVal = 0;
safef(ps->psName, sizeof(ps->psName), "%s", row->psName);

if(probeCount < minProbeNum)
    {
    ps->pVal=0;
    }
else if(doMedianComb)
    {
    ps->pVal = medianRefList(refList);
    }
else 
    {
    for(ref = refList; ref != NULL; ref = ref->next)
	{
	row = ref->val;
	if(doCombo)
	    {
	    double med = medianArray(row->pVal, row->repCount);
	    if(med == 0)
		med = .00000000001;
	    probRepCount++;
	    probProduct += log(med);
	    }
	else 
	    {
	    for(i = 0; i < row->repCount; i++)
		{
		probRepCount++;
		if(row->pVal[i] != 0)
		    probProduct = probProduct + log(row->pVal[i]);
		else
		    probProduct = probProduct + log(.000000001);
		}
	    }
	}
/* Fisher's method for combining probabilities. */
    ps->pVal = gsl_cdf_chisq_P(-2*probProduct,2*probRepCount); 
    }
if(ps->pVal > 1 || ps->pVal < 0) 
    warn("%s pVal is %.10f, wrong!");
slAddHead(&probePVals, ps);
}

doInvChiSq(int argc, char *argv[])
/* Test routine to make sure that gsl_cdf_chisq_P() is working correctly. */
{
int i = 0;
double result = 0;
for(i = 1; i < argc; i+=2)
    {
    result = gsl_cdf_chisq_P(atof(argv[i]), atoi(argv[i+1]));
    printf("%s\t%s\t%.2f\n", argv[i], argv[i+1], result);
    }
}

void printIntensities(struct row1lq *rowList, int limit)
/* Loop through and print out the row intensity for a list. */
{
struct row1lq *row = NULL;
int count = 0;
if(limit == -1)
    limit = slCount(rowList);

for(row = rowList; count < limit && row != NULL; row = row->next)
    {
    fprintf(stdout, "%.1f\n", row->intensity[repToSort]);
    count++;
    }
}

void fillInProbeSetPvals(double **matrix, int rowCount, int colCount, int repCount, int *repIndexes, 
			 int sampleIx, struct row1lq **row1lqArray, double ***probeSetPValsP, 
			 char ***probeSetNamesP, int *psCountP, double **probMatrix)
/* Calculate and fill in the probe set probabilities of expression given
   the data in the matrix. */
{
int i = 0, j = 0;
int rank = 0;
int binCount = 0;
struct psProb *probe = NULL;
struct row1lq *row = NULL;
int psCount = 0;

/* Fill in the values for the rows. */
for(i = 0; i < rowCount; i++)
    {
    row1lqArray[i]->repCount = repCount;
    for(j = 0; j < repCount; j++)
	{
	row1lqArray[i]->intensity[j] = matrix[i][repIndexes[j]];
	}
    }
/* Sort the gc bins by their rank and fill 
   in the pVals as an empiracal rank. */
for(i = minGcBin; i <= maxGcBin; i++)
    {
    /* Sort for each replicate. */
    for(j = 0; j < repCount; j++)
	{
	rank = 1;
	repToSort = j;
	slSort(&gcBins[i-minGcBin], row1lqCmp);
	binCount = slCount(gcBins[i-minGcBin]);
	for(row = gcBins[i-minGcBin]; row != NULL; row = row->next)
	    {
	    row->pVal[j] = ((double)rank)/binCount;
	    probMatrix[xy2i(row->x, row->y)][repIndexes[j]] = row->pVal[j];
	    rank++;
	    }
	}
    }
/* Do the bad bin. */
for(row = badBin; row != NULL; row = row->next)
    {
    for(j = 0; j < repCount; j++)
	{
	row->pVal[j] = 0;
	}
    }
hashTraverseVals(probeSetHash, probeSetCalcPval);
if(outputPsSetSeqs == NULL) 
    carefulClose(&outputPsSetSeqs);
slSort(&probePVals, psCmp);
psCount = slCount(probePVals);
/* If this is the first time through Allocate some memory. */
if(*probeSetPValsP == NULL) 
    {
    (*psCountP) = psCount;
    AllocArray((*probeSetNamesP), psCount);
    AllocArray((*probeSetPValsP), psCount);
    for(i = 0; i < psCount; i++)
	{
	AllocArray((*probeSetPValsP)[i], sampleCount);
	}
    i = 0;
    for(probe = probePVals; probe != NULL; probe = probe->next)
	{
	(*probeSetNamesP)[i++] = cloneString(probe->psName);
	}
    }

i = 0;
for(probe = probePVals; probe != NULL; probe = probe->next)
    {
    (*probeSetPValsP)[i++][sampleIx] = probe->pVal;
    }
slFreeList(&probePVals);
}

struct replicateMap *createReplicateMap(char **expNames, int expCount)
/* Group replicates together if required or
   just make a replicate for each one if no replicates
   are required. */
{
struct replicateMap *rMapList = NULL, *rMap = NULL;
int i = 0, j = 0;
boolean combineReplicates = optionExists("combineReplicates");
boolean *used = NULL;
char expBuff[256], nameBuff[256];
char *mark = NULL;

/* If we're combining results look for 
   expNames that have the same root (string before 
   first ".") */
if(combineReplicates)
    {
    AllocArray(used, expCount);
    for(i = 0; i < expCount; i++)
	{
	/* If we haven't seen this experiment before
	   start a new replicateMap. */
	if(!used[i])
	    {
	    safef(nameBuff, sizeof(nameBuff), "%s", expNames[i]);
	    mark = strchr(nameBuff, '.');
	    if(mark != NULL)
		*mark = '\0';
	    AllocVar(rMap);
	    used[i] = TRUE;
	    rMap->rootName = cloneString(nameBuff);
	    rMap->repCount = 1;
	    AllocArray(rMap->repIndexes, expCount);
	    rMap->repIndexes[0] = i;

	    /* Loop through the experiments finding the ones
	       that are replicates. */
	    for(j = i+1; j < expCount; j++)
		{
		safef(nameBuff, sizeof(nameBuff), "%s", expNames[j]);
		mark = strchr(nameBuff, '.');
		if(mark != NULL)
		    *mark = '\0';
		if(sameString(nameBuff, rMap->rootName))
		    {
		    used[j] = TRUE;
		    rMap->repIndexes[rMap->repCount++] = j;
		    }
		}
	    maxReplicates = max(maxReplicates, rMap->repCount);
	    slAddHead(&rMapList, rMap);
	    }
	}
    }
else /* Make each expName a new replicate. */
    {
    for(i = 0; i < expCount; i++)
	{
	AllocVar(rMap);
	rMap->rootName = cloneString(expNames[i]);
	rMap->repCount = 1;
	AllocArray(rMap->repIndexes, rMap->repCount);
	rMap->repIndexes[0] = i;
	maxReplicates = max(maxReplicates, rMap->repCount);
	slAddHead(&rMapList, rMap);
	}
    }

slReverse(&rMapList);
/* Do a summmary of the replicates... */
/* for(rMap = rMapList; rMap != NULL; rMap = rMap->next) */
/*     { */
/*     printf("%s\t", rMap->rootName); */
/*     for(i = 0; i < rMap->repCount; i++) */
/* 	{ */
/* 	printf("%s,", expNames[rMap->repIndexes[i]]); */
/* 	} */
/*     printf("\n"); */
/*     } */
return rMapList;
}

boolean reportThisPSet(char *name)
/* Return TRUE if thes pset should be outputed for intensity and pvals. */
{
if(outputValsHash == NULL)
    return TRUE;
return hashLookup(outputValsHash, name) != NULL;
}

void outputPvalValues(struct row1lq **row1lqArray, double **probMatrix,
		      char **expNames, int rowCount, int colCount)
/* Write out the pvals of the probe sets over all the tissues. */
{
int rowIx = 0, colIx = 0;
struct row1lq *row = NULL;
assert(outputPvals);
/* Print column headers. */
fprintf(outputPvals, "%s\t", "probeSetName");
for(colIx = 0; colIx < colCount-1; colIx++)
    fprintf(outputPvals, "%s\t", expNames[colIx]);
fprintf(outputPvals
, "%s\n", expNames[colIx]);

/* Print all the data. */
for(rowIx = 0; rowIx < rowCount; rowIx++)
    {
    row = row1lqArray[rowIx];
    if(reportThisPSet(row->psName))
	{
	fprintf(outputPvals,"%d\t%s",rowIx, row->psName);
	for(colIx = 0; colIx < colCount; colIx++)
	    {
	    fprintf(outputPvals, "\t%.5f", probMatrix[rowIx][colIx]);
	    }
	fputc('\n', outputPvals);
	}
    }
}

void outputValues(double **matrix, struct row1lq **row1lqArray, char **expNames, int rowCount, int colCount)
/* Write out the intensity values for each probe. */
{
int rowIx = 0, colIx = 0;
struct row1lq *row = NULL;
assert(outputIntenVals);
fprintf(outputIntenVals, "%s\t", "probeSetName");
for(colIx = 0; colIx < colCount-1; colIx++)
    fprintf(outputIntenVals, "%s\t", expNames[colIx]);
fprintf(outputIntenVals, "%s\n", expNames[colIx]);

for(rowIx = 0; rowIx < rowCount; rowIx++)
    {
    row = row1lqArray[rowIx];
    if(reportThisPSet(row->psName))
	{
	assert(rowIx == xy2i(row->x, row->y));
 	fprintf(outputIntenVals, "%d\t%s", rowIx, row->psName);
/* 	fprintf(outputIntenVals, "%s\t%s\t%d\t%d\t%d\t%d", row->psName, row->seq, row->x, row->y, rowIx, xy2i(row->x, row->y)); */
	for(colIx = 0; colIx < colCount; colIx++)
	    fprintf(outputIntenVals, "\t%.1f", matrix[rowIx][colIx]);
	fputc('\n', outputIntenVals);
	}
    }
}

void gcPresAbs(char *outFile, char *file1lq, int celCount, char *celFiles[])
/* Output all of the probe sets and their pvals for expression as
   calculated from the intensities in the cel files. */
{
struct row1lq **rowArray = NULL, *row=NULL;
char **expNames = NULL, **expOutNames = NULL;
double **matrix = NULL, **probMatrix = NULL;
double **probeSetPVals = NULL; /* Matrix of pVals of expression with probe sets ordered altphabetically. */
char **probeSetNames = NULL;   /* List of probe set names indexing the probeSetPVals matrix. */
char *badProbeName = NULL;     /* Name of file with bad probes. */
int psCount = 0;
int i=0, j=0, k=0;
int rowCount=0;
FILE *out = NULL;
struct replicateMap *rMapList = NULL, *rMap = NULL;
boolean outputDuplicates = optionExists("outputDuplicates");
dotForUserInit(1);
initGcBins(8,17); /* Determined by where most of the data lives...
		     On the altmousea chip the data drops of significantly
		     before 8 (15,732 3% <= 8) and after 18 (9636 2% >=18) */

/* See if we have a kill list. */
badProbeName = optionVal("badProbes", NULL);
if(badProbeName != NULL)
    {
    warn("Reading the kill list");
    readBadProbes(badProbeName);
    }

/* Read in a 1lq file and put it in an array, lists and hash
   all at the same time. */
warn("Reading 1lq file: %s", file1lq);
read1lqFile(file1lq, &rowArray, &rowCount);

/* Lets initialize some memory. */
AllocArray(expNames, celCount);
AllocArray(expOutNames, celCount);
AllocArray(matrix, rowCount);
AllocArray(probMatrix, rowCount);
for(i=0; i<rowCount; i++)
    {
    AllocArray(matrix[i], celCount);
    AllocArray(probMatrix[i], celCount);
    }

/* Read in the cel files. */
for(i=0; i<celCount; i++)
    {
    char *lastSlash = NULL;
    dotForUser();
    expNames[i] = cloneString(celFiles[i]);
    lastSlash = strrchr(expNames[i], '/');
    if(lastSlash != NULL)
	expNames[i] = lastSlash+1;
    fillInMatrix(matrix, i, rowCount, celFiles[i]);
    }

if(optionExists("outputVals"))
    outputValues(matrix, rowArray, expNames, rowCount, celCount);
rMapList = createReplicateMap(expNames, celCount);
sampleCount = slCount(rMapList);
/* Allocate enough memory for the maximum number of replicates. */
for(i = 0; i < rowCount; i++)
    {
    AllocArray(rowArray[i]->intensity, maxReplicates);
    AllocArray(rowArray[i]->pVal, maxReplicates);
    }

warn("Calculating pVals");
//for(i=0; i<celCount; i++)
i = 0;
for(rMap = rMapList; rMap != NULL; rMap = rMap->next)
    {
    dotForUser();
    for(j = 0; j < rMap->repCount; j++)
	expOutNames[i+j] = cloneString(expNames[rMap->repIndexes[j]]);
    fillInProbeSetPvals(matrix, rowCount, celCount, rMap->repCount, rMap->repIndexes, i++, rowArray,
			&probeSetPVals, &probeSetNames, &psCount, probMatrix);
    }
if(optionExists("outputVals"))
    outputPvalValues(rowArray, probMatrix, expOutNames, rowCount, celCount);

warn("Done.");
warn("Found %d bad probes.", numBadProbes);
warn("Writing out the results.");
out = mustOpen(outFile, "w");

if(outputDuplicates)
    {
    /* Output each name, this only works if duplicates are
       next to each other. */
    for(rMap = rMapList; rMap->next != NULL; rMap = rMap->next)
	{
	for(i = 0; i < rMap->repCount; i++)
	    fprintf(out, "%s\t", expNames[rMap->repIndexes[i]]);
	}
    for(i = 0; i < rMap->repCount - 1; i++)
	fprintf(out, "%s\t", expNames[rMap->repIndexes[i]]);
    fprintf(out, "%s\n", expNames[rMap->repIndexes[i]]);
    
    for(i = 0; i < psCount; i++) 
	{
	fprintf(out, "%s\t", probeSetNames[i]);
	for(j = 0; j < sampleCount; j++)
	    {
	    /* Output a column for each repetition. */
	    rMap = slElementFromIx(rMapList, j);
	    for(k = 0; k < rMap->repCount -1; k++)
		fprintf(out, "%.5g\t", probeSetPVals[i][j]);

	    /* If this is the last repetion then print the newline
	       otherwise continue with tabs. */
	    if(j == sampleCount -1)
		fprintf(out, "%.5g\n", probeSetPVals[i][j]);
	    else
		fprintf(out, "%.5g\t", probeSetPVals[i][j]);
	    }
	}
    }
else /* Only print one column per rMap. */
    {
    for(rMap = rMapList; rMap->next != NULL; rMap = rMap->next)
	fprintf(out, "%s\t", rMap->rootName);
    fprintf(out, "%s\n", rMap->rootName);
    
    for(i = 0; i < psCount; i++) 
	{
	fprintf(out, "%s\t", probeSetNames[i]);
	for(j = 0; j < sampleCount -1; j++)
	    fprintf(out, "%.5g\t", probeSetPVals[i][j]);
	fprintf(out, "%.5g\n", probeSetPVals[i][j]);
	}
    }
carefulClose(&out);
warn("Done");
}

void initCountFiles(char *prefix)
/* open files for probe counts. */
{
struct dyString *dy = newDyString(1024);
dyStringClear(dy);
dyStringPrintf(dy, "%s.gcCounts.tab", prefix);
gcCountFile = mustOpen(dy->string, "w");

dyStringClear(dy);
dyStringPrintf(dy, "%s.gCounts.tab", prefix);
gCountsFile = mustOpen(dy->string, "w");

dyStringClear(dy);
dyStringPrintf(dy, "%s.cCounts.tab", prefix);
cCountsFile = mustOpen(dy->string, "w");
dyStringFree(&dy);
}

void closeCountFiles()
/* Close the counting file handles. */
{
carefulClose(&gcCountFile);
carefulClose(&gCountsFile);
carefulClose(&cCountsFile);
}

void setupTestRow(struct slRef **refList, double *array, int count)
/* Create a test row and add it to the list. */
{
int i = 0;
struct row1lq *row = NULL;
struct slRef *ref = NULL;
AllocVar(row);
row->repCount = count;
AllocArray(row->pVal, row->repCount);
for(i = 0; i < count; i++)
    row->pVal[i] = array[i];

AllocVar(ref);
ref->val = row;
slAddHead(refList, ref);
}

boolean sameDoubles(double a, double b)
/* return TRUE if two doubles are the same to 6 significant
   digits. */
{
char buffA[256];
char buffB[256];
safef(buffA, sizeof(buffA), "%.6f", a);
safef(buffB, sizeof(buffB), "%.6f", b);
return sameString(buffA, buffB);
}
    
void testMedian()
/* Quick testing of the median function. */
{
struct slRef *refList = NULL;
int count = 0, i = 0;
double first[] = {.7, .8, .9, .6, .5};
double second[] = {.8, .7, .6};
double third[] = {.6, .5};
double median = 0;
/* set up the first guy. */
setupTestRow(&refList, first, ArraySize(first));

median = medianRefList(refList);
if(!sameDoubles(median, .3))
    errAbort("Median should be .3, but it isn't.");
setupTestRow(&refList, second, ArraySize(second));

median = medianRefList(refList);
if(!sameDoubles(median, .3))
    errAbort("Median should still be .3, but it isn't.");
setupTestRow(&refList, third, ArraySize(third));
median = medianRefList(refList);
if(!sameDoubles(median, .35))
    errAbort("Median should be .7, but it isn't.");
warn("Passed all median tests.");
exit(0);
}
    
void initOutputValsList()
/* Read in the output vals list from the file and inititalize
   the outputValsHash. */
{
struct lineFile *lf = NULL;
char *s = NULL;
char *fileName = optionVal("outputValsList", NULL);
assert(fileName);
lf = lineFileOpen(fileName, TRUE);
outputValsHash = newHash(10);
while(lineFileNextReal(lf, &s))
    hashAdd(outputValsHash, s, NULL);
lineFileClose(&lf);
}

void initOutputVals()
/* Open two files for writing the probe level intensities
   and probabilities to. */
{
struct dyString *s = newDyString(512);
char *filePrefix = optionVal("outputVals", NULL);
assert(filePrefix);
dyStringPrintf(s, "%s.inten", filePrefix);
outputIntenVals = mustOpen(s->string, "w");
dyStringClear(s);
dyStringPrintf(s, "%s.pvals", filePrefix);
outputPvals = mustOpen(s->string, "w");
dyStringFree(&s);
}

int main(int argc, char *argv[])
/* Everybodys favorite function. */
{
char *affy1lqName = NULL;
char *outFileName = NULL;
int origCount = argc;
optionInit(&argc, argv, optionSpecs);
if(optionExists("testMedian"))
    testMedian();
if(optionExists("help") || origCount < 3)
    usage();
if(optionExists("countPrefix"))
    initCountFiles(optionVal("countPrefix", NULL));
if(optionExists("doMedianComb"))
    doMedianComb = TRUE;
if(optionExists("doCombo"))
    doCombo = TRUE;
if(optionExists("outputValsList"))
    initOutputValsList();
if(optionExists("outputVals"))
    initOutputVals();
if(optionExists("outputPsSetSeqs")) 
    outputPsSetSeqs = mustOpen(optionVal("outputPsSetSeqs", NULL), "w");
affy1lqName = optionVal("1lqFile", NULL);
outFileName = optionVal("outFile", NULL);
if(optionExists("testVals"))
    doInvChiSq(argc, argv);
else if(affy1lqName == NULL || outFileName == NULL)
    errAbort("Need to specify 1lqFile and outFile, use -help for usage.");
else 
    gcPresAbs(outFileName, affy1lqName, argc-1, argv+1);

if(optionExists("countPrefix"))
    closeCountFiles();
return 0;
}


