/* hgLiftOver - CGI-script to convert coordinates using chain files */
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

static char const rcsid[] = "$Id: hgConvert.c,v 1.1 2005/11/10 06:16:45 kent Exp $";

/* CGI Variables */
#define HGLFT_USERDATA_VAR "hglft_userData"     /* typed/pasted in data */
#define HGLFT_DATAFILE_VAR "hglft_dataFile"     /* file of data to convert */
#define HGLFT_DATAFORMAT_VAR "hglft_dataFormat" /* format of data to convert */
#define HGLFT_FROMORG_VAR "hglft_fromOrg"         /* FROM organism */
#define HGLFT_FROMDB_VAR "hglft_fromDb"         /* FROM assembly */
#define HGLFT_TOORG_VAR   "hglft_toOrg"           /* TO organism */
#define HGLFT_TODB_VAR   "hglft_toDb"           /* TO assembly */
#define HGLFT_ERRORHELP_VAR "hglft_errorHelp"      /* Print explanatory text */
/* liftOver options: */
#define HGLFT_MINMATCH "hglft_minMatch"          
#define HGLFT_MINSIZEQ "hglft_minSizeQ"
#define HGLFT_MINSIZET "hglft_minSizeT"
#define HGLFT_MULTIPLE "hglft_multiple"
#define HGLFT_MINBLOCKS "hglft_minBlocks"
#define HGLFT_FUDGETHICK "hglft_fudgeThick"

/* Global Variables */
struct cart *cart;	        /* CGI and other variables */
struct hash *oldCart = NULL;

/* Data Formats */
#define POSITION_FORMAT "Position"
#define BED_FORMAT      "BED"
#define WIGGLE_FORMAT   "Wiggle"

char *formatList[] = 
        {BED_FORMAT, POSITION_FORMAT, 0};

#define DEFAULT_FORMAT  "BED"

/* Filename prefix */
#define HGLFT   "hglft"

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

void webMain(struct liftOverChain *chain, char *dataFormat)
/* set up page for entering data */
{
struct dbDb *dbList, *fromDb;
char *fromOrg = hArchiveOrganism(chain->fromDb), *toOrg = hArchiveOrganism(chain->toDb);
char *fromPos = cartString(cart, "position");
puts("This tool converts the current browser position from one assembly to another.");
puts("<BR><BR>");

dbList = hGetLiftOverFromDatabases();
fromDb = matchingDb(dbList, chain->fromDb);
printf("Convert %s from %s genome %s assembly to:", fromPos,
	fromDb->organism, fromDb->name);
puts("<BR><BR>");

/* create HMTL form */
puts("<FORM ACTION=\"../cgi-bin/hgConvert\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* create HTML table for layout purposes */
puts("\n<TABLE WIDTH=\"100%%\">\n");

/* top two rows -- genome and assembly menus */
cgiSimpleTableRowStart();
cgiTableField("New Genome: ");
cgiTableField("New Assembly: ");
cgiTableField(" ");
cgiTableRowEnd();

cgiSimpleTableRowStart();

/* to assembly */

cgiSimpleTableFieldStart();
dbDbFreeList(&dbList);
dbList = hGetLiftOverToDatabases(chain->fromDb);
printSomeGenomeListHtmlNamed(HGLFT_TOORG_VAR, chain->toDb, dbList, onChangeToOrg);
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(chain->toDb, dbList, HGLFT_TODB_VAR, TRUE, "");
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
cgiMakeButton("submit", "Submit");
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

puts("</FORM>\n");

/* Hidden form to support menu pulldown behavior */
printf("<FORM ACTION=\"/cgi-bin/hgConvert\""
       " METHOD=\"GET\" NAME=\"dbForm\">");
printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n", 
                        HGLFT_FROMORG_VAR, fromOrg);
printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n", 
                        HGLFT_FROMDB_VAR, chain->fromDb);
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

struct liftOverChain *defaultChoices(struct liftOverChain *chainList)
/* Out of a list of liftOverChains and a cart, choose a
 * list to display. */
{
char *fromOrg, *fromDb, *toOrg, *toDb, *orgFromDb, *orgToDb;
struct slName *fromOrgs = hLiftOverFromOrgs();
struct slName *fromDbs = hLiftOverFromDbs();
struct slName *toOrgs = hLiftOverToOrgs(NULL);
struct slName *toDbs = hLiftOverToDbs(NULL);
struct liftOverChain *choice = NULL;

/* Get the initial values. */
fromOrg = cartCgiUsualString(cart, HGLFT_FROMORG_VAR, "0");
fromDb = cartCgiUsualString(cart, HGLFT_FROMDB_VAR, "0");
toOrg = cartCgiUsualString(cart, HGLFT_TOORG_VAR, "0");
toDb = cartCgiUsualString(cart, HGLFT_TODB_VAR, "0");
orgFromDb = hArchiveOrganism(fromDb); 
orgToDb = hArchiveOrganism(toDb);
if (sameWord(fromOrg,"0"))
    fromOrg = NULL;
if (sameWord(fromDb,"0"))
    fromDb = NULL;
if (sameWord(toOrg,"0"))
    toOrg = NULL;
if (sameWord(toDb,"0"))
    toDb = NULL;
choice = findLiftOverChain(chainList,fromDb,toDb);
if (!choice)
    {
    /* Check the validness of the stuff first. */
    if (fromDb && toDb)
	toDb = fromDb = toOrg = fromOrg = NULL;
    if (fromDb && !slNameInList(fromDbs, fromDb))
	fromDb = fromOrg = NULL;
    if (toDb && !slNameInList(toDbs, toDb))
	toDb = toOrg = NULL;
    if (fromOrg && !slNameInList(fromOrgs, fromOrg))
	toDb = fromDb = toOrg = fromOrg = NULL;
    if (toOrg && !slNameInList(toOrgs, toOrg))
	toOrg = toDb = NULL;
    if (fromOrg && fromDb && orgFromDb && !sameWord(fromOrg,orgFromDb))
	fromDb = fromOrg = toOrg = toDb = NULL;
    if (toOrg && toDb && orgToDb && !sameWord(toOrg,orgToDb))
	toDb = toOrg = NULL;
    if (toOrg && !fromDb)
	fromOrg = fromDb = toOrg = toDb = NULL;
    if (toDb && !fromDb) 
	fromOrg = fromDb = toOrg = toDb = NULL;
    
    /* Find some defaults. The branching is incomplete because of all
     * the earlier variable manipulation. */
    if (fromOrg && !fromDb)
	{
	for (choice = chainList; choice != NULL; choice = choice->next)
	    {
	    char *org = hArchiveOrganism(choice->fromDb);
	    if (sameString(org,fromOrg))
		{
		freeMem(org);
		break;
		}
	    freeMem(org);
	    }
	}
    else if (fromOrg && fromDb && !toOrg)
	{
	for (choice = chainList; choice != NULL; choice = choice->next)
	    if (sameString(fromDb,choice->fromDb))
		break;
 	}
    else if (fromOrg && fromDb && toOrg && !toDb)
	{
	for (choice = chainList; choice != NULL; choice = choice->next)
	    {
	    char *org = hArchiveOrganism(choice->toDb);
	    if (sameString(choice->fromDb,fromDb) && sameString(org,toOrg))
		{
		freeMem(org);
		break;
		}
	    freeMem(org);
	    }
	}
    }

if (!choice)
    choice = chainList;
slFreeList(&fromOrgs);
slFreeList(&fromDbs);
slFreeList(&toOrgs);
slFreeList(&toDbs);
freeMem(orgFromDb);
freeMem(orgToDb);
return choice;
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
/* struct liftOverChain *chainList = NULL, *chain; */
char *userData;
/* char *dataFile; */
char *dataFormat;
char *organism;
char *db, *previousDb;    
float minBlocks, minMatch;
boolean multiple, fudgeThick;
int minSizeQ, minSizeT;

/* char *err = NULL; */
struct liftOverChain *chainList = NULL, *choice;

cart = theCart;

if (cgiOptionalString(HGLFT_ERRORHELP_VAR))
    {
    puts("<PRE>");
    puts(liftOverErrHelp());
    //system("/usr/bin/cal");
    puts("</PRE>");
    return;
    }

/* Get data to convert - from userData variable, or if 
 * that is empty from a file. */

if (cartOptionalString(cart, "SubmitFile"))
    userData = cartOptionalString(cart, HGLFT_DATAFILE_VAR);
else
    userData = cartOptionalString(cart, HGLFT_USERDATA_VAR);
dataFormat = cartCgiUsualString(cart, HGLFT_DATAFORMAT_VAR, DEFAULT_FORMAT);
cartWebStart(cart, "Convert Browser Coordinates");

getDbAndGenome(cart, &db, &organism);
previousDb = hPreviousAssembly(db);

chainList = liftOverChainList();
choice = defaultChoices(chainList);

minSizeQ = cartCgiUsualInt(cart, HGLFT_MINSIZEQ, choice->minSizeQ);
minSizeT = cartCgiUsualInt(cart, HGLFT_MINSIZET, choice->minSizeT);
minBlocks = cartCgiUsualDouble(cart, HGLFT_MINBLOCKS, choice->minBlocks);
minMatch = cartCgiUsualDouble(cart, HGLFT_MINMATCH, choice->minMatch);
fudgeThick = cartCgiUsualBoolean(cart, HGLFT_FUDGETHICK, (choice->fudgeThick[0]=='Y') ? TRUE : FALSE);
multiple = cartCgiUsualBoolean(cart, HGLFT_MULTIPLE, (choice->multiple[0]=='Y') ? TRUE : FALSE);

webMain(choice, dataFormat);
liftOverChainFreeList(&chainList);

if (userData != NULL && userData[0] != '\0')
    {
    struct hash *chainHash = newHash(0);
    char *chainFile;
    struct tempName oldTn, mappedTn, unmappedTn;
    FILE *old, *mapped, *unmapped;
    char *line;
    int lineSize;
    struct lineFile *errFile;
    char *fromDb, *toDb;
    int ct = 0, errCt = 0;

    /* read in user data and save to file */
    makeTempName(&oldTn, HGLFT, ".user");
    old = mustOpen(oldTn.forCgi, "w");
    fputs(userData, old);
    fputs("\n", old);           /* in case user doesn't end last line */
    carefulClose(&old);
    chmod(oldTn.forCgi, 0666);

    /* setup output files -- one for converted lines, the other
     * for lines that could not be mapped */
    makeTempName(&mappedTn, HGLFT, ".bed");
    makeTempName(&unmappedTn, HGLFT, ".err");
    mapped = mustOpen(mappedTn.forCgi, "w");
    chmod(mappedTn.forCgi, 0666);
    unmapped = mustOpen(unmappedTn.forCgi, "w");
    chmod(unmappedTn.forCgi, 0666);

    fromDb = cgiString(HGLFT_FROMDB_VAR);
    toDb = cgiString(HGLFT_TODB_VAR);
    chainFile = liftOverChainFile(fromDb, toDb);
    if (chainFile == NULL)
        errAbort("ERROR: Can't convert from %s to %s: no chain file loaded",
                                fromDb, toDb);
    readLiftOverMap(chainFile, chainHash);
    if (sameString(dataFormat, WIGGLE_FORMAT))
        /* TODO: implement Wiggle */
            {}
    else if (sameString(dataFormat, POSITION_FORMAT))
        {
        ct = liftOverPositions(oldTn.forCgi, chainHash, 
                        minMatch, minBlocks,
                        fudgeThick, mapped, unmapped, &errCt);
        }
    else if (sameString(dataFormat, BED_FORMAT))
        {
/* minSizeT here and in liftOverChain.c/h has been renamed minChainT in liftOver.c */
        ct = liftOverBed(oldTn.forCgi, chainHash, 
			 minMatch, minBlocks, 0, minSizeQ,
			 minSizeT, 0,
			 fudgeThick, mapped, unmapped, multiple, NULL, &errCt);
        }
    else
        /* programming error */
        errAbort("ERROR: Unsupported data format: %s\n", dataFormat);

    webNewSection("Results");
    if (ct)
        {
        /* some records succesfully converted */
        cgiParagraph("");
        printf("Successfully converted %d record", ct);
        printf("%s: ", ct > 1 ? "s" : "");
        printf("<A HREF=%s TARGET=_blank>View Conversions</A>\n", mappedTn.forCgi);
        }
    if (errCt)
        {
        /* some records not converted */
        cgiParagraph("");
        printf("Conversion failed on %d record", errCt);
        printf("%s. &nbsp;&nbsp;&nbsp;", errCt > 1 ? "s" : "");
        printf("<A HREF=%s TARGET=_blank>Display failure file</A>&nbsp; &nbsp;\n",
                         unmappedTn.forCgi);
        printf("<A HREF=\"/cgi-bin/hgLiftOver?%s=1\" TARGET=_blank>Explain failure messages</A>\n", HGLFT_ERRORHELP_VAR);
        puts("<P>Failed input regions:\n");
        fclose(unmapped);
        errFile = lineFileOpen(unmappedTn.forCgi, TRUE);
        puts("<BLOCKQUOTE><PRE>\n");
        while (lineFileNext(errFile, &line, &lineSize))
            puts(line);
        puts("</PRE></BLOCKQUOTE>\n");
        }
    }
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "SubmitFile",
                        HGLFT_USERDATA_VAR,
                        HGLFT_DATAFILE_VAR,
                        HGLFT_ERRORHELP_VAR,
                        NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}

