/* affyAllExonGSColumn - Calculate median expressions of UCSC genes using exons from Affy All Exon data. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "expData.h"
#include "microarray.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "affyAllExonGSColumn - Calculate median expressions of UCSC genes using exons from Affy All Exon data\n"
  "usage:\n"
  "   affyAllExonGSColumn expData.txt mapping.txt newExpData.txt\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *makeExpHash(struct expData *exps)
/* hash up the expression data. */
{
struct hash *expHash = newHash(22);
struct expData *exp;
for (exp = exps; exp != NULL; exp = exp->next)
    hashAdd(expHash, exp->name, exp);
return expHash;
}

struct expData *combinedExpData(struct slRef *refList, char *name)
/* Median all the exons for each gene at each tissue. */
{
struct expData *comb;
int i;
int numExons = 0;
double *array;
if (!refList || !refList->val)
    errAbort("Trying to combine nothing?  Check the data or better yet this code.");    
numExons = slCount(refList);
AllocArray(array, numExons);
AllocVar(comb);
comb->name = cloneString(name);
comb->expCount = ((struct expData *)refList->val)->expCount;
AllocArray(comb->expScores, comb->expCount);
for (i = 0; i < comb->expCount; i++)
    /* There's still the same number of experiments. */
    {
    struct slRef *cur;
    int j;
    for (cur = refList, j = 0; (cur != NULL) && (j < numExons); cur = cur->next, j++)
	/* Go through each exon for the experiment. */
	{
	struct expData *exp = cur->val;
	array[j] = (double)exp->expScores[i];
	}
    comb->expScores[i] = (float)maDoubleMedianHandleNA(numExons, array);
    }
freez(&array);
return comb;
}

void outputCombinedExpData(FILE *output, struct slRef *refList, char *name)
/* Combine and output expData */
{
struct expData *exp = combinedExpData(refList, name);
expDataTabOut(exp, output);
expDataFree(&exp);
}

void affyAllExonGSColumn(char *expDataFile, char *mappingFile, char *outputFile)
/* affyAllExonGSColumn - Calculate median expressions of UCSC genes using exons from Affy All Exon data. */
{
struct expData *exps = expDataLoadAll(expDataFile);
struct hash *expHash = makeExpHash(exps);
struct lineFile *lf = lineFileOpen(mappingFile, TRUE);
struct slRef *refList = NULL;
FILE *output = mustOpen(outputFile, "w");
char *words[2];
char *prevName = NULL;
while (lineFileRowTab(lf, words))
    {
    struct expData *exp = hashFindVal(expHash, words[1]);
    if (!exp)
	errAbort("Can't find %s\n", words[1]);
    if (!(prevName && (sameString(prevName, words[0]))))
	{
	/* output slRef list */
	if (prevName)
	    outputCombinedExpData(output, refList, prevName);
	freeMem(prevName);
	prevName = cloneString(words[0]);
	slFreeList(&refList);
	refList = NULL;
	}
    refAdd(&refList, exp);
    }
/* output slRef list */
outputCombinedExpData(output, refList, prevName);
freeMem(prevName);
slFreeList(&refList);
expDataFreeList(&exps);
hashFree(&expHash);
lineFileClose(&lf);
carefulClose(&output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
affyAllExonGSColumn(argv[1], argv[2], argv[3]);
return 0;
}
