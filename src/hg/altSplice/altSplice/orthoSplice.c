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

static char const rcsid[] = "$Id: orthoSplice.c,v 1.1 2003/05/22 18:52:21 sugnet Exp $";

struct orthoAgReport
/* Some summary information on comparison of splice graphs. */
{
    struct orthoAgReport *next;  /**< Next in list. */
    int exonCount;               /**< Total number of exons in ag. */
    int exonsFound;              /**< Exons found in orthologous genome. */
    int exonsMissing;            /**< Exons not found on orthologous genome. */
    int ssFound;                 /**< Number of splice sites in ag that are splice sites in orhtoAg. */
    int ssMissing;               /**< Number of splice sites in ag that are not splice sites in orhtoAg. */
    int ssDoubles;               /**< Number of splice sites in ag that map to two places in orhtoAg (hopefully rare). */
    int ssVeryClose;             /**< How many times were we off by one. */
    char agName[16];              /**< AltGraphX from first genome name. Allocated at run time to length of name. */
    char orthoAgName[16];          /**< Orthologous altGraphX name. Allocated at run time to length of name. */
};

struct spliceEdge 
/* Structure to hold information about one edge in 
   a splicing graph. */
{
    struct spliceEdge *next;    /* Next in list. */
    enum ggEdgeType type;       /* Type of edge: ggExon, ggIntron, ggSJ, ggCassette. */
    int start;                  /* Chrom start. */
    int end;                    /* End. */
    int v1;                     /* Vertex 1 in graph. */
    int v2;                     /* Vertex 2 in graph. */
    int itemNumber;             /* Number of altGraphX record derived from in list. */
    int row;                    /* Row that exon is stored in. */
    double conf;                /* Confidence. */
};

static boolean doHappyDots;   /* output activity dots? */
static char *altTable = "altGraphX";  /* Name of table to load splice graphs from. */
static struct orthoAgReport *agReportList = NULL; /* List of reports. */

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"orthoDb", OPTION_STRING},
    {"altInFile", OPTION_STRING},
    {"netTable", OPTION_STRING},
    {"chainTable", OPTION_STRING},
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
    "Prefix of datbase table containing chain records, i.e. mouseChain."
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
	 "orthoSplice -altInFile=ranbp1.tab -db=hg15 -orthoDb=mm3 -netTable=mouseNet -chainTable=mouseChain\n");
}

struct orthoAgReport *newOrthoAgReport(char *agName, char *orthoAgName)
/* Allocate an orthoAgReport as one big lump of memory and set the names. */
{
struct orthoAgReport *agRep = NULL;
AllocVar(agRep);
snprintf(agRep->agName, sizeof(agRep->agName), "%s",  agName);
agRep->agName[sizeof(agRep->agName) -1] = '\0';
snprintf(agRep->orthoAgName, sizeof(agRep->orthoAgName), "%s",  orthoAgName);
agRep->orthoAgName[sizeof(agRep->orthoAgName) -1] = '\0';
return agRep;
}

void orthoAgReportHeader(FILE *out)
/** Output a header for the orthoAgReport fields. */
{
fprintf(out, "#%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", "agName", "orhtoAgName", "exonCount",
	"exonsFound", "exonsMissing", "ssFound", "ssMissing", "ssDoubles", "ssVeryClose");
}

void orthoAgReportTabOut(FILE *out, struct orthoAgReport *r)
/** Write out a one line summary of a report. */
{
fprintf(out, "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", r->agName, r->orthoAgName, r->exonCount,
	r->exonsFound, r->exonsMissing, r->ssFound, r->ssMissing, r->ssDoubles, r->ssVeryClose);
}


enum ggEdgeType getSpliceEdgeType(struct altGraphX *ag, int edge)
/* Return edge type. */
{
if( (ag->vTypes[ag->edgeStarts[edge]] == ggHardStart || ag->vTypes[ag->edgeStarts[edge]] == ggSoftStart)  
    && (ag->vTypes[ag->edgeEnds[edge]] == ggHardEnd || ag->vTypes[ag->edgeEnds[edge]] == ggSoftEnd)) 
    return ggExon;
else if( (ag->vTypes[ag->edgeStarts[edge]] == ggHardEnd || ag->vTypes[ag->edgeStarts[edge]] == ggSoftEnd)  
	 && (ag->vTypes[ag->edgeEnds[edge]] == ggHardStart || ag->vTypes[ag->edgeEnds[edge]] == ggSoftStart)) 
    return ggSJ;
else
    return ggIntron;
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
    e->start = vPos[starts[i]];
    e->end = vPos[ends[i]];
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
chain = chainDbLoad(conn, chainTable, chrom, net->fillList->chainId);
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
    *retQs = chain->qStart+1;
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
    agRep->ssFound++;
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

struct altGraphX *makeCommonAltGraphX(struct altGraphX *ag, struct chain *chain)
/** For each edge in ag see if there is a similar edge in the
    orthologus altGraphX structure and output it if there. */
{
bool **em = NULL;
bool **orthoEm = NULL;
struct altGraphX *commonAg = NULL, *orthoAg = NULL;
struct orthoAgReport *agRep = NULL;
struct spliceEdge *se = NULL, *seList = NULL;
int *vertexMap = NULL; /* Map of ag's vertexes to agOrthos's vertexes. */
struct chain *subChain = NULL, *toFree = NULL;
int qs = 0, qe = 0;
int i=0;
int vCount = ag->vertexCount;
char query[256];
struct sqlConnection *orthoConn = hAllocConn2();
/* First find the orthologous splicing graph. */
chainSubsetOnT(chain, ag->tStart, ag->tEnd, &subChain, &toFree);    
qChainRangePlusStrand(subChain, &qs, &qe);
safef(query, sizeof(query), "select * from %s where tName='%s' and tStart<%d and tEnd>%d",
      altTable, subChain->qName, qe, qs);
orthoAg = altGraphXLoadByQuery(orthoConn, query);
hFreeConn2(&orthoConn);
chainFreeList(&toFree);
if(orthoAg == NULL)
    return NULL;

/* Init splice site array (vertexMap) to -1 as inidicative of not
   being found. */
AllocArray(vertexMap, vCount);
for(i=0;i<vCount; i++)
    vertexMap[i] = -1;
agRep = newOrthoAgReport(ag->name, orthoAg->name);

/* Found an orhtologous graph, determine mapping between edges. */
seList = altGraphXToEdges(ag);
for(se = seList; se != NULL; se = se->next)
    {
    if(se->type != ggExon && se->type != ggCassette)
	continue;
    chainSubsetOnT(chain, se->start, se->end, &subChain, &toFree);    
    agRep->exonCount++;
    if(subChain != NULL)
	{
	int v1 =-1, v2 = -1;
	agRep->exonsFound++;
	qChainRangePlusStrand(subChain, &qs, &qe);
	v1 = vertexForPosition(orthoAg, qs, agRep);
	if(subChain->qStrand == '-')
	    assignVertex(vertexMap, vCount, se->v2, v1, agRep);
	else
	    assignVertex(vertexMap, vCount, se->v1, v1, agRep);
	v2 = vertexForPosition(orthoAg, qe, agRep);
	if(subChain->qStrand == '-')
	    assignVertex(vertexMap, vCount, se->v1, v2, agRep);
	else
	    assignVertex(vertexMap, vCount, se->v2, v2, agRep);
	}
    else
	agRep->exonsMissing++;
    chainFreeList(&toFree);
    }
fprintf(stdout, "track name=\"ss\" description=\"splice sites\"\n");
for(i=0; i<vCount; i++)
    {
    if(vertexMap[i] != -1)
	{
	fprintf(stdout, "%s\t%d\t%d\t%d-%d\n", orthoAg->tName, orthoAg->vPositions[vertexMap[i]], 
		orthoAg->vPositions[vertexMap[i]] +1, i, vertexMap[i]);
	}
    else
	agRep->ssMissing++;
    }
warn("%d exons: %d found, %d missing.", agRep->exonCount, agRep->exonsFound, agRep->exonsMissing);
warn("%d splice sites: %d found, %d missing, %d doubles, %d very close (+/- 1)",
     vCount, agRep->ssFound, agRep->ssMissing, agRep->ssDoubles, agRep->ssVeryClose);
freez(&vertexMap);
slFreeList(&seList);
slAddHead(&agReportList, agRep);
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

seList = altGraphXToEdges(ag);
orthoAg = makeCommonAltGraphX(ag, chain);
fprintf(stdout, "track name=\"exons\" description=\"human exons mapped.\"\n");
for(se = seList; se != NULL; se = se->next)
    {
    if(se->type != ggExon)
	continue;
    chainSubsetOnT(chain, se->start, se->end, &subChain, &toFree);    

    if(subChain != NULL)
	{
	qChainRangePlusStrand(subChain, &qs, &qe);

	fprintf(stdout, "%s\t%d\t%d\t%d-%d\n",
		subChain->qName, qs, qe, se->v1, se->v2);
	}
    else
	warn("Yikes got a NULL for %s:%d-%d", ag->tName, se->start, se->end);
    chainFree(&toFree);
    }
hFreeConn(&conn);
slFreeList(&seList);
chainFreeList(&chain);
return orthoAg;
}

void orthoSplice(char *db, char *orthoDb)
/** Open the altFile for db1 and examine the ortholog in db2 for 
    each entry. */
{
struct lineFile *lf = NULL;
char *altIn = optionVal("altInFile", NULL);
struct sqlConnection *conn = hAllocConn();
char **row = NULL;
int rowCount = 0;
char *line = NULL;
struct altGraphX *ag = NULL, *agOrtho = NULL;
char *netTable = optionVal("netTable", NULL);
char *chainTable = optionVal("chainTable", NULL);
if(altIn == NULL)
    errAbort("orthoSplice - must specify altInFile.");
if(netTable == NULL)
    errAbort("orthoSplice - must specify netTable.");
if(chainTable == NULL)
    errAbort("orthoSplice - must specify chainTable.");
/* Want to read in and process each altGraphX record individually
   to save memory, so figure out how many columns in each row we have currently and
   load them one by one. */
lf = lineFileOpen(altIn, TRUE);
lineFileNextReal(lf, &line);
lineFileReuse(lf);
rowCount = countChars(line, '\t') +1;
AllocArray(row, rowCount);
warn("Reading records from %s", altIn);
while(lineFileNextCharRow(lf, '\t', row, rowCount))
    {
    ag = altGraphXLoad(row);
    agOrtho = findOrthoAltGraphX(ag, db,  netTable, chainTable);
    occassionalDot();
    altGraphXFree(&ag);
    }
warn("\nDone.");
freez(&row);
lineFileClose(&lf);
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
