/* agxStartsEnds.c - Program to output as simple bed
   exons that have soft starts and soft ends. That is,
   exons that indicate either transcription start or end sites. */

#include "common.h"
#include "bed.h"
#include "altGraphX.h"

boolean strict = FALSE;

void usage() 
{
errAbort("agxStartsEnds.c - Program to output as simple bed\n"
	 "exons that have soft starts and soft ends. That is,\n"
	 "exons that indicate either transcription start or end sites.\n"
	 "usage:\n   "
	 "agxStartsEnds graphs.agx out.bed\n");
}
boolean isHardExon(struct altGraphX *agx, int edge)
/* Return TRUE if exon only has hard starts and ends. */
{
unsigned char *vT = agx->vTypes;
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
if(getSpliceEdgeType(agx, edge) != ggExon)
    return FALSE;
if(vT[starts[edge]] == ggHardStart || vT[ends[edge]] == ggHardEnd)
    return TRUE;
return FALSE;
}

boolean isSoftExon(struct altGraphX *agx, int edge)
/* Return TRUE if edge is an exon and has a soft start or soft end. */
{
int *vPos = agx->vPositions;
unsigned char *vT = agx->vTypes;
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
boolean soft = FALSE;
int i;
if(getSpliceEdgeType(agx, edge) != ggExon)
    return FALSE;
else if(vT[starts[edge]] == ggSoftStart || vT[ends[edge]] == ggSoftEnd)
    {
    soft = TRUE;
    if(!strict)
	{
	for(i = 0; i < agx->edgeCount; i++)
	    {
	    if(i == edge)
		continue;
	    if(isHardExon(agx, i) && 
	       rangeIntersection(vPos[starts[edge]], vPos[ends[edge]], vPos[starts[i]], vPos[ends[i]]) > 0)
		{
		return FALSE;
		}
	    }
	}
    }
return soft;
}

void agxStartsEnds(char *agxFile, char *bedFile)
/* Loop through each graph and output the exons with soft starts or ends. */
{
FILE *out = mustOpen(bedFile, "w");
struct altGraphX *agxList = NULL, *agx = NULL;
int i;
int foundCount = 0;
warn("Loading splice graphs");
agxList = altGraphXLoadAll(agxFile);
warn("Looking for transcription starts and ends.");
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    int *vPos = agx->vPositions;
    int *starts = agx->edgeStarts;
    int *ends = agx->edgeEnds;
    for(i = 0; i < agx->edgeCount; i++) 
	{
	if(isSoftExon(agx, i))
	    {
	    fprintf(out, "%s\t%d\t%d\t%s\t0\t%s\n",
		    agx->tName, vPos[starts[i]], vPos[ends[i]], agx->name, agx->strand);
	    foundCount++;
	    }
	}
    }
warn("Found %d exons that start or end a transcript in %d total graphs. %.2f per graph", 
     foundCount, slCount(agxList), (double)foundCount/slCount(agxList));
altGraphXFreeList(&agxList);
carefulClose(&out);
}

int main(int argc, char *argv[])
{
if(argc != 3)
    usage();
agxStartsEnds(argv[1], argv[2]);
return 0;
}
