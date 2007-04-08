
#include "common.h"
#include "verbose.h"
#include "hdb.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "altGraphX.h"

int removed3pSubsumed = 0;
int removed3pExtended = 0;
int removed3bpAlt = 0;
int removedRetInt = 0;
int removedOrphanStarts = 0;
int transformedHardEnds = 0;

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"altInFile", OPTION_STRING},
    {"altOutFile", OPTION_STRING},
    {"no3pSubsumed", OPTION_BOOLEAN},
    {"no3pExtended", OPTION_BOOLEAN},
    {"no3bpAlt3", OPTION_BOOLEAN},
    {"minRetIntScore", OPTION_INT},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "File to get read the agx's from.",
    "File to write the agx's to.",
    "Remove 3' exons that are within a spliced exon.",
    "Remove 3' exons that overlap a spliced exon.",
    "Remove alt 3' splicing events that are only 3bp.",
    "Minimum support for retained introns.",
};

void usage()
/** Print usage and quit. */
{
int i=0;
warn("filterEdges - Program to remove some suspect edges from agx\n"
     "splicing graphs.");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   "
	 "filterEdges -altInFile=ranbp1.agx -altOutFile=ranbp1.filtered.agx\\\n");
}

bool isSoft(struct altGraphX *agx, int v) 
/* Return true if vertex is soft, false otherwise. */
{
if(agx->vTypes[v] == ggSoftStart || agx->vTypes[v] == ggSoftEnd)
    return TRUE;
return FALSE;
}

bool isHard(struct altGraphX *agx, int v) 
/* Return true if vertex is hard, false otherwise. */
{
if(agx->vTypes[v] == ggHardStart || agx->vTypes[v] == ggHardEnd)
    return TRUE;
return FALSE;
}

bool isSoftExon(struct altGraphX *agx, int v1, int v2) 
/* Is this an exon with soft ends? */
{
if(ggExon == altGraphXEdgeVertexType(agx, v1, v2) && 
   (isSoft(agx,v1) || isSoft(agx,v2))) 
    {
    return TRUE;
    }
return FALSE;
}

bool isHardExon(struct altGraphX *agx, int v1, int v2) 
/* Is this an exon with only hard ends? */
{
if(ggExon == altGraphXEdgeVertexType(agx, v1, v2) && 
   (isHard(agx,v1) && isHard(agx,v2))) 
    {
    return TRUE;
    }
return FALSE;
}

void removeEdge(struct altGraphX *agx, int edgeIx)
/* Remove the designated edge. */
{
int *vStarts = agx->edgeStarts;
int *vEnds = agx->edgeEnds;
int *eTypes = agx->edgeTypes;
int eCount = agx->edgeCount;
int eIx =  0;
struct evidence *ev = slElementFromIx(agx->evidence, edgeIx);
assert(slRemoveEl(&agx->evidence, ev));
freez(&ev);
/* replace current edge with next ones. */
for(eIx = edgeIx; eIx < eCount-1; eIx++) 
    {
    vStarts[eIx] = vStarts[eIx+1];
    vEnds[eIx] = vEnds[eIx+1];
    eTypes[eIx] = eTypes[eIx+1];
    }
agx->edgeCount--;
}

bool hardExonSubsumes(struct altGraphX *agx, int v1, int v2) 
/* Is the exon within another exon? */
{
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
int *vPos = agx->vPositions;
int v1Start = vPos[v1];
int v2End = vPos[v2];
int size = v2End - v1Start;
int edgeIx = 0;
int eCount = agx->edgeCount;
for(edgeIx = 0; edgeIx < eCount; edgeIx++) 
    {
    if(isHardExon(agx, starts[edgeIx],ends[edgeIx])  && (starts[edgeIx] != v1 || ends[edgeIx] != v2))
	{
	int eStart = vPos[starts[edgeIx]];
	int eEnd = vPos[ends[edgeIx]];
	if(rangeIntersection(v1Start,v2End, eStart, eEnd) == size) 
	    {
	    return TRUE;
	    }
	}
    }
return FALSE;
}

int hardExonOverlaps(struct altGraphX *agx, int v1, int v2) 
/* Is the exon within another exon? */
{
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
int *vPos = agx->vPositions;
int v1Start = vPos[v1];
int v2End = vPos[v2];
int edgeIx = 0;
int eCount = agx->edgeCount;
int maxOverlap = 0;
for(edgeIx = 0; edgeIx < eCount; edgeIx++) 
    {
    if(isHardExon(agx, starts[edgeIx], ends[edgeIx])  && (starts[edgeIx] != v1 || ends[edgeIx] != v2))
	{
	int eStart = vPos[starts[edgeIx]];
	int eEnd = vPos[ends[edgeIx]];
	int overlap = rangeIntersection(v1Start,v2End, eStart, eEnd);
	if(overlap >= maxOverlap) 
	    {
	    maxOverlap = overlap;
	    }
	}
    }
return maxOverlap;
}

void removeSubsumedEdges(struct altGraphX *agx) 
/* Remove edges that have a soft end, but are within an exon with two
 * hard edges */
{
int edgeIx = 0;
int eCount = agx->edgeCount; 
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
for(edgeIx = 0; edgeIx < eCount; edgeIx++) 
    {
    int start = starts[edgeIx];
    int end = ends[edgeIx];
    int size = agx->vPositions[end] - agx->vPositions[start];
    if(isSoftExon(agx,start,end)) 
	{
	if(hardExonOverlaps(agx, start, end) == size)
	    {
	    removed3pExtended++;
	    removeEdge(agx, edgeIx);
	    edgeIx--; /* We just removed an edge so don't skip. */
	    eCount = agx->edgeCount; /* update eCount as we've changed number of exons .*/
	    }
	}
    }
}


void removeExtendedEdges(struct altGraphX *agx) 
/* Remove edges that have a soft end, but are within an exon with two
 * hard edges */
{
int edgeIx = 0;
int eCount = agx->edgeCount; 
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
for(edgeIx = 0; edgeIx < eCount; edgeIx++) 
    {
    int start = starts[edgeIx];
    int end = ends[edgeIx];
    int size = agx->vPositions[end] - agx->vPositions[start];
    if(isSoftExon(agx,start,end)) 
	{
	if(hardExonOverlaps(agx, start, end) > (0.5 * size)) 
	    {
	    removed3pSubsumed++;
	    removeEdge(agx, edgeIx);
	    edgeIx--; /* We just removed an edge so don't skip. */
	    eCount = agx->edgeCount; /* update eCount as we've changed number of exons .*/
	    }
	}
    }
}

bool orphanStart(struct altGraphX *agx, int vertex) 
/* Does this start have any connections out of it? */
{
assert(agx->vTypes[vertex] == ggHardStart || agx->vTypes[vertex] == ggSoftStart);
int edgeIx = 0;
int *starts = agx->edgeStarts;
//int *ends = agx->edgeEnds;
int eCount = agx->edgeCount;
for(edgeIx = 0; edgeIx < eCount; edgeIx++)
    {
    if(starts[edgeIx] == vertex) 
	return FALSE;
    }
return TRUE;
}

bool orphanEnd(struct altGraphX *agx, int vertex) 
/* Does this end have any connections into it? */
{
assert(agx->vTypes[vertex] == ggHardEnd || agx->vTypes[vertex] == ggSoftEnd);
int edgeIx = 0;
int *ends = agx->edgeEnds;
//int *ends = agx->edgeEnds;
int eCount = agx->edgeCount;
for(edgeIx = 0; edgeIx < eCount; edgeIx++)
    {
    if(ends[edgeIx] == vertex) 
	return FALSE;
    }
return TRUE;
}


void removeVertexEdges(struct altGraphX *agx, int vertex) 
/* Remove edges associated with a particular vertex. */
{
int edgeIx = 0;
for(edgeIx = 0; edgeIx < agx->edgeCount; edgeIx++) 
    {
    if(agx->edgeEnds[edgeIx] == vertex ||
       agx->edgeStarts[edgeIx] == vertex) 
	{
	removeEdge(agx, edgeIx);
	edgeIx--; // edges have been adjusted down
	}
    }
}

void removeOrphanStartsEnds(struct altGraphX *agx) 
{
/* int *starts = agx->edgeStarts; */
/* int *ends = agx->edgeEnds; */
int vIx = 0;
for(vIx = 0; vIx < agx->vertexCount; vIx++) 
    {
    /* strange to thing about a soft start that is not connected to anything,
       but apparently it happens. */
    if(agx->vTypes[vIx] == ggHardStart || agx->vTypes[vIx] == ggSoftStart) 
	{
	if(orphanStart(agx,vIx)) 
	    {
	    removeVertexEdges(agx,vIx);
	    removedOrphanStarts++;
	    }
	}
    /* strange to thing about a soft start that is not connected to anything,
       but apparently it happens. */
    if(agx->vTypes[vIx] == ggHardEnd || agx->vTypes[vIx] == ggSoftEnd) 
	{
	if(orphanEnd(agx, vIx)) 
	    {
	    removeVertexEdges(agx,vIx);
	    removedOrphanStarts++;
	    }
	}
    }

}

bool noEdgeOut(struct altGraphX *agx, int vertex) 
/* Return FALSE if there is an edge out of this vertex, FALSE otherwise. */
{
int edgeIx = 0;
int *starts = agx->edgeStarts;
int eCount = agx->edgeCount;
for(edgeIx = 0; edgeIx < eCount; edgeIx++) 
    {
    if(starts[edgeIx] == vertex)
	return FALSE;
    }
return TRUE;
}

bool noEdgeIn(struct altGraphX *agx, int vertex) 
/* Return FALSE if there is an edge into this vertex, TRUE otherwise. */
{
int edgeIx = 0;
int *ends = agx->edgeEnds;
int eCount = agx->edgeCount;
for(edgeIx = 0; edgeIx < eCount; edgeIx++) 
    {
    if(ends[edgeIx] == vertex)
	return FALSE;
    }
return TRUE;
}

void transformOrphanHardEnds(struct altGraphX *agx) 
/* Make hard ends of exons without connections be soft. */
{
int vCount = agx->vertexCount;
int vIx = 0;
for(vIx = 0; vIx < vCount; vIx++) 
    {
    if(agx->vTypes[vIx] == ggHardEnd) 
	{
	if(noEdgeOut(agx,vIx))
	    {
	    agx->vTypes[vIx] = ggSoftEnd;
	    transformedHardEnds++;
	    }
	}
    if(agx->vTypes[vIx] == ggHardStart) 
	{
	if(noEdgeIn(agx,vIx))
	    {
	    agx->vTypes[vIx] = ggSoftStart;
	    transformedHardEnds++;
	    }
	}
    }
}

bool threeBpIntronOverlap(struct altGraphX *agx, int start1, int end1, int edge1,
			  int *start2, int *end2, int *edge2)
{
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
int eCount = agx->edgeCount;
int *vPos = agx->vPositions;
int edgeIx = 0;
for(edgeIx = 0; edgeIx < eCount; edgeIx++) 
    {
    if(edgeIx == edge1)
	continue;
    int v1 = starts[edgeIx];
    int v2 = ends[edgeIx];
    if(ggSJ == altGraphXEdgeVertexType(agx, v1, v2) && 
       (v1 == start1 || v2 == end1)) 
	{
	int size1 = vPos[end1] - vPos[start1];
	int size2 = vPos[v2] - vPos[v1];
	if(abs(size1 - size2) == 3) 
	    {
	    *start2 = v1;
	    *end2 = v2;
	    *edge2 = edgeIx;
	    return TRUE;
	    }
	}
    }
return FALSE;
}

void removeAlt3bpEdges(struct altGraphX *agx) 
/* Remove those pesky little 3bp alt 3' differences that turn 
   out to be relatively common. */
{
int edgeIx = 0;
int eCount = agx->edgeCount;
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
for(edgeIx = 0; edgeIx < eCount; edgeIx++) 
    {
    int s1 = starts[edgeIx];
    int e1 = ends[edgeIx];
    if(ggSJ == altGraphXEdgeVertexType(agx, s1, e1)) 
	{
	int s2 = -1, e2 = -1, edge2 = -1;
	if(threeBpIntronOverlap(agx, s1, e1, edgeIx, &s2, &e2, &edge2))
	    {
	    struct evidence *ev1 = slElementFromIx(agx->evidence, edgeIx);
	    struct evidence *ev2 = slElementFromIx(agx->evidence, edge2);
	    int toDelete = -1;
	    if(ev1->evCount > ev2->evCount) 
		toDelete = edge2;
	    else
		toDelete = edgeIx;
	    removeEdge(agx,toDelete);
	    removed3bpAlt++;
	    edgeIx--;
	    eCount--;
	    }
	}
    }
}

void removeRetIntEdges(struct altGraphX *agx, int minScore) 
/* Remove retained introns that don't have enough evidence. */
{
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
int eCount = agx->edgeCount;
int edgeIx = 0;
int *vPos = agx->vPositions;
for(edgeIx = 0; edgeIx < eCount; edgeIx++)
    {
    int s1 = starts[edgeIx];
    int e1 = ends[edgeIx];
    if(ggExon == altGraphXEdgeVertexType(agx, s1, e1)) 
	{
	int edge2 = 0;
	struct evidence *ev = slElementFromIx(agx->evidence, edgeIx);
	if(ev->evCount > minScore)
	    continue;
	for(edge2 = 0; edge2 < eCount; edge2++) 
	    {
	    int s2 = starts[edge2];
	    int e2 = ends[edge2];
	    if(edgeIx == edge2) 
		continue;
	    /* Intron inside of an exon indicates a retained exon. */
	    if(ggSJ == altGraphXEdgeVertexType(agx, s2, e2)) 
		{
		int size = vPos[e2] - vPos[s2];
		if(rangeIntersection(vPos[s2], vPos[e2], vPos[s1], vPos[e1]) == size)
		    {
		    removeEdge(agx, edgeIx);
		    eCount--;
		    edgeIx--;
		    removedRetInt++;
		    break;
		    }
		}
	    }
	}
    }
}

void filterGraph(struct altGraphX *agx, bool subsumed, bool extended, 
	    bool alt3bp, int minRetIntScore) 
/* Remove edges that have weak evidence. */
{
removeOrphanStartsEnds(agx);
transformOrphanHardEnds(agx);
if(subsumed) 
    removeSubsumedEdges(agx);
if(extended)
    removeExtendedEdges(agx);
if(alt3bp)
    removeAlt3bpEdges(agx);
removeRetIntEdges(agx, minRetIntScore);

}

void filterEdges(char *inFile, char *outFile, bool subsumed, bool extended,
	    bool alt3bp, int minRetIntScore)
/** Top level function to filter graphs. */
{
struct altGraphX *agx = NULL, *agxList = altGraphXLoadAll(inFile);
FILE *out = mustOpen(outFile, "w");

for(agx = agxList; agx != NULL; agx = agx->next) 
    {
    filterGraph(agx, subsumed, extended, alt3bp, minRetIntScore);
    if(agx->edgeCount > 0)
	altGraphXTabOut(agx, out);    
    }

}

int main(int argc, char *argv[]) 
/** Everybody's favorite function. */
{
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
char *inFile = optionVal("altInFile", NULL);
char *outFile = optionVal("altOutFile", NULL);
if(inFile == NULL || outFile == NULL) 
    errAbort("Must specify both input and output files, see -help for details.");
bool subsumed = !optionExists("no3pSubsumed");
bool extended = !optionExists("no3pExtended");
bool alt3bp = !optionExists("no3bpAlt3");
int minRetIntScore = optionInt("minRetIntScore", 6);
filterEdges(inFile, outFile, subsumed, extended, alt3bp, minRetIntScore);
verbose(1, "Removed %d orphans, transformed %d ends.\n", removedOrphanStarts, transformedHardEnds);
verbose(1, "Filtered: %d 3' subsumed, %d 3' extended, %d 3bp alt, %d retained intron\n", 
	removed3pSubsumed, removed3pExtended, removed3bpAlt, removedRetInt);
return 0;
}
