/* bwana.c - Do C. Briggsae alignment. */
#include "common.h"
#include "memalloc.h"
#include "obscure.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nt4.h"
#include "wormdna.h"
#include "cda.h"
#include "crudeali.h"
#include "fuzzyFind.h"
#include "htmshell.h"
#include "cheapcgi.h"

int chromCount;
char **chromNames;
struct nt4Seq **chrom;

void reportCrudeAli(struct crudeAli *caList, FILE *out, int max)
/* Print vital info about alignment to file. */
{
struct crudeAli *ca;
struct wormFeature *cosmidList = NULL, *cosmid;
int count = 0;

fprintf(out, "Got %d crude alignments\n", slCount(caList));
for (ca = caList; ca != NULL; ca = ca->next)
    {
    if (++count > max)
	{	
	fprintf(stdout, "only showing %d of %d\n", count, slCount(caList));
	fprintf(out, "only showing %d of %d\n", count, slCount(caList));
	break;
	}
    fprintf(out, "%d %s:%d-%d %c ", ca->score, chromNames[ca->chromIx], ca->start, ca->end, ca->strand);
    cosmidList = wormCosmidsInRange(chromNames[ca->chromIx], ca->start, ca->end);
    for (cosmid = cosmidList; cosmid != NULL; cosmid = cosmid->next)
        fprintf(out, "%s ", cosmid->name);
    slFreeList(&cosmidList);
    fprintf(out, "\n");
    }
fprintf(out, "\n");
}

void usage()
/* Explain what program is and how to use it. */
{
errAbort("bwana - do batch coarse alignment of C. briggsae and C. elegans genomes.\n"
         "usage:\n"
         "     bwana lots outFile file-with-list-of-fa-files\n"
         "     bwana few outFile file1.fa file2.fa ... fileN.fa\n"
         "     bwana frag outFile file.fa start end\n");
}


void alignSlice(struct dnaSeq *seq, int start, int end, FILE *out)
/* Align part of sequence to C. elegans. */
{
struct crudeAli *ca;
ca = crudeAliFind(seq->dna + start, end-start, chrom, chromCount, 8, 45);
if (ca)
    {
    reportCrudeAli(ca, out, 100);
    printf("Best alignment to %s:%d-%d score %d\n", 
        chromNames[ca->chromIx], ca->start, ca->end, ca->score);
    slFreeList(&ca);
    }
else
    {
    fprintf(out, "No alignment\n");
    printf("No alignment\n");
    }
fflush(out);
}

void alignOneFa(char *faFileName, FILE *out, int fileIx, int fileCount)
/* Align one FA file to C. elegans. */
{
struct dnaSeq *seq;
int start, end, winSize = 2000, stepSize = 1000;

seq = faReadDna(faFileName);
printf("Processing %s (%d bases)\n", faFileName, seq->size);
fprintf(out, "Processing %s (%d bases)\n", faFileName, seq->size);

for (start = 0; start < seq->size; start += stepSize)
    {
    end = start +  winSize;
    if (end > seq->size)
        end = seq->size;
    fprintf(stdout, "Aligning bases %d to %d of %s file %d of %d\n", 
        start, end,  faFileName, fileIx+1, fileCount);
    fprintf(out, "Aligning bases %d to %d of %s\n", start, end,  faFileName);
    alignSlice(seq, start, end, out);
    }                                   
fprintf(out, "\n");
freeDnaSeq(&seq);
}

void printInLines(FILE *out, DNA *dna, int size, int maxLineSize)
/* Print DNA to file broken up into lines. */
{
int lineSize;
while (size > 0)
    {
    lineSize = size;
    if (lineSize > maxLineSize)
        lineSize = maxLineSize;
    mustWrite(out, dna, lineSize);
    fputc('\n', out);
    dna += lineSize;
    size -= lineSize;
    }
}

void alignFrag(char *faFileName, int start, int end, FILE *out)
/* Align a fragment from one FA file. */
{
struct dnaSeq *seq;
struct crudeAli *caList, *ca;

if (start >= end)
    usage();
printf("Reading %s\n", faFileName);
seq = faReadDna(faFileName);
if (end > seq->size)
    errAbort("%s only has %d bases\n", seq->size);
printf("Aligning...\n");
caList = crudeAliFind(seq->dna + start, end-start, chrom, chromCount, 8, 45);
if (caList)
    {
    for (ca = caList; ca != NULL; ca = ca->next)
        {
        DNA *dna;
        int size = ca->end - ca->start;
        char *chrom = chromNames[ca->chromIx];
        fprintf(out, ">%s:%d-%d %c %d\n",
            chrom, ca->start, ca->end, ca->strand, ca->score);
        dna = wormChromPart(chrom, ca->start, size);
        if (ca->strand == '-')
            reverseComplement(dna, size);
        printInLines(out, dna, size, 50);
        freeMem(dna);
        }
    printf("Best alignment to %s:%d-%d score %d\n", 
        chromNames[caList->chromIx], caList->start, caList->end, caList->score);
    slFreeList(&caList);
    }
else
    {
    fprintf(out, "No alignment\n");
    printf("No alignment\n");
    }
freeDnaSeq(&seq);
}


int main(int argc, char *argv[])
{
char *command;
char **faFiles;
int faCount;
char *wordBuf = NULL;
char **words = NULL;
int i;
char *outName;
FILE *out; 
boolean doFrag = FALSE;
//pushCarefulMemHandler();

/* Check command line and obtain list of FA files to align. */
if (argc < 4)
    usage();
command = argv[1];
outName =argv[2];
if (sameWord(command, "few"))
    {
    faFiles = argv+3;
    faCount = argc-3;
    }
else if (sameWord(command, "frag"))
    {
    if (argc < 6 || !isdigit(argv[4][0]) || !isdigit(argv[5][0]))
        usage();
    doFrag = TRUE;
    }
else if (sameWord(command, "lots"))
    {
    readAllWords(argv[3], &faFiles, &faCount, &wordBuf);
    words = faFiles;
    }
else
    usage();

/* Get chromosomes and ready for DNA processing. */
dnaUtilOpen();
printf("Loading C. elegans genome.\n");
wormChromNames(&chromNames, &chromCount);
wormLoadNt4Genome(&chrom, &chromCount);

out = mustOpen(outName, "w");
if (doFrag)
    alignFrag(argv[3], atoi(argv[4]), atoi(argv[5]), out);
else
    {
    for (i=0; i<faCount; ++i)
        {
        printf("File %d of %d\n", i+1, faCount);
        alignOneFa(faFiles[i], out, i, faCount);
        fflush(out);
        }
    }
fclose(out);
freeMem(words);
freeMem(wordBuf);
//carefulCheckHeap();
return 0;
}
