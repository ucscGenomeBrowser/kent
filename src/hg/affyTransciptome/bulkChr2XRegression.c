#include "common.h"
#include "bed.h"
#include "linefile.h"

struct genomeBit
/* Piece of the genome */
{
    struct genomeBit *next;
    char *fileName;
    char *chrom;
    int chromStart;
    int chromEnd;
};

int bedsNotFound = 0;
int multipleBedForGp = 0;
int bedsAnalyzed = 0;
void usage() 
{
errAbort("bulkChr2XRegression - Script used to call samplesForCoordinates and R to\n"
	 "produce R based regression analysis and plots for list of splices supplied to\n"
	 "program. Format is to supply large bed file with \"splices\" (usually a bed\n"
	 "record with three exons) and a list specifying which splices are of intrest.\n"
	 "the interesting splices are supplied in chrX:1-1000 format.\n"
	 "usage:\n\t"
	 "bulkChr2XRegression hg12.chr2X.splices.conf-1.0.est-4.bed hg12.chr2X.interestingSplices.txt\n");
}

struct genomeBit *parseGenomeBit(char *bitName)
/* take a name like chrN:1-1000 and parse it into a genome bit */
{
struct genomeBit *gp = NULL;
char *pos1 = NULL, *pos2 = NULL;
char *name = cloneString(bitName);
AllocVar(gp);
pos1 = strstr(name, ":");
 if(pos1 == NULL)
   errAbort("bulkChr2XRegression::parseGenomeBit() - %s doesn't look like chrN:1-1000", name);
gp->chrom = name;
*pos1 = '\0';
gp->chrom = cloneString(gp->chrom);
pos1++;
pos2 = strstr(pos1, "-");
 if(pos2 == NULL)
   errAbort("bulkChr2XRegression::parseGenomeBit() - %s doesn't look like chrN:1-1000", name);
*pos2 = '\0';
pos2++;
gp->chromStart = atoi(pos1);
gp->chromEnd = atoi(pos2);
freez(&name);
return gp;
}

struct bed *loadBedFileWithHeader(char *fileName)
/* Read in a bed file into a bed list, dealing with header for custom track if necessary. */
{
struct bed *bedList = NULL, *bed = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[12];
int lineSize;
char *line;
/* Skip the headers. */
while(lineFileNext(lf, &line, &lineSize)) 
    {
    if(countChars(line, '\t') > 10) 
	{
	lineFileReuse(lf);
	break;
	}
    }
/* Load in the records. */
while(lineFileRow(lf, row)) 
    {
    bed = bedLoad12(row);
    slAddHead(&bedList, bed);
    }
lineFileClose(&lf);
slReverse(&bedList);
return bedList;
}

struct genomeBit *loadGpList(char *fileName)
/* Load all of the genome bits in a file. */
{
struct genomeBit *gpList = NULL, *gp = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[12];
int lineSize;
char *line;
while(lineFileNextReal(lf, &line)) 
    {
    gp = parseGenomeBit(line);
    slAddHead(&gpList, gp);
    }
lineFileClose(&lf);
slReverse(&gpList);
return gpList;
}

void putDot(FILE *out)
{
fprintf(out,".");
fflush(out);
}

struct bed *findBedsFromGp(struct genomeBit *gp, struct bed *bedList)
/* Look through the bed list for beds that are contained in the genome bit. */
{
struct bed *bed = NULL, *retList = NULL, *bedClone = NULL;
boolean unique = TRUE;
for(bed = bedList; bed != NULL; bed = bed->next) 
    {
    if(bed->chromStart > gp->chromStart && bed->chromEnd < gp->chromEnd && sameString(bed->chrom, gp->chrom)) 
	{
	if(retList != NULL)
	    {
	    if(unique) 
		{
		unique = FALSE;
		warn("More than one bed for genome bit %s:%d-%d.",
		     gp->chrom, gp->chromStart, gp->chromEnd);
		multipleBedForGp++;
		}
	    }
	bedClone = cloneBed(bed);
	slAddHead(&retList, bedClone);
	}
    }
return retList;
}

void doAnalysisForBed(struct bed *bed) 
{
bedsAnalyzed++;
}


void bulkChr2XRegression(char *spliceFile, char *spliceSelectionFile) 
/* Top level function to load files and iterate through splices of interest. */
{
struct genomeBit *gpList = NULL, *gp = NULL;
struct bed *bedList = NULL, *bed = NULL, *retList = NULL;
warn("Loading beds from %s", spliceFile);
bedList = loadBedFileWithHeader(spliceFile);
warn("Loading splices of interest from %s", spliceSelectionFile);
gpList = loadGpList(spliceSelectionFile);
warn("Loaded %d splices, and %d splices of interest.", slCount(bedList), slCount(gpList));
warn("Analyzing splices of interest.");
for(gp = gpList; gp != NULL; gp = gp->next) 
    {
    retList = findBedsFromGp(gp, bedList);
    if(retList != NULL) 
	{
	for(bed = retList; bed != NULL; bed = bed->next) 
	    {
	    doAnalysisForBed(bed);
	    }
	bedFreeList(&retList);
	}
    else
	{
	bedsNotFound++;
	warn("Couldn't find bed for genome bit %s:%d-%d", gp->chrom, gp->chromStart, gp->chromEnd);
	}
    }
warn("");
warn("%d genome bits had multiple beds, %d had no bed, %d analyzed", multipleBedForGp, bedsNotFound, bedsAnalyzed);
warn("Cleaning up.");

bedFreeList(&bedList);
}

int main(char argc, char *argv[]) 
{
if(argc != 3)
    usage();
bulkChr2XRegression(argv[1], argv[2]);
warn("Done.");
return 0;
}
