/**
 * altAnalysis.c - Program to summarize the altSplicing in altGraphX's.
 */
#include "common.h"
#include "options.h"
#include "altGraphX.h"
#include "geneGraph.h"
#include "altSpliceSite.h"
#include "sample.h"
#include "hdb.h"

static char const rcsid[] = "$Id: altAnalysis.c,v 1.2 2003/07/13 06:16:15 sugnet Exp $";

static int alt5PrimeCount = 0;
static int alt3PrimeCount = 0;
static int altCassetteCount = 0;
static int altRetIntCount = 0;
static int altIdentityCount = 0;
static int altOtherCount = 0;
static int altTotalCount = 0;
static int totalLoci = 0;
static int totalSplices = 0;
static int splicedLoci = 0;

FILE *RData = NULL;         /* File for outputting summaries about alts, useful for R. */
FILE *RDataCont = NULL;     /* File for outputting summaries about controls, useful for R. */
FILE *upStream100 = NULL;   /* Bed file for 100bp upstream of 3' splice site. */
FILE *downStream100 = NULL; /* Bed file for 100bp downstream of 5' splice site. */
FILE *altRegion = NULL;     /* Bed file representing an alt-spliced region. */
FILE *constRegion = NULL;   /* Bed file representing a constituative region. */
static boolean doSScores = FALSE;

void usage()
{
errAbort("altAnalysis - Analyze the altSplicing in a series of altGraphX's\n"
	 "usage:\n"
	 "   altAnalysis db altGraphXIn.agx spliceSummaryOut.txt htmlLinks.html htmlFrameIndex.html\n"
	 );
}

void logSpliceType(enum altSpliceType type)
/* Log the different types of splicing. */
{

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
fprintf(stderr, "totalSplices:\t%d\n", totalSplices);
fprintf(stderr, "%d alt spliced loci out of %d total loci with %.2f alt splices per alt spliced loci\n",
	splicedLoci, totalLoci, (float)totalSplices/splicedLoci);
}

char * nameForType(struct altSpliceSite *as)
/* Log the different types of splicing. */
{
enum altSpliceType type;
if(as->altCount >2)
    return "altOther";
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
	errAbort("nameForType() - Don't recognize type %d", type);
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

int edgesInArea(struct altGraphX *ag, bool **em, int v1, int v2)
/* Return the number of edges in an area of the adjacency matrix. */
{
int sum =0;
int i=0, j=0;
int vC = ag->vertexCount;
for(i=0; i<=v1; i++) 
    {
    for(j=v2; j<vC; j++)
	{
	if(em[i][j])
	    sum++;
	}
    }
return sum;
}

boolean areConstitutive(struct altGraphX *ag, bool **em, int v1, int v2)
/* Return TRUE if there is no edge that goes around v1->v2, FALSE
   otherwise. 
   Idea is that if there is no edge around then there 
   should be a blank area in the edge matrix from defined
   by the area {0,v2}, {v1,v2}, {v1,vMax}, {0,vMax}
   seseses
   0123456
  00101000 // 0->1 or 0->3
  10010000
  20001000
  30000100
  40000010
  50000001
  60000000
*/
{
int sum = edgesInArea(ag, em, v1,v2);
return sum == 1;
}


boolean isAlt3Prime(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		    int *altBpStart, int *altBpEnd)
/* Return TRUE if we have an edge pattern of:
   he->hs----->he
     \-->hs--/
   Which inicates two possible starts to an exon (alt 3' starts).

   Use edgesInArea() to investigate an area that begins in the rows with the common
   hard end and finishes with common hard end. 
   01234 (4 vertices involved in alt splicing)
  0  
  1  ++
  2    +
  3    +
  4   
*/
{
unsigned char *vTypes = ag->vTypes;
int i=0;
int numAltVerts = 4; /* Number of edges in alt spliced portion of graph. */
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
	       rowSum(em[ve2],ag->vTypes,ag->vertexCount) == 1 && 
	       edgesInArea(ag,em,i-1,vs+1) == numAltVerts)
		{
		int *vPos = ag->vPositions;
		struct bed bedUp, bedDown, bedAlt, bedConst;
                /* Initialize some beds for reporting. */
		bedConst.chrom = bedUp.chrom = bedDown.chrom = bedAlt.chrom = ag->tName;
		bedConst.name = bedUp.name = bedDown.name = bedAlt.name = ag->name;
		if(sameString(ag->strand, "+"))
		    bedConst.score = bedUp.score = bedDown.score = bedAlt.score = alt3Prime;
		else
		    bedConst.score = bedUp.score = bedDown.score = bedAlt.score = alt5Prime;
		safef(bedConst.strand, sizeof(bedConst.strand), "%s", ag->strand);
		safef(bedUp.strand, sizeof(bedUp.strand), "%s", ag->strand);
		safef(bedDown.strand, sizeof(bedDown.strand), "%s", ag->strand);
		safef(bedAlt.strand, sizeof(bedDown.strand), "%s", ag->strand);

                /* Alt spliced region. */
		bedAlt.chromStart = vPos[ve1];
		bedAlt.chromEnd = vPos[ve2];
		bedConst.chromStart = vPos[ve2];
		bedConst.chromEnd = vPos[i];

                /* Upstream/down stream */
		if(sameString(ag->strand, "+"))
		    {
		    bedDown.chromStart = vPos[i];
		    bedDown.chromEnd = vPos[i]+100;
		    bedUp.chromStart = vPos[ve1]-100;
		    bedUp.chromEnd = vPos[ve1];
		    }
		else 
		    {
		    bedUp.chromStart = vPos[i];
		    bedUp.chromEnd = vPos[i]+100;
		    bedDown.chromStart = vPos[ve1]-100;
		    bedDown.chromEnd = vPos[ve1];
		    }
		if(altRegion)
		    {
		    bedOutputN(&bedConst, 6, constRegion, '\t','\n');
		    bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
		    bedOutputN(&bedDown, 6, downStream100, '\t', '\n');
		    bedOutputN(&bedUp, 6, upStream100, '\t', '\n');
		    }		
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

   Use edgesInArea() to investigate an area that begins in the rows with the common
   hard end and finishes with common hard end. 
   esees
   01234 (4 vertices involved in alt splicing)
  0  
  1  ++
  2    +
  3    +
  4   
*/
{
unsigned char *vTypes = ag->vTypes;
int i=0;
int numAltVerts = 4;
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
	       rowSum(em[ve2],ag->vTypes,ag->vertexCount) == 1 &&
	       edgesInArea(ag,em,i-1,vs+1) == numAltVerts)
		{
		int *vPos = ag->vPositions;
		struct bed bedUp, bedDown, bedAlt, bedConst;
                /* Initialize some beds for reporting. */
		bedConst.chrom = bedUp.chrom = bedDown.chrom = bedAlt.chrom = ag->tName;
		bedConst.name = bedUp.name = bedDown.name = bedAlt.name = ag->name;
		if(sameString(ag->strand, "+"))
		    bedConst.score = bedUp.score = bedDown.score = bedAlt.score = alt5Prime;
		else
		    bedConst.score = bedUp.score = bedDown.score = bedAlt.score = alt3Prime;
		safef(bedConst.strand, sizeof(bedConst.strand), "%s", ag->strand);
		safef(bedUp.strand, sizeof(bedUp.strand), "%s", ag->strand);
		safef(bedDown.strand, sizeof(bedDown.strand), "%s", ag->strand);
		safef(bedAlt.strand, sizeof(bedDown.strand), "%s", ag->strand);

                /* Alt spliced region. */
		bedAlt.chromStart = vPos[ve1];
		bedAlt.chromEnd = vPos[ve2];
		bedConst.chromStart = vPos[vs];
		bedConst.chromEnd = vPos[ve1];

                /* Upstream/down stream */
		if(sameString(ag->strand, "+"))
		    {
		    bedDown.chromStart = vPos[ve2];
		    bedDown.chromEnd = vPos[ve2]+100;
		    bedUp.chromStart = vPos[vs]-100;
		    bedUp.chromEnd = vPos[vs];
		    }
		else 
		    {
		    bedUp.chromStart = vPos[ve2];
		    bedUp.chromEnd = vPos[ve2]+100;
		    bedDown.chromStart = vPos[vs]-100;
		    bedDown.chromEnd = vPos[vs];
		    }
		if(altRegion)
		    {
		    bedOutputN(&bedConst, 6, constRegion, '\t','\n');
		    bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
		    bedOutputN(&bedDown, 6, downStream100, '\t', '\n');
		    bedOutputN(&bedUp, 6, upStream100, '\t', '\n');
		    }		
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

   Use edgesInArea() to investigate that encompasses the common hard
   end and common hard start. Should only be 4 edges in area defined by
   splicing.
   sesese 
   012345 
  0  
  1  + +
  2   + 
  3    +
  4   
  5
*/
{
unsigned char *vTypes = ag->vTypes;
int i=0;
int numAltVerts = 4;
int *vPos = ag->vPositions;
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
	       colSum(em, ag->vTypes, ag->vertexCount, ve1) == 1 &&
	       edgesInArea(ag,em,ve2-1,vs+1) == numAltVerts)
		{
		struct bed bedUp, bedDown, bedAlt;
		/* Initialize some beds for reporting. */
		bedUp.chrom = bedDown.chrom = bedAlt.chrom = ag->tName;
		bedUp.name = bedDown.name = bedAlt.name = ag->name;
		bedUp.score = bedDown.score = bedAlt.score = altCassette;
		safef(bedUp.strand, sizeof(bedUp.strand), "%s", ag->strand);
		safef(bedDown.strand, sizeof(bedDown.strand), "%s", ag->strand);
		safef(bedAlt.strand, sizeof(bedDown.strand), "%s", ag->strand);
		/* Alt spliced region. */
		bedAlt.chromStart = vPos[ve1];
		bedAlt.chromEnd = vPos[i];

		/* Upstream/down stream */
		if(sameString(ag->strand, "+"))
		    {
		    bedUp.chromStart = vPos[ve1] - 100;
		    bedUp.chromEnd = vPos[ve1];
		    bedDown.chromStart = vPos[i];
		    bedDown.chromEnd = vPos[i] + 100;
		    }
		else 
		    {
		    bedDown.chromStart = vPos[ve1] - 100;
		    bedDown.chromEnd = vPos[ve1];
		    bedUp.chromStart = vPos[i];
		    bedUp.chromEnd = vPos[i] + 100;
		    }
		if(altRegion != NULL)
		    {
		    bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
		    bedOutputN(&bedUp, 6, upStream100, '\t', '\n');
		    bedOutputN(&bedDown, 6, downStream100, '\t', '\n');
		    }
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

   Use edgesInArea() to investigate that encompasses the common hard
   end and common hard start. Should only be 4 edges in area defined by
   splicing.
   eseses 
   012345 
  0  
  1  + +
  2   + 
  3    +
  4   
  5
*/
{
unsigned char *vTypes = ag->vTypes;
int i=0;
int numAltVerts = 4;

/* Quick check. */
if(vTypes[vs] != ggHardStart || vTypes[ve1] != ggHardEnd || vTypes[ve2] != ggHardEnd)
    return FALSE;
if(em[vs][ve1] && em[vs][ve2]) 
    {
    /* Try to find a hard end that connects ve1 and ve2. */
    for(i=0; i<ag->vertexCount; i++)
	{
	if(vTypes[i] == ggHardStart && em[ve1][i] && em[i][ve2] &&
	edgesInArea(ag,em,ve2-1,vs+1) == numAltVerts)
	    {
	    int *vPos = ag->vPositions;
	    struct bed bedAlt;
	    /* Initialize some beds for reporting. */
	    bedAlt.chrom = ag->tName;
	    bedAlt.name = ag->name;
	    bedAlt.score = altRetInt;
	    safef(bedAlt.strand, sizeof(bedAlt.strand), "%s", ag->strand);
	    /* Alt spliced region. */
	    bedAlt.chromStart = vPos[ve1];
	    bedAlt.chromEnd = vPos[i];
	    
	    /* Upstream/down stream */
	    if(altRegion != NULL)
		{
		bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
		}
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
    {
    ast = altIdentity;
    *altBpStart = ag->vPositions[vs];
    *altBpEnd = ag->vPositions[ve1];
    }
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
int i=0;
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

AllocArray(as->altCons, ag->vertexCount);
AllocArray(as->upStreamCons, ag->vertexCount);
AllocArray(as->downStreamCons, ag->vertexCount);
for(i=0; i<ag->vertexCount; i++)
    as->altCons[i] = as->upStreamCons[i] = as->downStreamCons[i] = -1;

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

void outputForR(struct altSpliceSite *as, int edge, FILE *out)
{
assert(edge < as->altCount);
fprintf(out, "%s\t%d\t%d\t%s\t%s\t%d\t%d\t",
	as->chrom,
	as->chromStart,
	as->chromEnd,
	as->agName,
	as->strand,
	as->index,
	as->type);
fprintf(out, "%d\t%d\t%d\t%d\t%d\t%f\t%f\t%f\n",
	as->vIndexes[edge],
	as->spliceTypes[edge],
	as->support[edge],
	as->altBpStarts[edge],
	as->altBpEnds[edge],
	as->altCons[edge],
	as->upStreamCons[edge],
	as->downStreamCons[edge]);
}

void outputRHeader(FILE *out)
{
fprintf(out, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t",
	"chrom",
	"chromStart",
	"chromEnd",
	"agName",
	"strand",
	"index",
	"type");
fprintf(out, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
	"vIndexes",
	"altTypes",
	"support",
	"altBpStarts",
	"altBpEnds",
	"altCons",
	"upStreamCons",
	"downStreamCons");
}

float aveSScoreForRegion(char *chrom, int chromStart, int chromEnd)
{
float currentSum = 0;
int count = 0;
float ave = 0;
int sPos = 0;
struct sample *sList=NULL, *s = NULL;
char *tableName = optionVal("sampleTable", "hg15Mm3L");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr=NULL;
char **row;
char buff[256];
int rowOffset=0;
safef(buff,sizeof(buff),"%s_%s",chrom, tableName);
if(hTableExists(buff))
    {
    sr = hRangeQuery(conn, tableName, chrom, chromStart, chromEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	s = sampleLoad(row + rowOffset);
	slAddHead(&sList, s);
	}
    
    for(s = sList; s != NULL; s = s->next)
	{
	int i=0;
	for(i=0; i<s->sampleCount; i++)
	    {
	    sPos = s->chromStart + s->samplePosition[i];
	    if(sPos >= chromStart && sPos <= chromEnd)
		{
		currentSum += ((float) s->sampleHeight[i]) / 1000 * 8.0;
		count++;
		}
	    }
	}
    sqlFreeResult(&sr);
    slFreeList(&sList);
    }
hFreeConn(&conn);
if(count > 0)
    ave = currentSum / count;
else
    ave = -1;
return ave;
}

void fillInSscores(struct altSpliceSite *as, int edge)
{
int offSet = 100;
as->altCons[edge] = aveSScoreForRegion(as->chrom, as->altBpStarts[edge], as->altBpEnds[edge]);
if(as->spliceTypes[edge] == alt3Prime)
    {
    as->upStreamCons[edge] = aveSScoreForRegion(as->chrom, as->altBpStarts[edge] - offSet, as->altBpStarts[edge]);
    /* We have a problem here which is that we don't know the downstream end. */
    as->downStreamCons[edge] = -1;
    }
else if(as->spliceTypes[edge] == alt5Prime)
    {
    as->upStreamCons[edge] = aveSScoreForRegion(as->chrom, as->altStarts[edge-1] - offSet, as->altStarts[edge-1]);
    as->downStreamCons[edge] = aveSScoreForRegion(as->chrom, as->altBpEnds[edge], as->altBpEnds[edge] + 100);
    }
else if(as->spliceTypes[edge] == altCassette || as->spliceTypes[edge] == altIdentity)
    {
    /* This works for cassette exons, and all exons in general. */
    as->upStreamCons[edge] = aveSScoreForRegion(as->chrom, as->altBpStarts[edge] - offSet, as->altBpStarts[edge]);
    as->downStreamCons[edge] = aveSScoreForRegion(as->chrom, as->altBpEnds[edge], as->altBpEnds[edge] +offSet);
    }
else 
    {
    as->upStreamCons[edge] = -1;
    as->downStreamCons[edge] = -1;
    }
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 20;
static int dot = 20;
if (--dot <= 0)
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

void fixOtherStrand(struct altSpliceSite *as)
/* Move some things around if we're on the '-' strand. */
{
float tmp = 0;
int i = 0;
if(sameString(as->strand, "+"))
    return;
assert(sameString(as->strand, "-"));
for(i=0; i<as->altCount; i++)
    {
    if(as->spliceTypes[i] == alt5Prime)
	as->spliceTypes[i] = alt3Prime;
    else if(as->spliceTypes[i] == alt3Prime)
	as->spliceTypes[i] = alt5Prime;
    tmp = as->upStreamCons[i];
    as->upStreamCons[i] = as->downStreamCons[i];
    as->downStreamCons[i] = tmp;
    }
}

boolean areHard(struct altGraphX *ag, int i, int j)
{
if(ag->vTypes[i] == ggSoftStart || ag->vTypes[i] == ggSoftEnd ||
   ag->vTypes[j] == ggSoftStart || ag->vTypes[j] == ggSoftEnd)
    return FALSE;
return TRUE;
}

void outputControlExonBeds(struct altGraphX *ag, int v1, int v2)
{
int *vPos = ag->vPositions;
struct bed bedUp, bedDown, bedAlt;
/* Initialize some beds for reporting. */
bedUp.chrom = bedDown.chrom = bedAlt.chrom = ag->tName;
bedUp.name = bedDown.name = bedAlt.name = ag->name;
bedUp.score = bedDown.score = bedAlt.score = altControl;
safef(bedUp.strand, sizeof(bedUp.strand), "%s", ag->strand);
safef(bedDown.strand, sizeof(bedDown.strand), "%s", ag->strand);
safef(bedAlt.strand, sizeof(bedDown.strand), "%s", ag->strand);
/* Alt spliced region. */
bedAlt.chromStart = vPos[v1];
bedAlt.chromEnd = vPos[v2];

/* Upstream/down stream */
if(sameString(ag->strand, "+"))
    {
    bedUp.chromStart = vPos[v1] - 100;
    bedUp.chromEnd = vPos[v1];
    bedDown.chromStart = vPos[v2];
    bedDown.chromEnd = vPos[v2] + 100;
    }
else 
    {
    bedDown.chromStart = vPos[v1] - 100;
    bedDown.chromEnd = vPos[v1];
    bedUp.chromStart = vPos[v2];
    bedUp.chromEnd = vPos[v2] + 100;
    }

bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
bedOutputN(&bedUp, 6, upStream100, '\t', '\n');
bedOutputN(&bedDown, 6, downStream100, '\t', '\n');
}

void lookForAltSplicing(struct altGraphX *ag, struct altSpliceSite **aSpliceList, 
			int *altSpliceSites, int *altSpliceLoci, int *totalSpliceSites)
/* Walk throught the altGraphX graph and look for evidence of altSplicing. */
{
struct altSpliceSite *aSplice = NULL;
struct altSpliceSite *notAlt = NULL, *notAltList = NULL;
bool **em = altGraphXCreateEdgeMatrix(ag);
int vCount = ag->vertexCount;
unsigned char *vTypes = ag->vTypes;
int *vPos = ag->vPositions;
int altSpliceSitesOrig = *altSpliceSites;
int i,j,k;
int altCount = 0;
boolean gotOne = FALSE;
occassionalDot();
totalLoci++;
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
		    totalSplices++;
		    if(aSplice == NULL)
			{
			splicedLoci++;
			gotOne = TRUE;
			aSplice = initASplice(ag, em, i, j, k);
			(*altSpliceSites)++;
			}
		    else
			addSpliceSite(ag, em, aSplice, k);
		    }
		}
	    }
	else 
	    {
	    /* Only want non alt-spliced exons for our controls. */
	    if(em[i][j] && rowSum(em[i], ag->vTypes, ag->vertexCount) &&
	       altGraphXEdgeVertexType(ag, i, j) == ggExon &&
	       areHard(ag, i, j) && 
	       areConstitutive(ag, em, i, j))
		{
		notAlt = initASplice(ag, em, i, j, j);
		if(altRegion != NULL) 
		    outputControlExonBeds(ag, i, j);
		slAddHead(&notAltList, notAlt);
		} 
	    }
	}
    if(aSplice != NULL) 
	{
	slAddHead(aSpliceList, aSplice);
	}
    /* If we have a simple splice classfy it and log it. */
    if(aSplice != NULL && aSplice->altCount == 2)
	{
	altTotalCount++;	
	logSpliceType(aSplice->spliceTypes[1]);
	if(doSScores)
	    fillInSscores(aSplice, 1);
	if(RData != NULL)
	    {
	    fixOtherStrand(aSplice);
	    outputForR(aSplice, 1, RData);
	    }
	}
    /* Otherwise log it as altOther. Start at 1 as 0->1 is the first
     * splice, 1->2 is the first alt spliced.*/
    else if(aSplice != NULL)
	{
	for(altCount=1; altCount<aSplice->altCount; altCount++)
	    {
	    altTotalCount++;
	    altOtherCount++;
	    }
	}
    }
for(notAlt = notAltList; notAlt != NULL; notAlt = notAlt->next)
{
if(doSScores)
    fillInSscores(notAlt, 1);
if(RData != NULL)
    {
    fixOtherStrand(notAlt);
    outputForR(notAlt, 1, RDataCont);
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
fprintf(out,"<tr><td><a target=\"browser\" "
	"href=\"http://hgwdev-sugnet.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d&altGraphXCon=full\">",
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
     "<frameset cols=\"18%,82%\">\n"
     "<frame name=\"_list\" src=\"./%s\">\n"
     "<frame name=\"browser\" src=\"http://hgwdev-sugnet.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=NM_005487&altGraphXCon=full\">\n"
     "</frameset>\n"
     "</html>\n", fileName, db);
}

void openBedFiles(char *prefix)
/* Opend all of the bed files with the prefix. */
{
struct dyString *name = newDyString(strlen(prefix)+20);

dyStringClear(name);
dyStringPrintf(name, "%s.upstream.bed", prefix);
upStream100 = mustOpen(name->string, "w");

dyStringClear(name);
dyStringPrintf(name, "%s.downstream.bed", prefix);
downStream100 = mustOpen(name->string, "w");

dyStringClear(name);
dyStringPrintf(name, "%s.alt.bed", prefix);
altRegion = mustOpen(name->string, "w");

dyStringClear(name);
dyStringPrintf(name, "%s.const.bed", prefix);
constRegion = mustOpen(name->string, "w");
dyStringFree(&name);
}

void altSummary(char *db, char *agxFileName, char *summaryOutName, char *htmlOutName, char *htmlFramesOutName)
/* Look through a bunch of splice sites and output some statistics and links. */
{
struct altGraphX *agList = NULL, *ag = NULL;
struct altSpliceSite *aSpliceList = NULL, *aSplice=NULL;
char *RDataName = optionVal("RData", NULL);
char *bedName = optionVal("bedName", NULL);
FILE *htmlOut = NULL;
FILE *htmlFramesOut = NULL;
FILE *summaryOut = NULL;
int altSpliceSites = 0, altSpliceLoci = 0, totalSpliceSites = 0;
warn("Loading splicing graphs.");
agList = altGraphXLoadAll(agxFileName);
hSetDb(db);
htmlFramesOut = mustOpen(htmlFramesOutName, "w");
htmlOut = mustOpen(htmlOutName, "w");
summaryOut = mustOpen(summaryOutName, "w");
if(RDataName != NULL)
    {
    char buff[256];
    safef(buff, sizeof(buff), "%s.control", RDataName);
    RDataCont = mustOpen(buff, "w");
    outputRHeader(RDataCont);
    safef(buff, sizeof(buff), "%s.alt", RDataName);
    RData = mustOpen(buff, "w");
    outputRHeader(RData);
    }
if(bedName != NULL)
    {
    openBedFiles(bedName);
    }

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
warn("\nDone.");
fprintf(htmlOut,"</body></html>\n");
warn("%d altSpliced sites in %d alt-spliced loci out of %d total loci.",
     altSpliceSites, altSpliceLoci, slCount(agList));
printSpliceTypeInfo();
altGraphXFreeList(&agList);
if(RData != NULL)
    {
    carefulClose(&RData);
    carefulClose(&RDataCont);
    }
carefulClose(&htmlOut);
carefulClose(&summaryOut);
}

int main(int argc, char *argv[])
{
if(argc < 6)
    usage();
optionHash(&argc, argv);
doSScores = optionExists("doSScores");
altSummary(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
