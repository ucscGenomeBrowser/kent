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
};

void usage()
/** Print usage and quit. */
{
int i=0;
warn("Program to determine if bed records are a valid\n"
     "path through a splice graph. Original motivation to see which\n"
     "alt-spliced probe sets are conserved in human from mouse.\n");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   ");
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
int posIx = 0, edgeIx = 0;
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
for(i = 0; i < bed->blockCount; i++)
    {
    int startIx = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i];
    int endIx = bed->chromStart + bed->chromStarts[i+1];
    for(edgeIx = 0; edgeIx < eC; edgeIx++)
	{
	if(starts[edgeIx] == startIx && ends[edgeIx] == endIx)
	    inGraph &= TRUE;
	}
    }
return inGraph;
}

boolean bedInSomeGraph(struct bed *bed)
/* Is this bed in a alt graph structures. */
{
struct altGraphX *agx = NULL;
struct binElement *beList = NULL, *be = NULL;
boolean inGraph = FALSE;
beList = chromKeeperFind(bed->chrom, bed->chromStart, bed->chromEnd);
for(be = beList; be != NULL; be = be->next)
    {
    agx = be->val;
    if(bedInGraph(bed, agx))
	inGraph = TRUE;
    }
slFreeList(&beList);
return inGraph;
}

void bedsInGraphs()
{
char * bedFile = NULL, *agxFile = NULL;
char *inGraphFile = NULL, *notInGraphFile = NULL;
char *db = NULL;
struct altGraphX *agxList = NULL, *agx = NULL;
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
dotForUserInit(slCount(bedList) / 20);
warn("Examiningg beds.");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    dotForUser();
    if(bedInSomeGraph(bed))
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
