/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "bed.h"
#include "linefile.h"
#include "cheapcgi.h"


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

char *getFileNameForBed(struct bed *bed) 
{
static char fileName[512];
int bedNum = 0;
while(1)
    {
    safef(fileName, sizeof(fileName), "%s:%d-%d-%d.bed", bed->chrom, bed->chromStart, bed->chromEnd, bedNum);
    if(!fileExists(fileName)) 
	{
	safef(fileName, sizeof(fileName), "%s:%d-%d-%d", bed->chrom, bed->chromStart, bed->chromEnd, bedNum);
	return fileName;
	}
    else
	bedNum++;
    }
return fileName;
}

void doAnalysisForBed(struct bed *bed) 
{
char *hgdbTestTable = cgiUsualString("hgdbTestTable","affyTrans_hg12");
char *hgdbTestName = "sugnet";
FILE *tmpFile = NULL;
char commandBuffer[4096];
char *fileNameRoot = getFileNameForBed(bed);
char bedFile[512];
char dataFile[512];
int retVal = 0;


/* Print out bed. */
safef(bedFile, sizeof(bedFile), "%s.bed", fileNameRoot);
tmpFile = mustOpen(bedFile, "w");
bedTabOutN(bed, 12, tmpFile);
carefulClose(&tmpFile);

/* Get samples for bed. */
safef(dataFile, sizeof(dataFile), "%s.data", fileNameRoot);
safef(commandBuffer, sizeof(commandBuffer), "samplesForCoordinates bedFile=%s hgdbTestName=%s hgdbTestTable=%s > %s", 
      bedFile, hgdbTestName, hgdbTestTable, dataFile);
retVal = system(commandBuffer);
if(retVal != 0)
    {
    warn("%s failed running command:\n%s", fileNameRoot, commandBuffer);
    return;
    }
safef(commandBuffer, sizeof(commandBuffer), "cp %s tmp.data", dataFile);
retVal = system(commandBuffer);

/* Run R analysis on data file. */
warn("Running R for %s", fileNameRoot);
fflush(stderr);
safef(commandBuffer, sizeof(commandBuffer), "R --vanilla < /cluster/home/sugnet/sugnet/R/maReg/R/runAnalysis.R");
warn("Done with R");
fflush(stderr);
retVal = system(commandBuffer);
if(retVal != 0)
    {
    warn("%s failed running command:\n%s", fileNameRoot, commandBuffer);
    return;
    }



bedsAnalyzed++;
}


void bulkChr2XRegression(char *spliceFile, char *spliceSelectionFile) 
/* Top level function to load files and iterate through splices of interest. */
{
FILE *tmpFile = NULL;
struct genomeBit *gpList = NULL, *gp = NULL;
struct bed *bedList = NULL, *bed = NULL, *retList = NULL;
warn("Loading beds from %s", spliceFile);
bedList = loadBedFileWithHeader(spliceFile);
warn("Loading splices of interest from %s", spliceSelectionFile);
gpList = loadGpList(spliceSelectionFile);
warn("Loaded %d splices, and %d splices of interest.", slCount(bedList), slCount(gpList));
warn("Analyzing splices of interest.");

/* Clean out the summary files produced by R script. */
tmpFile = mustOpen("maxScores.html", "w");
fprintf(tmpFile, "<head><body><table>\n");
fprintf(tmpFile, "<tr><th>Position</th><th>MaxDiff Levels</th><th>MaxDiff</th><th>Var Diff</th><th>MaxDiff/Var</th><th>Percent Diff</th><th>Cass Var/Stable Var</th><th>Plot</th></tr>\n");
carefulClose(&tmpFile);

tmpFile = mustOpen("allScores.tab", "w");
carefulClose(&tmpFile);

tmpFile = mustOpen("cassettes.sample", "w");
carefulClose(&tmpFile);

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
tmpFile = mustOpen("maxScores.html", "a");
fprintf(tmpFile, "</table></body></html>\n");
carefulClose(&tmpFile);
bedFreeList(&bedList);
}

int main(int argc, char *argv[]) 
{
cgiSpoof(&argc, argv);
if(argc != 3)
    usage();
bulkChr2XRegression(argv[1], argv[2]);
warn("Done.");
return 0;
}
