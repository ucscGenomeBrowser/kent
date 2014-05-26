/* analyzeCpG - cluster CpG regions from an input bed file and perform some analysis on them. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "basicBed.h"
#include "sqlNum.h"

struct bedGraphRecord
{
    struct bedGraphRecord *next;
    
    char *chrom;
    unsigned chromStart;
    unsigned chromEnd;
    char *name;
    double score;
    char strand;
};

struct bedGraphRecord *bedGraphLoadNext(struct lineFile *lf)
{
char *row[6];
int rowSize = lineFileChopNext(lf, row, ArraySize(row));
    
if (rowSize == 0)
    return NULL;
    
struct bedGraphRecord *bg;
AllocVar(bg);
    
bg->chrom = cloneString(row[0]);
bg->chromStart = sqlUnsigned(row[1]);
bg->chromEnd = sqlUnsigned(row[2]);
bg->name = cloneString(row[3]);
bg->score = sqlFloat(row[4]);
bg->strand = row[5][0];

return bg;
}

int clClusterSize = 100;
int clMinCluster = 100;
boolean clHist = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "analyzeCpG - cluster CpG regions from an input bed file and perform some analysis on them\n"
  "usage:\n"
  "   analyzeCpG input.bed output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"clusterSize", OPTION_INT},
   {"minCluster", OPTION_INT},
   {"hist", OPTION_BOOLEAN}
};

void writeCluster(struct bedGraphRecord *clusterList, FILE *out)
{
int size = slCount(clusterList);

if (size < clMinCluster)
    return;

int blockStarts[size];
int blockSizes[size];
double score = 0;
struct bedGraphRecord *last = clusterList;

slReverse(&clusterList);
        
struct bed *outBed;
AllocVar(outBed);

outBed->chrom = cloneString(clusterList->chrom);
outBed->chromStart = clusterList->chromStart;
outBed->chromEnd = last->chromEnd;
outBed->name = "asdf";
outBed->strand[0] = clusterList->strand;

outBed->blockCount = size;

int i;
for (i = 0; i < size; i++)
    blockSizes[i] = 1;
outBed->blockSizes = blockSizes;

i = 0;
struct bedGraphRecord *cur;
for (cur = clusterList; cur != NULL; cur = cur->next)
    {
    blockStarts[i] = cur->chromStart - outBed->chromStart;
    score += cur->score;
    i++;
    }
outBed->chromStarts = blockStarts;
outBed->score = (int)(score * 10 / size);
        
//printf("printing %s %d %d", outBed->chrom, outBed->chromStart, outBed->chromEnd);
        
bedTabOutN(outBed, 12, out);   
}

void analyzeCpG(char *inputName, char *outputName)
/* analyzeCpG - cluster CpG regions from an input bed file and perform some analysis on them. */
{

// open input bed file
// struct bed *input = bedLoadAll(inputName);
struct lineFile *input = lineFileOpen(inputName, TRUE);
FILE *out = fopen(outputName, "w");

struct bedGraphRecord *plusPrev = NULL;
struct bedGraphRecord *minusPrev = NULL;

// set up lists
struct bedGraphRecord *plusClusters = NULL;
struct bedGraphRecord *minusClusters = NULL;

int hist[8096];
int i;
for (i = 0; i < 8096; i++)
    hist[i] = 0;

// loop over bed file
for (;;)
    {
    struct bedGraphRecord *record = bedGraphLoadNext(input);
    
    if (record == NULL)
        break;
    
    if (record->strand == '+')
        {
        //if (plusPrev != NULL)
         //   printf("+: cur %s %d, prev %s %d = %d", record->chrom, record->chromStart, plusPrev->chrom, plusPrev->chromStart, record->chromStart - plusPrev->chromStart);
        if (plusPrev != NULL && (strcmp(record->chrom, plusPrev->chrom) != 0 || record->chromStart - plusPrev->chromStart > clClusterSize))
            {
            hist[slCount(plusClusters)]++;
            writeCluster(plusClusters, out);
            slFreeList(&plusClusters);
            }
        //printf("\n");
        slAddHead(&plusClusters, record);
        plusPrev = record;
        }
    else
        {
        if (minusPrev != NULL && (strcmp(record->chrom, minusPrev->chrom) != 0 || record->chromStart - minusPrev->chromStart > clClusterSize))
            {
            hist[slCount(minusClusters)]++;
            writeCluster(minusClusters, out);
            slFreeList(&minusClusters);
            }
        slAddHead(&minusClusters, record);
        minusPrev = record;
        }
    //printf("plusPrev: %p, minusPrev: %p, plusClusterCount: %d\n", plusPrev, minusPrev, slCount(plusClusters));
    }

lineFileClose(&input);

fclose(out);

if (clHist)
    {
    for (i = 0; i < 8096; i++)
        {
        if (hist[i] > 0)
            printf("%5d%10d\n", i, hist[i]);
        }
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clClusterSize = optionInt("clusterSize", clClusterSize);
clMinCluster = optionInt("minCluster", clMinCluster);
clHist = optionExists("hist");
if (argc != 3)
    usage();
analyzeCpG(argv[1], argv[2]);
return 0;
}
