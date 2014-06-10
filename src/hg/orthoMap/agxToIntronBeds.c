/* agxToIntronBeds.c - Program to transform an altGraphX record into a 
   series of introns. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "bed.h"
#include "altGraphX.h"
#include "dystring.h"

void usage()
{
errAbort("agxToIntronBeds - Program to output all introns from altGraphX\n"
	 "records as beds. Designed for use in MGC project looking for novel\n"
	 "introns from altGraphX records transferred over from mouse.\n"
	 "usage:\n"
	 "  agxToIntronBeds agxFile bedOutFile\n");
}

struct bed *bedForEdge(struct altGraphX *ag, bool **em, int v1, int v2, int edgeCount)
{
int i;
char buff[256];
int *vPos = ag->vPositions;
int vCount = ag->vertexCount;
int pos1=0, pos2=BIGNUM;
int ev1=0, ev2=0;
struct bed *bed = NULL;
boolean found1 = FALSE, found2 = FALSE;
/* Find first exon. */
for(i=0; i<vCount; i++)
    {
    if(em[i][v1] && altGraphXEdgeVertexType(ag, i, v1) == ggExon)
	{
	int edgeNum = altGraphXGetEdgeNum(ag, i, v1);
	struct evidence *ev = slElementFromIx(ag->evidence, edgeNum);
	if(ev->evCount > ev1)
	    {
	    found1 = TRUE;
	    ev1 = ev->evCount;
	    pos1 = vPos[i];
	    }
	}
    }

/* Find second exon. */
for(i=0; i<vCount; i++)
    {
    if(em[v2][i] && altGraphXEdgeVertexType(ag, v2, i) == ggExon)
	{
	int edgeNum = altGraphXGetEdgeNum(ag, v2, i);
	struct evidence *ev = slElementFromIx(ag->evidence, edgeNum);
	if(ev->evCount > ev2)
	    {
	    found2 = TRUE;
	    ev2 = ev->evCount;
	    pos2 = vPos[i];
	    }
	}
    }
if(found1 && found2)
    {
    AllocVar(bed);
    safef(buff, sizeof(buff), "%s.%d", ag->name, edgeCount);
    bed->name = cloneString(buff);
    safef(bed->strand, sizeof(bed->strand), "%s", ag->strand);
    bed->chrom = cloneString(ag->tName);
    bed->chromStart = bed->thickStart = pos1;
    bed->chromEnd = bed->thickEnd = pos2;
    bed->score = (ev1 + ev2)/2;
    AllocArray(bed->chromStarts, 2);
    AllocArray(bed->blockSizes, 2);
    bed->blockCount = 2;
    bed->chromStarts[0] = pos1 - bed->chromStart;
    bed->chromStarts[1] = vPos[v2] - bed->chromStart;
    bed->blockSizes[0] =  vPos[v1] - pos1;
    bed->blockSizes[1] = pos2 - vPos[v2];
    }
return bed;
}

struct bed *bedIntronsFromAgx(struct altGraphX *ag)
/* Produce an intron bed for every intron in the altGraphx. */
{
struct bed *bed = NULL, *bedList = NULL;
int i,j;
int count =0;
int vCount = ag->vertexCount;
bool **em  = altGraphXCreateEdgeMatrix(ag);
for(i=0; i<vCount; i++)
    {
    for(j=0; j<vCount; j++)
	{
	if(em[i][j] && altGraphXEdgeVertexType(ag, i, j) == ggSJ)
	    {
	    bed = bedForEdge(ag, em, i, j, count++);
	    if(bed != NULL)
		slAddHead(&bedList, bed);
	    }
	}
    }
altGraphXFreeEdgeMatrix(&em, vCount);
slReverse(&bedList);
return bedList;
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 500;
static int dot = 500;
if ((--dot <= 0))
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}
void createIntronBeds(char *agxFile, char *bedFile)
/* Make intron beds for evaluation. */
{
struct altGraphX *ag=NULL, *agList = NULL;
struct bed *bed=NULL, *bedList=NULL;
FILE *bedOut = NULL;
int count;
warn("Rading AltGraphX list.");
agList = altGraphXLoadAll(agxFile);
warn("Converting to intron beds.");
bedOut = mustOpen(bedFile, "w");
for(ag = agList; ag != NULL; ag = ag->next)
    {
    occassionalDot();
    bedList = bedIntronsFromAgx(ag);
    for(bed=bedList; bed != NULL; bed=bed->next)
	{
	bedTabOutN(bed, 12, bedOut);
	}
    bedFreeList(&bedList);
    }
altGraphXFreeList(&agList);
}

int main(int argc, char *argv[])
{
if(argc != 3)
    usage();
createIntronBeds(argv[1], argv[2]);
return 0;
}
