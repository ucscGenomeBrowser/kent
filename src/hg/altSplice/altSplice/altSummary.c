/**
 * altSummary.c - Program to summarize the altSplicing in altGraphX's.
 */
#include "common.h"
#include "options.h"
#include "altGraphX.h"
#include "geneGraph.h"
#include "altSpliceSite.h"




static int alt5PrimeCount = 0;
static int alt3PrimeCount = 0;
static int altCassetteCount = 0;
static int altRetIntCount = 0;
static int altIdentityCount = 0;
static int altOtherCount = 0;
static int altTotalCount = 0;

void usage()
{
errAbort("altSummary - Summarize the altSplicing in a series of altGraphX's\n"
	 "usage:\n"
	 "   altSummary db altGraphXIn.agx spliceSummaryOut.txt htmlLinks.html htmlFrameIndex.html\n"
	 );
}

void logSpliceType(enum altSpliceType type)
/* Log the different types of splicing. */
{
altTotalCount++;
switch (type) 
    {
    case alt5Prime:
	alt5PrimeCount++;
	break;
    case alt3Prime: 
	alt3PrimeCount++;
	break;
    case altCassette:
	altCassetteCount++;
	break;
    case altRetInt:
	altRetIntCount++;
	break;
    case altOther:
	altOtherCount++;
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
}

void printSpliceTypeInfo()
{
fprintf(stderr, "alt 5' Count:\t%d\n", alt5PrimeCount);
fprintf(stderr, "alt 3' Count:\t%d\n", alt3PrimeCount);
fprintf(stderr, "alt Cass Count:\t%d\n", altCassetteCount);
fprintf(stderr, "alt Ret. Int. Count:\t%d\n", altRetIntCount);
fprintf(stderr, "alt Ident. Count:\t%d\n", altIdentityCount);
fprintf(stderr, "alt Other Count:\t%d\n", altOtherCount);
fprintf(stderr, "alt Total Count:\t%d\n", altTotalCount);
}

char * nameForType(struct altSpliceSite *as)
/* Log the different types of splicing. */
{
enum altSpliceType type;
if(as->altCount >2)
    return "other";
else
    type = as->spliceTypes[1];
switch (type) 
    {
    case alt5Prime:
	return "alt5Prime";
	break;
    case alt3Prime: 
	return "alt3Prime";
	break;
    case altCassette:
	return "altCassette";
	break;
    case altRetInt:
	return "altRetInt";
	break;
    case altOther:
	return "altOther";
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
return "error";
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

int colSum(bool **em, unsigned char *vTypes, int vCount, int col)
{
int count =0;
int i=0;
for(i=0; i<vCount; i++)
   {
   if(em[i][col] && (vTypes[i] == ggHardStart || vTypes[i] == ggHardEnd) )
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

boolean isAlt3Prime(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		    int *altBpStart, int *altBpEnd)
/* Return TRUE if we have an edge pattern of:
   he->hs----->he
     \-->hs--/
   Which inicates two possible starts to an exon (alt 3' starts).
*/
{
unsigned char *vTypes = ag->vTypes;
int i=0;
/* Quick check. */
if(vTypes[vs] != ggHardEnd || vTypes[ve1] != ggHardStart || vTypes[ve2] != ggHardStart)
    return FALSE;

if(em[vs][ve1] && em[vs][ve2]) 
    {
    /* Try to find common end for the two hard starts. */
    for(i=0; i<ag->vertexCount; i++)
	{
	if(vTypes[i] == ggHardEnd && em[ve1][i] && em[ve2][i])
	    {
	    /* Make sure that they only connect to that one hard start. */
	    if(rowSum(em[ve1],ag->vTypes,ag->vertexCount) == 1 &&
	       rowSum(em[ve2],ag->vTypes,ag->vertexCount) == 1)
		{
		*altBpStart = ag->vPositions[ve1];
		*altBpEnd = ag->vPositions[ve2];
		return TRUE;
		}
	    }
	}
    }
return FALSE;
}

boolean isAlt5Prime(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		    int *altBpStart, int *altBpEnd)
/* Return TRUE if we have an edge pattern of:
   hs->he----->hs
     \-->he--/
   Which indicates two possible ends to an exon (alt 5' intron starts).
*/
{
unsigned char *vTypes = ag->vTypes;
int i=0;
/* Quick check. */
if(vTypes[vs] != ggHardStart || vTypes[ve1] != ggHardEnd || vTypes[ve2] != ggHardEnd)
    return FALSE;

if(em[vs][ve1] && em[vs][ve2]) 
    {
    /* Try to find common start for the two hard ends. */
    for(i=0; i<ag->vertexCount; i++)
	{
	if(vTypes[i] == ggHardStart && em[ve1][i] && em[ve2][i])
	    {
	    /* Make sure that they only connect to that one hard start. */
	    if(rowSum(em[ve1],ag->vTypes,ag->vertexCount) == 1 &&
	       rowSum(em[ve2],ag->vTypes,ag->vertexCount) == 1)
		{
		*altBpStart = ag->vPositions[ve1];
		*altBpEnd = ag->vPositions[ve2];
		return TRUE;
		}
	    }
	}
    }
return FALSE;
}

boolean isCassette(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		    int *altBpStart, int *altBpEnd)
/* Return TRUE if SIMPLE cassette exon.
   Looking for pattern:
   he--->hs---->he---->hs
     \----------------/
*/
{
unsigned char *vTypes = ag->vTypes;
int i=0;
/* Quick check. */
if(vTypes[vs] != ggHardEnd || vTypes[ve1] != ggHardStart || vTypes[ve2] != ggHardStart)
    return FALSE;

if(em[vs][ve1] && em[vs][ve2]) 
    {
    /* Try to find a hard end that connects ve1 and ve2. */
    for(i=0; i<ag->vertexCount; i++)
	{
	if(vTypes[i] == ggHardEnd && em[ve1][i] && em[i][ve2])
	    {
	    /* Make sure that our cassette only connect to downstream. otherwise not
	       so simple...*/
	    if(rowSum(em[i],ag->vTypes,ag->vertexCount) == 1 &&
	       rowSum(em[ve1],ag->vTypes,ag->vertexCount) == 1 &&
	       colSum(em, ag->vTypes, ag->vertexCount, ve1) == 1)
		{
		*altBpStart = ag->vPositions[ve1];
		*altBpEnd = ag->vPositions[i];
		return TRUE;
		}
	    }
	}
    }
return FALSE;
}

boolean isRetainIntron(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		       int *altBpStart, int *altBpEnd)
/* return TRUE if retained intron. 
   Looking for pattern:
   hs-->he---->hs--->he
    \---------------/
*/
{
unsigned char *vTypes = ag->vTypes;
int i=0;
/* Quick check. */
if(vTypes[vs] != ggHardStart || vTypes[ve1] != ggHardEnd || vTypes[ve2] != ggHardEnd)
    return FALSE;
if(em[vs][ve1] && em[vs][ve2]) 
    {
    /* Try to find a hard end that connects ve1 and ve2. */
    for(i=0; i<ag->vertexCount; i++)
	{
	if(vTypes[i] == ggHardStart && em[ve1][i] && em[i][ve2])
	    {
	    *altBpStart = ag->vPositions[ve1];
	    *altBpEnd = ag->vPositions[i];
	    return TRUE;
	    }
	}
    }
return FALSE;
}	    

int altSpliceType(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		  int *altBpStart, int *altBpEnd)
/* Return type of splicing. Depends on direction of transcription. 
        hs     he          hs      he  
IIIIIIIIEEEEEEEEEIIIIIIIIIIEEEEEEEEEEIIIIIIIIII
*/
{
enum altSpliceType ast = altOther;
if(ve1 == ve2)
    ast = altIdentity;
else if(isAlt3Prime(ag, em, vs, ve1, ve2, altBpStart, altBpEnd))
    ast = alt3Prime;
else if(isAlt5Prime(ag, em, vs, ve1, ve2, altBpStart, altBpEnd))
    ast = alt5Prime;
else if(isCassette(ag, em, vs, ve1, ve2, altBpStart, altBpEnd))
    ast = altCassette;
else if(isRetainIntron(ag, em, vs, ve1, ve2, altBpStart, altBpEnd))
    ast = altRetInt;
else
    ast = altOther;
return ast;
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

struct altSpliceSite *initASplice(struct altGraphX *ag, bool **agEm,  int vs, int ve1, int ve2)
/* Initialize an altSplice site report with vlaues. */
{
struct altSpliceSite *as = NULL;
struct evidence *ev = NULL;
int edgeNum = 0;
int altBpStart=0, altBpEnd=0;
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

/* Record splice types and bases alt spliced. */
AllocArray(as->spliceTypes, ag->vertexCount);
AllocArray(as->altBpEnds, ag->vertexCount);
AllocArray(as->altBpStarts, ag->vertexCount);
as->spliceTypes[0] = altSpliceType(ag, agEm, vs, ve1, ve1, &altBpStart, &altBpEnd);
as->altBpStarts[0] = altBpStart;
as->altBpEnds[0] = altBpEnd;

as->spliceTypes[1] = altSpliceType(ag, agEm,vs, ve1, ve2, &altBpStart, &altBpEnd);
as->altBpStarts[1] = altBpStart;
as->altBpEnds[1] = altBpEnd;

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

void addSpliceSite(struct altGraphX *ag, bool **agEm, struct altSpliceSite *aSplice, int ve)
/* Add another end site to an existing splicing event. */
{
struct evidence *ev = NULL;
int edgeNum = 0;
int altBpStart =0;
int altBpEnd = 0;
aSplice->altStarts[aSplice->altCount] = ag->vPositions[ve];
aSplice->altTypes[aSplice->altCount] = ag->vTypes[ve];
aSplice->spliceTypes[aSplice->altCount] = altSpliceType(ag, agEm, aSplice->index, 
							aSplice->vIndexes[aSplice->altCount-1], ve,
							&altBpStart, &altBpEnd);
aSplice->altBpStarts[aSplice->altCount] = altBpStart;
aSplice->altBpEnds[aSplice->altCount] = altBpEnd;
edgeNum = getEdgeNum(ag, (int)aSplice->index, ve);
ev = slElementFromIx(ag->evidence, edgeNum);
aSplice->support[aSplice->altCount] = ev->evCount;
aSplice->altCount++;
}

void lookForAltSplicing(struct altGraphX *ag, struct altSpliceSite **aSpliceList, 
			int *altSpliceSites, int *altSpliceLoci, int *totalSpliceSites)
/* Walk throught the altGraphX graph and look for evidence of altSplicing. */
{
bool **em = altGraphXCreateEdgeMatrix(ag);
int vCount = ag->vertexCount;
unsigned char *vTypes = ag->vTypes;
int altSpliceSitesOrig = *altSpliceSites;
int i,j,k;
for(i=0; i<vCount; i++)
    {
    struct altSpliceSite *aSplice = NULL;
    for(j=0; j<vCount; j++)
	{
	if(em[i][j] && areConsSplice(em, vCount, vTypes,i,j))
	    {
	    for(k=j+1; k<vCount; k++)
		{
		if(em[i][k] && areConsSplice(em, vCount, vTypes, i, k))
		    {
		    if(aSplice == NULL)
			{
			aSplice = initASplice(ag, em, i, j, k);
			(*altSpliceSites)++;
			}
		    else
			addSpliceSite(ag, em, aSplice, k);
		    }
		}
	    }
	}
    if(aSplice != NULL)
	slAddHead(aSpliceList, aSplice);
    if(aSplice != NULL && aSplice->altCount == 2)
	logSpliceType(aSplice->spliceTypes[1]);
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
fprintf(out,"<tr><td><a target=\"browser\" "
	"href=\"http://hgwdev-sugnet.gi.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d&altGraphXCon=full\">",
       db, as->chrom, as->chromStart-50, maxInArray(as->altStarts, as->altCount)+50);
fprintf(out,"%s (%d)</a></td>", as->agName, as->altCount-1);
fprintf(out,"<td>%s</td>", nameForType(as));
if(as->altCount == 2)
    fprintf(out,"<td>%d</td></tr>\n", as->altBpEnds[1] - as->altBpStarts[1]);
else
    fprintf(out,"<td>N/A</td></tr>\n");
}

void writeOutFrames(FILE *htmlOut, char *fileName, char *db)
{
fprintf(htmlOut, "<html><head><title>Human Alt Splicing Conserved in Mouse</title></head>\n"
     "<frameset cols=\"18%%,82%%\">\n"
     "<frame name=\"_list\" src=\"./%s\">\n"
     "<frame name=\"browser\" src=\"http://hgwdev-sugnet.gi.ucsc.edu/cgi-bin/hgTracks?db=%s&position=NM_005487&altGraphXCon=full\">\n"
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
fprintf(htmlOut, "<html>\n<body bgcolor=\"#FFF9D2\"><b>Alt-Splice List</b>\n"
	"<table border=1><tr><th>Name (count)</th><th>Type</th><th>Size</th></tr>\n");
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
printSpliceTypeInfo();
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
