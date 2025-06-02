/* hgConvert - CGI-script to convert browser window coordinates
 * using chain files */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "hash.h"
#include "errAbort.h"
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
#include "net.h"
#include "genark.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "quickLift.h"


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
char *onChangeToOrg = "document.mainForm.submit();";

static struct dbDb *matchingDb(struct dbDb *list, char *name)
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

static void askForDestination(struct liftOverChain *liftOver, char *fromPos,
	struct dbDb *fromDb, struct dbDb *toDb)
/* set up page for entering data */
{
struct dbDb *dbList;
boolean askAboutQuickLift = FALSE;

if (quickLiftEnabled())
    askAboutQuickLift = TRUE;

cartWebStart(cart, database, "Convert %s to New Assembly", fromPos);

/* create HMTL form */
puts("<FORM ACTION=\"../cgi-bin/hgConvert\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* create HTML table for layout purposes */
puts("\n<TABLE WIDTH=\"100%%\">\n");

/* top row -- labels */
cgiSimpleTableRowStart();
cgiTableField("Old genome: ");
cgiTableField("Old assembly: ");
cgiTableField("New genome: ");
cgiTableField("New assembly: ");
if (askAboutQuickLift)
    cgiTableField("QuickLift tracks: ");
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
printSomeGenomeListHtmlNamed(HGLFT_TOORG_VAR, liftOver->toDb, dbList, "change", onChangeToOrg);
cgiTableFieldEnd();

/* Destination assembly */
cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(liftOver->toDb, dbList, HGLFT_TODB_VAR, TRUE, NULL, NULL);
cgiTableFieldEnd();

if (askAboutQuickLift)
    {
    cgiSimpleTableFieldStart();
    boolean quickLift = cartUsualBoolean(cart, "doQuickLift", FALSE);
    cgiMakeCheckBoxWithId("doQuickLift", quickLift, "doQuickLift");
    cgiTableFieldEnd();
    }

cgiSimpleTableFieldStart();
cgiMakeButton(HGLFT_DO_CONVERT, "Submit");
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

puts("</FORM>\n");

cartWebEnd();
}

static double scoreLiftOverChain(struct liftOverChain *chain,
    char *fromOrg, char *fromDb, char *toOrg, char *toDb, struct hash *dbRank )
/* Score the chain in terms of best match for cart settings */
{
double score = 0;

char *chainFromOrg = hOrganism(chain->fromDb);
char *chainToOrg = hOrganism(chain->toDb);
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

if (toRank == 0)  // chains to db's that are not active shouldn't be considered
    return 0;
score += 10*(maxRank-fromRank);
score += (maxRank - toRank);

return score;
}


static struct liftOverChain *defaultChoices(struct liftOverChain *chainList, char *fromOrg, char *fromDb)
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

static char *skipWord(char *s)
/* Skip word, and any leading spaces before next word. */
{
return skipLeadingSpaces(skipToSpaces(s));
}

static long chainTotalBlockSize(struct chain *chain)
/* Return sum of sizes of all blocks in chain */
{
struct cBlock *block;
long total = 0;
for (block = chain->blockList; block != NULL; block = block->next)
    total += block->tEnd - block->tStart;
return total;
}

static struct chain *chainLoadIntersecting(char *fileName,
	char *chrom, int start, int end)
/* Load the chains that intersect given region. */
{
struct lineFile *lf = netLineFileOpen(fileName);
char *line;
int chromNameSize = strlen(chrom);
struct chain *chainList = NULL, *chain;
#ifdef SOON	/* Put in if we index. */
boolean gotChrom = FALSE;
#endif  /* SOON */
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
#ifdef SOON	/* Put in if we index. */
	    gotChrom = TRUE;
#endif  /* SOON */
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

static struct chain *chainLoadAndTrimIntersecting(char *fileName,
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

static void doConvert(char *fromPos)
/* Actually do the conversion */
{
struct dbDb *fromDb = hDbDb(trackHubSkipHubName(database)), *toDb = hDbDb(cartString(cart, HGLFT_TODB_VAR));
if (!fromDb || !toDb)
    errAbort("Early error - unable to find matching database records in dbDb - please contact support");

cartWebStart(cart, database, "%s %s %s to %s %s", fromDb->organism, fromDb->description,
	fromPos, toDb->organism, toDb->description);

char *fileName = liftOverChainFile(fromDb->name, toDb->name);
if (isEmpty(fileName))
    errAbort("Unable to find a chain file from %s to %s - please contact support", fromDb->name, toDb->name);

fileName = hReplaceGbdbMustDownload(fileName);
char *chrom;
int start, end;
int origSize;
struct chain *chainList, *chain;
struct dyString *visDy = NULL;

if (!hgParseChromRange(database, fromPos, &chrom, &start, &end))
    errAbort("position %s is not in chrom:start-end format", fromPos);
origSize = end - start;

boolean doQuickLift = cartUsualBoolean(cart, "doQuickLift", FALSE);
cartRemove(cart, "doQuickLift");

unsigned quickChain = 0;
unsigned quickHub = 0;
struct trackDb *badList = NULL;

if (doQuickLift)
    {
    quickChain = quickLiftGetChain(fromDb->name, toDb->name);

    if (quickChain == 0)
        errAbort("can't find quickChain from %s to %s", fromDb->name, toDb->name);

    visDy = newDyString(1024);
    char *newHub = trackHubBuild(fromDb->name, cart, visDy, &badList);
    char *error = NULL;
    quickHub = hubFindOrAddUrlInStatusTable(cart, newHub, &error);
    if (error != NULL)
        errAbort("can't add quickLift hub (error %s)",error);
    }

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

        boolean chromSeqExists = (sqlDatabaseExists(toDb->name) &&
				  chromSeqFileExists(toDb->name, chain->qName));
        /* Check if the toDb has active set to 1 in dbDb if the toDb
           database exists.
           If these conditions are met then print position link to
           browser for toDb, otherwise just print position without link. */
        boolean startedAnchor = FALSE;
        if ((hDbIsActive(toDb->name) && chromSeqExists) || startsWith("hub:",toDb->nibPath))
            {
            if (quickChain)
                printf("<A HREF=\"%s?db=%s&position=%s:%d-%d&quickLift.%d.%s=%d\">",
                   hgTracksName(), toDb->name,  chain->qName, qStart+1, qEnd, quickHub, toDb->name, quickChain);
            else
                printf("<A HREF=\"%s?db=%s&position=%s:%d-%d\">",
		   hgTracksName(), toDb->name, chain->qName, qStart+1, qEnd);
            startedAnchor = TRUE;
            }
        else if (sameString(toDb->nibPath, "genark"))
            {
            char *hubUrl = genarkUrl(toDb->name);
            if (hubUrl)
                {
                startedAnchor = TRUE;
                if (quickChain)
                    printf("<A HREF=\"%s?genome=%s&hubUrl=%s&position=%s:%d-%d&quickLift.%d.%s=%d\">",
                       hgTracksName(), toDb->name, hubUrl, chain->qName, qStart+1, qEnd, quickHub, toDb->name, quickChain);
                else
                    printf("<A HREF=\"%s?genome=%s&hubUrl=%s&position=%s:%d-%d\">",
                       hgTracksName(), toDb->name, hubUrl, chain->qName, qStart+1, qEnd);
                }
            }
	printf("%s:%d-%d",  chain->qName, qStart+1, qEnd);
        if (startedAnchor)
	    printf("</A>");
	printf(" (%3.1f%% of bases, %3.1f%% of span)<BR>\n",
	    100.0 * blockSize/origSize,
	    100.0 * (chain->tEnd - chain->tStart) / origSize);
	}
    }
if (badList)
    for(; badList; badList = badList->next)
        printf("%s %s<BR>", badList->track, badList->type);
cartWebEnd();
}

static struct liftOverChain *cleanLiftOverList(struct liftOverChain *list)
/* eliminate from the list where toDb doesn't exist in dbDb */
{
struct liftOverChain *cleanList = NULL;
struct hash *dbDbHash = hDbDbHash();
struct liftOverChain *this = NULL;
struct liftOverChain *next = NULL;
for (this = list; this != NULL; this = next)
    {
    next = this->next;
    if (hashLookup(dbDbHash, this->toDb))
        slAddHead(&cleanList, this);
    else
        liftOverChainFree(&this);
    }
slReverse(&cleanList);
return cleanList;
}

static void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *fromPos = cartString(theCart, "position");

cart = theCart;
getDbAndGenome(cart, &database, &organism, oldVars);

// Try to deal with virt chrom position used by hgTracks.
if (startsWith(    MULTI_REGION_CHROM, cartUsualString(cart, "position", ""))
 || startsWith(OLD_MULTI_REGION_CHROM, cartUsualString(cart, "position", "")))
    cartSetString(cart, "position", cartUsualString(cart, "nonVirtPosition", ""));

if (cartVarExists(cart, HGLFT_DO_CONVERT))
    doConvert(fromPos);
else
    {
    struct liftOverChain *checkLiftOverList = liftOverChainListForDbFiltered(trackHubSkipHubName(database));
    struct liftOverChain *liftOverList = cleanLiftOverList(checkLiftOverList);
    struct liftOverChain *choice = defaultChoices(liftOverList, organism, database);
    if (choice == NULL)
	errAbort("Sorry, no conversions available from this assembly.");
    struct dbDb *dbList, *fromDb, *toDb;
    dbList = hDbDbListMaybeCheck(FALSE);
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
long enteredMainTime = clock1000();
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgConvert", enteredMainTime);
return 0;
}

