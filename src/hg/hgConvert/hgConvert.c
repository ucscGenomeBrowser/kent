/* hgConvert - CGI-script to convert browser window coordinates 
 * using chain files */
#include "common.h"
#include "errabort.h"
#include "hCommon.h"
#include "jksql.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "web.h"
#include "hash.h"
#include "liftOver.h"
#include "liftOverChain.h"

static char const rcsid[] = "$Id: hgConvert.c,v 1.3 2005/11/16 23:39:31 kent Exp $";

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

void askForDestination(struct liftOverChain *chain)
/* set up page for entering data */
{
struct dbDb *dbList, *fromDb;
char *fromOrg = hOrganism(chain->fromDb);
char *toOrg = hOrganism(chain->toDb);
char *fromPos = cartString(cart, "position");
printf("Converts the current browser position (%s) to a new assembly.",
	fromPos);
puts("<BR><BR>");

dbList = hGetLiftOverFromDatabases();
fromDb = matchingDb(dbList, chain->fromDb);

/* create HMTL form */
puts("<FORM ACTION=\"../cgi-bin/hgConvert\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* create HTML table for layout purposes */
puts("\n<TABLE WIDTH=\"100%%\">\n");

/* top two rows -- genome and assembly menus */
cgiSimpleTableRowStart();
cgiTableField("Old Genome: ");
cgiTableField("Old Assembly: ");
cgiTableField("New Genome: ");
cgiTableField("New Assembly: ");
cgiTableField(" ");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField(fromDb->organism);
cgiTableField(fromDb->description);

/* Destination organism. */
cgiSimpleTableFieldStart();
dbDbFreeList(&dbList);
dbList = hGetLiftOverToDatabases(chain->fromDb);
printSomeGenomeListHtmlNamed(HGLFT_TOORG_VAR, chain->toDb, dbList, onChangeToOrg);
cgiTableFieldEnd();

/* Destination assembly */
cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(chain->toDb, dbList, HGLFT_TODB_VAR, TRUE, "");
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
                        HGLFT_TOORG_VAR, toOrg);
printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n",
                        HGLFT_TODB_VAR, chain->toDb);
cartSaveSession(cart);
puts("</FORM>");
freeMem(fromOrg);
freeMem(toOrg);
}

struct liftOverChain *findLiftOverChain(struct liftOverChain *chainList, char *fromDb, char *toDb)
/* Return TRUE if there's a chain with both fromDb and
 * toDb. */
{
struct liftOverChain *chain;
if (!fromDb || !toDb)
    return NULL;
for (chain = chainList; chain != NULL; chain = chain->next)
    if (sameString(chain->fromDb,fromDb) && sameString(chain->toDb,toDb))
	return chain;
return NULL;
}

struct liftOverChain *currentLiftOver(struct liftOverChain *chainList, 
	char *fromOrg, char *fromDb, char *toOrg, char *toDb)
/* Given list of liftOvers, and given databases find 
 * liftOver that goes between databases, or failing that
 * a chain that goes from the database to something else. */
{
struct liftOverChain *choice = NULL;
struct liftOverChain *over;

uglyf("currentLiftOver %d liftOvers, from %s %s, to %s %s<BR>", 
	slCount(chainList), fromOrg, fromDb, toOrg, toDb);

/* See if we can find what user asked for. */
if (toDb != NULL)
    choice = findLiftOverChain(chainList,fromDb,toDb);

/* If we have no valid choice from user then try and get
 * a different assembly from same to organism. */
if (toOrg != NULL)
    {
    for (over = chainList; over != NULL; over = over->next)
	{
	if (sameString(over->fromDb, fromDb))
	    {
	    if (sameString(hOrganism(over->toDb), toOrg))
	        {
		choice = over;
		break;
		}
	    }
	}
    }

/* If still no valid choice try and get to a different
 * assembly of the current organism. */
if (!choice)
    {
    /* First try to find a chain into another assembly of same organism. */
    for (over = chainList; over != NULL; over = over->next)
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


/* If still can't find choice, then try and find any chain from
 * current assembly */
if (!choice)
    {
    for (over = chainList; over != NULL; over = over->next)
	{
	if (sameString(over->fromDb, fromDb))
	    {
	    choice = over;
	    break;
	    }
	}
    }


/* If still can't find choice we give up */
if (!choice)
    errAbort("Can't find a chain from %s to anywhere, sorry", fromDb);

return choice;
}

void doConvert(struct liftOverChain *chain)
/* Actually do the conversion */
{
uglyAbort("Sorry, don't really know how to convert yet.");
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *organism;
char *db;    
struct liftOverChain *chainList = NULL, *choice;

cart = theCart;
cartWebStart(cart, "Convert to New Assembly");
getDbAndGenome(cart, &db, &organism);

chainList = liftOverChainList();
choice = currentLiftOver(chainList, organism, db, 
	cartOptionalString(cart, HGLFT_TOORG_VAR),
	cartOptionalString(cart, HGLFT_TODB_VAR));
if (cartVarExists(cart, HGLFT_DO_CONVERT))
    doConvert(choice);
else
    askForDestination(choice);
liftOverChainFreeList(&chainList);
cartWebEnd();
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

