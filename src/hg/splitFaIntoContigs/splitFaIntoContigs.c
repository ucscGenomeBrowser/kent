/*
splitFaIntoContigs - take a .agp file and a .fa file and a split size in kilobases, 
and split each chromosomes into subdirs and files for each supercontig.
*/

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "agpGap.h"
#include "agpFrag.h"

/* Default array size for file paths */
#define DEFAULT_PATH_SIZE 1024

/* 
Flag showing that we have a non-bridged gap, indicating
that we can split a sequence at the end of it into supercontigs
*/
static const char *NO = "no";

/* Default size of the files into which we are splitting fa entries */
static const int defaultSize = 1000000;

/*
The top-level dir where we are sticking all the split up data files
*/
static char outputDir[DEFAULT_PATH_SIZE];

void usage()
/* 
Explain usage and exit. 
*/
{
fflush(stdout);
    errAbort(
      "\nsplitFaIntoContigs - takes a .agp file and .fa file a destination directory in which to save data and a size (default 1Mbase) into which to split each chromosome of the fa file along non-bridged gaps\n"
      "usage:\n\n"
      "   splitFaIntoContigs in.agp in.fa storageDir [approx size in kilobases (default 1000)]\n"
      "\n");
}

void writeChromLiftFiles(char *chromName)
/*
Writes the lift and list files out for a single chromsome

param chromName - The name of the chromsome for which we are 
 writing the agp entries
 */
{

}

void writeChromAgpFile(char *chromName)
/*
Writes the agp file out for a single chromsome

param chromName - The name of the chromsome for which we are 
 writing the agp entries
 */
{

}

void writeChromFaFile(char *chromName, char *dna, int dnaSize)
/*
Writes the contents of a single chromsome out to a file in FASTA format.

param chromName - The name of the chromosome for which we are writing
 the fa file.
param dna - Pointer to the dna array.
param dnaSize - The size of the dna array.
 */
{
char command[DEFAULT_PATH_SIZE];
char destDir[DEFAULT_PATH_SIZE];
char filename [DEFAULT_PATH_SIZE];
int i = 0;

/* Strip off the leading "chr" prefix */
sprintf(destDir, "%s/%s", outputDir, &chromName[3]);
sprintf(command, "mkdir -p %s", destDir);
system(command);

sprintf(filename, "%s/%s.fa", destDir, chromName);
faWrite(filename, chromName, dna, dnaSize);
}

void createSuperContigAgpFile(DNA *dna, struct agpGap *startGap, struct agpGap *endGap)
/*
Creates an agp file containing the contents of a supercontig in agp format.

param dna - Pointer to the dna array.
param startGap - Pointer to the dna gap or fragment at which we are starting to
 write data. The data will include the contents of this gap/frag.
param endGap - Pointer to the dna gap or fragment at which we are stopping to
 write data. The data will include the contents of this gap/frag.
 */
{
int startOffset = startGap->chromStart;
int endOffset = endGap->chromEnd;
int i = 0;
char filename[DEFAULT_PATH_SIZE];
char command[DEFAULT_PATH_SIZE];
static int sequenceNum = 0;
char destDir[DEFAULT_PATH_SIZE];
int dnaSize = 0;
char sequenceName[DEFAULT_PATH_SIZE];

printf("Writing gap file for chromo %s\n", endGap->chrom);

/*
filename = outputDir/chromName/chromFrag/chromFrag.fa
example:

outputDir = output
chromName = chr1 - we strip off the chr
chromFrag = chr1_1
output/1/chr1_1/chr1_1.fa
*/

if (0 == startGap->chromStart)
    {
    /* Restart the sequence number since we are now
       in a new chromosome*/
    sequenceNum = 0;
    }

++sequenceNum;
sprintf(destDir, "%s/%s/%s_%d", outputDir, &(startGap->chrom[3]), startGap->chrom, sequenceNum);
sprintf(command, "mkdir -p %s", destDir);
system(command);
sprintf(filename, "%s/%s_%d.fa", destDir, startGap->chrom, sequenceNum);
printf("Filename = %s\n", filename);
printf("Writing file starting at dna[%d] up to but not including dna[%d]\n", startOffset, endOffset);

sprintf(sequenceName, "%s_%d %d-%d", startGap->chrom, sequenceNum, startOffset, endOffset);
dnaSize = endOffset - startOffset;
faWrite(filename, sequenceName, &dna[startOffset], dnaSize);
}

void createSuperContigFaFile(DNA *dna, struct agpGap *startGap, struct agpGap *endGap)
/*
Creates a fasta file containing the contents of a supercontig in FASTA format.

param dna - Pointer to the dna array.
param startGap - Pointer to the dna gap or fragment at which we are starting to
 write data. The data will include the contents of this gap/frag.
param endGap - Pointer to the dna gap or fragment at which we are stopping to
 write data. The data will include the contents of this gap/frag.
 */
{
int startOffset = startGap->chromStart;
int endOffset = endGap->chromEnd;
int i = 0;
char filename[DEFAULT_PATH_SIZE];
char command[DEFAULT_PATH_SIZE];
static int sequenceNum = 0;
char destDir[DEFAULT_PATH_SIZE];
int dnaSize = 0;
char sequenceName[DEFAULT_PATH_SIZE];

printf("Writing gap file for chromo %s\n", endGap->chrom);

/*
filename = outputDir/chromName/chromFrag/chromFrag.fa
example:

outputDir = output
chromName = chr1 - we strip off the chr
chromFrag = chr1_1
output/1/chr1_1/chr1_1.fa
*/

if (0 == startGap->chromStart)
    {
    /* Restart the sequence number since we are now
       in a new chromosome*/
    sequenceNum = 0;
    }

++sequenceNum;
sprintf(destDir, "%s/%s/%s_%d", outputDir, &(startGap->chrom[3]), startGap->chrom, sequenceNum);
sprintf(command, "mkdir -p %s", destDir);
system(command);
sprintf(filename, "%s/%s_%d.fa", destDir, startGap->chrom, sequenceNum);
printf("Filename = %s\n", filename);
printf("Writing file starting at dna[%d] up to but not including dna[%d]\n", startOffset, endOffset);

sprintf(sequenceName, "%s_%d %d-%d", startGap->chrom, sequenceNum, startOffset, endOffset);
dnaSize = endOffset - startOffset;
faWrite(filename, sequenceName, &dna[startOffset], dnaSize);
}

struct agpGap* nextAgpEntryToSplitOn(struct lineFile *lfAgpFile, int dnaSize, int splitSize, struct agpGap **retStartGap)
/*
Finds the next agp entry in the agp file at which to split on.

param lfAgpFile - The .agp file we are examining.
param dnaSize - The total size of the chromsome's dna sequence 
 we are splitting up. Used to prevent overrun of the algorithm
 that looks at agp entries.
param splitSize - The size of the split fragments that we are
 trying to make.
param retGapStart - An out param returning the starting(inclusive) gap that we
 will start to split on.

return struct agpGap* - The ending (inclusive) agp gap we are to split on.
 */
{
int startIndex = 0;
int numBasesRead = 0;
char *line = NULL;
char *words[9];
int lineSize = 0;
struct agpGap *agpGap = NULL;
struct agpGap *prevAgpGap = NULL;
struct agpFrag *agpFrag = NULL;
boolean splitPointFound = FALSE;

do 
{
lineFileNext(lfAgpFile, &line, &lineSize);

if (line[0] == '#' || line[0] == '\n')
    {
    continue;
    }

chopLine(line, words);

if ('N' == words[4][0])
    {
    agpGap = agpGapLoad(words);
    if (0 == startIndex)
        {
        /* 
          Decrement this chromStart index since that's how the agpFrags do it
           and we want to use 0-based addressing
	*/
        startIndex = --(agpGap->chromStart);
        }
    splitPointFound = (0 == strcasecmp(agpGap->bridge, NO));
    }
else
    {
    agpFrag = agpFragLoad(words);
    /* If we find a fragment and not a gap we can't split there */
    splitPointFound = FALSE;

    if (0 == startIndex)
        {
        startIndex = agpFrag->chromStart;
        }

    /* Using cast here to fake polymorphism */
    agpGap = (struct agpGap*) agpFrag;
    }

/* Save the start gap as the beginning of the section to write out */
if (NULL == *retStartGap) 
    {
    *retStartGap = agpGap;
    }
else
    {
    /* Build a linked list for use elewhere */
    prevAgpGap->next = agpGap;
    }

prevAgpGap = agpGap;

/* TODO: Free non-used returned agpGap and agpFrag entries */
numBasesRead = agpGap->chromEnd - startIndex;
} while ((numBasesRead < splitSize || !splitPointFound)
             && agpGap->chromEnd < dnaSize);

agpGap->next = NULL; /* Terminate the linked list */
return agpGap;
}

void makeSuperContigs(struct lineFile *agpFile, DNA *dna, int dnaSize, int splitSize)
/*
Makes supercontig files for each chromosome

param agpFile - The agpFile we are examining.
param dna - The dna sequence we are splitting up.
param dnaSize - The size of the dna sequence we are splitting up.
param splitSize - The sizes of the supercontigs we are splitting the 
 dna into.
 */
{
struct agpGap *startAgpGap = NULL;
struct agpGap *endAgpGap = NULL;

do
    {
    /* TODO: free endAgpGap if not NULL */
    endAgpGap = nextAgpEntryToSplitOn(agpFile, dnaSize, splitSize, &startAgpGap);
    createSuperContigFaFile(dna, startAgpGap, endAgpGap);
    createSuperContigAgpFile(dna, startAgpGap, endAgpGap);
    /* TODO: free startAgpGap */
    startAgpGap = NULL;
    } while (endAgpGap->chromEnd < dnaSize);
}

void splitFaIntoContigs(char *agpFile, char *faFile, int splitSize)
/* 
splitFaIntoContigs - read the .agp file the .fa file. and split each
 chromsome into supercontigs at non-bridged sections.

param agpFile - The pathname of the agp file to check.
param faFile - The pathname of the fasta file to check.
param splitSize - The sizes of the supercontigs we are splitting the
 dna into.
*/
{
struct lineFile *lfAgp = lineFileOpen(agpFile, TRUE);
struct lineFile *lfFa = lineFileOpen(faFile, TRUE);
int dnaSize = 0;
char *chromName = NULL;
DNA *dna = NULL;

printf("Processing agpFile %s and fasta file %s, with split boundaries of %d bases\n", agpFile, faFile, splitSize);

/* For each chromosome entry */
while (faSpeedReadNext(lfFa, &dna, &dnaSize, &chromName))
    {
    printf("\nProcessing data for Chromosome: %s, size: %d\n", chromName, dnaSize);
    writeChromFaFile(chromName, dna, dnaSize);
    writeChromAgpFile(chromName);
    makeSuperContigs(lfAgp, dna, dnaSize, splitSize);
    printf("Done processing chromosome %s\n", chromName);
    }

printf("Done processing agpFile %s and fasta file %s, with split boundaries of %d kbases\n", agpFile, faFile, splitSize);
}

int main(int argc, char *argv[])
/* 
Process command line then delegate main work to splitFaIntoContigs().
*/
{
int size = defaultSize;
char command[DEFAULT_PATH_SIZE];

cgiSpoof(&argc, argv);

if (4 != argc && 5 != argc)
    {
    usage();
    }

strcpy(outputDir, argv[3]);
sprintf(command, "mkdir -p %s", outputDir);
system(command);

if (5 == argc) 
    {
    /* TODO: ensure that this arg is a number/integer */
    size = atoi(argv[4]);
    size *= 1000;
    }

/* Turn off I/O buffering for more sequential output */
setbuf(stdout, NULL);

splitFaIntoContigs(argv[1], argv[2], size);
faFreeFastBuf();
return 0;
}
