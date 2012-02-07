/* regCompanionEnhProCellSpecificPairs - Select enh/pro pairs that are seen in a given cell 
 * lines. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "expRecord.h"
#include "basicBed.h"
#include "portable.h"


double minExp = 10.0;
double minAct = 15.0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionEnhProCellSpecificPairs - Select enh/pro pairs that are seen in a given cell lines\n"
  "usage:\n"
  "   regCompanionEnhProCellSpecificPairs enh.bed15 enh.exp gene.levels pairsIn.bed outDir\n"
  "where:\n"
  "   enh.bed15 is a file of enhancers with a score for each cell line\n"
  "   enh.exps describes the cell lines in the strange expRecord format\n"
  "   gene.levels is a tab-separated file where the first column is the gene name\n"
  "          and the rest are expression levels in the same order as enh.exp and enh.bed15\n"
  "   pairsIn.bed is the outPairBed output of regCompanionCorrelateEnhancerAndExpression\n"
  "          It just is a bed file with each item having two blocks, one for enhancer, one for\n"
  "          promoter.  The format of the name field is enh->pro\n"
  "   outDir is where the output is made.  This is one file for each cell line containing\n"
  "          only the pairs that are expressed in that cell.\n"
  "options:\n"
  "   -minExp=N (default %g) minimum expression level in cell\n"
  "   -minAct=N (default %g) minimum enhancer activation level in cell\n"
  , minExp, minAct
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *hashGeneLevels(char *fileName, int cellCount)
/* Get a hash with key of gene name and value an array of expression values. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(16);
int fieldCount = cellCount+1;
char *words[fieldCount+1];
int wordCount;
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, fieldCount, wordCount);
    char *name = words[0];
    double *vals;
    AllocArray(vals, cellCount);
    int i;
    for (i=0; i<cellCount; ++i)
        vals[i] = sqlDouble(words[i+1]);
    hashAdd(hash, name, vals);
    }
lineFileClose(&lf);
return hash;
}

void regCompanionEnhProCellSpecificPairs(char *enhBed, char *cellDescriptions, 
	char *geneLevels, char *pairsIn, char *outDir)
/* regCompanionEnhProCellSpecificPairs - Select enh/pro pairs that are seen in a given cell 
 * lines. */
{
/* Load up cell descriptions into cell array */
struct expRecord *cell, *cellList = expRecordLoadAll(cellDescriptions);
int cellCount = slCount(cellList);
struct expRecord **cellArray;
AllocArray(cellArray, cellCount);
int i;
for (i=0, cell = cellList; i < cellCount; ++i, cell = cell->next)
    cellArray[i] = cell;
verbose(2, "Got %d cells in %s\n", cellCount, cellDescriptions);

/* Load up enhBed into a hash keyed by name */
struct bed *enh, *enhList;
int fieldCount;
bedLoadAllReturnFieldCount(enhBed, &enhList, &fieldCount);
if (fieldCount != 15)
   errAbort("Expecting bed 15 format in %s", enhBed);
struct hash *enhHash = hashNew(16);
for (enh = enhList; enh != NULL; enh = enh->next)
    {
    if (enh->expCount != cellCount)
        errAbort("Inconsistent input: %d cells in %s, but %d in %s\n", 
		cellCount, cellDescriptions, enh->expCount, enhBed);
    hashAddUnique(enhHash, enh->name, enh);
    }
verbose(2, "Got %d enhancers in %s\n", enhHash->elCount, enhBed);

/* Get a hash with key of gene name and value an array of expression values. */
struct hash *geneHash = hashGeneLevels(geneLevels, cellCount);
verbose(2, "Got %d genes in %s\n", geneHash->elCount, geneLevels);

/* Open inPairs.bed, just to make sure it's there before we do any output. */
struct lineFile *lf = lineFileOpen(pairsIn, TRUE);

/* Remove trailing slash from output dir if any */
if (lastChar(outDir) == '/')
    {
    int len = strlen(outDir);
    outDir[len-1] = 0;
    }

/* Make output directory and open all output files. */
makeDirsOnPath(outDir);
FILE *outFiles[cellCount];
for (i=0, cell = cellList; i < cellCount; ++i, cell = cell->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s/%s.bed", outDir, cell->description);
    outFiles[i] = mustOpen(path, "w");
    }

/* Stream through input file and copy to appropriate outputs. */
char *words[bedKnownFields*2];	// Make a little bigger than any known bed
int wordCount, wordsRequired = 0;
char *separator = "->";
int separatorSize = strlen(separator);
int pairCount = 0;
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    /* Make sure all lines have same # of fields, and at least 4. */
    if (wordsRequired == 0)
	{
        wordsRequired = wordCount;
	lineFileExpectAtLeast(lf, 4, wordCount);
	}
    else
	lineFileExpectWords(lf, wordsRequired, wordCount);
    ++pairCount;

    /* Parse out name field. */
    char *name = words[3];
    char *sepPos = stringIn(separator, name);
    if (sepPos == NULL)
        errAbort("Expecting %s in %s line %d of %s", separator, name, lf->lineIx, lf->fileName);
    char *enhName = cloneStringZ(name, sepPos-name);
    char *geneName = sepPos + separatorSize;

    /* Look up enhancer and gene. */
    enh = hashMustFindVal(enhHash, enhName);
    double *geneLevels = hashMustFindVal(geneHash, geneName);
    freez(&enhName);

    /* Output ones over minimum levels. */
    for (i=0; i < cellCount; ++i)
        {
	double enhLevel = enh->expScores[i];
	double geneLevel = geneLevels[i];
	if (enhLevel >= minAct && geneLevel >= minExp)
	    {
	    int j;
	    FILE *f = outFiles[i];
	    fprintf(f, "%s", words[0]);
	    for (j=1; j<wordCount; ++j)
		fprintf(f, "\t%s", words[j]);
	    fprintf(f, "\n");
	    }
	}
    }
verbose(2, "Got %d pairs in %s\n", pairCount, pairsIn);

/* Clean up. */
lineFileClose(&lf);
for (i=0; i<cellCount; ++i)
    carefulClose(&outFiles[i]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
minExp = optionDouble("minExp", minExp);
minAct = optionDouble("minAct", minAct);
regCompanionEnhProCellSpecificPairs(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
