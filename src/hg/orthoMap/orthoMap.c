/** orthoMap - Map cDNAs from one genome to another. */
#include "common.h"
#include "hdb.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"
#include "axt.h"
#include "axtInfo.h"
#include "chainDb.h"
#include "chainNetDbLoad.h"
#include "altGraphX.h"
#include "psl.h"
#include "bed.h"
#include "rbTree.h"

static char const rcsid[] = "$Id: orthoMap.c,v 1.6 2003/08/10 06:37:55 sugnet Exp $";
static boolean doHappyDots;   /* output activity dots? */
static struct rbTree *netTree = NULL;
static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"orthoDb", OPTION_STRING},
    {"chrom", OPTION_STRING},
    {"pslFile", OPTION_STRING},
    {"pslTable", OPTION_STRING},
    {"altGraphXFile", OPTION_STRING},
    {"altGraphXTable", OPTION_STRING},
    {"netTable", OPTION_STRING},
    {"netFile", OPTION_STRING},
    {"chainFile", OPTION_STRING},
    {"outBed", OPTION_STRING},
    {"altGraphXOut", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Database (i.e. hg15) that altInFile comes from.",
    "Database (i.e. mm3) to look for orthologous splicing in.",
    "Chromosme in db that we are working on.",
    "File containing psl alignments.",
    "Table containing psl alignments.",
    "File containing altGraphX records.",
    "Table containing altGraphX records.",
    "Datbase table containing net records, i.e. mouseNet.",
    "File table containing net records, i.e. chr16.net.",
    "File containing the chains. Usually I do this on a chromosome basis.",
    "File to output beds to.",
    "File to output altGraphX records to."
};

void usage()
/** Print usage and quit. */
{
int i=0;
char *version = cloneString((char*)rcsid);
char *tmp = strstr(version, "orthoMap.c,v ");
if(tmp != NULL)
    version = tmp + 13;
tmp = strrchr(version, 'E');
if(tmp != NULL)
    (*tmp) = '\0';
warn("orthoMap - Map psl alignments from one organism to another.\n"
     "   (version: %s)", version );
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n"
	 "   orthoMap -chrom=chrX -db=mm3 -orthoDb=hg13 -netTable=humanNet \\\n"
	 "     -chainFile=mm3.hg13.chains/chrX.chain -pslTable=mrna -outBed=mm3.hg13.orthoMrna.bed");
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
if(id == 0)
    return NULL;
if(chainHash == NULL)
    {
    char *chainFile = optionVal("chainFile", NULL);
    if(chainFile == NULL)
	errAbort("orthoSplice::chainFromId() - Can't find file for 'chainFile' parameter");
    chainHash = allChainsHash(chainFile);
    }
safef(key, sizeof(key), "%d", id);
chain =  hashFindVal(chainHash, key);
if(chain == NULL && id != 0)
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

boolean checkChain(struct chain *chain, int start, int end)
/* Return true if chain covers start, end, false otherwise. */
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


void chainNetGetRange(char *db, char *netTable, char *chrom,
		      int start, int end, char *extraWhere, struct cnFill **fill,
		      struct chainNet **toFree)
{
struct cnFill *searchHi=NULL, *searchLo = NULL;
struct slRef *refList = NULL, *ref = NULL;
(*fill) = NULL;
(*toFree) = NULL;
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

struct chain *chainForBlocks(struct sqlConnection *conn, char *db, char *netTable, 
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

struct chain *chainForPosition(struct sqlConnection *conn, char *db, char *netTable, 
			       char *chrom, int start, int end)
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
chainNetGetRange(db, netTable, chrom,
		 start, end, NULL, &fill, &net);
if(fill != NULL)
    chain = lookForChain(fill, start, end);
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

void fillInBed(struct chain *chain, struct psl *psl, struct bed **pBed)
/** Fill in a bed structure with initial information for psl. */
{
struct bed *bed = NULL;
int qs, qe;
struct chain *subChain=NULL, *toFree=NULL;
AllocVar(bed);
chainSubsetOnT(chain, psl->tStart, psl->tEnd , &subChain, &toFree);    
if(subChain == NULL)
    {
    *pBed = NULL;
    return;
    }
qChainRangePlusStrand(subChain, &qs, &qe);
bed->chrom = cloneString(subChain->qName);
bed->name = cloneString(psl->qName);
bed->chromStart = bed->thickStart = qs;
bed->chromEnd = bed->thickEnd = qe;
bed->score = 1000 - 2*pslCalcMilliBad(psl, TRUE);
AllocArray(bed->chromStarts, psl->blockCount);
AllocArray(bed->blockSizes, psl->blockCount);
if (bed->score < 0) bed->score = 0;
strncpy(bed->strand,  psl->strand, sizeof(bed->strand));
chainFree(&toFree);
*pBed = bed;
}

void addExonToBed(struct chain *chain, struct psl *psl, struct bed *bed, int block)
/** Converte block in psl to block in orthologous genome for bed using chain. */
{
struct chain *subChain=NULL, *toFree=NULL;
int qs, qe;
int end = (psl->tStarts[block]+psl->blockSizes[block]);
chainSubsetOnT(chain, psl->tStarts[block], end , &subChain, &toFree);    
if(subChain == NULL)
    return;
qChainRangePlusStrand(subChain, &qs, &qe);
bed->chromStarts[bed->blockCount] = qs - bed->chromStart;
bed->blockSizes[bed->blockCount] = abs(qe-qs);
bed->blockCount++;
chainFree(&toFree);
}

struct bed *orthoBedFromPsl(struct sqlConnection *conn, char *db, char *orthoDb,
			    char *netTable, struct psl *psl)
/** Produce a bed on the orthologous genome from the original psl. */
{
struct bed *bed = NULL;
int i;
struct chain *chain = chainForBlocks(conn, db, netTable, 
				     psl->tName, psl->tStart, psl->tEnd,
				     psl->tStarts, psl->blockSizes, psl->blockCount);
int diff = 0;
if(chain == NULL)
    return NULL;
fillInBed(chain, psl, &bed);
if(bed == NULL)
    return NULL;
if(chain->qStrand == '+')
    {
    for(i=0; i<psl->blockCount; i++)
	{
	addExonToBed(chain, psl, bed, i);
	}
    }
else
    {
    for(i=psl->blockCount-1; i>=0; i--)
	{
	addExonToBed(chain, psl, bed, i);
	}
    }
if(bed->blockCount > 0 && bed->chromStarts[0] != 0)
    diff = bed->chromStarts[0];
for(i=0;i<bed->blockCount; i++)
    bed->chromStarts[i] -=  diff;
bed->chromStart += diff;
bed->thickStart = bed->chromStart;
bed->chromEnd = bed->thickEnd = bed->chromStart+bed->chromStarts[bed->blockCount-1]+bed->blockSizes[bed->blockCount-1];
return bed;
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 100;
static int dot = 100;
if (doHappyDots && (--dot <= 0))
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

struct psl *loadPslFromTable(struct sqlConnection *conn, char *table,
			     char *chrom, int chromStart, int chromEnd)
/** Load all of the psls between chromstart and chromEnd */
{
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = -100;
struct psl *pslList = NULL;
struct psl *psl = NULL;
int i=0;
sr = hRangeQuery(conn, table, chrom, chromStart, chromEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    slSafeAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
return pslList;
}

void altGraphXReverse(struct altGraphX *ag)
{
int vCount = ag->vertexCount;
int *vPos = ag->vPositions;
int *tmp = NULL;
unsigned char *vTypes = ag->vTypes;
int i;
for(i=0; i<vCount; i++)
    {
    if(ag->vTypes[i] == ggHardEnd)
	ag->vTypes[i] = ggHardStart;
    else if(ag->vTypes[i] ==ggHardStart)
	ag->vTypes[i] = ggHardEnd;
    else if(ag->vTypes[i] == ggSoftEnd)
	ag->vTypes[i] = ggSoftStart;
    else if(ag->vTypes[i] == ggSoftStart)
	ag->vTypes[i] = ggSoftEnd;
    }
tmp = ag->edgeEnds;
ag->edgeEnds = ag->edgeStarts;
ag->edgeStarts = tmp;
if(sameString(ag->strand, "+"))
    snprintf(ag->strand, sizeof(ag->strand), "%s", "-");
else if(sameString(ag->strand, "-"))
    snprintf(ag->strand, sizeof(ag->strand), "%s", "+");
else
    errAbort("orthoMap::altGraphXReverse() - Don't recognize strand: %s", ag->strand);
}


struct altGraphX *mapAltGraphX(struct altGraphX *ag, struct sqlConnection *conn,
			       char *db, char *netTable )
/* Map one altGraphX record. Return NULL if can't find. */
{
struct altGraphX *agNew = NULL;
struct chain *chain = chainForPosition(conn, db, netTable, ag->tName, ag->tStart, ag->tEnd);
struct chain *workingChain = NULL, *workingChainFree = NULL;
struct chain *subChain = NULL, *toFree = NULL;
int i,j,k;
int edgeCountNew =0;
int vCountNew=0;
bool reverse = FALSE;
if(chain == NULL)
    return NULL;
/* Make a smaller chain to work on... */
chainSubsetOnT(chain, ag->tStart-1, ag->tEnd+1, &workingChain, &workingChainFree);
if(workingChain == NULL)
    return NULL;
if ((chain->qStrand == '-'))
    reverse = TRUE;
agNew = altGraphXClone(ag);
freez(&agNew->tName);
agNew->tName = cloneString(chain->qName);
/* Map vertex positions using chain. */
for(i = 0; i < agNew->vertexCount; i++)
    {
    struct boxIn *bi = NULL;
    int targetPos = agNew->vPositions[i];
    struct chain *subChain=NULL, *toFree=NULL;
    agNew->vPositions[i] = -1;
    chainSubsetOnT(workingChain, targetPos , targetPos, &subChain, &toFree);    
    if(subChain != NULL)
	{
	int qs, qe;
	qChainRangePlusStrand(subChain, &qs, &qe);
	agNew->vPositions[i] = qs;
	}
    chainFree(&toFree);
    }
/* Prune out edges not found. */

/* Set up to remember how many edges we have and our start and stop. */
edgeCountNew = agNew->edgeCount;
vCountNew = agNew->vertexCount;
agNew->tStart = BIGNUM;
agNew->tEnd = 0;
for(i=0; i<agNew->vertexCount && i>= 0; i++)
    {
    struct evidence *ev = NULL;
    if(agNew->vPositions[i] == -1)
	{
	/* Adjust positions, overwriting one that isn't found. */
	vCountNew--;
	for(j=i; j<agNew->vertexCount-1; j++)
	    {
	    agNew->vPositions[j] = agNew->vPositions[j+1];
	    agNew->vTypes[j] = agNew->vTypes[j+1];
	    }
	/* Remove edges associated with this vertex. */
	for(j=0; j<agNew->edgeCount && j>=0; j++)
	    {
	    if(agNew->edgeStarts[j] == i || agNew->edgeEnds[j] == i)
		{
		edgeCountNew--;
		/* Remove evidence. */
		ev = slElementFromIx(agNew->evidence, j);
		slRemoveEl(&agNew->evidence, ev);
		for(k=j; k<agNew->edgeCount -1; k++)
		    {
		    agNew->edgeStarts[k] = agNew->edgeStarts[k+1];
		    agNew->edgeEnds[k] = agNew->edgeEnds[k+1];
		    agNew->edgeTypes[k] = agNew->edgeTypes[k+1];
		    }
		j--;
		agNew->edgeCount--;
		}
	    }
	/* Subtract off one vertex from all the others. */
	for(j=0; j<agNew->edgeCount; j++)
	    {
	    if(agNew->edgeStarts[j] > i)
		agNew->edgeStarts[j]--; 
	    if(agNew->edgeEnds[j] > i)
		agNew->edgeEnds[j]--; 
	    }
	i--;
	agNew->vertexCount--;
	}
    /* Else if vertex found set agNew start and ends. */
    else
	{
	agNew->tStart = min(agNew->vPositions[i], agNew->tStart);
	agNew->tEnd = max(agNew->vPositions[i], agNew->tEnd);
	}
    }
/* Not going to worry about mRNAs that aren't used anymore. Leave them in
   for now. */
agNew->vertexCount = vCountNew;
agNew->edgeCount = edgeCountNew;
if(agNew->vertexCount == 0 || agNew->edgeCount == 0)
    {
    altGraphXFree(&agNew);
    return NULL;
    }
for(i=0; i<agNew->edgeCount; i++)
    {
    if(agNew->edgeStarts[i] >= agNew->vertexCount ||
       agNew->edgeEnds[i] >= agNew->vertexCount)
	{
	warn("For %s vertexes occur at %d when in reality there are only %d vertices.",
	     agNew->name, max(agNew->edgeStarts[i], agNew->edgeEnds[i]), agNew->vertexCount);
	}
    }
/* If it is on the other strand reverse it. */
if(reverse)
    {
    altGraphXReverse(agNew);
    }
chainFree(&workingChainFree);
return agNew;
}

void mapAltGraphXFile(struct sqlConnection *conn, char *db, char *orthoDb, char *chrom,
		      char *netTable, char *altGraphXFileName, char *altGraphXTableName,
		      FILE *agxOut, int *foundCount, int *notFoundCount)
/* Map over altGraphX Structures from one organism to
another. Basically create a mapping for the vertices and then reverse
them if on '-' strand.*/
{
int count =0;
struct bed *bed = NULL;
struct altGraphX *agList = NULL, *ag = NULL, *agNew = NULL;

if(altGraphXFileName != NULL)
    {
    warn("Loading altGraphX Records from file %s.", altGraphXFileName);
    agList = altGraphXLoadAll(altGraphXFileName);
    }
else if(altGraphXTableName != NULL)
    {
    char query[256];
    warn("Reading altGraphX Records from table %s.", altGraphXTableName);
    safef(query, sizeof(query), "select * from %s where tName like '%s'", altGraphXTableName, chrom);
    agList = altGraphXLoadByQuery(conn, query);
    }
else
    errAbort("orthoMap::mapAlGraphXFile() - Need a table name or file name to load altGraphX records");
warn("Mapping altGraphX records.");
for(ag = agList; ag != NULL; ag = ag->next)
    {
    occassionalDot();
    agNew = mapAltGraphX(ag, conn, db, netTable);
    if(agNew == NULL)
	(*notFoundCount)++;
    else
	{
	(*foundCount)++;
	altGraphXTabOut(agNew, agxOut);
	altGraphXFree(&agNew);
	}
    count++;
    }	
}

void orthoMap()
/** Top level function. Load up the psls and transform them to beds. */
{
char *pslTableName = NULL;
char *pslFileName = NULL;
char *outBedName = NULL;
char *agxOutName = NULL;
char *altGraphXFileName = NULL;
char *altGraphXTableName = NULL;
char *db = NULL;
char *orthoDb = NULL;
char *netTable = NULL;
struct bed *bed = NULL;
struct psl *psl = NULL, *pslList = NULL;
struct sqlConnection *conn = NULL;
FILE *bedOut = NULL;
int foundCount=0, notFoundCount=0;
char *chrom = NULL;
char *netFile = NULL;
netFile=optionVal("netFile", NULL);
agxOutName = optionVal("altGraphXOut", NULL);
altGraphXFileName = optionVal("altGraphXFile", NULL);
altGraphXTableName = optionVal("altGraphXTable", NULL);
pslTableName  = optionVal("pslTable", NULL);
pslFileName = optionVal("pslFile", NULL);
outBedName = optionVal("outBed", NULL);
netTable = optionVal("netTable", NULL);
db = optionVal("db", NULL);
orthoDb = optionVal("orthoDb", NULL);
chrom = optionVal("chrom", NULL);

if(orthoDb == NULL || db == NULL || (netTable == NULL && netFile == NULL) || chrom == NULL || 
   (outBedName == NULL && agxOutName == NULL) ||
   (pslTableName == NULL && pslFileName == NULL && altGraphXFileName == NULL && altGraphXTableName == NULL))
    usage();
hSetDb(db);
hSetDb2(orthoDb);
if(netFile != NULL)
    {
    warn("Loading net info from file %s", netFile);
    netTree = rbTreeFromNetFile(netFile);
    }
if(netTable != NULL || altGraphXTableName != NULL)
    conn = hAllocConn();
if(altGraphXFileName != NULL || altGraphXTableName != NULL)
    {
    FILE *agxOut = NULL;
    if(agxOutName == NULL)
	errAbort("Must specify altGraphXOut if specifying altGraphXFile. Use -help for help");
    agxOut = mustOpen(agxOutName, "w");
    mapAltGraphXFile(conn, db, orthoDb, chrom, netTable, altGraphXFileName, altGraphXTableName, 
		     agxOut, &foundCount, &notFoundCount);
    carefulClose(&agxOut);
    }
else 
    {
/* Load psls. */
    warn("Loading psls.");
    if(pslFileName)
	pslList=pslLoadAll(pslFileName);
    else
	pslList=loadPslFromTable(conn, pslTableName, chrom, 0, BIGNUM);
/* Convert psls. */
    warn("Converting psls.");
    bedOut = mustOpen(outBedName, "w");
    for(psl = pslList; psl != NULL; psl = psl->next)
	{
	occassionalDot();
	bed = orthoBedFromPsl(conn, db, orthoDb, netTable, psl);
	if(bed != NULL && bed->blockCount > 0)
	    {
	    foundCount++;
	    bedTabOutN(bed, 12, bedOut);
	    }
	else
	    notFoundCount++;
	bedFree(&bed);
	}
    }
warn("\n%d found %d not found when moving from genome %s to genome %s", 
     foundCount, notFoundCount, db, orthoDb); 
warn("Done");
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
orthoMap();
return 0;
}

