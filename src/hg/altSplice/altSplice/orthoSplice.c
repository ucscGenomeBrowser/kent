/** orthoSplice.c - Program to examine orthologous altGraphX
 * structures. Using Jim's chaining/netting
 */

/** Basic outline is like so:

 - Input is altGraphX for two genome and chains/nets to map between
   genomes.
 - For each altGraphX record on primary genome look up altGraph record
   on orthologous genome using Jim's chaining/netting results.
 - Still using the chains create a mapping between the splice sites
   (vertexes) of the two altGraphX records. Then use this mapping to
   compare the actual splice graphs for the two records.
 - Look at properties of the common splicing graph including,
   conserved exons, introns, and alternative splicing. I'm looking for
   alternative splicing by looking for vertexes that have more than
   one connection in their row. Further, I currently require that both
   vertexes be hard starts or stops. That is I won't consider a
   different polyadenylation sites as alt-splicing. By focusing on the
   rows, I also won't even detect different promoters, have to look at
   the columns for those. As far as i can tell though I should be able
   to detect all other kinds of alternative splicing.
 - Report results loci by loci and also edge by edge in the graph.
*/
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
#include "rbTree.h"

static char const rcsid[] = "$Id: orthoSplice.c,v 1.16 2003/09/14 23:05:52 sugnet Exp $";
static struct rbTree *netTree = NULL;  /* Global red-black tree to store cnfills in for quick searching. */
static struct rbTree *orthoAgxTree = NULL; /* Global red-black tree to store agx's so don't need db. */
static char *workingChrom = NULL;      /* Chromosme we are working on. */

struct orthoSpliceEdge 
/* Structure to hold information about one edge in 
   a splicing graph. */
{
    struct orthoSpliceEdge *next;    /* Next in list. */
    char *chrom;                /* Chromsome. Memory not owned here.*/
    int chromStart;             /* Chrom start. */
    int chromEnd;               /* End. */
    char *agName;               /* Name of altGraphX that edge is from. Memory not owned here. */
    int conf;                   /* Confidence. Generally number of mRNA's supporting edge. */
    char strand[2];             /* Strand. */
    enum ggEdgeType type;       /* Type of edge: ggExon, ggIntron, ggSJ, ggCassette. */
    int v1;                     /* Vertex 1 in graph. */
    int v2;                     /* Vertex 2 in graph. */
    int orthoV1;                /* Vertex 1 in orthoGraph. */
    int orthoV2;                /* Vertex 2 in orhtoGraph. */
    int alt;                    /* Possible alt splice? 1=TRUE 0=FALSE */
    int altCons;                /* Possible alt in orthoAg too? */
    int conserved;              /* Conserved in ortholgous genome? 1=TRUE 0=FALSE */    
    int soft;                   /* TRUE (1) if a vertex is soft, FALSE otherwise. */
};

struct orthoAgReport
/* Some summary information on comparison of splice graphs. */
{
    struct orthoAgReport *next;  /**< Next in list. */
    struct orthoSpliceEdge *seList;   /**< List of edges and their attributes. */
    char *agName;                /**< AltGraphX from first genome name. */
    char *orthoAgName;           /**< Orthologous altGraphX name. */
    char *chrom;                 /**< Chrom name. */
    int numVertexes;             /**< Number of vertexes in ag. */
    int *vertexMap;              /**< Maping of vertexes in ag to orthoAgName. */
    int ssSoftFound;             /**< Number of splice sites in ag that are splice sites in orhtoAg. */
    int ssSoftMissing;           /**< Number of splice sites in ag that are not splice sites in orhtoAg. */
    int ssHardFound;             /**< Number of splice sites in ag that are splice sites in orhtoAg. */
    int ssHardMissing;           /**< Number of splice sites in ag that are not splice sites in orhtoAg. */
    int ssDoubles;               /**< Number of splice sites in ag that map to two places in orhtoAg (hopefully rare). */
    int ssVeryClose;             /**< How many times were we off by one. */
    int alt;                     /**< Number of alternative splices in graph. */
    int altFound;                /**< Number of alternative splices that are conserved. */
};

static boolean doHappyDots;   /* output activity dots? */
static char *altTable = "altGraphX";  /* Name of table to load splice graphs from. */
static struct orthoAgReport *agReportList = NULL; /* List of reports. */
static int trumpNum = BIGNUM; /* If we seen an edge trumpNum times, keep it even if it isn't conserved.*/
FILE *rFile = NULL;       /* Loci report file. */
FILE *edgeFile = NULL;    /* Edge report file. */

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"orthoDb", OPTION_STRING},
    {"chrom", OPTION_STRING},
    {"altInFile", OPTION_STRING},
    {"netTable", OPTION_STRING},
    {"netFile", OPTION_STRING},
    {"chainFile", OPTION_STRING},
    {"reportFile", OPTION_STRING},
    {"commonFile", OPTION_STRING},
    {"edgeFile", OPTION_STRING},
    {"trumpNum", OPTION_INT},
    {"orthoAgxFile", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Database (i.e. hg15) that altInFile comes from.",
    "Database (i.e. mm3) to look for orthologous splicing in.",
    "Chromosome to map (i.e. chr1).",
    "File containing altGraphX records to look for.",
    "Datbase table containing net records, i.e. mouseNet.",
    "File containing net records.",
    "File containing the chains. Usually I do this on a chromosome basis.",
    "File name to print individual reports to.",
    "File name to print common altGraphX records to.",
    "File name to print individual edge reports to.",
    "[optional: default infinite] Number of mRNAs required to keep edge even if not conserved.",
    "[optional] Don't use database, read in possible orthologous altGraphX records from file.",
};

void usage()
/** Print usage and quit. */
{
int i=0;
warn("orthoSplice - program to compare splicing in different organisms\n"
     "initially human and mouse as they both have nice EST and cDNA data\n"
     "still working out algorithm but options are:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   "
	 "orthoSplice -altInFile=ranbp1.tab -db=hg15 -orthoDb=mm3 -netTable=mouseNet \\\n"
	 "\t   -chainFile=chains/chr22.chain -commonFile=out.agx -reportFile=out.report -edgeFile=out.edge.report\n");
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

void orthoAgReportHeader(FILE *out)
/** Output a header for the orthoAgReport fields. */
{
fprintf(out, "#%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", "agName", "orhtoAgName", "numVertexes", "vertexMap",  "ssSoftFound", "ssSoftMissing", "ssHardFound", "ssHardMissing", "ssDoubles", "ssVeryClose", "altEdges", "altEdgesFound");
}

void orthoAgReportTabOut(struct orthoAgReport *r, FILE *out)
/** Write out a one line summary of a report. */
{
int i;
fprintf(out, "%s\t%s\t%d\t", r->agName, r->orthoAgName, r->numVertexes);
for(i=0; i<r->numVertexes; i++)
    fprintf(out, "%d,", r->vertexMap[i]);
fprintf(out, "\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", r->ssSoftFound, r->ssSoftMissing, r->ssHardFound, 
	r->ssHardMissing, r->ssDoubles, r->ssVeryClose, r->alt, r->altFound);
}

void orthoSpliceEdgeHeader(FILE *out)
/** Write out a header for splice edge report. */
{
fprintf(out, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", "#chrom", "chromStart", "chromEnd", 
	"agName", "conf", "strand",
	"type", "v1", "v2", "orthoV1", "orthoV2", "alt", "altCons", "conserved", "soft");
}

void orthoSpliceEdgeTabOut(struct orthoSpliceEdge *se, FILE *out)
/** Write out a splice edge record. */
{
fprintf(out, "%s\t%d\t%d\t%s\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", se->chrom, se->chromStart, se->chromEnd, 
	se->agName, se->conf, se->strand,
	se->type, se->v1, se->v2, se->orthoV1, se->orthoV2,
	se->alt, se->altCons, se->conserved, se->soft);
}

void outputReport(struct orthoAgReport *rep)
/** Write out a reprort. Have to write top level orthoAgReport and the
    list of orthoSpliceEdge mini reports. */
{
static boolean repHeader = FALSE;
static boolean edgeHeader = FALSE;
struct orthoSpliceEdge *se = NULL;
if(!repHeader)
    {
    orthoAgReportHeader(rFile);
    repHeader = TRUE;
    }
orthoAgReportTabOut(rep, rFile);
if(!edgeHeader)
    {
    orthoSpliceEdgeHeader(edgeFile);
    edgeHeader = TRUE;
    }
for(se = rep->seList; se != NULL; se = se->next)
    {
    orthoSpliceEdgeTabOut(se, edgeFile);
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

struct orthoSpliceEdge *altGraphXToOSEdges(struct altGraphX *ag)
/* Return a list of splice edges based on data in altGraphX. */
{
int i;
struct orthoSpliceEdge *e = NULL, *eList = NULL;
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

int agxRangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct altGraphX *a = va;
struct altGraphX *b = vb;
int diff;
diff = strcmp(a->tName, b->tName);
if(diff != 0) 
    {
    if (diff > 0)
	return 1; 
    else
	return -1;
    }
diff = strcmp(a->strand, b->strand);
if(diff != 0) 
    {
    if (diff > 0)
	return 1; 
    else
	return -1;
    }
if (a->tEnd <= b->tStart)
    return -1;
else if (b->tEnd <= a->tStart)
    return 1;
else
    return 0;
}

struct rbTree *rbTreeFromAgxFile(char *fileName)
/* Build an rbTree from an agxt file */
{
struct rbTree *rbTree = rbTreeNew(agxRangeCmp);
struct altGraphX *ag = NULL, *agList = NULL;
agList = altGraphXLoadAll(fileName);

for(ag=agList; ag != NULL; ag = ag->next)
    {
    rbTreeAdd(rbTree, ag);
    }
return rbTree;
}

int cnFillRangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct cnFill *a = va;
struct cnFill *b = vb;
if (a->tStart + a->tSize <= b->tStart)
    return -1;
else if (b->tStart + b->tSize <= a->tStart)
    return 1;
else
    return 0;
}

struct rbTree *rbTreeFromNetFile(char *fileName)
/* Build an rbTree from a net file */
{
struct rbTree *rbTree = rbTreeNew(cnFillRangeCmp);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chainNet *cn = chainNetRead(lf);
struct cnFill *fill = NULL;
for(fill=cn->fillList; fill != NULL; fill = fill->next)
    {
    rbTreeAdd(rbTree, fill);
    }
return rbTree;
}

struct hash *allChainsHash(char *fileName)
/** Hash all the chains in a given file by their ids. */
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
    char *chainFile = optionVal("chainFile", NULL);
    if(chainFile == NULL)
	errAbort("orthoSplice::chainFromId() - Can't find file for 'chainFile' parameter");
    chainHash = allChainsHash(chainFile);
    }
if(id == 0)
    return NULL;
safef(key, sizeof(key), "%d", id);
chain =  hashFindVal(chainHash, key);
if(chain == NULL)
    warn("Chain not found for id: %d", id);
return chain;
}

struct chain *chainDbLoad(struct sqlConnection *conn, char *track,
			  char *chrom, int id)
/** Load chain. Not using this anymore as it is much faster to do chrom
    by chrom hashing the chains directly from a file. */
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

int chainBlockCoverage(struct chain *chain, int start, int end,
		       int* blockStarts, int *blockSizes, int blockCount)
/* Calculate how many of the blocks are in a chain. */
{
struct boxIn *boxIn = NULL, *boxInList=NULL;
int blocksCovered = 0;
int i=0;

/* Find the part of the chain of interest to us. */
for(boxIn = chain->blockList; boxIn != NULL; boxIn = boxIn->next)
    {
    if(boxIn->tEnd >= start)
	{
	boxInList = boxIn;
	break;
	}
    }
/* Check to see how many of our exons the boxInList contains covers. 
   For each block check to see if the blockStart and blockEnd are 
   found in the boxInList. */
for(i=0; i<blockCount; i++)
    {
    boolean startFound = FALSE;
    int blockStart = blockStarts[i];
    int blockEnd = blockStarts[i] + blockSizes[i];
    for(boxIn = boxInList; boxIn != NULL && boxIn->tStart < end; boxIn = boxIn->next)
	{
	if(boxIn->tStart < blockStart && boxIn->tEnd > blockStart)
	    startFound = TRUE;
	if(startFound && boxIn->tStart < blockEnd && boxIn->tEnd > blockEnd)
	    {
	    blocksCovered++;
	    break;
	    }
	}
    }
return blocksCovered;
}

boolean betterChain(struct chain *chain, int start, int end,
		    int* blockStarts, int *blockSizes, int blockCount,
		    struct chain **bestChain, int *bestCover)
/* Return TRUE if chain is a better fit than bestChain. If TRUE
   fill in bestChain and bestCover. */
{
struct chain *subChain=NULL, *toFree=NULL;
int blocksCovered = 0;
boolean better = FALSE;
/* Check for easy case. */
if(chain == NULL || chain->tStart > end || chain->tStart + chain->tSize < start)
    return FALSE;
blocksCovered = chainBlockCoverage(chain, start, end, blockStarts, blockSizes, blockCount);
if(blocksCovered > (*bestCover))
    {
    *bestChain = chain;
    *bestCover = blocksCovered;
    better = TRUE;
    }
return better;
}

void lookForBestChain(struct cnFill *list, int start, int end,
		      int* blockStarts, int *blockSizes, int blockCount,
		      struct chain **bestChain, int *bestCover)
/* Recursively look for the best chain. Best is defined as the chain
   that covers the most number of blocks found in starts and sizes. This
   will be stored in bestChain and number of blocks that it covers will
   be stored in bestCover. */
{
struct cnFill *fill=NULL;
struct cnFill *gap=NULL;

struct chain *chain = NULL;
for(fill = list; fill != NULL; fill = fill->next)
    {
    chain = chainFromId(fill->chainId);
    betterChain(chain, start, end, blockStarts, blockSizes, blockCount, bestChain, bestCover);
    for(gap = fill->children; gap != NULL; gap = gap->next)
	{
	chain = chainFromId(gap->chainId);
	betterChain(chain, start, end, blockStarts, blockSizes, blockCount, bestChain, bestCover);
	if(gap->children)
	    {
	    lookForBestChain(gap->children, start, end, 
				       blockStarts, blockSizes, blockCount,
				       bestChain, bestCover);
	    }
	}
    }
}

void chainNetGetRange(char *db, char *netTable, char *chrom,
		      int start, int end, char *extraWhere, struct cnFill **fill,
		      struct chainNet **toFree)
/* Load up the appropriate chainNet list for this range. */
{
struct cnFill *searchHi=NULL, *searchLo = NULL;
struct slRef *refList = NULL, *ref = NULL;

(*fill) = NULL;
(*toFree) = NULL;
if(differentString(workingChrom, chrom))
    return;
if(netTree != NULL)
    {
    AllocVar(searchLo);
    searchLo->tStart = start;
    searchLo->tSize = 0;
    AllocVar(searchHi);
    searchHi->tStart = end;
    searchHi->tSize = 0;
    refList = rbTreeItemsInRange(netTree, searchLo, searchHi);
    for(ref = refList; ref != NULL; ref = ref->next)
	{
	slSafeAddHead(fill, ((struct slList *)ref->val));
	}
    slReverse(fill);
    freez(&searchHi);
    freez(&searchLo);
    (*toFree) = NULL;
    }
else
    {
    struct chainNet *net =chainNetLoadRange(db, netTable, chrom,
					   start, end, NULL);
    if(net != NULL)
	(*fill) = net->fillList;
    else
	(*fill) = NULL;
    (*toFree) = net;
    }
}

struct chain *chainForBlocks(char *db, char *netTable, 
			     char *chrom, int start, int end,
			     int *blockStarts, int *blockSizes, int blockCount)
/** Load the chain in the database for this position from the net track. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct cnFill *fill=NULL;
struct cnFill *gap=NULL;
struct chain *subChain=NULL, *toFree=NULL;
struct chain *chain = NULL;
struct chainNet *net = NULL;
int bestOverlap = 0;
chainNetGetRange(db, netTable, chrom,
		 start, end, NULL, &fill, &net);
if(fill != NULL)
    lookForBestChain(fill, start, end, 
		     blockStarts, blockSizes, blockCount, 
		     &chain, &bestOverlap);
chainNetFreeList(&net);
return chain;
}

boolean checkChain(struct chain *chain, int start, int end)
/** Return true if chain covers start, end, false otherwise. */
{
struct chain *subChain=NULL, *toFree=NULL;
boolean good = FALSE;
if(chain == NULL || chain->tStart > end || chain->tStart + chain->tSize < start)
    return FALSE;
chainSubsetOnT(chain, start, end, &subChain, &toFree);    
if(subChain != NULL)
    good = TRUE;
chainFree(&toFree);
return good;
}

struct chain *lookForChain(struct cnFill *list, int start, int end)
/* Recursively look for a chain in this cnFill list containing the coordinates 
   desired. */
{
struct cnFill *fill=NULL;
struct cnFill *gap=NULL;

struct chain *chain = NULL;
for(fill = list; fill != NULL; fill = fill->next)
    {
    chain = chainFromId(fill->chainId);
    if(checkChain(chain, start,end))
	return chain;
    for(gap = fill->children; gap != NULL; gap = gap->next)
	{
	chain = chainFromId(gap->chainId);
	if(checkChain(chain, start,end))
	    return chain;
	if(gap->children)
	    {
	    chain = lookForChain(gap->children, start, end);
	    if(checkChain(chain, start, end))
		return chain;
	    }
	}
    }
return chain;
}

struct chain *chainForPosition(struct sqlConnection *conn, char *db, char *netTable, 
			       char *chainFile, char *chrom, int start, int end)
/** Load the chain in the database for this position from the net track. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct cnFill *fill=NULL;
struct cnFill *gap=NULL;
struct chain *subChain=NULL, *toFree=NULL;
struct chain *chain = NULL;
struct chainNet *net = chainNetLoadRange(db, netTable, chrom,
					 start, end, NULL);
if(net == NULL)
    return NULL;
fill = net->fillList;
chain = lookForChain(fill, start, end);
//chain = chainDbLoad(conn, chainFile, chrom, net->fillList->chainId);
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

void assignVertex(int *vMap, int *orthoAgIx, int vertexCount, int oIndex,
		  int v, int orthoV, struct orthoAgReport *agRep)
/** Assign the vertex v in vMap to point to orthoV. Keep track of 
    some reporting as well. */
{
assert(v < vertexCount);
if(orthoV == -1)
    {}  /* Do nothing, can't say that we won't find this vertex later. */
else if(vMap[v] == -1)
    {
    orthoAgIx[v] = oIndex;
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

void printIntArray(int *array, int count)
{
int i;
for(i=0; i<count; i++)
    fprintf(stdout,"%d,",array[i]);
fprintf(stdout,"\n");
fflush(stdout);
}

void makeVertexMap(struct altGraphX *ag, struct altGraphX *orthoAg, struct chain *chain,
		   int *vertexMap, int *orthoAgIx, int vCount, int oIndex, 
		   struct orthoSpliceEdge *seList, struct orthoAgReport *agRep, bool reverse)
/** Make a mapping from the vertexes in ag to the vertexes in orthoAg. */
{
struct chain *subChain = NULL, *toFree = NULL;
int qs = 0, qe = 0;
struct orthoSpliceEdge *se = NULL;
int i;
for(se = seList; se != NULL; se = se->next)
    {
    chainSubsetOnT(chain, se->chromStart, se->chromEnd, &subChain, &toFree);    
    if(subChain != NULL)
	{
	int v1 =-1, v2 = -1;
	qChainRangePlusStrand(subChain, &qs, &qe);
	v1 = vertexForPosition(orthoAg, qs, agRep);
	if(reverse)
	    assignVertex(vertexMap, orthoAgIx, vCount, oIndex, se->v2, v1, agRep);
	else
	    assignVertex(vertexMap, orthoAgIx, vCount, oIndex, se->v1, v1, agRep);
	v2 = vertexForPosition(orthoAg, qe, agRep);
	if(reverse)
	    assignVertex(vertexMap, orthoAgIx, vCount, oIndex, se->v1, v2, agRep);
	else
	    assignVertex(vertexMap, orthoAgIx, vCount, oIndex, se->v2, v2, agRep);
	}
    chainFreeList(&toFree);
    }
}

boolean isHardVertex(struct altGraphX *ag, int vertex)
{
return (ag->vTypes[vertex] == ggHardStart || ag->vTypes[vertex] == ggHardEnd);
}

int findBestOrthoStartByOverlap(struct altGraphX *ag, bool **em, struct altGraphX *orthoAg, bool **orthoEm,
				struct chain *chain, int *vMap, int softStart, int hardEnd,  bool reverse )
/** Look for a good match for softStart by looking an edge in
    orthoAg that overlaps well with softStart->hardEnd in orthologous
    genome using chain to check for overlap. */
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
if(subChain == NULL || vMap[hardEnd] >= oVCount)
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
	if(overlap > bestOverlap && (overlap < 2*min(abs(qs-qe),abs(oStart-oEnd))))
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
/** Look for a good match for softEnd by looking an edge in
    orthoAg that overlaps well with hardStart->softEnd in orthologous
    genome using chain to check for overlap. */
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
	int oStart = orthoPos[vMap[hardStart]];
	int oEnd = orthoPos[i];
	int overlap = 0;
	if(reverse)
	    overlap = rangeIntersection(qs, qe, oEnd, oStart);
	else
	    overlap = rangeIntersection(qs, qe, oStart, oEnd);
	if(overlap > bestOverlap  && (overlap < 2*min(abs(qs-qe),abs(oStart-oEnd))))
	    {
	    overlap = bestOverlap;
	    bestVertex = i;
	    }
	}
    }
chainFree(&toFree);
return bestVertex;
}
	
struct altGraphX *commonAgInit(struct altGraphX *ag)
/** Make a clone of ag with same arrays but edgeCount = 0 even
    though memory allocated. Free with freeCommonAg().*/
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
errAbort("orthoSplice::getEdgeNum() - Didn't find edge with vertices %d-%d in %s.", v1, v2, ag->name);
return -1;
}

bool isExon(struct altGraphX *ag, int v1, int v2)
/** Return TRUE if edge defined by v1->v2 is an exon. */
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
/** Look through vMap, return FALSE if val is already in vMap, TRUE otherwise. */
{
int i;
for(i=0; i<vCount; i++)
    {
    if(vMap[i] == val)
	return FALSE;
    }
return TRUE;
}

boolean currentAssignment(int *vMap, int vCount, int val)
/** Look through vMap returning index that val is found at or -1 if not found. */
{
int i;
for(i=0; i<vCount; i++)
    {
    if(vMap[i] == val)
	return i;
    }
return -1;
}

boolean moreEvidence(struct altGraphX *ag, bool **em, int vertex1, int vertex2)
/** Return TRUE if vertex1 has more(or equal) evidence associated with it than vertex2. */
{
int i=0;
int vC = ag->vertexCount;
int vertex1Count = 0, vertex2Count =0;
int edge = 0;
struct evidence *ev = NULL, *evList = ag->evidence;
for(i=0; i<vC; i++)
    {
    if(em[i][vertex1])
	{
	ev = slElementFromIx(evList, altGraphXGetEdgeNum(ag,i,vertex1));
	vertex1Count+=ev->evCount;
	}
    if(em[vertex1][i])
	{
	ev = slElementFromIx(evList, altGraphXGetEdgeNum(ag,vertex1,i));
	vertex1Count+=ev->evCount;
	}
    if(em[i][vertex2])
	{
	ev = slElementFromIx(evList, altGraphXGetEdgeNum(ag,i,vertex2));
	vertex2Count+=ev->evCount;
	}
    if(em[vertex2][i])
	{
	ev = slElementFromIx(evList, altGraphXGetEdgeNum(ag,vertex2,i));
	vertex2Count+=ev->evCount;
	}
    }
return (vertex1Count > vertex2Count);
}



void findSoftStartsEnds(struct altGraphX *ag, bool **em, struct altGraphX *orthoAg, bool **orthoEm,
			struct chain *chain, int *vMap, int *orthoAgIx, int vertexCount, int oIndex,
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
	    int orthoV = -1;
	    int currentMap = -1;
	    /* If there is an edge with one vertex already found. */
	    if( vTypes[i] == ggSoftStart && em[i][j] && vMap[j] != -1 && isExon(ag, i, j) && orthoAgIx[j] == oIndex)
		{
		orthoV = findBestOrthoStartByOverlap(ag, em, orthoAg, orthoEm, chain, vMap, i, j, reverse);
		}
	    else if(vTypes[i] == ggSoftEnd && em[j][i] && vMap[j] != -1 && isExon(ag, j, i) && orthoAgIx[j] == oIndex)
		{
		orthoV = findBestOrthoEndByOverlap(ag, em, orthoAg, orthoEm, chain, vMap, j, i, reverse);
		}
	    /* Try to see if there is a consensus mapping with the most evidence, don't
	       change a hard vertex though. */
	    currentMap = currentAssignment(vMap, vCount, orthoV);
	    if(currentMap == -1 || (!isHardVertex(ag,currentMap) && moreEvidence(ag, em,currentMap, i)))
		{
		vMap[i] = orthoV;
		orthoAgIx[i] = oIndex;
		}
	    orthoV = -1;
	    }
	}
    }
}

void fillInEdge(struct orthoSpliceEdge *se, struct orthoAgReport *agRep , 
		struct altGraphX *ag, bool **agEm,
		struct altGraphX *commonAg, bool **commonEm, int v1, int v2, 
		int *edgeSeen, int *cEdgeSeen, int *vMap)
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
se->orthoV1 = vMap[v1];
se->orthoV2 = vMap[v2];
if(ag->vTypes[v1] == ggSoftStart || ag->vTypes[v1] == ggSoftEnd)
    se->soft = TRUE;
if(ag->vTypes[v2] == ggSoftStart || ag->vTypes[v2] == ggSoftEnd)
    se->soft = TRUE;
edgeNum = getEdgeNum(ag, v1, v2);
e = slElementFromIx(ag->evidence, edgeNum);
se->conf = e->evCount;

/* Check edge in ag for alt-splicing, by checking to se if this edge
 * has been connected to any other vertices. */
if(agEm[v1][v2])
    {
    eCount =0;
    for(k=v2; k<vCount; k++)
	{
	if(agEm[v1][k] && (ag->vTypes[k] == ggHardStart || ag->vTypes[k] == ggHardEnd || se->soft))
	    eCount++;
	}
    if(eCount > 1)
	se->alt = TRUE;
    }
/* Check edge in common subgraph for alt-splicing, by checking to se
 * if this edge has been connected to any other vertices. */
if(commonEm[v1][v2])
    {
    eCount=0;
    for(k=v2; k<vCount; k++)
	{
	if(commonEm[v1][k] && (commonAg->vTypes[k] == ggHardStart || commonAg->vTypes[k] == ggHardEnd || se->soft))
	    eCount++;
	}
    if(eCount > 1)
	se->altCons = TRUE;
    }
}

void fillInReport(struct orthoAgReport *agRep, struct altGraphX *ag, bool **agEm,
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
int *edgeSeen = NULL, *cEdgeSeen = NULL; /* Used to keep track of alt
					  * splicing. If a vertex is
					  * connected to more than one
					  * thing then it is
					  * alt-splicing. */
struct orthoSpliceEdge *seList = NULL, *se = NULL;
AllocArray(edgeSeen, vCount);
AllocArray(cEdgeSeen, vCount);
agRep->ssSoftMissing = agRep->ssSoftFound = 0;
agRep->ssHardMissing = agRep->ssHardFound = 0;
agRep->numVertexes = ag->vertexCount;
agRep->vertexMap = vMap;
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
	    fillInEdge(se, agRep, ag, agEm, commonAg, commonEm, i,j, edgeSeen, cEdgeSeen, vMap);
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

struct altGraphX *agxForCoordinates(char *chrom, int chromStart, int chromEnd, char strand, 
				    char *altTable, struct altGraphX **toFree)
{
struct slRef *refList=NULL, *ref = NULL;
struct altGraphX *ag = NULL, *agList = NULL;
struct altGraphX *range = NULL;

if(orthoAgxTree == NULL)
    {
    char query[256];
    struct sqlConnection *orthoConn = NULL; 
    orthoConn = hAllocConn2();
    safef(query, sizeof(query), "select * from %s where tName='%s' and tStart<%d and tEnd>%d and strand like '%s'",
	  altTable, chrom, chromEnd, chromStart, strand);
    agList = altGraphXLoadByQuery(orthoConn, query);
    hFreeConn2(&orthoConn);
    *toFree = agList;
    return agList;
    }
else 
    {
    AllocVar(range);
    range->tName = chrom;
    range->tStart = chromStart;
    range->tEnd = chromEnd;
    safef(range->strand, sizeof(range->strand), "%c", strand);
    refList = rbTreeItemsInRange(orthoAgxTree, range, range);
    for(ref = refList; ref != NULL; ref = ref->next)
	{
	slSafeAddHead(&agList, ((struct slList *)ref->val));
	}
    slReverse(&agList);
    freez(&range);
    (*toFree) = NULL;
    return agList;
    }
return NULL;
}

struct altGraphX *makeCommonAltGraphX(struct altGraphX *ag, struct chain *chain)
/** For each edge in ag see if there is a similar edge in the
    orthologus altGraphX structure and output it if there. */
{
bool **agEm = NULL;
bool **orthoEm = NULL;
bool **commonEm = NULL;
bool ***orthoEmP = NULL;
struct altGraphX *commonAg = NULL, *orthoAgList = NULL, 
    *oAg = NULL, *agxToFree = NULL;
struct orthoAgReport *agRep = NULL;
struct orthoSpliceEdge *seList = NULL;
char *strand = NULL;
int *vertexMap = NULL; /* Map of ag's vertexes to agOrthos's vertexes. */
int *orthoAgIx = NULL; /* Map of which orthoAg the vertexes in vertexMap belong to. */
struct chain *subChain = NULL, *toFree = NULL;
int i=0,j=0;
int vCount = ag->vertexCount;
int qs = 0, qe = 0;
bool match = FALSE;
bool reverse = FALSE;

/* First find the orthologous splicing graph. */
chainSubsetOnT(chain, ag->tStart, ag->tEnd, &subChain, &toFree);    
if(subChain == NULL)
    return NULL;

qChainRangePlusStrand(subChain, &qs, &qe);
if ((subChain->qStrand == '-'))
    reverse = TRUE;
if(reverse)
    { 
    if(ag->strand[0] == '+')
	strand = "-";
    else
	strand = "+";
    }
else
    strand = ag->strand;
    
orthoAgList = agxForCoordinates(subChain->qName, qs, qe, strand[0], altTable, &agxToFree);

chainFreeList(&toFree);
if(orthoAgList == NULL)
    return NULL;

/* Got at least one orhtologous graph. Do some analysis. */
AllocVar(agRep);
/* Init splice site array (vertexMap) to -1 as inidicative of not
   being found. */
AllocArray((orthoAgIx), ag->vertexCount);
AllocArray((vertexMap), ag->vertexCount);
AllocArray((orthoEmP), slCount(orthoAgList));
for(i=0;i<ag->vertexCount; i++)
    (vertexMap)[i] = -1;

agRep->agName = cloneString(ag->name);
agRep->orthoAgName = cloneString(orthoAgList->name);
agRep->chrom = cloneString(ag->tName);
/* Found at least one orhtologous graph, determine mapping between edges. */
seList = altGraphXToOSEdges(ag);
for(oAg = orthoAgList; oAg != NULL; oAg = oAg->next)
    {
    int oIndex = slIxFromElement(orthoAgList, oAg);
    makeVertexMap(ag, oAg, chain, vertexMap, orthoAgIx, vCount, oIndex, seList, agRep, reverse);
    }
agEm = altGraphXCreateEdgeMatrix(ag);

commonAg = commonAgInit(ag);
for(oAg = orthoAgList; oAg != NULL; oAg = oAg->next)
    {
    int oIndex = slIxFromElement(orthoAgList, oAg);
    orthoEm = altGraphXCreateEdgeMatrix(oAg);    
    orthoEmP[oIndex] = orthoEm;
    findSoftStartsEnds(ag, agEm, oAg, orthoEm, chain, vertexMap, orthoAgIx, vCount, oIndex, agRep, reverse);
    }
for(i=0;i<vCount; i++) 
    {
    for(j=0; j<vCount; j++)
	{
	if(agEm[i][j])
	    {
	    struct evidence *oldEv = NULL;
	    oldEv = slElementFromIx(ag->evidence, getEdgeNum(ag, i, j));
	    /* Have to look at the splice graph differently if we're on 
	       the chain is on the '-' strand. */
	    if(vertexMap[i] != -1 && vertexMap[j] != -1 && (orthoAgIx[i] == orthoAgIx[j]))
		{
		bool **oEm = orthoEmP[orthoAgIx[i]];
		if(reverse) { match = oEm[vertexMap[j]][vertexMap[i]]; }
		else 	  {  match = oEm[vertexMap[i]][vertexMap[j]]; }
		}
	    else
		match = FALSE;
	    if(match || (oldEv->evCount >= trumpNum && isHardVertex(ag,i) && isHardVertex(ag,j)))
		{
		struct evidence *commonEv = NULL;
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
fillInReport(agRep, ag, agEm, commonAg, commonEm, vertexMap);
outputReport(agRep);
orthoAgReportFree(&agRep);
altGraphXFreeEdgeMatrix(&agEm, ag->vertexCount);
altGraphXFreeEdgeMatrix(&commonEm, commonAg->vertexCount);
for(i=0, oAg=orthoAgList; i<slCount(orthoAgList); i++, oAg=oAg->next)
 altGraphXFreeEdgeMatrix(&orthoEmP[i], oAg->vertexCount);
if(commonAg->edgeCount == 0)
    freeCommonAg(&commonAg);
freez(&vertexMap);
altGraphXFreeList(&agxToFree);
slFreeList(&seList);
return commonAg;
}

struct altGraphX *findOrthoAltGraphX(struct altGraphX *ag, char *db,
				     char *netTable, char *chainFile)
/** Find an orthologous altGraphX record in orthoDb for ag in db if 
    it exists. Otherwise return NULL. */
{
struct altGraphX *orthoAg = NULL;
struct chain *chain = NULL;
struct chain *subChain = NULL, *toFree = NULL;
struct orthoSpliceEdge *seList = NULL, *se = NULL;
int i;
int qs = 0, qe = 0;
int *starts = NULL, *sizes = NULL;
int blockCount =0;
/* Find the best chain (one that overlaps the most exons. */
AllocArray(starts, ag->edgeCount);
AllocArray(sizes, ag->edgeCount);
for(i=0; i<ag->edgeCount; i++)
    {
    if(getSpliceEdgeType(ag, i) == ggExon)
	{
	starts[blockCount] = ag->vPositions[ag->edgeStarts[i]];
	sizes[blockCount] = ag->vPositions[ag->edgeEnds[i]] - ag->vPositions[ag->edgeStarts[i]];
	blockCount++;
	}
    }
chain = chainForBlocks(db, netTable, ag->tName, ag->tStart, ag->tEnd,
		       starts, sizes, blockCount);
freez(&starts);
freez(&sizes);
if(chain == NULL)
    return NULL;
seList = altGraphXToOSEdges(ag);
orthoAg = makeCommonAltGraphX(ag, chain);
slFreeList(&seList);
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
char *chainFile = optionVal("chainFile", NULL);
char *commonFileName = optionVal("commonFile", NULL);
char *reportFileName = optionVal("reportFile", NULL);
char *edgeFileName = optionVal("edgeFile", NULL);
FILE *cFile = NULL;
char *netFile = optionVal("netFile", NULL);
char *chrom = optionVal("chrom", NULL);
char *orthoAgxFile = optionVal("orthoAgxFile", NULL);
int foundOrtho=0, noOrtho=0;

trumpNum = optionInt("trumpNum", BIGNUM); 
if(altIn == NULL)
    errAbort("orthoSplice - must specify altInFile.");
if(netTable == NULL && netFile == NULL)
    errAbort("orthoSplice - must specify netTable or netFile.");
if(chainFile == NULL)
    errAbort("orthoSplice - must specify chainFile.");
if(commonFileName == NULL)
    errAbort("orthoSplice - must specify commonFile.");
if(reportFileName == NULL)
    errAbort("orthoSplice - must specify reportFile.");
if(edgeFileName == NULL)
    errAbort("orthoSplice - must specify edgeFile.");
if(chrom == NULL)
    errAbort("Must specify -chrom");

workingChrom = chrom;
if(netFile != NULL)
    {
    warn("Loading net info from file %s", netFile);
    netTree = rbTreeFromNetFile(netFile);
    }
if(orthoAgxFile != NULL)
    {
    warn("Loading orthoAltGraphX records from file", orthoAgxFile);
    orthoAgxTree = rbTreeFromAgxFile(orthoAgxFile);
    }
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
	agOrtho = findOrthoAltGraphX(ag, db,  netTable, chainFile);
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
    errAbort("orthoSplice - Must set db and orhtoDb. Try -help for usage.");
hSetDb(db);
hSetDb2(orthoDb);
orthoSplice(db, orthoDb);
return 0;
}
