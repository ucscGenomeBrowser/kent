/*
splitFaIntoContigs - take a .agp file and a .fa file and a split size in kilobases, and split each chromosomes into subdirs and files for each supercontig
*/

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "agpGap.h"
#include "agpFrag.h"

/* 
Flag showing that we have a non-bridged gap, indicating
that we can split a sequence here into supercontigs
*/
static const char *NO = "no";

/* Default size of the files into which we are splitting fa entries */
static const int defaultSize = 1000000;

/*
The top-level dir where we are sticking all the split up data files
*/
static char destDir[256];

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

void createSplitFile(DNA *dna, struct agpGap *startGap, struct agpGap *endGap)
{
int startOffset = startGap->chromStart;
int endOffset = endGap->chromEnd;
int i = 0;
FILE *fp = NULL;
char filename[32];
char command[32];
static int sequenceNum = 0;

printf("Writing gap file for chromo %s\n", endGap->chrom);
printf("Writing file starting at dna[%d] up to but not including dna[%d]\n", startOffset, endOffset);

sprintf(command, "mkdir -p %s", destDir);
sprintf(command, "mkdir -p %s/%s", destDir, startGap->chrom);
system(command);

/*
filename = storageDir/chromName/sequenceNum:startOffset->endOffset
*/

sprintf(filename, "%s/%s/%d:%d->%d", destDir, startGap->chrom, ++sequenceNum, startOffset, endOffset);
printf("Filename = %s\n", filename);
fp = fopen(filename, "w");

if (0 == startGap->chromStart)
    {
    fputs(">", fp);
    fputs(startGap->chrom, fp);
    fputs("\n", fp);
    }

for (i = startOffset; i < endOffset; i++)
    {
    fputc(dna[i], fp);
    }

fputs("\n", fp);
}

struct agpGap* nextAgpEntryToSplitOn(struct lineFile *lfAgpFile, int dnaSize, int splitSize, struct agpGap **retStartGap)
{
int startIndex = 0;
int numBasesRead = 0;
char *line = NULL;
char *words[9];
int lineSize = 0;
struct agpGap *agpGap = NULL;
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
    if (0 == startIndex)
        {
        startIndex = agpFrag->chromStart;
        }

    /* Using cast here to fake polymorphism */
    agpGap = (struct agpGap*) agpFrag;
    splitPointFound = FALSE;
    }

/* Save the start gap as the beginning of the section to write out */
if (NULL == *retStartGap) 
    {
    *retStartGap = agpGap;
    }

numBasesRead = agpGap->chromEnd - startIndex;
} while ((numBasesRead < splitSize || !splitPointFound)
             && agpGap->chromEnd < dnaSize);

return agpGap;
}

void makeSuperContigs(struct lineFile *agpFile, DNA *dna, int dnaSize, int splitSize)
{
struct agpGap *startAgpGap = NULL;
struct agpGap *endAgpGap = NULL;

do
    {
    endAgpGap = nextAgpEntryToSplitOn(agpFile, dnaSize, splitSize, &startAgpGap);
    createSplitFile(dna, startAgpGap, endAgpGap);
    /* TODO: Free the start gap if not null */
    startAgpGap = NULL;
    } while (endAgpGap->chromEnd < dnaSize);
}

void splitFaIntoContigs(char *agpFile, char *faFile, int splitSize)
/* 
splitFaIntoContigs - read the .agp file and make sure that it agrees with the .fa file. 

param agpFile - The pathname of the agp file to check.
param faFile - The pathname of the fasta file to check.

exceptions - this function aborts if it detects an entry where the agp and fasta
files do not agree
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
    printf("\nAnalyzing data for Chromosome: %s, size: %d\n", chromName, dnaSize);
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

cgiSpoof(&argc, argv);

if (4 != argc && 5 != argc)
    {
    usage();
    }

strcpy(destDir, argv[3]);

if (5 == argc) 
    {
    /* TODO: ensure that this arg is a number/integer */
    size = atoi(argv[4]);
    size *= 1000;
    }

/* Turn off I/O buffering for more sequential output */
setbuf(stdout, NULL);

splitFaIntoContigs(argv[1], argv[2], size);
return 0;
}
