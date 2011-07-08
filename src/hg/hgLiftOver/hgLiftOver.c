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

static char const rcsid[] = "$Id: hgLiftOver.c,v 1.62 2009/07/14 20:17:30 markd Exp $";

/* CGI Variables */
#define HGLFT_USERDATA_VAR "hglft_userData"     /* typed/pasted in data */
#define HGLFT_DATAFILE_VAR "hglft_dataFile"     /* file of data to convert */
#define HGLFT_FROMORG_VAR "hglft_fromOrg"         /* FROM organism */
#define HGLFT_FROMDB_VAR "hglft_fromDb"         /* FROM assembly */
#define HGLFT_TOORG_VAR   "hglft_toOrg"           /* TO organism */
#define HGLFT_TODB_VAR   "hglft_toDb"           /* TO assembly */
#define HGLFT_ERRORHELP_VAR "hglft_errorHelp"      /* Print explanatory text */
#define HGLFT_REFRESHONLY_VAR "hglft_doRefreshOnly"      /* Just refresh drop-down lists */
#define HGLFT_LAST_CHAIN "hglft_lastChain"

/* liftOver options: */
#define HGLFT_MINMATCH "hglft_minMatch"          
#define HGLFT_MINSIZEQ "hglft_minSizeQ"
#define HGLFT_MINCHAINT "hglft_minChainT"
#define HGLFT_MULTIPLE "hglft_multiple"
#define HGLFT_MINBLOCKS "hglft_minBlocks"
#define HGLFT_FUDGETHICK "hglft_fudgeThick"

/* Global Variables */
struct cart *cart;	        /* CGI and other variables */
struct hash *oldVars = NULL;

/* Filename prefix */
#define HGLFT   "hglft"

/* Javascript to support New Assembly pulldown when Orig Assembly changes */
/* Copies selected value from the Original Assembly pulldown to a hidden form
*/
char *onChange = 
"onchange=\"document.mainForm."
HGLFT_REFRESHONLY_VAR
".value = 1;"
"document.mainForm.submit();\"";

char *chainStringVal(struct liftOverChain *chain)
/* keep the last chain in memory in this format */
{
char chainS[64];
safef(chainS, sizeof(chainS), "%s.%s", chain->fromDb, chain->toDb);
return cloneString(chainS);
}

void webMain(struct liftOverChain *chain, boolean multiple, boolean keepSettings, int minSizeQ, 
	     int minChainT, float minBlocks, float minMatch, boolean fudgeThick)
/* set up page for entering data */
{
struct dbDb *dbList;
char *fromOrg = hArchiveOrganism(chain->fromDb), *toOrg = hArchiveOrganism(chain->toDb);
char *chainString = chainStringVal(chain);
cgiParagraph(
    "This tool converts genome coordinates and genome annotation files "
    "between assemblies.&nbsp;&nbsp;"
    "The input data can be pasted into the text box, or uploaded from a file.&nbsp;&nbsp;"
    "If a pair of assemblies cannot be selected from the pull-down menus,"
    " a direct lift between them is unavailable.&nbsp;&nbsp;"
    "However, a sequential lift may be possible.&nbsp;&nbsp;"
    "Example: lift from Mouse, May 2004, to Mouse, Feb. 2006, and then from Mouse, "
    "Feb. 2006 to Mouse, July 2007 to achieve a lift from mm5 to mm9.&nbsp;&nbsp;"
    "");

/* create HMTL form */
puts("<FORM ACTION=\"../cgi-bin/hgLiftOver\" METHOD=\"POST\" "
       " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* create HTML table for layout purposes */
puts("\n<TABLE WIDTH=\"100%\">\n");

/* top two rows -- genome and assembly menus */
cgiSimpleTableRowStart();
cgiTableField("Original Genome: ");
cgiTableField("Original Assembly: ");
cgiTableField("New Genome: ");
cgiTableField("New Assembly: ");
cgiTableRowEnd();

cgiSimpleTableRowStart();

/* genome */
cgiSimpleTableFieldStart();
dbList = hGetLiftOverFromDatabases();
printSomeGenomeListHtmlNamed(HGLFT_FROMORG_VAR, chain->fromDb, dbList, onChange);
cgiTableFieldEnd();

/* from assembly */
cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(chain->fromDb, dbList, HGLFT_FROMDB_VAR, 
			     TRUE, onChange);
cgiTableFieldEnd();

/* to assembly */

cgiSimpleTableFieldStart();
dbDbFreeList(&dbList);
dbList = hGetLiftOverToDatabases(chain->fromDb);
printLiftOverGenomeList(HGLFT_TOORG_VAR, chain->toDb, dbList, onChange);
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(chain->toDb, dbList, HGLFT_TODB_VAR, TRUE, "");
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

cgiParagraph("&nbsp;");
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiTableField("Minimum ratio of bases that must remap:");
cgiSimpleTableFieldStart();
cgiMakeDoubleVar(HGLFT_MINMATCH, (keepSettings) ? minMatch : chain->minMatch,6);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("<B>BED 4 to BED 6 Options</B>");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Allow multiple output regions:");
cgiSimpleTableFieldStart();
cgiMakeCheckBox(HGLFT_MULTIPLE,multiple);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;&nbsp;Minimum hit size in query:");
cgiSimpleTableFieldStart();
cgiMakeIntVar(HGLFT_MINSIZEQ,(keepSettings) ? minSizeQ : chain->minSizeQ,4);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;&nbsp;Minimum chain size in target:");
cgiSimpleTableFieldStart();
cgiMakeIntVar(HGLFT_MINCHAINT,(keepSettings) ? minChainT : chain->minChainT,4);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("<B>BED 12 Options</B>");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Min ratio of alignment blocks or exons that must map:");
cgiSimpleTableFieldStart();
cgiMakeDoubleVar(HGLFT_MINBLOCKS,(keepSettings) ? minBlocks : chain->minBlocks,6);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("If thickStart/thickEnd is not mapped, use the closest mapped base:");
cgiSimpleTableFieldStart();
cgiMakeCheckBox(HGLFT_FUDGETHICK,(keepSettings) ? fudgeThick : (chain->fudgeThick[0]=='Y'));
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();

/* text box and two buttons (submit, reset) */
cgiParagraph("&nbsp;Paste in data:\n");
cgiSimpleTableStart();
cgiSimpleTableRowStart();

cgiSimpleTableFieldStart();
cgiMakeTextArea(HGLFT_USERDATA_VAR, cartCgiUsualString(cart, HGLFT_USERDATA_VAR, NULL), 10, 80);
cgiTableFieldEnd();

/* right element of table is a nested table
 * with two buttons stacked on top of each other */
cgiSimpleTableFieldStart();
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeSubmitButton();
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeClearButton("mainForm", HGLFT_USERDATA_VAR);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

/* next  row -- file upload controls */
cgiParagraph("&nbsp;Or upload data from a file:");
cgiSimpleTableStart();
cgiSimpleTableRowStart();
printf("<TD><INPUT TYPE=FILE NAME=\"%s\"></TD>\n", HGLFT_DATAFILE_VAR);
puts("<TD><INPUT TYPE=SUBMIT NAME=SubmitFile VALUE=\"Submit File\"></TD>\n");
cgiTableRowEnd();
cgiTableEnd();
printf("<input type=\"hidden\" name=\"%s\" value=\"0\">\n",
                        HGLFT_REFRESHONLY_VAR);
printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n", HGLFT_LAST_CHAIN, chainString);
puts("</FORM>\n");

cartSaveSession(cart);
puts("</FORM>");
freeMem(fromOrg);
freeMem(toOrg);
}

void webParamsUsed(float minMatch, boolean multiple, int minSizeQ, int minChainT, float minBlocks, boolean fudgeThick)
{
webNewSection("Parameters Used");
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiTableField("Minimum ratio of bases that must remap:");
cgiSimpleTableFieldStart();
printf("%.2f",minMatch);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("BED 4 to BED 6 Options");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Allow multiple output regions:");
cgiSimpleTableFieldStart();
printf("%s", multiple ? "on" : "off");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;&nbsp;Minimum hit size in query:");
cgiSimpleTableFieldStart();
printf("%d",minSizeQ);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;&nbsp;Minimum chain size in target:");
cgiSimpleTableFieldStart();
printf("%d",minChainT);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("BED 12 Options");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Min ratio of alignment blocks or exons that must map:");
cgiSimpleTableFieldStart();
printf("%.2f",minBlocks);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("If thickStart/thickEnd is not mapped, use the closest mapped base:");
cgiSimpleTableFieldStart();
printf("%s", fudgeThick ? "on" : "off");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
}

void webDataFormats()
{
webNewSection("Data Formats");
puts("<ul>");
puts("<LI>");
puts(
    "<A HREF=\"../goldenPath/help/customTrack.html#BED\" TARGET=_blank>"
    "Browser Extensible Data (BED)</A>\n");
puts("</LI>");
puts("<LI>");
puts("Genomic Coordinate Position<BR>");
puts("&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; chrN<B>:</B>start<B>-</B>end");
puts("</LI>");
puts("</ul>");
}

void webDownloads()
{
webNewSection("Command Line Tool");
cgiParagraph(
"To lift genome annotations locally on Linux systems, download the "
"<A HREF=\"http://hgdownload.cse.ucsc.edu/admin/exe/\">" 
"<I>liftOver</I></A> executable and the appropriate "
"<A HREF=\"http://hgdownload.cse.ucsc.edu/downloads.html#liftover\">"
"chain file</A>."
" Run <I>liftOver</I> with no arguments to see the usage message.\n");
}


double scoreLiftOverChain(struct liftOverChain *chain,
    char *fromOrg, char *fromDb, char *toOrg, char *toDb,
    char *cartOrg, char *cartDb, struct hash *dbRank, 
    struct hash *dbHash)
/* Score the chain in terms of best match for cart settings */
{
double score = 0;
struct dbDb *chainFromDbDb = hashFindVal(dbHash, chain->fromDb);
struct dbDb *chainToDbDb = hashFindVal(dbHash, chain->toDb);

char *chainFromOrg = (chainFromDbDb) ? chainFromDbDb->organism : NULL;
char *chainToOrg = (chainToDbDb) ? chainToDbDb->organism : NULL;
int fromRank = hashIntValDefault(dbRank, chain->fromDb, 0);  /* values up to approx. #assemblies */
int toRank = hashIntValDefault(dbRank, chain->toDb, 0);
int maxRank = hashIntVal(dbRank, "maxRank"); 

if (!chainFromOrg || !chainToOrg)
    return 0;

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

if (sameOk(cartDb,chain->fromDb)) 
    score +=  20000;
if (sameOk(cartDb,chain->toDb)) 
    score +=  10000;

if (sameOk(cartOrg,chainFromOrg)) 
    score +=  2000;
if (sameOk(cartOrg,chainToOrg)) 
    score +=  1000;

score += 10*(maxRank-fromRank);
score += (maxRank - toRank);

return score;
}


struct liftOverChain *defaultChoices(struct liftOverChain *chainList,
				     char *cartDb)
/* Out of a list of liftOverChains and a cart, choose a
 * list to display. */
{
char *fromOrg, *fromDb, *toOrg, *toDb, *cartOrg;
struct liftOverChain *choice = NULL;  
struct hash *dbRank = hGetDatabaseRank();
struct hash *dbDbHash = hDbDbAndArchiveHash();
double bestScore = -1;
struct liftOverChain *this = NULL;

/* Get the initial values. */
fromOrg = cartCgiUsualString(cart, HGLFT_FROMORG_VAR, "0");
fromDb = cartCgiUsualString(cart, HGLFT_FROMDB_VAR, "0");
toOrg = cartCgiUsualString(cart, HGLFT_TOORG_VAR, "0");
toDb = cartCgiUsualString(cart, HGLFT_TODB_VAR, "0");
cartOrg = hArchiveOrganism(cartDb);

if (sameWord(fromOrg,"0"))
    fromOrg = NULL;
if (sameWord(fromDb,"0"))
    fromDb = NULL;
if (sameWord(toOrg,"0"))
    toOrg = NULL;
if (sameWord(toDb,"0"))
    toDb = NULL;
if (sameWord(cartDb,"0"))
    cartDb = NULL;

for (this = chainList; this != NULL; this = this->next)
    {
    double score = scoreLiftOverChain(this, fromOrg, fromDb, toOrg, toDb, cartOrg, cartDb, dbRank, dbDbHash);
    if (score > bestScore)
	{
	choice = this;
	bestScore = score;
	}
    }

return choice;
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *userData;
char *organism;
char *db;
float minBlocks, minMatch;
boolean multiple, fudgeThick;
int minSizeQ, minChainT;
boolean refreshOnly = FALSE;
boolean keepSettings = FALSE;
char *thisChain = NULL;
char *lastChain = NULL;

struct liftOverChain *chainList = NULL, *choice;

cart = theCart;

if (cgiOptionalString(HGLFT_ERRORHELP_VAR))
    {
    puts("<PRE>");
    puts(liftOverErrHelp());
    puts("</PRE>");
    return;
    }

/* Get data to convert - from userData variable, or if 
 * that is empty from a file. */

if (cartOptionalString(cart, "SubmitFile"))
    userData = cartOptionalString(cart, HGLFT_DATAFILE_VAR);
else
    userData = cartOptionalString(cart, HGLFT_USERDATA_VAR);
cartWebStart(cart, NULL, "Lift Genome Annotations");

getDbAndGenome(cart, &db, &organism, oldVars);

chainList = liftOverChainList();

choice = defaultChoices(chainList, db);
thisChain = chainStringVal(choice);
if (choice == NULL)
    errAbort("Sorry, no conversions available from this assembly\n");

minSizeQ = cartCgiUsualInt(cart, HGLFT_MINSIZEQ, choice->minSizeQ);
minChainT = cartCgiUsualInt(cart, HGLFT_MINCHAINT, choice->minChainT);
minBlocks = cartCgiUsualDouble(cart, HGLFT_MINBLOCKS, choice->minBlocks);
minMatch = cartCgiUsualDouble(cart, HGLFT_MINMATCH, choice->minMatch);
fudgeThick = cartCgiUsualBoolean(cart, HGLFT_FUDGETHICK, (choice->fudgeThick[0]=='Y') ? TRUE : FALSE);
multiple = cartCgiUsualBoolean(cart, HGLFT_MULTIPLE, (choice->multiple[0]=='Y') ? TRUE : FALSE);
refreshOnly = cartCgiUsualInt(cart, HGLFT_REFRESHONLY_VAR, 0);
lastChain = cartCgiUsualString(cart, HGLFT_LAST_CHAIN, NULL);
if (lastChain && thisChain && sameString(lastChain, thisChain))
    keepSettings = TRUE;

webMain(choice, multiple, keepSettings, minSizeQ, minChainT, minBlocks, minMatch, fudgeThick);
liftOverChainFreeList(&chainList);

if (!refreshOnly && userData != NULL && userData[0] != '\0')
    {
    struct hash *chainHash = newHash(0);
    char *chainFile;
    struct tempName oldTn, mappedTn, unmappedTn;
    FILE *old, *mapped, *unmapped;
    char *line;
    int lineSize;
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
    ct = liftOverBedOrPositions(oldTn.forCgi, chainHash, 
			minMatch, minBlocks, 0, minSizeQ,
			minChainT, 0,
			fudgeThick, mapped, unmapped, multiple, NULL, &errCt);
    if (ct == -1)
        /* programming error */
        errAbort("ERROR: Unsupported data format.\n");

    webNewSection("Results");
    if (ct > 0)
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
        printf("<A HREF=\"../cgi-bin/hgLiftOver?%s=1\" TARGET=_blank>Explain failure messages</A>\n", HGLFT_ERRORHELP_VAR);
        puts("<P>Failed input regions:\n");
        struct lineFile *errFile = lineFileOpen(unmappedTn.forCgi, TRUE);
        puts("<BLOCKQUOTE><PRE>\n");
        while (lineFileNext(errFile, &line, &lineSize))
            puts(line);
        lineFileClose(&errFile);
        puts("</PRE></BLOCKQUOTE>\n");
        }
    puts("<BLOCKQUOTE><PRE>\n");
    puts("Note: &quot;multiple&quot; option is not supported for position format.");
    puts("</PRE></BLOCKQUOTE>\n");

    webParamsUsed(minMatch, multiple, minSizeQ, minChainT, minBlocks, fudgeThick);

    carefulClose(&unmapped);
    }
webDataFormats();
webDownloads();
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
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

