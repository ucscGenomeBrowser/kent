/* hgConvert - CGI-script to convert browser window coordinates 
 * using chain files */
#include "common.h"
#include "hash.h"
#include "errabort.h"
#include "jksql.h"
#include "linefile.h"
#include "hCommon.h"
#include "fa.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "web.h"
#include "chain.h"
#include "liftOver.h"
#include "liftOverChain.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: hgConvert.c,v 1.31 2008/11/10 19:04:09 angie Exp $";

/* CGI Variables */
#define HGLFT_TOORG_VAR   "hglft_toOrg"           /* TO organism */
#define HGLFT_TODB_VAR   "hglft_toDb"           /* TO assembly */
#define HGLFT_DO_CONVERT "hglft_doConvert"	/* Do the actual conversion */

/* Global Variables */
static struct cart *cart;	        /* CGI and other variables */
static struct hash *oldVars = NULL;
static char *organism = NULL;
static char *database = NULL;

/* Javascript to support New Assembly pulldown when New Genome changes. */
/* Copies selected values to a hidden form */
char *onChangeToOrg = "onchange=\"document.mainForm.submit();\"";

struct dbDb *matchingDb(struct dbDb *list, char *name)
/* Find database of given name in list or die trying. */
{
struct dbDb *db;
for (db = list; db != NULL; db = db->next)
    {
    if (sameString(name, db->name))
        return db;
    }
errAbort("Can't find %s in matchingDb", name);
return NULL;
}

void askForDestination(struct liftOverChain *liftOver, char *fromPos,
	struct dbDb *fromDb, struct dbDb *toDb)
/* set up page for entering data */
{
struct dbDb *dbList;

cartWebStart(cart, database, "Convert %s to New Assembly", fromPos);

/* create HMTL form */
puts("<FORM ACTION=\"../cgi-bin/hgConvert\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* create HTML table for layout purposes */
puts("\n<TABLE WIDTH=\"100%%\">\n");

/* top row -- labels */
cgiSimpleTableRowStart();
cgiTableField("Old Genome: ");
cgiTableField("Old Assembly: ");
cgiTableField("New Genome: ");
cgiTableField("New Assembly: ");
cgiTableField(" ");
cgiTableRowEnd();

/* Next row -- data and controls */
cgiSimpleTableRowStart();

/* From organism and assembly. */
cgiTableField(fromDb->organism);
cgiTableField(fromDb->description);

/* Destination organism. */
cgiSimpleTableFieldStart();
dbList = hGetLiftOverToDatabases(liftOver->fromDb);
printSomeGenomeListHtmlNamed(HGLFT_TOORG_VAR, liftOver->toDb, dbList, onChangeToOrg);
cgiTableFieldEnd();

/* Destination assembly */
cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(liftOver->toDb, dbList, HGLFT_TODB_VAR, TRUE, "");
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
cgiMakeButton(HGLFT_DO_CONVERT, "Submit");
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

puts("</FORM>\n");

cartWebEnd();
}


double scoreLiftOverChain(struct liftOverChain *chain,
    char *fromOrg, char *fromDb, char *toOrg, char *toDb, struct hash *dbRank )
/* Score the chain in terms of best match for cart settings */
{
double score = 0;

char *chainFromOrg = hArchiveOrganism(chain->fromDb);
char *chainToOrg = hArchiveOrganism(chain->toDb);
int fromRank = hashIntValDefault(dbRank, chain->fromDb, 0);
int toRank = hashIntValDefault(dbRank, chain->toDb, 0);
int maxRank = hashIntVal(dbRank, "maxRank");

if (sameOk(fromOrg,chainFromOrg) &&
    sameOk(fromDb,chain->fromDb) && 
    sameOk(toOrg,chainToOrg) &&
    sameOk(toDb,chain->toDb))
    score += 10000000;

if (sameOk(fromOrg,chainFromOrg)) 
    score += 2000000;
if (sameOk(fromDb,chain->fromDb)) 
    score += 1000000;

if (sameOk(toOrg,chainToOrg))
    score += 200000;
if (sameOk(toDb,chain->toDb))
    score += 100000;

score += 10*(maxRank-fromRank);
score += (maxRank - toRank);

return score;
}


struct liftOverChain *defaultChoices(struct liftOverChain *chainList, char *fromOrg, char *fromDb)
/* Out of a list of liftOverChains and a cart, choose a
 * list to display. */
{
char *toOrg, *toDb;
struct liftOverChain *choice = NULL;
struct hash *dbRank = hGetDatabaseRank();
double bestScore = -1;
struct liftOverChain *this = NULL;

/* Get the initial values. */
toOrg = cartCgiUsualString(cart, HGLFT_TOORG_VAR, "0");
toDb = cartCgiUsualString(cart, HGLFT_TODB_VAR, "0");

if (sameWord(toOrg,"0"))
    toOrg = NULL;
if (sameWord(toDb,"0"))
    toDb = NULL;

for (this = chainList; this != NULL; this = this->next)
    {
    double score = scoreLiftOverChain(this, fromOrg, fromDb, toOrg, toDb, dbRank);
    if (score > bestScore)
	{
	choice = this;
	bestScore = score;
	}
    }  

return choice;
}


char *skipWord(char *s)
/* Skip word, and any leading spaces before next word. */
{
return skipLeadingSpaces(skipToSpaces(s));
}

long chainTotalBlockSize(struct chain *chain)
/* Return sum of sizes of all blocks in chain */
{
struct cBlock *block;
long total = 0;
for (block = chain->blockList; block != NULL; block = block->next)
    total += block->tEnd - block->tStart;
return total;
}

struct chain *chainLoadIntersecting(char *fileName, 
	char *chrom, int start, int end)
/* Load the chains that intersect given region. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int chromNameSize = strlen(chrom);
struct chain *chainList = NULL, *chain;
boolean gotChrom = FALSE;
int chainCount = 0;

while (lineFileNextReal(lf, &line))
    {
    if (startsWith("chain", line) && isspace(line[5]))
        {
	++chainCount;
	line = skipWord(line);	/* Skip over 'chain' */
	line = skipWord(line);	/* Skip over chain score */
	if (startsWith(chrom, line) && isspace(line[chromNameSize]))
	    {
	    gotChrom = TRUE;
	    lineFileReuse(lf);
	    chain = chainReadChainLine(lf);
	    if (rangeIntersection(chain->tStart, chain->tEnd, start, end) > 0)
		{
		chainReadBlocks(lf, chain);
		slAddHead(&chainList, chain);
		}
	    else
	        chainFree(&chain);
	    }
#ifdef SOON	/* Put in if we index. */
	else if (gotChrom)
	    break;	/* We assume file is sorted by chromosome, so we're done. */
#endif /* SOON */
	}
    }
lineFileClose(&lf);
slReverse(&chainList);
return chainList;
}

struct chain *chainLoadAndTrimIntersecting(char *fileName,
	char *chrom, int start, int end)
/* Load the chains that intersect given region, and trim them
 * to fit region. */
{
struct chain *rawList, *chainList = NULL, *chain, *next;
rawList = chainLoadIntersecting(fileName, chrom, start, end);
for (chain = rawList; chain != NULL; chain = next)
    {
    struct chain *subChain, *chainToFree;
    next = chain->next;
    chainSubsetOnT(chain, start, end, &subChain, &chainToFree);
    if (subChain != NULL)
	slAddHead(&chainList, subChain);
    if (chainToFree != NULL)
        chainFree(&chain);
    }
slSort(&chainList, chainCmpScore);
return chainList;
}

void doConvert(char *fromPos)
/* Actually do the conversion */
{
struct dbDb *fromDb = hDbDb(database), *toDb = hDbDb(cartString(cart, HGLFT_TODB_VAR));
char *fileName = liftOverChainFile(fromDb->name, toDb->name);
char *chrom;
int start, end;
int origSize;
struct chain *chainList, *chain;

cartWebStart(cart, database, "%s %s %s to %s %s", fromDb->organism, fromDb->description,
	fromPos, toDb->organism, toDb->description);
if (!hgParseChromRange(database, fromPos, &chrom, &start, &end))
    errAbort("position %s is not in chrom:start-end format", fromPos);
origSize = end - start;

chainList = chainLoadAndTrimIntersecting(fileName, chrom, start, end);
if (chainList == NULL)
    printf("Sorry this position couldn't be found in new assembly");
else
    {
    for (chain = chainList; chain != NULL; chain = chain->next)
        {
	int blockSize;
	int qStart, qEnd;
	if (chain->qStrand == '-')
	    {
	    qStart = chain->qSize - chain->qEnd;
	    qEnd = chain->qSize - chain->qStart;
	    }
	else
	    {
	    qStart = chain->qStart;
	    qEnd = chain->qEnd;
	    }
	blockSize = chainTotalBlockSize(chain);
        /* Check if the toDb database exists and if the chromosome 
           sequence file (of the hgConvert result) exists in the location 
           specified in chromInfo for the toDb. */
 
        boolean chromSeqExists = chromSeqFileExists(toDb->name, chain->qName);
        /* Check if the toDb has active set to 1 in dbDb if the toDb 
           database exists. 
           If these conditions are met then print position link to 
           browser for toDb, otherwise just print position without link. */
        if (hDbIsActive(toDb->name) && chromSeqExists) 
	    printf("<A HREF=\"%s?db=%s&position=%s:%d-%d\">",
		   hgTracksName(), toDb->name, chain->qName, qStart+1, qEnd);
	printf("%s:%d-%d",  chain->qName, qStart+1, qEnd);
        if (hDbIsActive(toDb->name) && chromSeqExists) 
	    printf("</A>");
	printf(" (%3.1f%% of bases, %3.1f%% of span)<BR>\n",
	    100.0 * blockSize/origSize,  
	    100.0 * (chain->tEnd - chain->tStart) / origSize);
	}
    }
cartWebEnd();
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *fromPos = cartString(theCart, "position");

cart = theCart;
getDbAndGenome(cart, &database, &organism, oldVars);

if (cartVarExists(cart, HGLFT_DO_CONVERT))
    doConvert(fromPos);
else
    {
    struct liftOverChain *liftOverList = liftOverChainListForDbFiltered(database);
    struct liftOverChain *choice = defaultChoices(liftOverList, organism, database);
    if (choice == NULL)
	errAbort("Sorry, no conversions available from this assembly.");
    struct dbDb *dbList, *fromDb, *toDb;
    dbList = hDbDbList();
    fromDb = matchingDb(dbList, choice->fromDb);
    toDb = matchingDb(dbList, choice->toDb);
    askForDestination(choice, fromPos, fromDb, toDb);
    liftOverChainFreeList(&liftOverList);
    }
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = { "submit", HGLFT_DO_CONVERT, NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

