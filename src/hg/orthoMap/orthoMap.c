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
#include "psl.h"
#include "bed.h"

static char const rcsid[] = "$Id: orthoMap.c,v 1.1 2003/06/10 18:44:39 sugnet Exp $";
static boolean doHappyDots;   /* output activity dots? */
static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"orthoDb", OPTION_STRING},
    {"chrom", OPTION_STRING},
    {"pslFile", OPTION_STRING},
    {"pslTable", OPTION_STRING},
    {"netTable", OPTION_STRING},
    {"chainFile", OPTION_STRING},
    {"outBed", OPTION_STRING},
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
    "File to output beds to.",
    "Datbase table containing net records, i.e. mouseNet.",
    "File containing the chains. Usually I do this on a chromosome basis."
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
    char *chainFile = optionVal("chainFile", NULL);
    if(chainFile == NULL)
	errAbort("orthoSplice::chainFromId() - Can't find file for 'chainFile' parameter");
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

boolean checkChain(struct chain *chain, int start, int end)
/* Return true if chain covers start, end, false otherwise. */
{
struct chain *subChain=NULL, *toFree=NULL;
boolean good = FALSE;
chainSubsetOnT(chain, start, end, &subChain, &toFree);    
if(subChain != NULL)
    good = TRUE;
chainFree(&toFree);
return good;
}

struct chain *lookForChain(struct cnFill *list, int start, int end)
/* Recursively look for a chain in this list containing the coordinates 
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
	chain = chainFromId(fill->chainId);
	if(checkChain(chain, start,end))
	    return chain;
	if(gap->children)
	    {
	    chain = lookForChain(gap->children, start, end);
	    if(checkChain(chain, start,end))
		return chain;
	    }
	}
    }
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
struct chainNet *net = chainNetLoadRange(db, netTable, chrom,
					 start, end, NULL);
if(net == NULL)
    return NULL;
fill = net->fillList;
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
bed->chrom = cloneString(psl->tName);
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
struct chain *chain = chainForPosition(conn, db, netTable, psl->tName, psl->tStart, psl->tEnd);
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
static int dotMod = 20;
static int dot = 20;
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


void orthoMap()
/** Top level function. Load up the psls and transform them to beds. */
{
char *pslTableName = NULL;
char *pslFileName = NULL;
char *outBedName = NULL;
char *db = NULL;
char *orthoDb = NULL;
char *netTable = NULL;
struct bed *bed = NULL;
struct psl *psl = NULL, *pslList = NULL;
struct sqlConnection *conn = NULL;
FILE *bedOut = NULL;
int foundCount=0, notFoundCount=0;
char *chrom = NULL;
pslTableName  = optionVal("pslTable", NULL);
pslFileName = optionVal("pslFile", NULL);
outBedName = optionVal("outBed", NULL);
netTable = optionVal("netTable", NULL);
db = optionVal("db", NULL);
orthoDb = optionVal("orthoDb", NULL);
chrom = optionVal("chrom", NULL);

if(orthoDb == NULL || db == NULL || netTable == NULL || outBedName == NULL ||
   chrom == NULL || (pslTableName == NULL && pslFileName == NULL))
    usage();
hSetDb(db);
hSetDb2(orthoDb);
conn = hAllocConn();
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

