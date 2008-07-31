/* Take a sample file and create averages at different levels for display in the browser when zoomed out. */
#include "common.h"
#include "sample.h"
#include "hdb.h"
#include "options.h"
#include <string.h>

static char const rcsid[] = "$Id: averageZoomLevels.c,v 1.5.338.1 2008/07/31 02:23:57 markd Exp $";


#define MAX_WINDOW_SIZE  2000
void usage()
{
errAbort("averageZoomLevels - takes a sorted sample file and creates averaged\n"
"'zoomed-out' summaries for a few different levels.\n"
"Basic idea is to get the size of a chromosome, divide it by 2000 as that is\n"
"about how many pixels there may be and then caclulate an average for each\n"
"of those 2000 bins. Then reduce size of each bin by maginification and \n"
"calculate average for each of those bins, etc. until we are less than\n"
"magnification times the minimum separation in the file or we are at maxZoom\n."
"The -max option uses the maximum score to summarize each bin rather than the mean.\n"
"usage:\n"
"    averageZoomLevels magnification maxZoom database sampleFile\n"
"options:\n"
"    -max  Use max rather than average in zoomed files\n"
"example:\n"
"     averageZoom 50 2500 hg13 sample.bed\n");
}

struct bin 
/* One averaged point. */
    {
    struct bin *next;         /* Next in list. */
    char *chrom;              /* chromosome */
    int chromStart, chromEnd; /* bin start and end on chrom. */
    char *name;               /* Name of experiment for bin. */
    float aveScore;           /* Average score over the range. */
    int sampleCount;          /* How many samples were in this bin. */
    int pinStart, pinEnd;     /* Coordinates of the sample closest to 
    			       * middle of bin. Don't want to put a value 
			       * where there isn't one. */
    };

struct bin *newBin(char *name, char *chrom, int chromStart, int chromEnd)
/* Create a bin structure. */
{
struct bin *ret = NULL;
AllocVar(ret);
ret->name = cloneString(name);
ret->chrom = cloneString(chrom);
ret->chromStart = chromStart;
ret->chromEnd = chromEnd;
ret->aveScore = 0;
return ret;
}

void binFree(struct bin **pEl)
/* Free a bin structure. */
{
struct bin *el;
if((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->name);
freez(pEl);
}

void binFreeList(struct bin **pList)
/* free a list of bin structures. */
{
struct bin *el, *next;
for(el = *pList; el != NULL; el = next)
    {
    next = el->next;
    binFree(&el);
    }
*pList = NULL;
}


int getBinNumForSample(struct sample *s, int sampleIndex, int binCount, int binSize)
/* Calculate which bin a sample belongs in using the chromStart field. */
{
int binNum = -1;
binNum = (s->chromStart + s->samplePosition[sampleIndex]) / binSize;
if(binNum >= binCount)
    binNum = binCount-1;
return binNum;
}

void addSampleToBin(struct bin *b, struct sample *s, int sampleIndex, boolean useMax )
/* Add a sample to the average of a bin. Also if the
   coordinates of the sample are closer to the center coordinates of
   than current pinStart and pinEnd, substitute them. */
{
int midway = (b->chromEnd - b->chromStart)/2 + b->chromStart;
int sampPos = (s->chromStart + s->samplePosition[sampleIndex]);
b->sampleCount++;

if( useMax )
    b->aveScore = max( s->sampleHeight[sampleIndex], b->aveScore );
else //using the mean
    b->aveScore += s->sampleHeight[sampleIndex];

if( abs(b->pinStart - midway) > abs(midway - sampPos))
    {
    b->pinStart = sampPos;
    b->pinEnd = sampPos + 1;
    }
}

void averageBinTabOutSample(struct bin **pBin,int binCount,FILE * out, boolean useMax )
{
int i =0;
struct bin *b = NULL;
char *name = "Empty";
for(i = 0; i < binCount; i++)
    {
    b = pBin[i];
    if(b->sampleCount != 0)
	{

    if( !useMax )
	    b->aveScore  = b->aveScore / b->sampleCount;
    
	name = b->name;
	}
    if(b->pinStart == 0 || b->pinEnd == 0)
	{
	b->pinStart = (b->chromEnd - b->chromStart)/2 + b->chromStart;
	b->pinEnd = b->pinStart +1;
	}
    if(differentString(name, "Empty")) 
	{
	fprintf(out, "%s\t%d\t%d\t%s\t%d\t+\t1\t0,\t%d\n",
		b->chrom,
		b->pinStart, 
		b->pinEnd,
		name,
		0,
		(int)b->aveScore);
	}
    }
}
		


void averageZoomLevels(int mag, int maxZoom, char *db, 
	char *inputFile, boolean useMax )
/* Main function, zooms through at different levels */
{
int currentZoom = 0;
struct bin **pBin = NULL;
struct bin *bin = NULL;
int binCount = 0;
int binSize = 0;
unsigned int chromSize = 0;
struct sample *sampList = NULL, *samp = NULL, *boundarySamp = NULL;
char *buff = NULL;
int maxDensity = 0;
FILE *out = NULL;
int i;


warn("Loading file %s\n", inputFile);
if( useMax )
    warn("Using MAX option not MEAN!!!\n");
sampList = sampleLoadAll(inputFile);
AllocArray(buff, (strlen(inputFile) + 100));

for(currentZoom = 1; currentZoom <= maxZoom; currentZoom *= mag)
    {
    warn("Doing zoom level %d", currentZoom);
    boundarySamp = sampList;   /* First boundary is start */
    snprintf(buff, (strlen(inputFile) + 100), "zoom%d_%s", 
    	currentZoom, inputFile);
    out = mustOpen(buff, "w");
    warn("Opened file: %s", buff);
    while(boundarySamp != NULL)
	{
	chromSize = hChromSize(db, boundarySamp->chrom);
	uglyf("%s size %d\n", boundarySamp->chrom, chromSize);
	binSize = chromSize/(currentZoom == 1 ? MAX_WINDOW_SIZE : MAX_WINDOW_SIZE * currentZoom);
	if (binSize < 1)
	    binSize = 1;
	binCount = chromSize/binSize +1;
	AllocArray(pBin, binCount);
	for(i=0; i<binCount; i++)
	    pBin[i] = newBin(boundarySamp->name, boundarySamp->chrom, i*binSize, ((i+1)*binSize) -1);
	for(samp = boundarySamp; 
		samp != NULL && sameString(samp->chrom, boundarySamp->chrom); 
		samp = samp->next )
	    {
	    for(i =0; i<samp->sampleCount; i++)
		{
		int binIndex = getBinNumForSample(samp, i, binCount, binSize);
		addSampleToBin(pBin[binIndex], samp, i, useMax );
		}
	    }
	averageBinTabOutSample(pBin, binCount, out, useMax );
	for(i=0;i<binCount;i++)
	    binFreeList(&pBin[i]);
	boundarySamp = samp;
	}
    carefulClose(&out);
    }
warn("Cleaning up.");
freez(&buff);
sampleFreeList(&sampList);
}

int main(int argc, char *argv[])
{
optionHash(&argc, argv);
if(argc != 5)
    usage();
else
    {
    int mag = atoi(argv[1]);
    int maxZoom = atoi(argv[2]);
    averageZoomLevels(mag, maxZoom, argv[3], argv[4], optionExists("max"));
    }
return 0;
}
