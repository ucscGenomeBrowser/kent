/** orthoSplice.c - Program to examine orthologous altGraphX structures. */
#include "common.h"
#include "hdb.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"
#include "axt.h"
#include "axtInfo.h"
#include "altGraphX.h"
#include "chainDb.h"
#include "chainNetDbLoad.h"
#include "geneGraph.h"

static char const rcsid[] = "$Id: orthoSplice.c,v 1.6 2003/05/26 21:32:48 sugnet Exp $";

struct spliceEdge 
/* Structure to hold information about one edge in 
   a splicing graph. */
{
    struct spliceEdge *next;    /* Next in list. */
    char *chrom;                /* Chromsome. Memory not owned here.*/
    int chromStart;             /* Chrom start. */
    int chromEnd;               /* End. */
    char *agName;               /* Name of altGraphX that edge is from. Memory not owned here. */
    int conf;                   /* Confidence. Generally number of mRNA's supporting edge. */
    char strand[2];             /* Strand. */
    enum ggEdgeType type;       /* Type of edge: ggExon, ggIntron, ggSJ, ggCassette. */
    int v1;                     /* Vertex 1 in graph. */
    int v2;                     /* Vertex 2 in graph. */
    int alt;                    /* Possible alt splice? 1=TRUE 0=FALSE */
    int conserved;              /* Conserved in ortholgous genome? 1=TRUE 0=FALSE */    
    int soft;                   /* TRUE (1) if a vertex is soft, FALSE otherwise. */
};

struct orthoAgReport
/* Some summary information on comparison of splice graphs. */
{
    struct orthoAgReport *next;  /**< Next in list. */
    struct spliceEdge *seList;   /**< List of edges and their attributes. */
    char *agName;             /**< AltGraphX from first genome name. */
    char *orthoAgName;        /**< Orthologous altGraphX name. */
    char *chrom;                 /**< Chrom name. */
    int ssSoftFound;                 /**< Number of splice sites in ag that are splice sites in orhtoAg. */
    int ssSoftMissing;               /**< Number of splice sites in ag that are not splice sites in orhtoAg. */
    int ssHardFound;                 /**< Number of splice sites in ag that are splice sites in orhtoAg. */
    int ssHardMissing;               /**< Number of splice sites in ag that are not splice sites in orhtoAg. */
    int ssDoubles;               /**< Number of splice sites in ag that map to two places in orhtoAg (hopefully rare). */
    int ssVeryClose;             /**< How many times were we off by one. */
    int alt;                     /**< Number of alternative splices in graph. */
    int altFound;                /**< Number of alternative splices that are conserved. */

};

static boolean doHappyDots;   /* output activity dots? */
static char *altTable = "altGraphX";  /* Name of table to load splice graphs from. */
static struct orthoAgReport *agReportList = NULL; /* List of reports. */
FILE *rFile = NULL; /* report file. */
FILE *edgeFile = NULL; /* edge report file. */

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"orthoDb", OPTION_STRING},
    {"altInFile", OPTION_STRING},
    {"netTable", OPTION_STRING},
    {"chainTable", OPTION_STRING},
    {"reportFile", OPTION_STRING},
    {"commonFile", OPTION_STRING},
    {"edgeFile", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Database (i.e. hg15) that altInFile comes from.",
    "Database (i.e. mm3) to look for orthologous splicing in.",
    "File containing altGraphX records to look for.",
    "Datbase table containing net records, i.e. mouseNet.",
    "Prefix of datbase table containing chain records, i.e. mouseChain.",
    "File name to print individual reports to.",
    "File name to print common altGraphX records to.",
    "File name to print individual edge reports to."
};

void usage()
{
int i=0;
warn("orthoSplice - program to compare splicing in different organisms\n"
     "initially human and mouse as they both have nice EST and cDNA data\n"
     "still working out algorithm but options are:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   "
	 "orthoSplice -altInFile=ranbp1.tab -db=hg15 -orthoDb=mm3 -netTable=mouseNet \\\n"
	 "\t   -chainTable=mouseChain -commonFile=out.agx -reportFile=out.report -edgeFile=out.edge.report\n");
}

boolean isUnique(struct altGraphX *ag)
/* return true if we have seen and altGraphX with same chrom, chromStart, chromEnd, strand before. */
{
static struct hash *agHash = NULL;
char* something = "s";
char key[256];
if(agHash == NULL)
    agHash = newHash(8);
safef(key, sizeof(key), "%s:%d-%d_%s", ag->tName, ag->tStart, ag->tEnd, ag->strand);
if(hashFindVal(agHash, key) == NULL)
    {
    hashAdd(agHash, key, something);
    return TRUE;
    }
return FALSE;
}

void orthoAgReportFree(struct orthoAgReport **pEl)
/* Free a single dynamically allocated orthoAgReport such as created
 * with orthoAgReportLoad(). */
{
struct orthoAgReport *el;

if ((el = *pEl) == NULL) return;
slFreeList(&el->seList);
freeMem(el->chrom);
freeMem(el->agName);
freeMem(el->orthoAgName);
freez(pEl);
}

void orthoAgReportFreeList(struct orthoAgReport **pList)
/* Free a list of dynamically allocated orthoAgReport's */
{
struct orthoAgReport *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    orthoAgReportFree(&el);
    }
*pList = NULL;
}

/* struct orthoAgReport *newOrthoAgReport(char *agName, char *orthoAgName) */
/* /\* Allocate an orthoAgReport as one big lump of memory and set the names. *\/ */
/* { */
/* struct orthoAgReport *agRep = NULL; */
/* AllocVar(agRep); */
/* snprintf(agRep->agName, sizeof(agRep->agName), "%s",  agName); */
/* agRep->agName[sizeof(agRep->agName) -1] = '\0'; */
/* snprintf(agRep->orthoAgName, sizeof(agRep->orthoAgName), "%s",  orthoAgName); */
/* agRep->orthoAgName[sizeof(agRep->orthoAgName) -1] = '\0'; */
/* return agRep; */
/* } */

void orthoAgReportHeader(FILE *out)
/** Output a header for the orthoAgReport fields. */
{
fprintf(out, "#%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", "agName", "orhtoAgName", "altEdges", "altEdgesFound", "ssSoftFound", "ssSoftMissing", "ssHardFound", "ssHardMissing", "ssDoubles", "ssVeryClose");
}

void orthoAgReportTabOut(struct orthoAgReport *r, FILE *out)
/** Write out a one line summary of a report. */
{
fprintf(out, "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", r->agName, r->orthoAgName, r->alt, r->altFound, 
	r->ssSoftFound, r->ssSoftMissing, r->ssHardFound, r->ssHardMissing, r->ssDoubles, r->ssVeryClose);
}

void spliceEdgeHeader(FILE *out)
{
fprintf(out, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", "#chrom", "chromStart", "chromEnd", 
	"agName", "conf", "strand",
	"type", "v1", "v2", "alt", "conserved", "soft");
}

void spliceEdgeTabOut(struct spliceEdge *se, FILE *out)
{
fprintf(out, "%s\t%d\t%d\t%s\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n", se->chrom, se->chromStart, se->chromEnd, 
	se->agName, se->conf, se->strand,
	se->type, se->v1, se->v2, se->alt, se->conserved, se->soft);
}

void outputReport(struct orthoAgReport *rep)
{
static boolean repHeader = FALSE;
static boolean edgeHeader = FALSE;
struct spliceEdge *se = NULL;
if(!repHeader)
    {
    orthoAgReportHeader(rFile);
    repHeader = TRUE;
    }
orthoAgReportTabOut(rep, rFile);
if(!edgeHeader)
    {
    spliceEdgeHeader(edgeFile);
    edgeHeader = TRUE;
    }
for(se = rep->seList; se != NULL; se = se->next)
    {
    spliceEdgeTabOut(se, edgeFile);
    }
}

enum ggEdgeType getEdgeType(struct altGraphX *ag, int v1, int v2)
/* Return edge type. */
{
if( (ag->vTypes[v1] == ggHardStart || ag->vTypes[v1] == ggSoftStart)  
    && (ag->vTypes[v2] == ggHardEnd || ag->vTypes[v2] == ggSoftEnd)) 
    return ggExon;
else if( (ag->vTypes[v1] == ggHardEnd || ag->vTypes[v1] == ggSoftEnd)  
	 && (ag->vTypes[v2] == ggHardStart || ag->vTypes[v2] == ggSoftStart)) 
    return ggSJ;
else
    return ggIntron;
}

enum ggEdgeType getSpliceEdgeType(struct altGraphX *ag, int edge)
/* Return edge type. */
{
return getEdgeType(ag, ag->edgeStarts[edge], ag->edgeEnds[edge]);
}

struct spliceEdge *altGraphXToEdges(struct altGraphX *ag)
/* Return a list of splice edges based on data in altGraphX. */
{
int i;
struct spliceEdge *e = NULL, *eList = NULL;
int *vPos = ag->vPositions;
int *starts = ag->edgeStarts;
int *ends = ag->edgeEnds;
for(i=0; i<ag->edgeCount; i++)
    {
    AllocVar(e);
    e->type = getSpliceEdgeType(ag,i);
    e->chromStart = vPos[starts[i]];
    e->chromEnd = vPos[ends[i]];
    e->v1 = starts[i];
    e->v2 = ends[i];
//    e->conf = ag->evidence[i].evCount;
//    e->conf = altGraphConfidenceForEdge(ag, i);
    slAddHead(&eList, e);
    } 
return eList;
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 20;
static int dot = 20;
if (doHappyDots && (--dot <= 0))
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

struct hash *allChainsHash(char *fileName)
{
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chain *chain;
char chainId[128];

while ((chain = chainRead(lf)) != NULL)
    {
    safef(chainId, sizeof(chainId), "%d", chain->id);
    hashAddUnique(hash, chainId, chain);
    }
lineFileClose(&lf);
return hash;
}

struct chain *chainFromId(int id)
/** Return a chain given the id. */
{
static struct hash *chainHash = NULL;
char key[128];
struct chain *chain = NULL;
if(chainHash == NULL)
    {
    char *chainFile = optionVal("chainTable", NULL);
    if(chainFile == NULL)
	errAbort("orthoSplice::chainFromId() - Can't find file for 'chainTable' parameter");
    chainHash = allChainsHash(chainFile);
    }
safef(key, sizeof(key), "%d", id);
chain =  hashFindVal(chainHash, key);
if(chain == NULL)
    warn("Chain not found for id: %d", id);
return chain;
}

struct chain *chainDbLoad(struct sqlConnection *conn, char *track,
			  char *chrom, int id)
/** Load chain. */
{
char table[64];
char query[256];
struct sqlResult *sr;
char **row;
int rowOffset;
struct chain *chain = NULL;

if (!hFindSplitTable(chrom, track, table, &rowOffset))
    errAbort("No %s track in database", track);
snprintf(query, sizeof(query), 
	 "select * from %s where id = %d", table, id);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Can't find %d in %s", id, table);
chain = chainHeadLoad(row + rowOffset);
sqlFreeResult(&sr);
chainDbAddBlocks(chain, track, conn);
return chain;
}

struct chain *chainForPosition(struct sqlConnection *conn, char *db, char *netTable, 
			       char *chainTable, char *chrom, int start, int end)
/** Load the chain in the database for this position from the net track. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct chain *chain = NULL;
struct chainNet *net = chainNetLoadRange(db, netTable, chrom,
					 start, end, NULL);
if(net == NULL)
    return NULL;
chain = chainFromId(net->fillList->chainId);
//chain = chainDbLoad(conn, chainTable, chrom, net->fillList->chainId);
chainNetFreeList(&net);
return chain;
}

void qChainRangePlusStrand(struct chain *chain, int *retQs, int *retQe)
/* Return range of bases covered by chain on q side on the plus
 * strand. */
{
if (chain == NULL)
    errAbort("Can't find range in null query chain.");
if (chain->qStrand == '-')
    {
    *retQs = chain->qSize - chain->qEnd;
    *retQe = chain->qSize - chain->qStart;
    }
else
    {
    *retQs = chain->qStart;
    *retQe = chain->qEnd;
    }
}

void assignVertex(int *vMap, int vertexCount, int v, int orthoV, struct orthoAgReport *agRep)
/** Assign the vertex v in vMap to point to orthoV. Keep track of 
    reporting as well. */
{
assert(v < vertexCount);
if(orthoV == -1)
    {}  /* Do nothing, can't say that we won't find this vertex later. */
else if(vMap[v] == -1)
    {
    vMap[v] = orthoV;
    }
else if(vMap[v] != -1 && vMap[v] != orthoV)
    {
    agRep->ssDoubles++;
    }
}

int vertexForPosition(struct altGraphX *ag, int position, struct orthoAgReport *agRep)
/** Return the vetex that occurs at position otherwise return -1. */
{
int i;
int v = -1;
for(i=0; i<ag->vertexCount; i++)
    {
    if(ag->vPositions[i] == position)
	return i;
    else if((ag->vPositions[i] +1 == position) || (ag->vPositions[i] - 1 == position))
	agRep->ssVeryClose++;
    }
return v;
}

void makeVertexMap(struct altGraphX *ag, struct altGraphX *orthoAg, struct chain *chain,
		   int **vertexMap, int *vertexCount, struct orthoAgReport *agRep, bool reverse)
/** Make a mapping from the vertexes in ag to the vertexes in orthoAg. */
{
struct chain *subChain = NULL, *toFree = NULL;
int qs = 0, qe = 0;
struct spliceEdge *se = NULL, *seList = NULL;
int vCount = ag->vertexCount;
int i;
/* Init splice site array (vertexMap) to -1 as inidicative of not
   being found. */
AllocArray((*vertexMap), vCount);
for(i=0;i<vCount; i++)
    (*vertexMap)[i] = -1;


/* Found an orhtologous graph, determine mapping between edges. */
seList = altGraphXToEdges(ag);
for(se = seList; se != NULL; se = se->next)
    {
    chainSubsetOnT(chain, se->chromStart, se->chromEnd, &subChain, &toFree);    
    if(subChain != NULL)
	{
	int v1 =-1, v2 = -1;
	qChainRangePlusStrand(subChain, &qs, &qe);
	v1 = vertexForPosition(orthoAg, qs, agRep);
	if(reverse)
	    assignVertex(*vertexMap, vCount, se->v2, v1, agRep);
	else
	    assignVertex(*vertexMap, vCount, se->v1, v1, agRep);
	v2 = vertexForPosition(orthoAg, qe, agRep);
	if(reverse)
	    assignVertex(*vertexMap, vCount, se->v1, v2, agRep);
	else
	    assignVertex(*vertexMap, vCount, se->v2, v2, agRep);
	}
    chainFreeList(&toFree);
    }
//fprintf(stdout, "track name=\"ss\" description=\"splice sites\"\n");
for(i=0; i<vCount; i++)
    {
    if((*vertexMap)[i] != -1)
	{
//	fprintf(stdout, "%s\t%d\t%d\t%d-%d\n", orthoAg->tName, orthoAg->vPositions[(*vertexMap)[i]], 
//		orthoAg->vPositions[(*vertexMap)[i]] +1, i, (*vertexMap)[i]);
	}
    }
warn("%d splice sites: %d found, %d missing, %d doubles, %d very close (+/- 1)",
     vCount, agRep->ssHardFound, agRep->ssHardMissing, agRep->ssDoubles, agRep->ssVeryClose);
slFreeList(&seList);
}

int findBestOrthoStartByOverlap(struct altGraphX *ag, bool **em, struct altGraphX *orthoAg, bool **orthoEm,
				struct chain *chain, int *vMap, int softStart, int hardEnd, bool reverse )
{
int bestVertex = -1;
struct chain *subChain = NULL, *toFree = NULL;
int *vPos = ag->vPositions;
int *orthoPos = orthoAg->vPositions;
int bestOverlap = 0;
int i=0,j=0;
int qs = 0, qe = 0;
int oVCount = orthoAg->vertexCount;
bool edge; 
chainSubsetOnT(chain, vPos[softStart], vPos[hardEnd], &subChain, &toFree);    
if(subChain == NULL)
    return -1;
qChainRangePlusStrand(subChain, &qs, &qe);
for(i=0; i<oVCount; i++)
    {
    if(reverse)
	edge = orthoEm[vMap[hardEnd]][i];
    else
	edge = orthoEm[i][vMap[hardEnd]];
    if(edge)
	{
	int oStart = orthoPos[i];
	int oEnd = orthoPos[vMap[hardEnd]];
	int overlap = 0;
	if(reverse)
	    overlap = rangeIntersection(qs, qe, oEnd, oStart);
	else
	    overlap = rangeIntersection(qs, qe, oStart, oEnd);
	if(overlap > bestOverlap)
	    {
	    overlap = bestOverlap;
	    bestVertex = i;
	    }
	}
    }
chainFree(&toFree);
return bestVertex;
}

int findBestOrthoEndByOverlap(struct altGraphX *ag, bool **em, struct altGraphX *orthoAg, bool **orthoEm,
				struct chain *chain, int *vMap, int hardStart, int softEnd, bool reverse )
{
int bestVertex = -1;
struct chain *subChain = NULL, *toFree = NULL;
int *vPos = ag->vPositions;
int *orthoPos = orthoAg->vPositions;
int bestOverlap = 0;
int i=0,j=0;
int qs = 0, qe = 0;
int oVCount = orthoAg->vertexCount;
bool edge; 
chainSubsetOnT(chain, vPos[hardStart], vPos[softEnd], &subChain, &toFree);    
if(subChain == NULL)
    return -1;
qChainRangePlusStrand(subChain, &qs, &qe);
for(i=0; i<oVCount; i++)
    {
    if(reverse)
	edge = orthoEm[i][vMap[hardStart]];
    else
	edge = orthoEm[vMap[hardStart]][i];
    if(edge)
	{
	int oStart = orthoPos[i];
	int oEnd = orthoPos[vMap[hardStart]];
	int overlap = 0;
	if(reverse)
	    overlap = rangeIntersection(qs, qe, oStart, oEnd);
	else
	    overlap = rangeIntersection(qs, qe, oEnd, oStart);
	if(overlap > bestOverlap)
	    {
	    overlap = bestOverlap;
	    bestVertex = i;
	    }
	}
    }
chainFree(&toFree);
return bestVertex;
}


/* int findBestOrthoEndByOverlap(struct altGraphX *ag, bool **em, struct altGraphX *orthoAg, bool **orthoEm, */
/* 			      struct chain *chain, int *vMap, int hardStart, int softEnd) */
/* { */
/* int bestVertex = -1; */
/* struct chain *subChain = NULL, *toFree = NULL; */
/* int *vPos = ag->vPositions; */
/* int *orthoPos = ag->vPositions; */
/* int bestOverlap = 0; */
/* int i=0,j=0; */
/* int qs = 0, qe = 0; */
/* int oVCount = orthoAg->vertexCount; */
/* chainSubsetOnT(chain, vPos[hardStart], vPos[softEnd], &subChain, &toFree);     */
/* qChainRangePlusStrand(subChain, &qs, &qe); */
/* if(subChain == NULL) */
/*     return -1; */
/* for(i=0; i<oVCount; i++) */
/*     { */
/*     if(orthoEm[vMap[hardStart]][softEnd]) */
/* 	{ */
/* 	int oStart = orthoPos[vMap[hardStart]]; */
/* 	int oEnd = orthoPos[i]; */
/* 	int overlap = 0; */
/* 	overlap = rangeIntersection(qs, qe, oStart, oEnd); */
/* 	if(overlap > bestOverlap) */
/* 	    { */
/* 	    overlap = bestOverlap; */
/* 	    bestVertex = i; */
/* 	    } */
/* 	} */
/*     } */
/* chainFree(&toFree); */
/* return bestVertex; */
/* } */
	
struct altGraphX *commonAgInit(struct altGraphX *ag)
/** Make a clone of ag with same arrays but edgeCount = 0 even
    though memory allocated. Free with altGraphXFree().*/
{
struct altGraphX *ret = CloneVar(ag);
int vC = ag->vertexCount;
int eC = ag->edgeCount; 
ret->tName = cloneString(ag->tName);
ret->name = cloneString(ag->name);
ret->edgeCount = 0;
ret->vTypes = CloneArray(ag->vTypes, vC);
ret->vPositions = CloneArray(ag->vPositions, vC);
ret->evidence = NULL;
AllocArray(ret->edgeStarts, eC);
AllocArray(ret->edgeEnds, eC);
AllocArray(ret->edgeTypes, eC);
return ret;
}

int getEdgeNum(struct altGraphX *ag, int v1, int v2)
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
errAbort("orthoSplice::getEdgeNum() - Didn't find edge with vertices %d-%d in %s.", v1, v2, ag->name);
return -1;
}

bool isExon(struct altGraphX *ag, int v1, int v2)
{
int i = 0;
int eC = ag->edgeCount;
for(i=0;i<eC; i++)
    {
    if(ag->edgeStarts[i] == v1 && ag->edgeEnds[i] == v2)
	{
	enum ggEdgeType t= getSpliceEdgeType(ag, i);
	return (t == ggExon);
	}
    }
errAbort("orthoSplice::isExon() - Can't find edge with vertexes %d and %d in altGraphX %s",
	 v1, v2, ag->name);
return FALSE;
}

boolean notAssigned(int *vMap, int vCount, int val)
/* Look through vMap, return FALSE if val is already in vMap, TRUE otherwise. */
{
int i;
for(i=0; i<vCount; i++)
    {
    if(vMap[i] == val)
	return FALSE;
    }
return TRUE;
}



void findSoftStartsEnds(struct altGraphX *ag, bool **em, struct altGraphX *orthoAg, bool **orthoEm,
			struct chain *chain, int *vMap, int vertexCount, 
			struct orthoAgReport *agRep, bool reverse)
/** Transcription start and end in much more variable than splice sites.
    Map soft vertexes by looking for a vertex in the orthologous
    graph with an edge that overlaps. */
{
int i=0,j=0;
int vCount = ag->vertexCount;
unsigned char *vTypes = ag->vTypes;
for(i=0; i<vCount; i++)
    {
    /* If we haven't mapped this vertex and it is softStart or softEnd. */
    if(vMap[i] == -1 && (vTypes[i] == ggSoftStart || vTypes[i] == ggSoftEnd))
	{
	/* Look for an edge with a vertex that is mapped. */
	for(j=0; j<vCount; j++)
	    {
	    /* If there is an edge with one vertex already found. */
	    if(em[i][j] && vMap[j] != -1 && isExon(ag, i, j))
		{
		int orthoV = -1;
		if(vTypes[i] == ggSoftStart)
		    orthoV = findBestOrthoStartByOverlap(ag, em, orthoAg, orthoEm, chain, vMap, i, j, reverse);
		else
		    orthoV = findBestOrthoEndByOverlap(ag, em, orthoAg, orthoEm, chain, vMap, i, j, reverse);
		if(notAssigned(vMap, vCount, orthoV))
		    vMap[i] = orthoV;
		}
	    }
	}
    }
}

		

void *freeCommonAg(struct altGraphX **ag)
/** Free a common stub copied from another algGraphX */
{
struct altGraphX *el = *ag;
freeMem(el->tName);
freeMem(el->name);
freeMem(el->vTypes);
freeMem(el->vPositions);
freeMem(el->edgeStarts);
freeMem(el->edgeEnds);
freeMem(el->edgeTypes);
evidenceFreeList(&el->evidence);
freeMem(el);
*ag = NULL;
}

void fillInEdge(struct spliceEdge *se, struct orthoAgReport *agRep , 
		struct altGraphX *ag, bool **agEm, int v1, int v2, int *edgeSeen)
/** Fill in one edge with information from ag. */
{
struct evidence *e = NULL;
int edgeNum = 0;
int eCount = 0;
int k =0;
int vCount = ag->vertexCount;
se->type = getEdgeType(ag, v1, v2);
se->chrom = agRep->chrom; /* Memory not owned in se. Free'd with agRep. */
se->agName = agRep->agName; /* Memory not owned in se. Free'd with agRep. */
se->strand[0] = ag->strand[0];
se->chromStart = ag->vPositions[v1];
se->chromEnd = ag->vPositions[v2];
se->v1 = v1;
se->v2 = v2;
if(ag->vTypes[v1] == ggSoftStart || ag->vTypes[v1] == ggSoftEnd)
    se->soft = TRUE;
if(ag->vTypes[v2] == ggSoftStart || ag->vTypes[v2] == ggSoftEnd)
    se->soft = TRUE;
edgeNum = getEdgeNum(ag, v1, v2);
e = slElementFromIx(ag->evidence, edgeNum);
se->conf = e->evCount;
/* Check to se if this edge has been connected to any other vertices. */
if(edgeSeen[v1] > 1)
    se->alt = TRUE;
/* If not search through to see if any are comming. */
else 
    {
    eCount =0;
    for(k=v2; k<vCount; k++)
	{
	if(agEm[v1][k])
	    eCount++;
	}
    if(eCount > 1)
	se->alt = TRUE;
    }
}

void fillInReport(struct orthoAgReport *agRep, struct altGraphX *ag, bool **agEm,
		  struct altGraphX *orthoAg, bool **orthoEm,
		  struct altGraphX *commonAg, bool **commonEm,
		  int *vMap)
/** Look at the three graphs and output some of the interesting
    statistics including:
    - Splice sites found and missing.
    - exons, introns, and splice juctions found and missing.
    - Alt-splicing found and missing.
*/
{
int i=0,j=0, k=0, vCount = ag->vertexCount;
int eCount = 0;
int *edgeSeen = NULL, *cEdgeSeen = NULL; /* Used to keep track of alt splicing. */
struct spliceEdge *seList = NULL, *se = NULL;
AllocArray(edgeSeen, vCount);
AllocArray(cEdgeSeen, vCount);
agRep->ssSoftMissing = agRep->ssSoftFound = 0;
agRep->ssHardMissing = agRep->ssHardFound = 0;

/* Count up vertices found. */
for(i=0; i<vCount; i++)
    if(vMap[i] != -1)
	{
	if(ag->vTypes[i] == ggHardStart || ag->vTypes[i] == ggHardEnd)
	    agRep->ssHardFound++;
	else
	    agRep->ssSoftFound++;
	}
    else 
	{
	if(ag->vTypes[i] == ggHardStart || ag->vTypes[i] == ggHardEnd)
	    agRep->ssHardMissing++;
	else
	    agRep->ssSoftMissing++;
	}

/* Count up the edges found. */
for(i=0; i<vCount; i++)
    for(j=0;j<vCount; j++)
	{
	if(agEm[i][j])
	    {
	    AllocVar(se);
	    edgeSeen[i]++;
	    fillInEdge(se, agRep, ag, agEm, i,j, edgeSeen);
	    if(commonEm[i][j])
		{
		se->conserved = TRUE;
		cEdgeSeen[i]++;
		}
	    slAddHead(&agRep->seList, se);
	    }
	}
slReverse(&agRep->seList);
/* Count up alt splicing, that is when one vertex connects to 
   more than one down stream. Should catch most kinds of alt splicing
   except alternative promoters. */
for(i=0; i<vCount; i++)
    {
    if(edgeSeen[i] > 1)
	agRep->alt++;
    if(cEdgeSeen[i] > 1)
	agRep->altFound++;
    }
freez(&edgeSeen);
freez(&cEdgeSeen);
}

struct altGraphX *makeCommonAltGraphX(struct altGraphX *ag, struct chain *chain)
/** For each edge in ag see if there is a similar edge in the
    orthologus altGraphX structure and output it if there. */
{
bool **em = NULL;
bool **orthoEm = NULL;
bool **commonEm = NULL;
struct altGraphX *commonAg = NULL, *orthoAg = NULL;
struct orthoAgReport *agRep = NULL;
int *vertexMap = NULL; /* Map of ag's vertexes to agOrthos's vertexes. */
struct chain *subChain = NULL, *toFree = NULL;
int i=0,j=0;
int vCount = ag->vertexCount;
char query[256];
int qs = 0, qe = 0;
bool match = FALSE;
bool reverse = FALSE;
struct sqlConnection *orthoConn = NULL; //hAllocConn2();
/* First find the orthologous splicing graph. */
chainSubsetOnT(chain, ag->tStart, ag->tEnd, &subChain, &toFree);    
if(subChain == NULL)
    return NULL;
orthoConn = hAllocConn2();
qChainRangePlusStrand(subChain, &qs, &qe);
safef(query, sizeof(query), "select * from %s where tName='%s' and tStart<%d and tEnd>%d",
      altTable, subChain->qName, qe, qs);
orthoAg = altGraphXLoadByQuery(orthoConn, query);
if ((subChain->qStrand == '-'))
    reverse = TRUE;
hFreeConn2(&orthoConn);
chainFreeList(&toFree);
if(orthoAg == NULL)
    return NULL;

/* Got an orhtologous graph. Do some analysis. */
AllocVar(agRep);
agRep->agName = cloneString(ag->name);
agRep->orthoAgName = cloneString(orthoAg->name);
agRep->chrom = cloneString(ag->tName);
makeVertexMap(ag, orthoAg, chain, &vertexMap, &vCount, agRep, reverse);
em = altGraphXCreateEdgeMatrix(ag);
orthoEm = altGraphXCreateEdgeMatrix(orthoAg);
findSoftStartsEnds(ag, em, orthoAg, orthoEm, chain, vertexMap, vCount, agRep, reverse);
commonAg = commonAgInit(ag);
for(i=0;i<vCount; i++) 
    {
    for(j=0; j<vCount; j++)
	{
	if(em[i][j])
	    {
	    /* Have to look at the splice graph differently if we're on 
	       the chain is on the '-' strand. */
	    if(vertexMap[i] != -1 && vertexMap[j] != -1)
		{
		if(reverse) { match = orthoEm[vertexMap[j]][vertexMap[i]]; }
		else 	  {  match = orthoEm[vertexMap[i]][vertexMap[j]]; }
		}
	    else
		match = FALSE;
	    if(match)
		{
		struct evidence *commonEv = NULL;
		struct evidence *oldEv = NULL;
		oldEv = slElementFromIx(ag->evidence, getEdgeNum(ag, i, j));
		AllocVar(commonEv);
		commonEv->evCount = oldEv->evCount;
		commonEv->mrnaIds = CloneArray(oldEv->mrnaIds, oldEv->evCount);
		slAddTail(&commonAg->evidence, commonEv);
		commonAg->edgeStarts[commonAg->edgeCount] = i;
		commonAg->edgeEnds[commonAg->edgeCount] = j;
		commonAg->edgeTypes[commonAg->edgeCount] = getSpliceEdgeType(commonAg, commonAg->edgeCount);
		commonAg->edgeCount++;
		}
	    }
	}
    }
commonEm = altGraphXCreateEdgeMatrix(commonAg);
fillInReport(agRep, ag, em, orthoAg, orthoEm, commonAg, commonEm, vertexMap);
outputReport(agRep);
orthoAgReportFree(&agRep);
altGraphXFreeEdgeMatrix(&em, ag->vertexCount);
altGraphXFreeEdgeMatrix(&commonEm, commonAg->vertexCount);
altGraphXFreeEdgeMatrix(&orthoEm, orthoAg->vertexCount);
if(commonAg->edgeCount > 0)
    {
//    altGraphXTabOut(commonAg, stdout);
    }
else
    freeCommonAg(&commonAg);
freez(&vertexMap);
altGraphXFreeList(&orthoAg);
return commonAg;
}

struct altGraphX *findOrthoAltGraphX(struct altGraphX *ag, char *db,
				     char *netTable, char *chainTable)
/** Find an orthologous altGraphX record in orthoDb for ag in db if 
    it exists. Otherwise return NULL. */
{
struct altGraphX *orthoAg = NULL;
struct chain *chain = NULL;
struct chain *subChain = NULL, *toFree = NULL;
struct sqlConnection *conn = hAllocConn();
struct spliceEdge *seList = NULL, *se = NULL;
int qs = 0, qe = 0;
chain = chainForPosition(conn, db, netTable, chainTable, ag->tName, ag->tStart, ag->tEnd);
hFreeConn(&conn);
if(chain == NULL)
    return NULL;
seList = altGraphXToEdges(ag);
orthoAg = makeCommonAltGraphX(ag, chain);
//fprintf(stdout, "track name=\"exons\" description=\"human exons mapped.\"\n");
for(se = seList; se != NULL; se = se->next)
    {
    if(se->type != ggExon)
	continue;
    chainSubsetOnT(chain, se->chromStart, se->chromEnd, &subChain, &toFree);    

    if(subChain != NULL)
	{
	qChainRangePlusStrand(subChain, &qs, &qe);

//	fprintf(stdout, "%s\t%d\t%d\t%d-%d\n",
//		subChain->qName, qs, qe, se->v1, se->v2);
	}
    else
	warn("Yikes got a NULL for %s:%d-%d", ag->tName, se->chromStart, se->chromEnd);
    chainFree(&toFree);
    }

slFreeList(&seList);
//chainFreeList(&chain); /* Hashed in memory, don't free. */
return orthoAg;
}

void orthoSplice(char *db, char *orthoDb)
/** Open the altFile for db1 and examine the ortholog in db2 for 
    each entry. */
{
struct lineFile *lf = NULL;
char *altIn = optionVal("altInFile", NULL);
char **row = NULL;
int rowCount = 0;
char *line = NULL;
struct altGraphX *ag = NULL, *agOrtho = NULL;
char *netTable = optionVal("netTable", NULL);
char *chainTable = optionVal("chainTable", NULL);
char *commonFileName = optionVal("commonFile", NULL);
char *reportFileName = optionVal("reportFile", NULL);
char *edgeFileName = optionVal("edgeFile", NULL);
FILE *cFile = NULL;

int foundOrtho=0, noOrtho=0;
if(altIn == NULL)
    errAbort("orthoSplice - must specify altInFile.");
if(netTable == NULL)
    errAbort("orthoSplice - must specify netTable.");
if(chainTable == NULL)
    errAbort("orthoSplice - must specify chainTable.");
if(commonFileName == NULL)
    errAbort("orthoSplice - must specify commonFile.");
if(reportFileName == NULL)
    errAbort("orthoSplice - must specify reportFile.");
if(edgeFileName == NULL)
    errAbort("orthoSplice - must specify edgeFile.");
/* Want to read in and process each altGraphX record individually
   to save memory, so figure out how many columns in each row we have currently and
   load them one by one. */
lf = lineFileOpen(altIn, TRUE);
lineFileNextReal(lf, &line);
lineFileReuse(lf);
rowCount = countChars(line, '\t') +1;
AllocArray(row, rowCount);
warn("Reading records from %s", altIn);
cFile = mustOpen(commonFileName, "w");
rFile = mustOpen(reportFileName, "w");
edgeFile = mustOpen(edgeFileName, "w");
while(lineFileNextCharRow(lf, '\t', row, rowCount))
    {
    ag = altGraphXLoad(row);
    occassionalDot();
    if(isUnique(ag))
	{
	agOrtho = findOrthoAltGraphX(ag, db,  netTable, chainTable);
	if(agOrtho != NULL)
	    {
	    foundOrtho++;
	    printf("FoundOne.\n");
	    altGraphXTabOut(agOrtho,cFile);
	    freeCommonAg(&agOrtho);
	    }
	else
	    {
	    noOrtho++;
	    printf("NoOrtho\n");
	    }
	}
    altGraphXFree(&ag);
    }
warn("%d graphs, %d orthos found %d no ortho splice graph",
     (noOrtho+foundOrtho), foundOrtho, noOrtho);
freez(&row);
lineFileClose(&lf);
carefulClose(&cFile);
carefulClose(&rFile);
warn("\nDone.");
}
    
   

int main(int argc, char *argv[])
{
char *db = NULL, *orthoDb = NULL;
if(argc == 1)
    usage();
doHappyDots = isatty(1);  /* stdout */
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
db = optionVal("db", NULL);
orthoDb = optionVal("orthoDb", NULL);
if(db == NULL || orthoDb == NULL)
    errAbort("orthoSplice - Must set db and orhtoDb.");
hSetDb(db);
hSetDb2(orthoDb);
orthoSplice(db, orthoDb);
return 0;
}
