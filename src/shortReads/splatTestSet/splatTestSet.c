/* splatTestSet - Create test set for splat. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"

static char const rcsid[] = "$Id: splatTestSet.c,v 1.3 2008/10/28 23:34:51 kent Exp $";

/* Command line variables. */
int chromCount = 1;
int chromSize = 1000;
int insPerRead = 0;
int delPerRead = 0;
int subPerRead = 0;
int stepSize = 1;
int readSize = 25;
int readCount;
boolean existingGenome = FALSE;
boolean separateMutations = FALSE;
boolean spaceForDel = FALSE;

/* Other global */
int readsGenerated;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "splatTestSet - Create test set for splat.  This will be a pretty easy to interpret set.\n"
  "usage:\n"
  "   splatTestSet genome.fa reads.fa\n"
  "Creates random sequence in genome.fa, and reads that are this sequence.\n"
  "The reads just step through the genome in order.  The mutations if any will be\n"
  "applied in a very predictable way too, advancing in position one base with each read.\n"
  "options:\n"
  "   -chromCount=N - number of chromosomes in genome.fa (default %d)\n"
  "   -chromSize=N - bases per chromosome (default %d)\n"
  "   -insPerRead=N - number of insertions per read (default %d)\n"
  "   -delPerRead=N - number of deletions per read (default %d)\n"
  "   -subPerRead=N - number of substitutions per read (default %d)\n"
  "   -separateMutations - if set, then don't allow two mutations on same base\n"
  "   -spaceForDel - if set put in a space where deletion is\n"
  "   -stepSize=N - number of bases to step between reads (default %d)\n"
  "   -readSize=N - size of read (default %d)\n"
  "   -readCount=N - number of reads (default = 1x coverage of genome)\n"
  "   -existingGenome - If set genome.fa is an existing file, otherwise it's created.\n"
  , chromCount, chromSize, insPerRead, delPerRead, subPerRead, stepSize, readSize
  );
}

static struct optionSpec options[] = {
   {"chromCount", OPTION_INT},
   {"chromSize", OPTION_INT},
   {"insPerRead", OPTION_INT},
   {"delPerRead", OPTION_INT},
   {"subPerRead", OPTION_INT},
   {"separateMutations", OPTION_BOOLEAN},
   {"spaceForDel", OPTION_BOOLEAN},
   {"stepSize", OPTION_INT},
   {"readSize", OPTION_INT},
   {"readCount", OPTION_INT},
   {"existingGenome", OPTION_BOOLEAN},
   {NULL, 0},
};

void generateFakeGenome(char *fileName)
/* Generate fake DNA sequence. */
{
int chromIx;
FILE *f = mustOpen(fileName, "w");
for (chromIx = 1; chromIx <= chromCount; ++chromIx)
    {
    fprintf(f, ">c%d\n", chromIx);
    int baseIx;
    for (baseIx=1; baseIx <= chromSize; ++baseIx)
        {
	int base = rand()&3;
	fputc(valToNt[base], f);
	if (baseIx%50 == 0)
	    fputc('\n', f);
	}
    if (chromSize%50 != 0)
        fputc('\n', f);
    }
carefulClose(&f);
}

boolean allDifferent(int *array, int size)
/* Return TRUE if all elements of array of given size are different. */
{
int i,j;
for (i=0; i<size; ++i)
   for (j=i+1; j<size; ++j)
       if (array[i] == array[j])
           return FALSE;
return TRUE;
}

void fakeRead(char *name, DNA *dna, int insertPos, int deletePos, int *subPos, int subCount, FILE *f)
/* Generate fake read from dna, possibly mutating it at given positions. */
{
if (separateMutations)
    {
    int mutCount = 0;
    int mutArray[16];
    CopyArray(subPos, mutArray, subCount);
    mutCount += subCount;
    if (insPerRead > 0)
        {
	mutArray[mutCount] = insertPos;
	mutCount += 1;
	}
    if (delPerRead > 0)
        {
	mutArray[mutCount] = deletePos;
	mutCount += 1;
	}
    if (!allDifferent(mutArray, mutCount))
        return;
    }
fprintf(f, ">%s\n", name);
int inputSize = readSize;
if (insPerRead > 0)
    inputSize -= insPerRead;
if (delPerRead > 0)
    inputSize += delPerRead;
int i;
for (i=0; i<inputSize; ++i)
    {
    if (delPerRead > 0 && i == deletePos)
	{
	if (spaceForDel)
	    fputc(' ', f);
        continue;
	}
    if (insPerRead > 0 && i == insertPos)
        fputc(toupper(valToNt[rand()&3]), f);
    DNA base = dna[i];
    boolean mutate = FALSE;
    if (subCount > 0)
        {
	int subIx;
	for (subIx=0; subIx < subCount; ++subIx)
	    if (i == subPos[subIx])
	        mutate = TRUE;
	}
    if (mutate)
        base = toupper(ntCompTable[(int)base]);
    fputc(base, f);
    }
if (insPerRead > 0 && insertPos == inputSize)
    fputc(toupper(valToNt[rand()&3]), f);
fputc('\n', f);
++readsGenerated;
}

void generateFakeReads(struct dnaSeq *chrom, FILE *f)
/* Generate fake reads that cover chrom. */
{
DNA *dna = chrom->dna;
int lastReadStart = chrom->size - readSize - delPerRead;
int i;
int insertPos = 10, deletePos = 20;
int subPos[subPerRead];
for (i=0; i<subPerRead; ++i)
    subPos[i] = 0;
for (i=0; i<lastReadStart; i += stepSize)
    {
    char name[64];
    safef(name, sizeof(name), "%s_%d_%d", chrom->name, i, readsGenerated);
    fakeRead(name, dna + i, insertPos, deletePos, subPos, subPerRead, f);
    if (readsGenerated >= readCount)
        break;


    /* Increment all the places where we mutate. */
    if (++insertPos >= readSize)
        insertPos = 0;
    if (++deletePos >= readSize)
        deletePos = 0;
    int j;
    for (j=0; j<subPerRead; ++j)
	{
	if (++subPos[j] >= readSize)
	    subPos[j] = 0;
	else
	    break;
	}
    }
}

void splatTestSet(char *genomeFile, char *readFile)
/* splatTestSet - Create test set for splat. */
{
if (!existingGenome)
    generateFakeGenome(genomeFile);
struct dnaSeq *genome = faReadAllDna(genomeFile);
struct dnaSeq *chrom;
FILE *f = mustOpen(readFile, "w");
for (;;)
    {
    for (chrom = genome; chrom != NULL; chrom = chrom->next)
	{
	generateFakeReads(chrom, f);
	if (readsGenerated >= readCount)
	    break;
	}
    if (readsGenerated >= readCount)
        break;
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dnaUtilOpen();
chromSize = optionInt("chromSize", chromSize);
chromCount = optionInt("chromCount", chromCount);
insPerRead = optionInt("insPerRead", insPerRead);
if (insPerRead > 1)
   errAbort("Sorry, currently can only do up to one insert per read.");
delPerRead = optionInt("delPerRead", delPerRead);
if (delPerRead > 1)
   errAbort("Sorry, currently can only do up to one deletion per read.");
subPerRead = optionInt("subPerRead", subPerRead);
separateMutations = optionExists("separateMutations");
spaceForDel = optionExists("spaceForDel");
stepSize = optionInt("stepSize", stepSize);
readSize = optionInt("readSize", readSize);
readCount = chromCount * (chromSize-readSize+1) / stepSize;
readCount = optionInt("readCount", readCount);
existingGenome = optionExists("existingGenome");
if (argc != 3)
    usage();
splatTestSet(argv[1], argv[2]);
return 0;
}
