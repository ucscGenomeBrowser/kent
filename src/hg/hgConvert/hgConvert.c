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

static char const rcsid[] = "$Id: hgConvert.c,v 1.14 2006/06/06 00:50:37 galt Exp $";

/* CGI Variables */
#define HGLFT_TOORG_VAR   "hglft_toOrg"           /* TO organism */
#define HGLFT_TODB_VAR   "hglft_toDb"           /* TO assembly */
#define HGLFT_DO_CONVERT "hglft_doConvert"	/* Do the actual conversion */

/* Global Variables */
struct cart *cart;	        /* CGI and other variables */
struct hash *oldCart = NULL;

/* Javascript to support New Assembly pulldown when New Genome changes. */
/* Copies selected values to a hidden form */
char *onChangeToOrg = 
"onchange=\"document.dbForm.hglft_toOrg.value = "
"document.mainForm.hglft_toOrg.options[document.mainForm.hglft_toOrg.selectedIndex].value;"
"document.dbForm.hglft_toDb.value = 0;"
"document.dbForm.submit();\"";

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

cartWebStart(cart, "Convert %s to New Assembly", fromPos);

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

/* Hidden form to support menu pulldown behavior */
printf("<FORM ACTION=\"/cgi-bin/hgConvert\""
       " METHOD=\"GET\" NAME=\"dbForm\">");
printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n", 
                        HGLFT_TOORG_VAR, toDb->organism);
printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n",
                        HGLFT_TODB_VAR, liftOver->toDb);
cartSaveSession(cart);
puts("</FORM>");
cartWebEnd();
}

struct liftOverChain *findLiftOver(struct liftOverChain *liftOverList, 
	char *fromDb, char *toDb)
/* Return TRUE if there's a liftOver with both fromDb and
 * toDb. */
{
struct liftOverChain *liftOver;
if (!fromDb || !toDb)
    return NULL;
for (liftOver = liftOverList; liftOver != NULL; liftOver = liftOver->next)
    if (sameString(liftOver->fromDb,fromDb) && sameString(liftOver->toDb,toDb))
	return liftOver;
return NULL;
}

struct liftOverChain *currentLiftOver(struct liftOverChain *liftOverList, 
	char *fromOrg, char *fromDb, char *toOrg, char *toDb)
/* Given list of liftOvers, and given databases find 
 * liftOver that goes between databases, or failing that
 * a liftOver that goes from the database to something else. */
{
struct liftOverChain *choice = NULL;
struct liftOverChain *over;

/* See if we can find what user asked for. */
if (toDb != NULL)
    choice = findLiftOver(liftOverList,fromDb,toDb);

/* If we have no valid choice from user then try and get
 * a different assembly from same to organism. */
if (choice == NULL && toOrg != NULL)
    {
    for (over = liftOverList; over != NULL; over = over->next)
	{
	if (sameString(over->fromDb, fromDb))
	    {
	    if (sameString(hOrganism(over->toDb), toOrg))
	        {
		if (!choice || (compareDbs(over->toDb,choice->toDb)>0))
		    choice = over;
		}
	    }
	}
    }

/* If still no valid choice try and get to a different
 * assembly of the current organism. */
if (choice == NULL)
    {
    /* First try to find a liftOver into another assembly of same organism. */
    for (over = liftOverList; over != NULL; over = over->next)
	{
	if (sameString(over->fromDb, fromDb))
	    {
	    if (sameString(hOrganism(over->toDb), fromOrg))
	        {
		choice = over;
		break;
		}
	    }
	}
    }


/* If still can't find choice, then try and find any liftOver from
 * current assembly */
if (choice == NULL)
    {
    for (over = liftOverList; over != NULL; over = over->next)
	{
	if (sameString(over->fromDb, fromDb))
	    {
	    choice = over;
	    break;
	    }
	}
    }


/* If still can't find choice we give up */
if (choice == NULL)
    errAbort("Can't find a liftOver from %s to anywhere, sorry", fromDb);

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

void doConvert(struct liftOverChain *liftOver, char *fromPos,
	struct dbDb *fromDb, struct dbDb *toDb)
/* Actually do the conversion */
{
char *fileName = liftOverChainFile(liftOver->fromDb, liftOver->toDb);
char *chrom;
int start, end;
int origSize;
struct chain *chainList, *chain;

cartWebStart(cart, "%s %s %s to %s %s", fromDb->organism, fromDb->description,
	fromPos, toDb->organism, toDb->description);
if (!hgParseChromRange(fromPos, &chrom, &start, &end))
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
	printf("<A HREF=\"%s?%s=%u&db=%s&position=%s:%d-%d\">",
	       hgTracksName(), cartSessionVarName(), cartSessionId(cart),
	       liftOver->toDb,
	       chain->qName, qStart+1, qEnd);
	printf("%s:%d-%d",  chain->qName, qStart+1, qEnd);
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
char *organism;
char *db;    
struct liftOverChain *liftOverList = NULL, *choice;
char *fromPos = cartString(theCart, "position");
struct dbDb *dbList, *fromDb, *toDb;

cart = theCart;
getDbAndGenome(cart, &db, &organism);
hSetDb(db);

liftOverList = liftOverChainListFiltered();
choice = currentLiftOver(liftOverList, organism, db, 
	cartOptionalString(cart, HGLFT_TOORG_VAR),
	cartOptionalString(cart, HGLFT_TODB_VAR));
dbList = hDbDbList();
fromDb = matchingDb(dbList, choice->fromDb);
toDb = matchingDb(dbList, choice->toDb);
if (cartVarExists(cart, HGLFT_DO_CONVERT))
    doConvert(choice, fromPos, fromDb, toDb);
else
    askForDestination(choice, fromPos, fromDb, toDb);
liftOverChainFreeList(&liftOverList);
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = { "submit", HGLFT_DO_CONVERT, NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}

