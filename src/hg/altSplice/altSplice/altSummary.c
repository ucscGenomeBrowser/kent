/**
 * altSummary.c - Program to summarize the altSplicing in altGraphX's.
 */
#include "common.h"
#include "options.h"
#include "altGraphX.h"
#include "geneGraph.h"
#include "altSpliceSite.h"

static char const rcsid[] = "$Id: altSummary.c,v 1.1 2003/06/17 22:41:18 sugnet Exp $";

enum altSpliceType
/* Type of alternative splicing event. */
{
    alt5Prime,
    alt3Prime,
    altCassette,
    altOther
};

void usage()
{
errAbort("altSummary - Summarize the altSplicing in a series of altGraphX's\n"
	 "usage:\n"
	 "   altSummary db altGraphXIn.agx spliceSummaryOut.txt htmlLinks.html htmlFrameIndex.html\n"
	 );
}

int rowSum(bool *em, unsigned char *vTypes, int vCount)
{
int count =0;
int i=0;
for(i=0; i<vCount; i++)
   {
   if(em[i] && (vTypes[i] == ggHardStart || vTypes[i] == ggHardEnd) )
       count++;
   }
return count;
}


boolean areConsSplice(bool **em, int vCount,  unsigned char *vTypes, int v1, int v2)
/* Return TRUE if v1 an v2 are both hard starts or hard ends. */
{
if((vTypes[v1] == ggHardStart || vTypes[v1] == ggHardEnd) &&
   (vTypes[v2] == ggHardStart || vTypes[v2] == ggHardEnd) &&
   (rowSum(em[v2], vTypes, vCount ) > 0) )
    return TRUE;
return FALSE;
}

int altSpliceType(struct altGraphX *ag, int v1, int v2)
/* Return type of splicing. */
{
return altOther;
}
    
int getEdgeNum(struct altGraphX *ag, int v1, int v2)
/** Find the edge index that corresponds to v1 and v2 */
{
int eC = ag->edgeCount;
int i=0;
for(i=0;i<eC; i++)
    {
    if(ag->edgeStarts[i] == v1 && ag->edgeEnds[i] == v2)
	{
	return i;
	}
    }
errAbort("altSummary::getEdgeNum() - Didn't find edge with vertices %d-%d in %s.", v1, v2, ag->name);
return -1;
}

struct altSpliceSite *initASplice(struct altGraphX *ag, int vs, int ve1, int ve2)
/* Initialize an altSplice site report with vlaues. */
{
struct altSpliceSite *as = NULL;
struct evidence *ev = NULL;
int edgeNum = 0;
AllocVar(as);
as->chrom = cloneString(ag->tName);
as->chromStart = ag->vPositions[vs];
as->chromEnd = ag->vPositions[vs];
as->agName = cloneString(ag->name);
safef(as->strand, sizeof(as->strand), "%s", ag->strand);
as->index = vs;
as->type = ag->vTypes[vs];
as->altCount+=2;

/* Return starts of vertices. */
AllocArray(as->altStarts, ag->vertexCount);
as->altStarts[0] = ag->vPositions[ve1];
as->altStarts[1] = ag->vPositions[ve2];

/* Record indices of vertices. */
AllocArray(as->vIndexes, ag->vertexCount);
as->vIndexes[0] = ve1;
as->vIndexes[1] = ve2;

/* Record type of vertices. */
AllocArray(as->altTypes, ag->vertexCount);
as->altTypes[0] = ag->vTypes[ve1];
as->altTypes[1] = ag->vTypes[ve2];

/* Record splice types. Not supported currently. */
AllocArray(as->spliceTypes, ag->vertexCount);
as->spliceTypes[0] = altSpliceType(ag, vs, ve1);
as->spliceTypes[1] = altSpliceType(ag, vs, ve2);

/* Look up the evidence. */
AllocArray(as->support, ag->vertexCount);

edgeNum = getEdgeNum(ag, vs, ve1);
ev = slElementFromIx(ag->evidence, edgeNum);
as->support[0] = ev->evCount;

edgeNum = getEdgeNum(ag, vs, ve2);
ev = slElementFromIx(ag->evidence, edgeNum);
as->support[1] = ev->evCount;

return as;
}

void addSpliceSite(struct altGraphX *ag, struct altSpliceSite *aSplice, int ve)
/* Add another end site to an existing splicing event. */
{
struct evidence *ev = NULL;
int edgeNum = 0;
aSplice->altStarts[aSplice->altCount] = ag->vPositions[ve];
aSplice->altTypes[aSplice->altCount] = ag->vTypes[ve];
aSplice->spliceTypes[aSplice->altCount] = altSpliceType(ag, aSplice->index, ve);
edgeNum = getEdgeNum(ag, (int)aSplice->index, ve);
ev = slElementFromIx(ag->evidence, edgeNum);
aSplice->support[aSplice->altCount] = ev->evCount;
aSplice->altCount++;
}

void lookForAltSplicing(struct altGraphX *ag, struct altSpliceSite **aSpliceList, 
			int *altSpliceSites, int *altSpliceLoci, int *totalSpliceSites)
/* Walk throught the altGraphX graph and look for evidence of altSplicing. */
{
struct altSpliceSite *aSplice = NULL;
bool **em = altGraphXCreateEdgeMatrix(ag);
int vCount = ag->vertexCount;
unsigned char *vTypes = ag->vTypes;
int *vPos = ag->vPositions;
int altSpliceSitesOrig = *altSpliceSites;
int i,j,k;
for(i=0; i<vCount; i++)
    {
    for(j=0; j<vCount; j++)
	{
	if(em[i][j] && areConsSplice(em, vCount, vTypes,i,j))
	    {
	    struct altSpliceSite *aSplice = NULL;
	    for(k=j+1; k<vCount; k++)
		{
		if(em[i][k] && areConsSplice(em, vCount, vTypes, i, k))
		    {
		    if(aSplice == NULL)
			{
			aSplice = initASplice(ag, i, j, k);
			(*altSpliceSites)++;
			}
		    else
			addSpliceSite(ag, aSplice, k);
		    }
		}
	    if(aSplice != NULL)
		slAddHead(aSpliceList, aSplice);
	    }
	}
    }
if(*altSpliceSites != altSpliceSitesOrig)
    (*altSpliceLoci)++;
altGraphXFreeEdgeMatrix(&em, vCount);
}

unsigned int maxInArray(unsigned int *array, int arrayCount)
{
int i;
unsigned int maxVal = 0;
for(i=0;i<arrayCount; i++)
    {
    maxVal = max(maxVal, array[i]);
    }
return maxVal;
}

void htmlLinkOut(char *db, struct altSpliceSite *as, FILE *out)
{
fprintf(out,"<br><a target=\"browser\" "
	"href=\"http://hgwdev-sugnet.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d&altGraphXCon=full\">",
       db, as->chrom, as->chromStart-25, maxInArray(as->altStarts, as->altCount)+25);
fprintf(out,"%s (%d)</a>\n", as->agName, as->altCount-1);
}

void writeOutFrames(FILE *htmlOut, char *fileName, char *db)
{
fprintf(htmlOut, "<html><head><title>Human Alt Splicing Conserved in Mouse</title></head>\n"
     "<frameset cols=\"18%,82%\">\n"
     "<frame name=\"_list\" src=\"./%s\">\n"
     "<frame name=\"browser\" src=\"http://hgwdev-sugnet.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=NM_005487&altGraphXCon=full\">\n"
     "</frameset>\n"
     "</html>\n", fileName, db);
}

void altSummary(char *db, char *agxFileName, char *summaryOutName, char *htmlOutName, char *htmlFramesOutName)
/* Look through a bunch of splice sites and output some statistics and links. */
{
struct altGraphX *agList = NULL, *ag = NULL;
struct altSpliceSite *aSpliceList = NULL, *aSplice=NULL;
FILE *htmlOut = NULL;
FILE *htmlFramesOut = NULL;
FILE *summaryOut = NULL;
int altSpliceSites = 0, altSpliceLoci = 0, totalSpliceSites = 0;
warn("Loading splicing graphs.");
agList = altGraphXLoadAll(agxFileName);
htmlFramesOut = mustOpen(htmlFramesOutName, "w");
htmlOut = mustOpen(htmlOutName, "w");
summaryOut = mustOpen(summaryOutName, "w");
writeOutFrames(htmlFramesOut, htmlOutName, db);
carefulClose(&htmlFramesOut);
warn("Examining splicing graphs.");
fprintf(htmlOut, "<html>\n<body bgcolor=\"#FFF9D2\"><b>Alt-Splice List</b><br>\n");
for(ag=agList; ag != NULL; ag=ag->next)
    {
    lookForAltSplicing(ag, &aSpliceList, &altSpliceSites, &altSpliceLoci, &totalSpliceSites);
    for(aSplice=aSpliceList; aSplice != NULL; aSplice= aSplice->next)
	{
 	altSpliceSiteOutput(aSplice, summaryOut, '\t', '\n');
	htmlLinkOut(db, aSplice, htmlOut);
	}
    altSpliceSiteFreeList(&aSpliceList);
    }
fprintf(htmlOut,"</body></html>\n");
warn("%d altSpliced sites in %d alt-spliced loci out of %d total loci.",
     altSpliceSites, altSpliceLoci, slCount(agList));
altGraphXFreeList(&agList);
carefulClose(&htmlOut);
carefulClose(&summaryOut);
}

int main(int argc, char *argv[])
{
if(argc != 6)
    usage();
altSummary(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
