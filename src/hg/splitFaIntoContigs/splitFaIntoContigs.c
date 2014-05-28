/*
splitFaIntoContigs - take a .agp file and a .fa file and a split size in kilobases, 
and split each chromosomes into subdirs and files for each supercontig.
*/

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "agpGap.h"
#include "agpFrag.h"
#include "options.h"
#include "portable.h"


/* Default array size for file paths */
#define DEFAULT_PATH_SIZE 1024

/* Default array size for normal char arrays */
#define BUF_SIZE 128

/* 
Flag showing that we have a non-bridged gap, indicating
that we can split a sequence at the end of it into supercontigs
*/
static const char *NO = "no";

/* The default size at which we split the genome on a non-bridged contig gap bound */
int _nSize = 1000000;
/* The size at which we split the genome on a bridged or non-bridged contig gap bound */
int _bSize = 0;
/* The size at which we split the genome on any contig fragment or gap bound */
int _aSize = 0;

/*
The top-level dir where we are sticking all the split up data files
*/
static char outputDir[DEFAULT_PATH_SIZE];

/*
 Struct to hold either gap or frag data depending on what
was read from the AGP file.

 */
struct agpData
{
/* Flag to indicate either base gap or valid fragment */
    boolean isGap;

/* Flag to indicate whether this data entry is the last in
   a supercontig */
    boolean endOfContig;

/* Pointer to prev in doubly-linked list */
    struct agpData *prev;
/* Pointer to next in doubly-linked list */
    struct agpData *next;

/* Name of the contig that this agp entry starts */
    char contigName[BUF_SIZE];

/* Union to hold either and agpGap or agpFrag, depending on the isGap flag. */
    union
    {
	struct agpGap *pGap;
	struct agpFrag *pFrag;
    } data;
};

void usage()
/* 
Explain usage and exit. 
*/
{
fflush(stdout);
    printf(
      "\nusage:\n"
      "\tsplitFaIntoContigs in.agp in.fa outputDir [-nSize=N (default 100000)] [-bSize=N] [-aSize=N]\n"
      "\n"
      "splitFaIntoContigs - takes a .agp file, a .fa file, and an output directory in which to save the data \n"
      " along with hierarchical size parameters into which to split each chromosome of the fa file.\n"
      "The following parameters may be used alone or in combination: "
      "\n -nSize (non-bridged size) refers to the minimum size at which we want to split after NON-BRIDGED contig boundaries"
      "\n -bSize (bridged size) refers to the minimum size at which we want to split after BRIDGED contig boundaries"
      "\n -aSize (any fragment size) refers to the size after which we want to split no matter what (i.e., no contig boundary is available)\n"
      "\nWhen given more than one of these options, splitFaIntoContigs will first try to split at the nSize number on a non-bridged contig.\n"
      "If it reaches the bSize number without finding a non-bridged contig, it will try to split on a non-bridged or bridged contig.\n"
      "Finally, if given an aSize param, it will always split after that fragment if no prior suitable bridged or\n"
      " non-bridged contig split boundary has been found.\n"
      "NOTE: aSize > bSize > nSize must hold true\n\n");
    exit(-1);
}

void writeLiftData(struct agpData *startData, int chromSize, int contigSize, FILE *fp, char sep, char lastSep)
{
fprintf(fp, "%d%c", startData->data.pGap->chromStart, sep);
fprintf(fp, "%s/%s%c", &(startData->data.pGap->chrom[3]), startData->contigName, sep);
fprintf(fp, "%d%c", contigSize, sep);
fprintf(fp, "%s%c", startData->data.pGap->chrom, sep);
fprintf(fp, "%d%c", chromSize, lastSep);
}

void writeListData(struct agpData *agpData, FILE *fp)
/* 
Writes the ordered.lst output lift file 

param startData - The contig that we are writing data for
param fp - The file pointer we are writing to
*/
{
fprintf(fp, "%s\n", agpData->contigName);
}

void writeListOutData(struct agpData *agpData, FILE *fp)
/* 
Writes the oOut.lst output lift file 

param startData - The contig that we are writing data for
param fp - The file pointer we are writing to
*/
{
fprintf(fp, "%s/%s.fa.out\n", agpData->contigName, agpData->contigName);
}

void writeLiftFiles(char *chromName, int chromSize, struct agpData *startAgpData, char *destDir)
/*
Writes the lift and list files out for a single chromsome

param chromName - The name of the chromsome.
param chromSize - The number of bases in this chromosome.
param startGap - Pointer to the dna gap or fragment at which we are starting to
 write data. The data will include the contents of this gap/frag.
param destDir - The destination dir to which to write the agp file.
 */
{
char liftDir[DEFAULT_PATH_SIZE];
char filename[DEFAULT_PATH_SIZE];
struct agpData *loopStartData = NULL;
struct agpData *curData = NULL;
FILE *fpLft = NULL;
FILE *fpLst = NULL;
FILE *fpLstOut = NULL;

int contigSize = 0;

sprintf(liftDir, "%s/lift", destDir);
makeDir(liftDir);

sprintf(filename, "%s/lift/ordered.lft", destDir);
fpLft = fopen(filename, "w");

sprintf(filename, "%s/lift/ordered.lst", destDir);
fpLst = fopen(filename, "w");

sprintf(filename, "%s/lift/oOut.lst", destDir);
fpLstOut = fopen(filename, "w");

curData = startAgpData;
while (NULL != curData) 
    {
    if (NULL == loopStartData)
	{
	loopStartData = curData;
	}

    if (curData->endOfContig)
	{
	contigSize = curData->data.pGap->chromEnd - loopStartData->data.pGap->chromStart;
	writeLiftData(loopStartData, chromSize, contigSize, fpLft, '\t', '\n');
	writeListData(loopStartData, fpLst);
	writeListOutData(loopStartData, fpLstOut);
	loopStartData = NULL;
	}

    curData = curData->next;
    }

fclose(fpLft);
fclose(fpLst);
fclose(fpLstOut);
}

void writeAgpFile(char *chromName, struct agpData *startAgpData, char *filename)
/*
Writes the agp file out for a single chromsome

param chromName - The name of the chromsome.
param startGap - Pointer to the dna gap or fragment at which we are starting to
 write data. The data will include the contents of this gap/frag.
param filename - The file to write.
 */
{
struct agpData *curData = NULL;
FILE *fp = NULL;

fp = fopen(filename, "w");
/*printf("Writing agp file %s for chromo %s\n", filename, startAgpData->data.pGap->chrom);*/

curData = startAgpData;
while (NULL != curData) 
    {
    if (curData->isGap)
	{
	 /* Undo the decrement we did earlier.
	    This was done in nextAgpEntryToSplitOn() in order
	    to be compatible with the 0-based frag addressing scheme.
	  */
	curData->data.pGap->chromStart++;
	agpGapOutput(curData->data.pGap, fp, '\t', '\n');
	/* Redo the above decrement - don't want to create side effects */
	curData->data.pGap->chromStart--;
	}
    else
	{
	agpFragOutput(curData->data.pFrag, fp, '\t', '\n');
	}

    curData = curData->next;
    }

fclose(fp);
}

void writeChromAgpFile(char *chromName, struct agpData *startAgpData, char *destDir)
/*
Writes the agp file out for a single chromsome

param chromName - The name of the chromsome.
param startGap - Pointer to the dna gap or fragment at which we are starting to
 write data. The data will include the contents of this gap/frag.
param destDir - The destination dir to which to write the agp file.
 */
{
char filename[DEFAULT_PATH_SIZE];

sprintf(filename, "%s/%s.agp", destDir, chromName);
printf("Writing chromosome agp file %s\n", filename);
writeAgpFile(chromName, startAgpData, filename);
}

void writeSuperContigAgpFile(struct agpData *startAgpData, struct agpData *endAgpData, char *filename, int sequenceNum)
/*
Creates an agp file containing the contents of a supercontig in agp format.

param startAgpData - Pointer to the dna gap or fragment at which we are starting to
 write data. The data will include the contents of this gap/frag.
param endAgpData - Pointer to the dna gap or fragment at which we are stopping to
 write data. The data will include the contents of this gap/frag.
param filename - The file name to which to write.
param sequenceNum - The 1-based number of this clone supercontig in the chromsome.
 */
{
printf("Writing supercontig agp file %s\n", filename);
writeAgpFile( endAgpData->data.pGap->chrom, startAgpData, filename);
}

void writeChromFaFile(char *chromName, char *dna, int dnaSize, char *destDir)
/*
Writes the contents of a single chromsome out to a file in FASTA format.

param chromName - The name of the chromosome for which we are writing
 the fa file.
param dna - Pointer to the dna array.
param dnaSize - The size of the dna array.
 */
{
char filename [DEFAULT_PATH_SIZE];

sprintf(filename, "%s/%s.fa", destDir, chromName);
printf("Writing fa file %s for chromosome %s\n", filename, chromName);
faWrite(filename, chromName, dna, dnaSize);
}

void writeSuperContigFaFile(DNA *dna, struct agpData *startData, struct agpData *endData, char *filename, int sequenceNum)
/*
Creates a fasta file containing the contents of a supercontig in FASTA format.

param dna - Pointer to the dna array.
param startData - Pointer to the dna gap or fragment at which we are starting to
 write data. The data will include the contents of this gap/frag.
param end - Pointer to the dna gap or fragment at which we are stopping to
 write data. The data will include the contents of this gap/frag.
param filename - The file name to which to write.
param sequenceNum - The 1-based number of this clone supercontig in the chromsome.
 */
{
int startOffset = startData->data.pGap->chromStart;
int endOffset = endData->data.pGap->chromEnd;
int dnaSize = 0;
char sequenceName[BUF_SIZE];

printf("Writing supercontig fa file %s\n", filename);
// starting at dna[%d] up to but not including dna[%d]\n", filename, startOffset, endOffset);
sprintf(sequenceName, "%s_%d %d-%d", startData->data.pGap->chrom, sequenceNum, startOffset, endOffset);
dnaSize = endOffset - startOffset;
faWrite(filename, sequenceName, &dna[startOffset], dnaSize);
}

struct agpData* nextAgpEntryToSplitOn(struct lineFile *lfAgpFile, int dnaSize, struct agpData **retStartData)
/*
Finds the next agp entry in the agp file at which to split on.

param lfAgpFile - The .agp file we are examining.
param dnaSize - The total size of the chromsome's dna sequence 
 we are splitting up. Used to prevent overrun of the algorithm
 that looks at agp entries.
param retStartData - An out param returning the starting(inclusive) gap that we
 will start to split on.

return struct agpData* - The ending (inclusive) agp data we are to split on.
 */
{
int startIndex = 0;
int numBasesRead = 0;
char *line = NULL;
char *words[9];
int lineSize = 0;
struct agpGap *agpGap = NULL;
struct agpFrag *agpFrag = NULL;
struct agpData *curAgpData = NULL;
struct agpData *prevAgpData = NULL;
boolean splitPointFound = FALSE;
int splitSize = _nSize;

do 
{
lineFileNext(lfAgpFile, &line, &lineSize);

if (line[0] == '#' || line[0] == '\n')
    {
    continue;
    }

AllocVar(curAgpData);
curAgpData->endOfContig = FALSE;
curAgpData->isGap = FALSE;
curAgpData->prev = NULL;
curAgpData->next = NULL;

chopLine(line, words);
if (words[4][0] == 'N' || words[4][0] == 'U')
    {
    agpGap = agpGapLoad(words);
    /* 
      Decrement the chromStart index since that's how the agpFrags do it
       and we want to use 0-based addressing
    */
    --(agpGap->chromStart);

    if (0 == startIndex)
	{
        startIndex = agpGap->chromStart;
        }

    if (numBasesRead >= _bSize)
        {
        splitPointFound = TRUE;
        }
    else
        {
        /* Split points are made after non-bridged contigs only here */
        splitPointFound = (0 == strcasecmp(agpGap->bridge, NO));
        }
     
    curAgpData->isGap = TRUE;
    curAgpData->data.pGap = agpGap;
    }
else
    {
    agpFrag = agpFragLoad(words);
    // file is 1-based but agpFragLoad() now assumes 0-based:
    agpFrag->chromStart -= 1;
    agpFrag->fragStart  -= 1;
    /* If we find a fragment and not a gap */
    if (0 == startIndex)
        {
        startIndex = agpFrag->chromStart;
        }

    if (numBasesRead >= _aSize) 
        {
        splitPointFound = TRUE;
        }
    else
        {
        splitPointFound = FALSE;
        }

    curAgpData->isGap = FALSE;
    curAgpData->data.pFrag = agpFrag;
    }

/* Since this our first loop iteration,
 save the start gap as the beginning of the section to write out */
if (NULL == prevAgpData) 
    {    
    *retStartData = curAgpData; /* Save the pointer to the head of the list */
    }
else
    {
    /* Build a doubly linked list for use elsewhere */
    prevAgpData->next = curAgpData;
    curAgpData->prev = prevAgpData;
    }

prevAgpData = curAgpData;
numBasesRead = curAgpData->data.pGap->chromEnd - startIndex;

} while ((numBasesRead < splitSize || !splitPointFound)
       && curAgpData->data.pGap->chromEnd < dnaSize);

curAgpData->next = NULL; /* Terminate the linked list */
curAgpData->endOfContig = TRUE;
return curAgpData;
}

struct agpData* makeSuperContigs(struct lineFile *agpFile, DNA *dna, int dnaSize, char *destDir)
/*
Makes supercontig files for each chromosome

param agpFile - The agpFile we are examining.
param dna - The dna sequence we are splitting up.
param dnaSize - The size of the dna sequence we are splitting up.
param destDir - The destination directory where to store files.

return startAgpData - The first agp entry in the sequence.
 */
{
struct agpData *startAgpData = NULL;
struct agpData *endAgpData = NULL;
struct agpData *prevAgpData = NULL;
struct agpData *startChromAgpData = NULL;

char filename[DEFAULT_PATH_SIZE];
char contigDir[DEFAULT_PATH_SIZE];
int sequenceNum = 0;

do
    {
    endAgpData = nextAgpEntryToSplitOn(agpFile, dnaSize, &startAgpData);
    /* Point the end of the previous loop iteration's linked list at
       the start of this new one */
    if (NULL != prevAgpData)
	{
	prevAgpData->next = startAgpData;
	startAgpData->prev = prevAgpData;
	}
    prevAgpData = endAgpData;

    sequenceNum++;
    sprintf(contigDir, "%s/%s_%d", destDir, startAgpData->data.pGap->chrom, sequenceNum);
    makeDir(contigDir);

    sprintf(startAgpData->contigName, "%s_%d", startAgpData->data.pGap->chrom, sequenceNum);

    sprintf(filename, "%s/%s_%d.fa", contigDir, startAgpData->data.pGap->chrom, sequenceNum);
    writeSuperContigFaFile(dna, startAgpData, endAgpData, filename, sequenceNum);

    sprintf(filename, "%s/%s_%d.agp", contigDir, startAgpData->data.pGap->chrom, sequenceNum);
    writeSuperContigAgpFile(startAgpData, endAgpData, filename, sequenceNum);

    /* Save the start of the whole chromosome */
    if (NULL == startChromAgpData)
	{
	startChromAgpData = startAgpData;
	}
    } while (endAgpData->data.pGap->chromEnd < dnaSize);

return startChromAgpData;
}

void splitFaIntoContigs(char *agpFile, char *faFile)
/* 
splitFaIntoContigs - read the .agp file the .fa file. and split each
 chromsome into supercontigs at non-bridged sections.

param agpFile - The pathname of the agp file to check.
param faFile - The pathname of the fasta file to check.
*/
{
struct lineFile *lfAgp = lineFileOpen(agpFile, TRUE);
struct lineFile *lfFa = lineFileOpen(faFile, TRUE);
int dnaSize = 0;
char *chromName = NULL;
DNA *dna = NULL;
struct agpData *startAgpData = NULL;
char destDir[DEFAULT_PATH_SIZE];

printf("Processing agpFile %s and fasta file %s, with nonbridged split boundaries of %d bases, bridged split boundaries of %d bases\n", agpFile, faFile, _nSize, _bSize);

/* For each chromosome entry */
while (faMixedSpeedReadNext(lfFa, &dna, &dnaSize, &chromName))
    {
    printf("\nProcessing data for Chromosome: %s, size: %d\n", chromName, dnaSize);

    /* Make the output directory. Strip off the leading "chr" prefix.
       output filename = outputDir/chromName/chromFrag/chromFrag.xxx
       example:
       outputDir = "output"
       chromName = "chr1" - we strip off the "chr"
       chromFrag = "chr1_1"
       result    = "output/1/chr1_1/chr1_1.fa"
     */


    sprintf(destDir, "%s/%s", outputDir, &chromName[3]);
    makeDir(destDir);

    startAgpData = makeSuperContigs(lfAgp, dna, dnaSize, destDir);
    writeChromFaFile(chromName, dna, dnaSize, destDir);
    writeChromAgpFile(chromName, startAgpData, destDir);
    writeLiftFiles(chromName, dnaSize, startAgpData, destDir);

    printf("Done processing chromosome %s\n", chromName);
    }

printf("Done processing agpFile %s and fasta file %s, with nonbridged split boundaries of %d, bridged split boundaries of %d, absolute split boundaries of %d  bases\n", agpFile, faFile, _nSize, _bSize, _aSize);
}

int main(int argc, char *argv[])
/* 
Process command line then delegate main work to splitFaIntoContigs().
*/
{
char *arg = NULL;

/* Turn off I/O buffering for more sequential output */
setbuf(stdout, NULL);

optionHash(&argc, argv);

if (argc < 4 || argc > 6)
    {
    usage();
    }

arg = optionVal("nSize", NULL);
if (NULL != arg && atoi(arg) > 0)
    {
    _nSize = atoi(arg);
    }
else if (NULL != arg)
    {
    usage();
    }

arg = optionVal("bSize", NULL);
if (NULL != arg && atoi(arg) > 0)
    {
    _bSize = atoi(arg);
    }
else if (NULL != arg)
    {
    usage();
    }

arg = optionVal("aSize", NULL);
if (NULL != arg && atoi(arg) > 0)
    {
    _aSize = atoi(arg);
    }
else if (NULL != arg)
    {
    usage();
    }

if ((_aSize != 0 && _aSize <= _bSize) 
 || (_bSize != 0 && _bSize <= _nSize)
 || (_aSize != 0 && _aSize <= _nSize))
    {
    usage();
    }

strcpy(outputDir, argv[3]);
makeDir(outputDir);

splitFaIntoContigs(argv[1], argv[2]);
faFreeFastBuf();
return 0;
}
