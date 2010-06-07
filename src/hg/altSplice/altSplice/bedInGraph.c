/* bindInGraph.c - Program to determine if bed records are a valid
   path through a splice graph. Original motivation to see which
   alt-spliced probe sets are conserved in human from mouse. */
#include "common.h"
#include "bed.h"
#include "altGraphX.h"
#include "chromKeeper.h"
#include "options.h"
#include "obscure.h"

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"bedFile", OPTION_STRING},
    {"agxFile", OPTION_STRING},
    {"inGraph", OPTION_STRING},
    {"notInGraph", OPTION_STRING},
    {"cassetteBeds", OPTION_BOOLEAN},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Database to use (i.e. mm4)",
    "File containing bed records.",
    "File containing altGraphX records.",
    "File to output beds found in graphs.",
    "File to output beds not found in graphs.",
    "Assume beds are cassette exons with 1bp on either end and look\n\t\tfor skipping in addition to include",
};

void usage()
/** Print usage and quit. */
{
int i=0;
warn("Program to determine if bed exons are contained in\n"
     "a splice graph. Original motivation to see which alt-spliced\n"
     "probe sets are also alt in human (in addition to mouse).\n");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   "
	 "bedInGraph -db=mm5 -agxFile=file.agx -bedFile=file.bed -inGraph=contained.bed -notInGraph=notContained.bed\n");
}

void setupChromKeeper(char *db, char *agxFile)
/* Start up a chromKeeper and insert the agx
   records into it. */
{
struct altGraphX *agx = NULL, *agxList = NULL;
chromKeeperInit(db);
agxList = altGraphXLoadAll(agxFile);
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    chromKeeperAdd(agx->tName, agx->tStart, agx->tEnd, agx);
    }
}

boolean bedInGraph(struct bed *bed, struct altGraphX *agx)
/* Is this bed in this altGraphX? */
{
int *vPos = agx->vPositions;
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
int edgeIx = 0;
boolean inGraph = TRUE;
int eC = agx->edgeCount;
int i = 0;
/* Quick check for single exon beds. We are only counting things that
 * are spliced here. */
if(bed->blockCount <= 1)
    return FALSE;

/* Check to see if each intron in the bed is present in the graph. 
   XXXXXXX--------XXXXXXXX---------XXXXXXX
   cs
   css  bs        css
*/
for(i = 0; i < bed->blockCount-1; i++)
    {
    int startIx = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i];
    int endIx = bed->chromStart + bed->chromStarts[i+1];
    boolean edgeFound = FALSE;
    for(edgeIx = 0; edgeIx < eC; edgeIx++)
	{
	if(vPos[starts[edgeIx]] == startIx && vPos[ends[edgeIx]] == endIx)
	    edgeFound = TRUE;
	}
    if(!edgeFound)
	return FALSE;
    edgeFound = FALSE;
    }
return inGraph;
}

boolean edgeExists(struct altGraphX *agx, int pos1, int pos2)
/* Return TRUE if there exists an edge connecting position 1 and
   position 2 in the graph. */
{
int *vPos = agx->vPositions;
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
int edgeIx = 0;
int eC = agx->edgeCount;
for(edgeIx = 0; edgeIx < eC; edgeIx++)
    {
    if(vPos[starts[edgeIx]] == pos1 && vPos[ends[edgeIx]] == pos2)
	return TRUE;
    }
return FALSE;
}

boolean cassetteInGraph(struct bed *bed, struct altGraphX *agx)
/* Assume bed is a cassette exon 1bp---intron---cassette---intron---1bp and
   see if it is in the graph. */
{
boolean inGraph = TRUE;
assert(bed);
assert(agx);
if(bed->blockCount != 3 || bed->blockSizes[0] != 1 || bed->blockSizes[2] != 1)
    errAbort("%s doesn't look like a cassette bed.", bed->name);
/* Check first intron. */
inGraph &= edgeExists(agx, 
		     bed->chromStart + bed->chromStarts[0] + bed->blockSizes[0],
		     bed->chromStart + bed->chromStarts[1]);
/* Check exon. */
inGraph &= edgeExists(agx, 
		      bed->chromStart + bed->chromStarts[1],
		      bed->chromStart + bed->chromStarts[1] + bed->blockSizes[1]);

/* Check second intron. */
inGraph &= edgeExists(agx, 
		     bed->chromStart + bed->chromStarts[1] + bed->blockSizes[1],
		     bed->chromStart + bed->chromStarts[2]);

/* Check skipping intron. */
inGraph &= edgeExists(agx, 
		     bed->chromStart + bed->chromStarts[0] + bed->blockSizes[0],
		     bed->chromStart + bed->chromStarts[2]);
return inGraph;
}

boolean bedInSomeGraph(struct bed *bed, boolean doCassette)
/* Is this bed in a alt graph structures. */
{
struct altGraphX *agx = NULL;
struct binElement *beList = NULL, *be = NULL;
boolean inGraph = FALSE;
beList = chromKeeperFind(bed->chrom, bed->chromStart, bed->chromEnd);
for(be = beList; be != NULL; be = be->next)
    {
    agx = be->val;
    if(doCassette && cassetteInGraph(bed, agx))
	inGraph = TRUE;
    if(!doCassette && bedInGraph(bed, agx))
	inGraph = TRUE;
    }
slFreeList(&beList);
return inGraph;
}

void bedsInGraphs()
/* Look in a graph for the beds. */
{
char * bedFile = NULL, *agxFile = NULL;
char *inGraphFile = NULL, *notInGraphFile = NULL;
char *db = NULL;
boolean doCassette = optionExists("cassetteBeds");
struct bed *bedList = NULL, *bed = NULL;
FILE *inAgx = NULL, *notInAgx = NULL;
int inAgxCount = 0, notInAgxCount = 0;
int bedCount = 0;
/* Get some options. */

bedFile = optionVal("bedFile", NULL);
agxFile = optionVal("agxFile", NULL);
inGraphFile = optionVal("inGraph", NULL);
notInGraphFile = optionVal("notInGraph", NULL);
db = optionVal("db", NULL);
if(bedFile == NULL || agxFile == NULL || db == NULL ||
   inGraphFile == NULL || notInGraphFile == NULL)
    {
    warn("Missing options. Please specify all options.");
    usage();
    }
warn("Reading in alt graphs.");
setupChromKeeper(db, agxFile);
warn("Reading in beds.");
bedList = bedLoadAll(bedFile);
inAgx = mustOpen(inGraphFile, "w");
notInAgx = mustOpen(notInGraphFile, "w");
dotForUserInit(max(slCount(bedList) / 20, 1));
warn("Examiningg beds.");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    dotForUser();
    if(bedInSomeGraph(bed, doCassette))
	{
	inAgxCount++;
	bedTabOutN(bed, 12, inAgx);
	}
    else
	{
	notInAgxCount++;
	bedTabOutN(bed, 12, notInAgx);
	}
    }
warn("\tDone.");
bedCount = slCount(bedList);
warn("%d (%.2f%%) in graphs. %d (%.2f%%) not in graphs. %d total",
     inAgxCount, (float) inAgxCount/bedCount, 
     notInAgxCount, (float) notInAgxCount/bedCount, bedCount);
warn("Cleaning up.");
bedFreeList(&bedList);
carefulClose(&inAgx);
carefulClose(&notInAgx);
}

int main(int argc, char *argv[])
{
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
bedsInGraphs();
return 0;
}
