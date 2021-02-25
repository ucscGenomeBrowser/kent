/* matrixToBarChartBed - Attach a labeled expression matrix to a bed file joining on the bed's 
 * name column.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "rainbow.h"
#include "sqlNum.h"

int clBedIx = 4;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixToBarChartBed - Attach a labeled expression matrix to a bed file joining\n"
  "on the matrix's first column and the bed's name column.\n"
  "usage:\n"
  "   matrixToBarChartBed matrix.tsv mapping.bed barChartOutput.bed\n"
  "where\n"
  "   matrix is tab-separated values with the first row and column used as labels\n"
  "   mapping.bed maps a row of the matrix using the bed's name field and matrix's 1st field\n"
  "        The mapping.bed file is expected to have a 'name2' field as it's last column\n"
  "        and otherwise be at least bed 6.   Only the first 6 fields plus the name2 field are\n"
  "        used.  Often this file will be made with gencodeVersionForGenes\n"
  "options:\n"
  "   -bedIx=N - use the N'th column of the mapping.bed as the id column. Default %d\n"
  "   -trackDb=stanza.txt -output a trackDb stanza for this as a track\n"
  "   -stats=stats.tsv - stats file from matrixClusterColumns, makes coloring in trackDb better\n"
  , clBedIx);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bedIx", OPTION_INT},
   {"trackDb", OPTION_STRING},
   {"stats", OPTION_STRING},
   {NULL, 0},
};

struct matrixClusterStats
/* Stats on a clustering of a matrix */
    {
    struct matrixClusterStats *next;  /* Next in singly linked list. */
    char *cluster;	/* The name of the cluster */
    int count;	/* The number of samples clustered together from old matrix */
    double total;	/* The sum of all samples */
    };

struct matrixClusterStats *matrixClusterStatsLoad(char **row)
/* Load a matrixClusterStats from row fetched with select * from matrixClusterStats
 * from database.  Dispose of this with matrixClusterStatsFree(). */
{
struct matrixClusterStats *ret;

AllocVar(ret);
ret->cluster = cloneString(row[0]);
ret->count = sqlSigned(row[1]);
ret->total = sqlDouble(row[2]);
return ret;
}

struct matrixClusterStats *matrixClusterStatsLoadAllByChar(char *fileName, char chopper) 
/* Load all matrixClusterStats from a chopper separated file.
 * Dispose of this with matrixClusterStatsFreeList(). */
{
struct matrixClusterStats *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];

while (lineFileNextCharRow(lf, chopper, row, ArraySize(row)))
    {
    el = matrixClusterStatsLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void writeBarChartTrack(char *fileName, char *statsFile,
    char *name, int sampleCount, char *sampleNames[])
/* Make a one-stanza trackDb file that describes our barchart */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "track %s\n", name);
fprintf(f, "type bigBarChart\n");
fprintf(f, "visibility pack\n");
fprintf(f, "shortLabel %s\n", name);
fprintf(f, "longLabel %s\n", name);
fprintf(f, "barChartMetric unknown\n");
fprintf(f, "barChartUnit unknown\n");
fprintf(f, "defaultLabelFields name\n");
fprintf(f, "labelFields name,name2\n");
fprintf(f, "bigDataUrl %s.bb\n", name);

/* Load optional stats into hash */
struct hash *statsHash = NULL;
if (statsFile != NULL)
    {
    struct matrixClusterStats *el, *list = matrixClusterStatsLoadAllByChar(statsFile, '\t');
    uglyf("Got %d items on list\n", slCount(list));
    statsHash = hashNew(0);
    for (el = list; el != NULL; el = el->next)
        hashAdd(statsHash, el->cluster, el);
    }

/* Print colors */
fprintf(f, "barChartColors");
double pos = 0.0;
double step = 1.0/sampleCount;
int i;
for (i=0; i<sampleCount; ++i)
    {
    struct rgbColor color = saturatedRainbowAtPos(pos);
    if (statsHash == NULL)
	fprintf(f, " #%02x%02x%02x", color.r, color.g, color.b);
    else
        {
	double belief = 1.0;
	char *sample = sampleNames[i];
	struct matrixClusterStats *stats = hashFindVal(statsHash, sample);
	if (stats == NULL)
	    errAbort("%s is in matrixFile but not %s", sample, statsFile);
	if (stats->count < 50)
	    belief *= 0.7;
	if (stats->count < 10)
	    belief *= 0.7;
	if (stats->total < 1e6)
	    belief *= 0.7;
	if (stats->total < 1e5)
	    belief *= 0.7;
	double doubt = 1.0 - belief;
	int maxColor = 255;
	int r = color.r * belief + maxColor*doubt;
	int g = color.g * belief + maxColor*doubt;
	int b = color.b * belief + maxColor*doubt;
	fprintf(f, " #%02x%02x%02x", r, g, b);
	}
    pos += step;
    }
fprintf(f, "\n");

/* print labels */
fprintf(f, "barChartBars");
for (i=0; i<sampleCount; ++i)
    {
    char *sample = cloneString(sampleNames[i]);
    subChar(sample, ' ', '_');
    fprintf(f, " %s", sample);
    freeMem(sample);
    }
fprintf(f, "\n");

carefulClose(&f);
}


void matrixToBarChartBed(char *matrixTsv, char *geneBed, char *statsFile, 
    char *output, int bedIx, char *trackDb)
/* matrixToBarChartBed - Attach a labeled expression matrix to a bed file joining on the bed's 
 * name column.. */
{
/* Load up gene bed hash */
int bedRowSize = 0;
struct hash *geneHash = hashTsvBy(geneBed, bedIx, &bedRowSize);
int name2Ix = (bedIx == 3 ? bedRowSize-1 : 3);
verbose(2, "%d columns about %d genes in %s\n", bedRowSize, geneHash->elCount, geneBed);

/* Open up matrix file and read first line into sample labeling */
struct lineFile *lf = lineFileOpen(matrixTsv, TRUE);
char *line;
lineFileNeedNext(lf, &line, NULL);
if (line[0] == '#')	// Opening sharp on labels is optional, skip it if there
    line = skipLeadingSpaces(line+1);
int colCount = chopByChar(line, '\t', NULL, 0);
int colAlloc = colCount + 1;
char **sampleNames;
AllocArray(sampleNames, colAlloc);
chopByChar(line, '\t', sampleNames, colCount);

/* Output sample names and maybe other stuff to trackDb output */
if (trackDb != NULL)
    {
    char name[FILENAME_LEN];
    splitPath(output, NULL, name, NULL);
    writeBarChartTrack(trackDb, statsFile, name, colCount-1, sampleNames+1);
    }

/* Open output */
FILE *f = mustOpen(output, "w");

/* Set up row for reading one row of matrix at a time. */
char **matrixRow;
AllocArray(matrixRow, colAlloc);
int hitCount = 0, missCount = 0;
for (;;)
    {
    /* Fetch next line and remember how long it is.  Just skip over lines that are empty or
     * start with # character. */
    int lineLength = 0;
    char *line;
    if (!lineFileNext(lf, &line, &lineLength))
        break;
    char *s = skipLeadingSpaces(line);
    char c = s[0];
    if (c == 0 || c == '#')
        continue;

    /* Chop it into tabs */
    int rowSize = chopByChar(line, '\t', matrixRow, colAlloc);
    lineFileExpectWords(lf, colCount, rowSize);

    char *geneName = matrixRow[0];
    struct hashEl *onePos = hashLookup(geneHash, geneName);
    if (onePos == NULL)
	{
	verbose(2, "Can't find gene %s in %s", geneName, geneBed);
	++missCount;
	continue;
	}
    else
	{
	++hitCount;
	}

    /* A gene may map multiple places.  This loop takes care of that */
    for (; onePos != NULL; onePos = hashLookupNext(onePos))
        {
	char **geneBedVal = onePos->val;	// Get our bed as string array out of hash

	/* Output info - first six from the bed, then name2, then our barchart */
	int i;
	for (i=0; i<6; ++i)
	    fprintf(f, "%s\t",  geneBedVal[i]);

	char *name2 = geneBedVal[name2Ix];
	fprintf(f, "%s\t", name2);

	/* Print out number of expression values, and comma-sep-list of values themselves. */
	fprintf(f, "%d\t", colCount-1);	// -1 for skipping label
	fprintf(f, "%s", matrixRow[1]);
	for (i=2; i<colCount; ++i)
	    fprintf(f, ",%s", matrixRow[i]);
	
	fprintf(f, "\n");
	}
    }
carefulClose(&f);
verbose(1, "%d genes found, %d (%0.2f%%) missed\n", hitCount, missCount, 100.0*missCount/(hitCount+missCount));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clBedIx = optionInt("bedIx", clBedIx);
char *trackDb = optionVal("trackDb", NULL);
char *stats = optionVal("stats", NULL);
matrixToBarChartBed(argv[1], argv[2], stats, argv[3], clBedIx-1, trackDb);
return 0;
}
