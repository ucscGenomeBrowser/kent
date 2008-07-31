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

static char const rcsid[] = "$Id: altAnalysis.c,v 1.17.78.1 2008/07/31 02:23:57 markd Exp $";
static int alt5Flipped = 0;
static int alt3Flipped = 0;
static int minConfidence = 0;
static int flankingSize = 0;
static int alt5PrimeCount = 0;
static int alt3PrimeCount = 0;
static int alt3Prime3bpCount = 0;
static int altCassetteCount = 0;
static int altRetIntCount = 0;
static int altIdentityCount = 0;
static int altOtherCount = 0;
static int altTotalCount = 0;
static int totalLoci = 0;
static int totalSplices = 0;
static int splicedLoci = 0;
static int minControlConf = 0; /* Minimum number of edges before an
				* exon is considered a control. */
FILE *bedViewOutFile = NULL;/* File for bed containing region of alt-splicing. */
FILE *RData = NULL;         /* File for outputting summaries about alts, useful for R. */
FILE *RDataCont = NULL;     /* File for outputting summaries about controls, useful for R. */
FILE *upStream100 = NULL;   /* Bed file for 100bp upstream of 3' splice site. */
FILE *downStream100 = NULL; /* Bed file for 100bp downstream of 5' splice site. */
FILE *altRegion = NULL;     /* Bed file representing an alt-spliced region. */
FILE *constRegion = NULL;   /* Bed file representing a constituative region. */
FILE *alt3bpRegion = NULL;  /* Bed file representing 3bp alt3 prime splices. */
FILE *altGraphXOut = NULL;  /* AltgraphX exon representing alt 3', alt 5' and cassette exons. */
FILE *altLogFile = NULL;    /* File to output names of graphs that have alt splicing. */
static boolean doSScores = FALSE;

void usage()
{
errAbort("altAnalysis - Analyze the altSplicing in a series of altGraphX's\n"
	 "usage:\n"
	 "   altAnalysis db altGraphXIn.agx spliceSummaryOut.txt htmlLinks.html htmlFrameIndex.html\n"
	 );
}

void logSpliceType(enum altSpliceType type, int size)
/* Log the different types of splicing. */
{
switch (type) 
    {
    case alt5Prime:
	alt5PrimeCount++;
	break;
    case alt3Prime: 
	alt3PrimeCount++;
	if(size == 3)
	    alt3Prime3bpCount++;
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
	
void printSpliceTypeInfo(int altSpliceLoci)
{
fprintf(stderr, "alt 5' Count:\t%d (%d on '-' strand)\n", alt5PrimeCount, alt3Flipped);
fprintf(stderr, "alt 3' Count:\t%d (%d on '-' strand, %d are 3bp)\n", alt3PrimeCount, alt5Flipped, alt3Prime3bpCount);
fprintf(stderr, "alt Cass Count:\t%d\n", altCassetteCount);
fprintf(stderr, "alt Ret. Int. Count:\t%d\n", altRetIntCount);
fprintf(stderr, "alt Ident. Count:\t%d\n", altIdentityCount);
fprintf(stderr, "alt Other Count:\t%d\n", altOtherCount);
fprintf(stderr, "alt Total Count:\t%d\n", altTotalCount);
fprintf(stderr, "totalSplices:\t%d\n", totalSplices);
fprintf(stderr, "%d alt spliced sites out of %d total loci with %.2f alt splices per alt spliced loci\n",
	splicedLoci, totalLoci, (float)totalSplices/altSpliceLoci);
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
   (vTypes[v2] == ggHardStart || vTypes[v2] == ggHardEnd) ) // && (rowSum(em[v2], vTypes, vCount ) > 0) )
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

int findClosestDownstreamVertex(struct altGraphX *ag, bool **em, int v)
/* Return the closest vertex that connects from v. -1 if none connect. */ 
{
int i;
int minV=-1;
int minDist = BIGNUM;
int *vPos = ag->vPositions;
for(i=v+1; i<ag->vertexCount; i++)
    {
    if(em[v][i]) 
	{
	int dist = vPos[i] - vPos[v];
	if(dist < minDist)
	    {
	    minDist = dist;
	    minV = i;
	    }
	}
    }
return minV;
}

int findClosestUpstreamVertex(struct altGraphX *ag, bool **em, int v)
/* Return the closest vertex that connects to v. -1 if none connect. */ 
{
int i;
int minV=-1;
int minDist = BIGNUM;
int *vPos = ag->vPositions;
for(i=0; i<v; i++)
    {
    if(em[i][v]) 
	{
	int dist = vPos[v] - vPos[i];
	if(dist < minDist)
	    {
	    minDist = dist;
	    minV = i;
	    }
	}
    }
return minV;
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
int sum = 0;
int i = 0;
int vC = ag->vertexCount;
for(i = 0; i < vC; i++)
    {
    if(em[i][v1])
	{
	sum = edgesInArea(ag, em, i, v1);
	if(sum != 1)
	    return FALSE;
	}
    if(em[v2][i])
	{
	sum = edgesInArea(ag, em, v2, i);
	if(sum != 1)
	    return FALSE;
	}
    }
sum = edgesInArea(ag, em, v1, v2);
return sum == 1;
}

boolean isAlt3Prime(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		    int *altBpStart, int *altBpEnd, int *firstVertex, int *lastVertex)
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
		*lastVertex = i;
		*firstVertex = findClosestUpstreamVertex(ag, em, vs);
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
		    bedDown.chromEnd = vPos[i]+flankingSize;
		    bedUp.chromStart = vPos[ve1]-flankingSize;
		    bedUp.chromEnd = vPos[ve1];
		    }
		else 
		    {
		    bedUp.chromStart = vPos[i];
		    bedUp.chromEnd = vPos[i]+flankingSize;
		    bedDown.chromStart = vPos[ve1]-flankingSize;
		    bedDown.chromEnd = vPos[ve1];
		    }
		if(altRegion)
		    {
		    bedOutputN(&bedConst, 6, constRegion, '\t','\n');
		    bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
		    bedOutputN(&bedDown, 6, downStream100, '\t', '\n');
		    bedOutputN(&bedUp, 6, upStream100, '\t', '\n');
		    }		
		*altBpStart = ve1;
		*altBpEnd = ve2;
		return TRUE;
		}
	    }
	}
    }
return FALSE;
}

boolean isAlt5Prime(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		    int *altBpStart, int *altBpEnd, int *termExonStart, int *termExonEnd)
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
		
		/* Report the first and last vertexes. */
		*termExonStart = i;
		*termExonEnd = findClosestDownstreamVertex(ag, em, i);
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
		    bedDown.chromEnd = vPos[ve2]+flankingSize;
		    bedUp.chromStart = vPos[vs]-flankingSize;
		    bedUp.chromEnd = vPos[vs];
		    }
		else 
		    {
		    bedUp.chromStart = vPos[ve2];
		    bedUp.chromEnd = vPos[ve2]+flankingSize;
		    bedDown.chromStart = vPos[vs]-flankingSize;
		    bedDown.chromEnd = vPos[vs];
		    }
		if(altRegion)
		    {
		    bedOutputN(&bedConst, 6, constRegion, '\t','\n');
		    bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
		    bedOutputN(&bedDown, 6, downStream100, '\t', '\n');
		    bedOutputN(&bedUp, 6, upStream100, '\t', '\n');
		    }		
		*altBpStart = ve1;
		*altBpEnd = ve2;
		return TRUE;
		}
	    }
	}
    }
return FALSE;
}

boolean isCassette(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		    int *altBpStartV, int *altBpEndV, int *startV, int *endV)
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
		*startV = findClosestUpstreamVertex(ag, em, vs);
		*endV = findClosestDownstreamVertex(ag, em, ve2);
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
		    bedUp.chromStart = vPos[ve1] - flankingSize;
		    bedUp.chromEnd = vPos[ve1];
		    bedDown.chromStart = vPos[i];
		    bedDown.chromEnd = vPos[i] + flankingSize;
		    }
		else 
		    {
		    bedDown.chromStart = vPos[ve1] - flankingSize;
		    bedDown.chromEnd = vPos[ve1];
		    bedUp.chromStart = vPos[i];
		    bedUp.chromEnd = vPos[i] + flankingSize;
		    }
		if(altRegion != NULL)
		    {
		    bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
		    bedOutputN(&bedUp, 6, upStream100, '\t', '\n');
		    bedOutputN(&bedDown, 6, downStream100, '\t', '\n');
		    }
		*altBpStartV = ve1;
		*altBpEndV = i;
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
    /* Try to find a hard start that connects ve1 and ve2. */
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
	    if(optionExists("printRetIntAccs"))
		{
		int incEdge = altGraphXGetEdgeNum(ag, vs, ve2);
		int exEdge = altGraphXGetEdgeNum(ag, ve1, i);
		struct evidence *ev= slElementFromIx(ag->evidence, incEdge);
		int i;
		fprintf(stdout, "RETINT\t%s\t%d\t", ag->name,ev->evCount);
		for(i = 0; i < ev->evCount; i++)
		    {
		    fprintf(stdout, "%s,", ag->mrnaRefs[ev->mrnaIds[i]]);
		    }
		fprintf(stdout, "\t");
		ev= slElementFromIx(ag->evidence, exEdge);
		fprintf(stdout, "%d\t", ev->evCount);
		for(i = 0; i < ev->evCount; i++)
		    {
		    fprintf(stdout, "%s,", ag->mrnaRefs[ev->mrnaIds[i]]);
		    }
		fprintf(stdout, "\n");
		}
	    /* Upstream/down stream */
	    if(altRegion != NULL)
		{
		bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
		}
	    *altBpStart = ve1;
	    *altBpEnd = i;
	    return TRUE;
	    }
	}
    }
return FALSE;
}	    

struct evidence *evidenceForEdge(struct altGraphX *ag, int v1, int v2)
/* Return the evidence associated with this edge. */
{
int edgeNum = altGraphXGetEdgeNum(ag, v1, v2);
struct evidence *ev = slElementFromIx(ag->evidence, edgeNum);
return ev;
}


void reportAlt5Prime(struct altGraphX *ag, bool **em, int vs, int ve1, int ve2, 
		    int altBpStart, int altBpEnd, int termExonStart, int termExonEnd, FILE *out)
/* Write out an altGraphX record for an alt5Prime splicing
event. Variable names are consistent with the rest of the program, but
can be misleading. Specifically vs = start of alt splicing, ve1 =
first end of alt splicing, etc. even though "vs" is really the end of
an exon. Note that we are cutting out the alt-spliced part of the exon as a separate exon.
For an alt5Prime splice the edges are:

 Name       Vertexes         Class
 ------     ----------       -----
exon1:      vs->ve1       constituative (0)
exon2:      ve1->ve2        alternative (1)
junction1:  ve1->termExonStart          alternative (2)
junction2:  ve2->termExonStart          alternative (1)
exon3:      termExonStart->termExonEnd        constituative(0)
*/
{
struct altGraphX *agLoc = NULL;  /* Local altGraphX. */
struct evidence *ev = NULL, *evLoc = NULL;
int *vPos = ag->vPositions;
unsigned char *vT = ag->vTypes;
int *vPosLoc = NULL;    /* Vertex Positions. */
int *eStartsLoc = NULL; /* Edge Starts. */
int *eEndsLoc = NULL;   /* Edge ends. */
unsigned char *vTLoc = NULL;      /* Vertex Types. */
int *eTLoc = NULL;      /* Edge Types. */
int vCLoc = 0;
int eCLoc = 0;
int edgeIx = 0, vertexIx = 0;
int i =0;
struct dyString *dy = NULL;

if(out == NULL)
    return;
AllocVar(agLoc);
agLoc->tName = cloneString(ag->tName);
agLoc->name = cloneString(ag->name);
agLoc->tStart = vPos[vs];
agLoc->tEnd = vPos[termExonEnd];
agLoc->strand[0] = ag->strand[0];
agLoc->vertexCount = vCLoc = 6;
agLoc->edgeCount = eCLoc = 5;
agLoc->id = alt5Prime;
/* Allocate some arrays. */
AllocArray(vPosLoc, vCLoc);
AllocArray(eStartsLoc, eCLoc);
AllocArray(eEndsLoc, eCLoc);
AllocArray(vTLoc, vCLoc);
AllocArray(eTLoc, eCLoc);

/* Fill in the vertex positions. */
vertexIx = 0;
vPosLoc[vertexIx++] = vPos[vs];            /* 0 */
vPosLoc[vertexIx++] = vPos[ve1];           /* 1 */
vPosLoc[vertexIx++] = vPos[ve1];           /* 2 */
vPosLoc[vertexIx++] = vPos[ve2];           /* 3 */
vPosLoc[vertexIx++] = vPos[termExonStart]; /* 4 */
vPosLoc[vertexIx++] = vPos[termExonEnd];   /* 5 */

/* Fill in the vertex types. */
vertexIx = 0;
vTLoc[vertexIx++] = vT[vs];
vTLoc[vertexIx++] = vT[ve1];
vTLoc[vertexIx++] = vT[vs];
vTLoc[vertexIx++] = vT[ve2];
vTLoc[vertexIx++] = vT[termExonStart];
vTLoc[vertexIx++] = vT[termExonEnd];

edgeIx = 0;
/* Constitutive portion of first exon. */
eStartsLoc[edgeIx] = 0;
eEndsLoc[edgeIx] = 1;
eTLoc[edgeIx] = 0;
ev = evidenceForEdge(ag, vs, ve1);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Alternative portion of first exon. */
eStartsLoc[edgeIx] = 2;
eEndsLoc[edgeIx] = 3;
eTLoc[edgeIx] = 1;
ev = evidenceForEdge(ag, vs, ve2);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Alt2 junction (longer) */
eStartsLoc[edgeIx] = 1;
eEndsLoc[edgeIx] = 4;
eTLoc[edgeIx] = 2;
ev = evidenceForEdge(ag, ve1, termExonStart);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Alt1 junction (shorter) */
eStartsLoc[edgeIx] = 3;
eEndsLoc[edgeIx] = 4;
eTLoc[edgeIx] = 1;
ev = evidenceForEdge(ag, ve2, termExonStart);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Exon 3 constitutive (longer exon) */
eStartsLoc[edgeIx] = 4;
eEndsLoc[edgeIx] = 5;
eTLoc[edgeIx] = 0;
ev = evidenceForEdge(ag, termExonStart, termExonEnd);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Package up the evidence, tissues, etc. */
slReverse(&agLoc->evidence);
dy = newDyString(ag->mrnaRefCount*36);
agLoc->mrnaRefCount = ag->mrnaRefCount;
for(i=0; i<ag->mrnaRefCount; i++)
    dyStringPrintf(dy, "%s,", ag->mrnaRefs[i]);
sqlStringDynamicArray(dy->string, &agLoc->mrnaRefs, &i);
dyStringFree(&dy);
agLoc->mrnaTissues = CloneArray(ag->mrnaTissues, ag->mrnaRefCount);
agLoc->mrnaLibs = CloneArray(ag->mrnaLibs, ag->mrnaRefCount);
agLoc->vPositions = vPosLoc;
agLoc->edgeStarts = eStartsLoc;
agLoc->edgeEnds = eEndsLoc;
agLoc->vTypes = vTLoc;
agLoc->edgeTypes = eTLoc;
altGraphXTabOut(agLoc, out);
altGraphXFree(&agLoc);
}

void reportAlt3Prime(struct altGraphX *ag, bool **em, int vs, int ve1, int ve2, 
		    int altBpStart, int altBpEnd, int startV, int endV, FILE *out)
/* Write out an altGraphX record for an alt3Prime splicing
event. Variable names are consistent with the rest of the program, but
can be misleading. Specifically vs = start of alt splicing, ve1 =
first end of alt splicing, etc. even though "vs" is really the end of
an exon. For an alt5Prime splice the edges are:

 Name       Vertexes         Class
 ------     ----------       -----
exon1:      startV->vs       constituative (0)
junction1:  vs->ve1          alternative (1)
junction2:  vs->ve2          alternative (2)
exon2:      ve1->e2        alternative (1)
exon3:      ve2->endV        constituative (0)
*/
{
struct altGraphX *agLoc = NULL;  /* Local altGraphX. */
struct evidence *ev = NULL, *evLoc = NULL;
int *vPos = ag->vPositions;
unsigned char *vT = ag->vTypes;
int *vPosLoc = NULL;    /* Vertex Positions. */
int *eStartsLoc = NULL; /* Edge Starts. */
int *eEndsLoc = NULL;   /* Edge ends. */
unsigned char *vTLoc = NULL;      /* Vertex Types. */
int *eTLoc = NULL;      /* Edge Types. */
int vCLoc = 0;
int eCLoc = 0;
int edgeIx = 0, vertexIx = 0;
int i =0;
struct dyString *dy = NULL;

if(out == NULL)
    return;
AllocVar(agLoc);
agLoc->tName = cloneString(ag->tName);
agLoc->name = cloneString(ag->name);
agLoc->tStart = vPos[startV];
agLoc->tEnd = vPos[endV];
agLoc->strand[0] = ag->strand[0];
agLoc->vertexCount = vCLoc = 6;
agLoc->edgeCount = eCLoc = 5;
agLoc->id = alt3Prime;
/* Allocate some arrays. */
AllocArray(vPosLoc, vCLoc);
AllocArray(eStartsLoc, eCLoc);
AllocArray(eEndsLoc, eCLoc);
AllocArray(vTLoc, vCLoc);
AllocArray(eTLoc, eCLoc);

/* Fill in the vertex positions. */
vertexIx = 0;
vPosLoc[vertexIx++] = vPos[startV]; /* 0 */
vPosLoc[vertexIx++] = vPos[vs];     /* 1 */
vPosLoc[vertexIx++] = vPos[ve1];    /* 2 */
vPosLoc[vertexIx++] = vPos[ve2];    /* 3 */
vPosLoc[vertexIx++] = vPos[ve2];    /* 4 */
vPosLoc[vertexIx++] = vPos[endV];   /* 5 */

/* Fill in the vertex types. */
vertexIx = 0;
vTLoc[vertexIx++] = vT[startV];
vTLoc[vertexIx++] = vT[vs];
vTLoc[vertexIx++] = vT[ve1];
vTLoc[vertexIx++] = vT[vs]; /* Faking a separate exon for the alt spliced portion. */
vTLoc[vertexIx++] = vT[ve2];
vTLoc[vertexIx++] = vT[endV];

edgeIx = 0;

/* Constitutive first exon. */
eStartsLoc[edgeIx] = 0;
eEndsLoc[edgeIx] = 1;
eTLoc[edgeIx] = 0;
ev = evidenceForEdge(ag, startV, vs);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Alternative1 junction (shorter). */
eStartsLoc[edgeIx] = 1;
eEndsLoc[edgeIx] = 2;
eTLoc[edgeIx] = 1;
ev = evidenceForEdge(ag, vs, ve1);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Alt2 junction (longer). */
eStartsLoc[edgeIx] = 1;
eEndsLoc[edgeIx] = 4;
eTLoc[edgeIx] = 2;
ev = evidenceForEdge(ag, vs, ve2);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Alt1 portion of second exon. */
eStartsLoc[edgeIx] = 2;
eEndsLoc[edgeIx] = 3;
eTLoc[edgeIx] = 1;
ev = evidenceForEdge(ag, ve1, endV);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Exon 2 constitutive (shorter exon) */
eStartsLoc[edgeIx] = 4;
eEndsLoc[edgeIx] = 5;
eTLoc[edgeIx] = 0;
ev = evidenceForEdge(ag, ve2, endV);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
edgeIx++;

/* Package up the evidence, tissues, etc. */
slReverse(&agLoc->evidence);
dy = newDyString(ag->mrnaRefCount*36);
agLoc->mrnaRefCount = ag->mrnaRefCount;
for(i=0; i<ag->mrnaRefCount; i++)
    dyStringPrintf(dy, "%s,", ag->mrnaRefs[i]);
sqlStringDynamicArray(dy->string, &agLoc->mrnaRefs, &i);
dyStringFree(&dy);
agLoc->mrnaTissues = CloneArray(ag->mrnaTissues, ag->mrnaRefCount);
agLoc->mrnaLibs = CloneArray(ag->mrnaLibs, ag->mrnaRefCount);
agLoc->vPositions = vPosLoc;
agLoc->edgeStarts = eStartsLoc;
agLoc->edgeEnds = eEndsLoc;
agLoc->vTypes = vTLoc;
agLoc->edgeTypes = eTLoc;
altGraphXTabOut(agLoc, out);
altGraphXFree(&agLoc);
}

void reportCassette(struct altGraphX *ag, bool **em, int vs, int ve1, int ve2, 
		    int altBpStart, int altBpEnd, int startV, int endV, FILE *out)
/* Write out both an altGraphX and two bed files. For a cassette exon the
 edges are - 
 Name       Vertexes         Class
 ------     ----------       -----
 exon1:     startV->vs       constitutive (cons 0)
 junction1: vs->ve1          alternative1 (alt1 1)
 exon2:     ve1->altBpEnd    alternative1 (alt1 1)
 junction2: altBpEnd->ve2    alternative1 (alt1 1)
 exon3:     ve2->endV        constitutive (cons 0)
 junction3: vs->ve2          alternative2 (alt2 2)
*/
{
struct altGraphX *agLoc = NULL;  /* Local altGraphX. */
struct evidence *ev = NULL, *evLoc = NULL;
int *vPos = ag->vPositions;
unsigned char *vT = ag->vTypes;
int *vPosLoc = NULL;    /* Vertex Positions. */
int *eStartsLoc = NULL; /* Edge Starts. */
int *eEndsLoc = NULL;   /* Edge ends. */
unsigned char *vTLoc = NULL;      /* Vertex Types. */
int *eTLoc = NULL;      /* Edge Types. */
int vCLoc = 0;
int eCLoc = 0;
int i =0;
struct dyString *dy = NULL;
if(out == NULL)
    return;
AllocVar(agLoc);
agLoc->tName = cloneString(ag->tName);
agLoc->name = cloneString(ag->name);
agLoc->tStart = vPos[startV];
agLoc->tEnd = vPos[endV];
agLoc->strand[0] = ag->strand[0];
agLoc->vertexCount = vCLoc = 6;
agLoc->edgeCount = eCLoc = 6;
agLoc->id = altCassette;
/* Allocate some arrays. */
AllocArray(vPosLoc, vCLoc);
AllocArray(eStartsLoc, vCLoc);
AllocArray(eEndsLoc, vCLoc);
AllocArray(vTLoc, vCLoc);
AllocArray(eTLoc, vCLoc);

/* Fill in the vertex positions. */
vPosLoc[0] = vPos[startV];
vPosLoc[1] = vPos[vs];
vPosLoc[2] = vPos[ve1];
vPosLoc[3] = vPos[altBpEnd];
vPosLoc[4] = vPos[ve2];
vPosLoc[5] = vPos[endV];

/* Fill in the vertex types. */
vTLoc[0] = vT[startV];
vTLoc[1] = vT[vs];
vTLoc[2] = vT[ve1];
vTLoc[3] = vT[altBpEnd];
vTLoc[4] = vT[ve2];
vTLoc[5] = vT[endV];

/* Fill in the edges. */
/* Constitutive first exon. */
eStartsLoc[0] = 0;
eEndsLoc[0] = 1;
eTLoc[0] = 0;
ev = evidenceForEdge(ag, startV, vs);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);
/* Exon inclusion junction. */
eStartsLoc[1] = 1;
eEndsLoc[1] = 2;
eTLoc[1] = 1;
ev = evidenceForEdge(ag, vs, ve1);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);

/* Exon exclusion junction. */
eStartsLoc[2] = 1;
eEndsLoc[2] = 4;
eTLoc[2] = 2;
ev = evidenceForEdge(ag, vs, ve2);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);

/* Cassette exon. */
eStartsLoc[3] = 2;
eEndsLoc[3] = 3;
eTLoc[3] = 1;
ev = evidenceForEdge(ag, ve1, altBpEnd);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);

/* Exon inclusion junction. */
eStartsLoc[4] = 3;
eEndsLoc[4] = 4;
eTLoc[4] = 1;
ev = evidenceForEdge(ag, altBpEnd, ve2);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);

/* Constitutive second exon. */
eStartsLoc[5] = 4;
eEndsLoc[5] = 5;
eTLoc[5] = 0;
ev = evidenceForEdge(ag, ve2, endV);
evLoc = CloneVar(ev);
evLoc->mrnaIds = CloneArray(ev->mrnaIds, ev->evCount);
slAddHead(&agLoc->evidence, evLoc);

slReverse(&agLoc->evidence);

dy = newDyString(ag->mrnaRefCount*36);
agLoc->mrnaRefCount = ag->mrnaRefCount;
for(i=0; i<ag->mrnaRefCount; i++)
    dyStringPrintf(dy, "%s,", ag->mrnaRefs[i]);
sqlStringDynamicArray(dy->string, &agLoc->mrnaRefs, &i);
dyStringFree(&dy);
agLoc->mrnaTissues = CloneArray(ag->mrnaTissues, ag->mrnaRefCount);
agLoc->mrnaLibs = CloneArray(ag->mrnaLibs, ag->mrnaRefCount);
agLoc->vPositions = vPosLoc;
agLoc->edgeStarts = eStartsLoc;
agLoc->edgeEnds = eEndsLoc;
agLoc->vTypes = vTLoc;
agLoc->edgeTypes = eTLoc;
altGraphXTabOut(agLoc, out);
altGraphXFree(&agLoc);
}

int altSpliceType(struct altGraphX *ag, bool **em,  int vs, int ve1, int ve2,
		  int *altBpStart, int *altBpEnd)
/* Return type of splicing. Depends on direction of transcription. 
        hs     he          hs      he  
IIIIIIIIEEEEEEEEEIIIIIIIIIIEEEEEEEEEEIIIIIIIIII
*/
{
enum altSpliceType ast = altOther;
int startVertex=-1, endVertex = -1;
int termExonStart = -1, termExonEnd = -1;
if(ve1 == ve2)
    {
    ast = altIdentity;
    *altBpStart = vs;
    *altBpEnd = ve1;
    }
else if(isAlt3Prime(ag, em, vs, ve1, ve2, altBpStart, altBpEnd, &startVertex, &endVertex))
    {
    ast = alt3Prime;
    reportAlt3Prime(ag, em, vs, ve1, ve2, *altBpStart, *altBpEnd, startVertex, endVertex, altGraphXOut);
    }
else if(isAlt5Prime(ag, em, vs, ve1, ve2, altBpStart, altBpEnd, &termExonStart, &termExonEnd))
    {
    ast = alt5Prime;
    reportAlt5Prime(ag, em, vs, ve1, ve2, *altBpStart, *altBpEnd, termExonStart, termExonEnd, altGraphXOut);
    }
else if(isCassette(ag, em, vs, ve1, ve2, altBpStart, altBpEnd, &startVertex, &endVertex))
    {
    ast = altCassette;
    reportCassette(ag, em, vs, ve1, ve2, *altBpStart, *altBpEnd, startVertex, endVertex, altGraphXOut);
    }
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
int vMax = 2*ag->vertexCount+ag->edgeCount;
AllocVar(as);
as->chrom = cloneString(ag->tName);
as->chromStart = ag->vPositions[vs];
as->chromEnd = ag->vPositions[vs];
as->agName = cloneString(ag->name);
safef(as->strand, sizeof(as->strand), "%s", ag->strand);
as->index = vs;
as->type = ag->vTypes[vs];
as->altCount+=2;
as->altMax = vMax;
/* Return starts of vertices. */
AllocArray(as->altStarts, vMax);
as->altStarts[0] = ag->vPositions[ve1];
as->altStarts[1] = ag->vPositions[ve2];

/* Record indices of vertices. */
AllocArray(as->vIndexes, vMax);
as->vIndexes[0] = ve1;
as->vIndexes[1] = ve2;

/* Record type of vertices. */
AllocArray(as->altTypes, vMax);
as->altTypes[0] = ag->vTypes[ve1];
as->altTypes[1] = ag->vTypes[ve2];

/* Record splice types and bases alt spliced. */
AllocArray(as->spliceTypes, vMax);
AllocArray(as->altBpEnds, vMax);
AllocArray(as->altBpStarts, vMax);
as->spliceTypes[0] = altSpliceType(ag, agEm, vs, ve1, ve1, &altBpStart, &altBpEnd);
as->altBpStarts[0] = ag->vPositions[altBpStart];
as->altBpEnds[0] = ag->vPositions[altBpEnd];

as->spliceTypes[1] = altSpliceType(ag, agEm,vs, ve1, ve2, &altBpStart, &altBpEnd);
as->altBpStarts[1] = ag->vPositions[altBpStart];
as->altBpEnds[1] = ag->vPositions[altBpEnd];

/* Look up the evidence. */
AllocArray(as->support, vMax);

edgeNum = getEdgeNum(ag, vs, ve1);
ev = slElementFromIx(ag->evidence, edgeNum);
as->support[0] = ev->evCount;

edgeNum = getEdgeNum(ag, vs, ve2);
ev = slElementFromIx(ag->evidence, edgeNum);
as->support[1] = ev->evCount;

AllocArray(as->altCons, vMax);
AllocArray(as->upStreamCons, vMax);
AllocArray(as->downStreamCons, vMax);
for(i=0; i<vMax; i++)
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
if(aSplice->altCount >= aSplice->altMax) 
    {
    warn("Why the hell?");
    }
aSplice->altStarts[aSplice->altCount] = ag->vPositions[ve];
aSplice->altTypes[aSplice->altCount] = ag->vTypes[ve];
aSplice->vIndexes[aSplice->altCount] = ve;
aSplice->spliceTypes[aSplice->altCount] = altSpliceType(ag, agEm, aSplice->index, 
							aSplice->vIndexes[aSplice->altCount-1], ve,
							&altBpStart, &altBpEnd);
aSplice->altBpStarts[aSplice->altCount] = ag->vPositions[altBpStart];
aSplice->altBpEnds[aSplice->altCount] = ag->vPositions[altBpEnd];
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

float aveSScoreForRegion(char *db, char *chrom, int chromStart, int chromEnd)
{
float currentSum = 0;
int count = 0;
float ave = 0;
int sPos = 0;
struct sample *sList=NULL, *s = NULL;
char *tableName = optionVal("sampleTable", "hg15Mm3L");
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr=NULL;
char **row;
char buff[256];
int rowOffset=0;
safef(buff,sizeof(buff),"%s_%s",chrom, tableName);
if(hTableExists(db, buff))
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

void fillInSscores(char *db, struct altSpliceSite *as, int edge)
{
int offSet = flankingSize;
as->altCons[edge] = aveSScoreForRegion(db, as->chrom, as->altBpStarts[edge], as->altBpEnds[edge]);
if(as->spliceTypes[edge] == alt3Prime)
    {
    as->upStreamCons[edge] = aveSScoreForRegion(db, as->chrom, as->altBpStarts[edge] - offSet, as->altBpStarts[edge]);
    /* We have a problem here which is that we don't know the downstream end. */
    as->downStreamCons[edge] = -1;
    }
else if(as->spliceTypes[edge] == alt5Prime)
    {
    as->upStreamCons[edge] = aveSScoreForRegion(db, as->chrom, as->altStarts[edge-1] - offSet, as->altStarts[edge-1]);
    as->downStreamCons[edge] = aveSScoreForRegion(db, as->chrom, as->altBpEnds[edge], as->altBpEnds[edge] + 100);
    }
else if(as->spliceTypes[edge] == altCassette || as->spliceTypes[edge] == altIdentity)
    {
    /* This works for cassette exons, and all exons in general. */
    as->upStreamCons[edge] = aveSScoreForRegion(db, as->chrom, as->altBpStarts[edge] - offSet, as->altBpStarts[edge]);
    as->downStreamCons[edge] = aveSScoreForRegion(db, as->chrom, as->altBpEnds[edge], as->altBpEnds[edge] +offSet);
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
static int dotMod = 500;
static int dot = 500;
if (--dot <= 0)
    {
    putc('.', stderr);
    fflush(stderr);
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
	{
	as->spliceTypes[i] = alt3Prime;
	alt5Flipped++;
	}
    else if(as->spliceTypes[i] == alt3Prime)
	{
	as->spliceTypes[i] = alt5Prime;
	alt3Flipped++;
	}
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
struct evidence *ev = slElementFromIx(ag->evidence, altGraphXGetEdgeNum(ag, v1, v2));

/* Make sure that this edge has enough support 
   that we believe it. */
if(ev->evCount >= minControlConf)
    {
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
	bedUp.chromStart = vPos[v1] - flankingSize;
	bedUp.chromEnd = vPos[v1];
	bedDown.chromStart = vPos[v2];
	bedDown.chromEnd = vPos[v2] + flankingSize;
	}
    else 
	{
	bedDown.chromStart = vPos[v1] - flankingSize;
	bedDown.chromEnd = vPos[v1];
	bedUp.chromStart = vPos[v2];
	bedUp.chromEnd = vPos[v2] + flankingSize;
	}
    bedOutputN(&bedAlt, 6, altRegion, '\t','\n');
    bedOutputN(&bedUp, 6, upStream100, '\t', '\n');
    bedOutputN(&bedDown, 6, downStream100, '\t', '\n');
    }
}



int agxEvCount(struct altGraphX *ag, int v1, int v2)
/* Return the number of ESTs supporting this edge. */
{
int edgeNum = altGraphXGetEdgeNum(ag, v1, v2);
struct evidence *ev = slElementFromIx(ag->evidence, edgeNum);
return ev->evCount;
}

void reportThreeBpBed(struct altSpliceSite *as)
/** Print out a bed for the wierd alt3 3bp sites. */
{
if(alt3bpRegion)
    fprintf(alt3bpRegion, "%s\t%d\t%d\t%s\t%d\t%s\n", as->chrom, as->altBpStarts[1] - 10, as->altBpEnds[1] + 10,
	    as->agName, as->altTypes[1], as->strand);
}

void lookForAltSplicing(char *db, struct altGraphX *ag, struct altSpliceSite **aSpliceList, 
			int *altSpliceSites, int *altSpliceLoci, int *totalSpliceSites)
/* Walk throught the altGraphX graph and look for evidence of altSplicing. */
{
struct altSpliceSite *notAlt = NULL, *notAltList = NULL;
bool **em = altGraphXCreateEdgeMatrix(ag);
int vCount = ag->vertexCount;
unsigned char *vTypes = ag->vTypes;
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
	if(em[i][j] && areConsSplice(em, vCount, vTypes,i,j) && 
	   (agxEvCount(ag, i,j) >= minConfidence)) 
	    {
	    for(k=j+1; k<vCount; k++)
		{
		if(em[i][k] && areConsSplice(em, vCount, vTypes, i, k) &&
		   (agxEvCount(ag, i,k) >= minConfidence)) 
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
	/* Only want non alt-spliced exons for our controls.  Some of
	 these checks are historical and probably redundant....*/
	if(em[i][j] && 
	   rowSum(em[i], ag->vTypes, ag->vertexCount) == 1 &&
	   rowSum(em[j], ag->vTypes, ag->vertexCount) == 1 &&
	   colSum(em, ag->vTypes, ag->vertexCount, j) == 1 &&
	   colSum(em, ag->vTypes, ag->vertexCount, j) == 1 &&
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
    if(aSplice != NULL) 
	{
	if(altLogFile)
	    fprintf(altLogFile, "%s\n", ag->name);
	slAddHead(aSpliceList, aSplice);
	}
    /* If we have a simple splice classfy it and log it. */
    if(aSplice != NULL && aSplice->altCount == 2)
	{
	altTotalCount++;	
	fixOtherStrand(aSplice);
	logSpliceType(aSplice->spliceTypes[1], abs(aSplice->altBpEnds[1] - aSplice->altBpStarts[1]));
	if(aSplice->spliceTypes[1] == alt3Prime && (aSplice->altBpEnds[1] - aSplice->altBpStarts[1] == 3))
	    reportThreeBpBed(aSplice);
	if(doSScores)
	    fillInSscores(db, aSplice, 1);
	if(RData != NULL)
	    {
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
	fillInSscores(db, notAlt, 1);
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

void bedViewOut(struct altSpliceSite *as, FILE *out)
{
struct bed *bed = NULL;
AllocVar(bed);
bed->chrom = cloneString(as->chrom);
bed->chromStart  = as->chromStart;
bed->chromEnd  = maxInArray(as->altStarts, as->altCount);
AllocArray(bed->chromStarts, 2);
AllocArray(bed->blockSizes, 2);
bed->thickStart = as->altBpStarts[1];
bed->thickEnd = as->altBpEnds[1];
bed->blockCount = 2;
bed->chromStarts[0] = 0;
bed->chromStarts[1] = bed->chromEnd - bed->chromStart -1; 
bed->blockSizes[0] = bed->blockSizes[1] = 1;
bed->name = cloneString(as->agName);
bed->score = as->spliceTypes[1];
safef(bed->strand, sizeof(bed->strand), "%s", as->strand);
bedTabOutN(bed, 12, out);
bedFree(&bed);
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
     "<frameset cols=\"18%%,82%%\">\n"
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

dyStringClear(name);
dyStringPrintf(name, "%s.alt3.3bp.bed", prefix);
alt3bpRegion = mustOpen(name->string, "w");
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
    lookForAltSplicing(db, ag, &aSpliceList, &altSpliceSites, &altSpliceLoci, &totalSpliceSites);
    for(aSplice=aSpliceList; aSplice != NULL; aSplice= aSplice->next)
	{
 	altSpliceSiteOutput(aSplice, summaryOut, '\t', '\n');
	htmlLinkOut(db, aSplice, htmlOut);
	if(bedViewOutFile != NULL)
	    bedViewOut(aSplice, bedViewOutFile);
	}
    altSpliceSiteFreeList(&aSpliceList);
    }
warn("\nDone.");
fprintf(htmlOut,"</body></html>\n");
warn("%d altSpliced sites in %d alt-spliced loci out of %d total loci.",
     altSpliceSites, altSpliceLoci, slCount(agList));
printSpliceTypeInfo(altSpliceLoci);
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
char *altGraphXOutName = NULL;
char *altLogFileName = NULL;
char *bedViewOutFileName =NULL;
if(argc < 6)
    usage();
optionHash(&argc, argv);
doSScores = optionExists("doSScores");
altGraphXOutName = optionVal("altGraphXOut", NULL);
minConfidence = optionInt("minConf", 0);
altLogFileName = optionVal("altLogFile", NULL);
flankingSize = optionInt("flankingSize", 100);
bedViewOutFileName = optionVal("bedViewFile", NULL);
minControlConf = optionInt("minControlConf", 0);
warn("Flanking size is: %d", flankingSize);
if(altLogFileName != NULL)
    altLogFile = mustOpen(altLogFileName, "w");
if(bedViewOutFileName != NULL)
    bedViewOutFile = mustOpen(bedViewOutFileName, "w");
if(altGraphXOutName)
    {
    warn("Writing alts to %s", altGraphXOutName);
    altGraphXOut = mustOpen(altGraphXOutName, "w");
    }
altSummary(argv[1], argv[2], argv[3], argv[4], argv[5]);
if(altGraphXOut != NULL)
    carefulClose(&altGraphXOut);
if(altLogFile != NULL)
    carefulClose(&altLogFile);
if(bedViewOutFile != NULL)
    carefulClose(&bedViewOutFile);
return 0;
}
