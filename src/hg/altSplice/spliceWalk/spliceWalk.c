/* spliceWalk.c - program to construct major splice isoforms by
   walking a series of paths through the splice graph. */
#include "common.h"
#include "geneGraph.h"
#include "altGraphX.h"
#include "spliceGraph.h"
#include "jksql.h"

void usage()
{
errAbort("spliceWalk - uses altGraphX files to construct a splicing graph\n"
	 "which is a combination of all the paths seen by RNA and EST evidence.\n"
	 "The paths are joined where there are overlaps and the resulting super\n"
	 "graph is walked to determine a set of transcripts which are not sub-paths\n"
	 "of each other.\n"
	 "usage:\n\t"
	 "spliceWalk <infile.altGraphX.tab> <outFile.bed>\n");
}

void addNodeToPath(struct spliceGraph *sg, struct splicePath *sp, struct altGraphX *agx, int edge)
/* Add the necessary node to the splicePath to include the indicated edge
   from the altGraphX. */
{
struct spliceNode *e1 = NULL, *e2 = NULL;
struct spliceNode *tail = NULL;

AllocVar(e1);
AllocVar(e2);

/* Create the first node. */
e1->tName = cloneString(agx->tName);
snprintf(e1->strand,sizeof(e1->strand),"%s", agx->strand);
e1->tStart = e1->tEnd = e1->class = agx->vPositions[agx->edgeStarts[edge]];
e1->type = agx->vTypes[agx->edgeStarts[edge]];

/* Create the second node. */
e2->tName = cloneString(agx->tName);
snprintf(e2->strand,sizeof(e2->strand),"%s", agx->strand);
e2->tStart = e2->tEnd = e2->class = agx->vPositions[agx->edgeEnds[edge]];
e2->type = agx->vTypes[agx->edgeEnds[edge]];

/* Register new nodes with the spliceGraph. */
spliceGraphAddNode(sg, e1);
spliceGraphAddNode(sg, e2);

/* Connect nodes to form path. */
tail = slLastEl(sp->nodes);
spliceNodeConnect(tail, e1);
spliceNodeConnect(e1, e2);

/* Add nodes to splicePath. */
slAddTail(&sp->nodes, e1);
slAddTail(&sp->nodes, e2);
sp->nodeCount += 2;

/* Expand the limits of the path as necessary. */
if(e2->tEnd > sp->tEnd)
    sp->tEnd = e2->tEnd;
if(sp->tStart == 0)
    sp->tStart = e1->tStart;
}

int splicePathCountExons(struct splicePath *sp)
/* Count the number of exons in a splicePath. */
{
int i=0, count=0;
struct spliceNode *sn = NULL;
for(sn=sp->nodes; sn->next != NULL; sn = sn->next)
    {
    if((sn->type == ggHardStart || sn->type == ggSoftStart)
       && (sn->next->type == ggHardEnd || sn->next->type == ggSoftEnd))
	count++;
    }
return count;
}

void splicePathBedOut(struct splicePath *sp, FILE *out)
/* Print out a splice path in the form of a bed file, for viewing in the browser. */    
{
int i=0;
struct spliceNode *sn = NULL;
fprintf(out, "%s\t%u\t%u\t%s\t1000\t%s\t%u\t%u\t0\t", 
	sp->tName, sp->tStart, sp->tEnd, sp->qName, sp->strand, sp->tStart, sp->tEnd);
/* Only print out blocks that correspond to exons. */
fprintf(out, "%d\t",  splicePathCountExons(sp));
for(sn=sp->nodes; sn->next != NULL; sn = sn->next)
    {
    int size = sn->next->tStart - sn->tStart;
    if((sn->type == ggHardStart || sn->type == ggSoftStart)
       && (sn->next->type == ggHardEnd || sn->next->type == ggSoftEnd))
	fprintf(out, "%d,",size);
    }
fprintf(out, "\t");
for(sn=sp->nodes; sn->next != NULL; sn = sn->next)
    {
    int start = sn->tStart - sp->tStart;
    if((sn->type == ggHardStart || sn->type == ggSoftStart)
       && (sn->next->type == ggHardEnd || sn->next->type == ggSoftEnd))
	fprintf(out, "%d,",start);
    }
fprintf(out, "\n");
}

struct splicePath *splicePathsFromAgx(struct altGraphX *agx)
/* Construct a list of splice paths, one for every mrnaRef in the altGraphX, and return it. */
{
int i,j,k;
struct splicePath *sp = NULL, *spList = NULL;
struct spliceGraph *sg = NULL;
struct evidence *ev = NULL;
struct spliceNode *sn = NULL;
char *acc = NULL;
AllocVar(sg);
for(i=0; i < agx->mrnaRefCount; i++)
    {
    AllocVar(sp);
    acc = agx->mrnaRefs[i];
    sp->qName = cloneString(acc);
    snprintf(sp->strand, sizeof(sp->strand), "%s", agx->strand);
    sp->tName = cloneString(agx->tName);
    for(ev=agx->evidence; ev != NULL; ev = ev->next)///    for(j=0; j < agx->edgeCount; j++)
	{
//	ev = &agx->evidence[j];
        for(k=0; k<ev->evCount; k++)
	    {
	    if(sameString(acc, agx->mrnaRefs[ev->mrnaIds[k]]))
		{
		addNodeToPath(sg, sp, agx, slIxFromElement(agx->evidence,ev));
		}
	    }
	}
    slAddHead(&spList, sp);
    } 
return spList;
}

void spliceWalk(char *inFile, char *bedOut)
/* Starting routine. */
{
struct altGraphX *agx = NULL, *agxList = NULL;
struct spliceGraph *sg = NULL;
FILE *f = NULL;
struct splicePath *sp=NULL, *spList = NULL;
agxList = altGraphXLoadAll(inFile);
f = mustOpen(bedOut, "w");
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    spList = splicePathsFromAgx(agx);
    for(sp = spList; sp != NULL; sp = sp->next)
	splicePathBedOut(sp,f);
    splicePathFreeList(&spList);
    }
carefulClose(&f);
altGraphXFreeList(&agxList);
}


int main(int argc, char *argv[])
{
if(argc != 3)
    usage();
spliceWalk(argv[1], argv[2]);
return 0;
}
