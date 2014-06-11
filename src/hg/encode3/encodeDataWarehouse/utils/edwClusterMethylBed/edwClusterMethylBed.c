/* edwClusterMethylBed - cluster CpG regions from an input bed file and perform some analysis on them. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "basicBed.h"
#include "sqlNum.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwClusterMethylBed - cluster CpG regions from an input bed file and perform some analysis on them\n"
  "usage:\n"
  "   edwClusterMethylBed input.bed output.bed\n"
  "options:\n"
  "   -joinSize=100\n"
  "   -minCluster=1\n"
  "   -hist"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"joinSize", OPTION_INT},
   {"minCluster", OPTION_INT},
   {"hist", OPTION_BOOLEAN},
   {NULL, 0},
};

int clJoinSize = 100;
int clMinCluster = 1;
boolean clHist = FALSE;

struct bedNamedScore
/* Stores a modified bedGraph record as output by the methylation pipeline */
{
    struct bedNamedScore *next;
    
    char *chrom;
    unsigned chromStart;
    unsigned chromEnd;
    char *name;
    double score;
    char strand;
};

struct bedNamedScore *bedNamedScoreLoadNext(struct lineFile *lf)
/* Takes in an open lineFile and reads out the next bedNamedScore line */
{
char *row[6];
int rowSize = lineFileChopNext(lf, row, ArraySize(row));

if (rowSize == 0)
    return NULL;
    
struct bedNamedScore *bg;
AllocVar(bg);
    
bg->chrom = cloneString(row[0]);
bg->chromStart = sqlUnsigned(row[1]);
bg->chromEnd = sqlUnsigned(row[2]);
bg->name = cloneString(row[3]);
bg->score = sqlFloat(row[4]);
bg->strand = row[5][0];

return bg;
}

void writeCluster(struct bedNamedScore *clusterList, FILE *out)
/* Takes a list of bed lines and writes out a single blocked bed line into the out file */
{
int size = slCount(clusterList);
if (size < clMinCluster)
    return;

int blockStarts[size];
int blockSizes[size];
double score = 0;
struct bedNamedScore *last = clusterList;

slReverse(&clusterList);

// create our output bed object and assign values to all the fields we care about
struct bed outBed;

outBed.chrom = cloneString(clusterList->chrom);
outBed.chromStart = clusterList->chromStart;
outBed.chromEnd = last->chromEnd;

// the name of each record is merely the size of the cluster, mostly for viewing on the browser
char sizeBuf[8];
safef(sizeBuf, 8, "%d", size);
outBed.name = sizeBuf;

outBed.strand[0] = clusterList->strand;
outBed.strand[1] = '\0';
outBed.blockCount = size;

int i;
for (i = 0; i < size; i++)
    blockSizes[i] = 1;
outBed.blockSizes = blockSizes;

// get the blockStarts and also calculate the final score, which is just the average of the scores * 10
// because the input values are decimal numbers from 0.0000-100.0000 and our bed output is an int 0-1000
i = 0;
struct bedNamedScore *cur;
for (cur = clusterList; cur != NULL; cur = cur->next)
    {
    blockStarts[i] = cur->chromStart - outBed.chromStart;
    score += cur->score;
    i++;
    }
outBed.chromStarts = blockStarts;
outBed.score = (int)(score * 10 / size);
        
// zero out unused fields
outBed.thickStart = outBed.chromStart;
outBed.thickEnd = outBed.chromStart;
outBed.itemRgb = 0;
        
// finally print our struct out as a bed12
bedTabOutN(&outBed, 12, out);   
}

void edwClusterMethylBed(char *inputName, char *outputName)
/* edwClusterMethylBed - cluster CpG regions from an input bed file and perform some analysis on them. */
{

// open the input and output files
struct lineFile *input = lineFileOpen(inputName, TRUE);
FILE *out = fopen(outputName, "w");

// keep 2 sets of everything, one for plus one for minus. Here are pointers to the previous element in the list
struct bedNamedScore *plusPrev = NULL;
struct bedNamedScore *minusPrev = NULL;

// set up lists for the clusters as we build them up
struct bedNamedScore *plusClusters = NULL;
struct bedNamedScore *minusClusters = NULL;

int plusClusterSize = 0;
int minusClusterSize = 0;

// this could be done better, but it seems to work okay for now. It will crash on sufficiently large clusters
int hist[16384];
int i;
for (i = 0; i < 16384; i++)
    hist[i] = 0;

// loop over bed file
for (;;)
    {
    struct bedNamedScore *record = bedNamedScoreLoadNext(input);
    
    // at the end, we print out the last cluster
    if (record == NULL)
        {
        if (plusClusterSize > 0)
            {
            hist[plusClusterSize]++;
            writeCluster(plusClusters, out);
            }
        if (minusClusterSize > 0)
            {
            hist[minusClusterSize]++;
            writeCluster(minusClusters, out);
            }
    
        break;
        }
    
    // handling each strand separately seemed easier but this could be refactored
    if (record->strand == '+')
        {
        // if we're out of range (one way or another) then we print out this cluster and start anew
        if (plusPrev != NULL && (strcmp(record->chrom, plusPrev->chrom) != 0 || record->chromStart - plusPrev->chromStart > clJoinSize))
            {
            hist[plusClusterSize]++;
            writeCluster(plusClusters, out);
            slFreeList(&plusClusters);
            plusClusterSize = 0;
            }
        slAddHead(&plusClusters, record);
        plusClusterSize++;
        plusPrev = record;
        }
    else
        {
        if (minusPrev != NULL && (strcmp(record->chrom, minusPrev->chrom) != 0 || record->chromStart - minusPrev->chromStart > clJoinSize))
            {
            hist[minusClusterSize]++;
            writeCluster(minusClusters, out);
            slFreeList(&minusClusters);
            minusClusterSize = 0;
            }
        slAddHead(&minusClusters, record);
        minusClusterSize++;
        minusPrev = record;
        }
    }

// close input and output
lineFileClose(&input);
fclose(out);

// print out the histogram if option is enabled
if (clHist)
    {
    for (i = 0; i < 16384; i++)
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
clJoinSize = optionInt("joinSize", clJoinSize);
clMinCluster = optionInt("minCluster", clMinCluster);
clHist = optionExists("hist");
if (argc != 3)
    usage();
edwClusterMethylBed(argv[1], argv[2]);
return 0;
}
