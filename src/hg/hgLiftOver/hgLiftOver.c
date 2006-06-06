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

static char const rcsid[] = "$Id: hgLiftOver.c,v 1.43 2006/06/06 00:46:34 galt Exp $";

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

/* Javascript to support New Assembly pulldown when Orig Assembly changes */
/* Copies selected value from the Original Assembly pulldown to a hidden form
*/
char *onChangeFromOrg = 
"onchange=\"document.dbForm.hglft_fromOrg.value = "
"document.mainForm.hglft_fromOrg.options[document.mainForm.hglft_fromOrg.selectedIndex].value;"
"document.dbForm.hglft_fromDb.value = 0;"
"document.dbForm.submit();\"";

char *onChangeFromDb = 
"onchange=\"document.dbForm.hglft_fromDb.value = "
"document.mainForm.hglft_fromDb.options[document.mainForm.hglft_fromDb.selectedIndex].value;"
"document.dbForm.submit();\"";

char *onChangeToOrg = 
"onchange=\"document.dbForm.hglft_toOrg.value = "
"document.mainForm.hglft_toOrg.options[document.mainForm.hglft_toOrg.selectedIndex].value;"
"document.dbForm.hglft_toDb.value = 0;"
"document.dbForm.submit();\"";

void webMain(struct liftOverChain *chain, char *dataFormat)
/* set up page for entering data */
{
struct dbDb *dbList;
char *fromOrg = hArchiveOrganism(chain->fromDb), *toOrg = hArchiveOrganism(chain->toDb);
cgiParagraph(
    "This tool converts genome coordinates and genome annotation files "
    "between assemblies.&nbsp;&nbsp;"
    "The input data can be pasted into the text box, or uploaded from a file."
    "");

/* create HMTL form */
puts("<FORM ACTION=\"../cgi-bin/hgLiftOver\" METHOD=\"POST\" "
       " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* create HTML table for layout purposes */
puts("\n<TABLE WIDTH=\"100%%\">\n");

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
printSomeGenomeListHtmlNamed(HGLFT_FROMORG_VAR, chain->fromDb, dbList, onChangeFromOrg);
cgiTableFieldEnd();

/* from assembly */
cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(chain->fromDb, dbList, HGLFT_FROMDB_VAR, 
			     TRUE, onChangeFromDb);
cgiTableFieldEnd();

/* to assembly */

cgiSimpleTableFieldStart();
dbDbFreeList(&dbList);
dbList = hGetLiftOverToDatabases(chain->fromDb);
printSomeGenomeListHtmlNamed(HGLFT_TOORG_VAR, chain->toDb, dbList, onChangeToOrg);
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(chain->toDb, dbList, HGLFT_TODB_VAR, TRUE, "");
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

cgiParagraph("&nbsp;");
cgiSimpleTableStart();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiTableField("Minimum ratio of bases that must remap:");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeDoubleVar(HGLFT_MINMATCH,chain->minMatch,6);
cgiTableFieldEnd();
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiTableField("Minimum chain size in target:");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeIntVar(HGLFT_MINSIZET,chain->minSizeT,4);
cgiTableFieldEnd();
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiTableField("Minimum hit size in query:");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeIntVar(HGLFT_MINSIZEQ,chain->minSizeQ,4);
cgiTableFieldEnd();
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiTableField("Allow multiple output regions:");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeCheckBox(HGLFT_MULTIPLE,(chain->multiple[0]=='Y') ? TRUE : FALSE);
cgiTableFieldEnd();
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiTableField("Min ratio of alignment blocks/exons that must map:");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeDoubleVar(HGLFT_MINBLOCKS,chain->minBlocks,6);
cgiTableFieldEnd();
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiTableField("If thickStart/thickEnd is not mapped, use the closest mapped base:");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeCheckBox(HGLFT_FUDGETHICK,(chain->fudgeThick[0]=='Y') ? TRUE : FALSE);
cgiTableFieldEnd();
cgiTableRowEnd();
cgiTableEnd();

/* next row -- file format menu */
//printf("Data input formats marked with star (*) are suitable for "
        //"ENCODE data submission.&nbsp;&nbsp;"
cgiParagraph(
         "&nbsp;For descriptions of the supported data formats, see the bottom of this page.");
cgiSimpleTableStart();
cgiSimpleTableRowStart();
cgiTableField("Data Format: ");
cgiSimpleTableFieldStart();
cgiMakeDropList(HGLFT_DATAFORMAT_VAR, 
                formatList, sizeof(formatList)/sizeof (char*) - 1, dataFormat);
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
puts("</FORM>\n");

/* Hidden form to support menu pulldown behavior */
printf("<FORM ACTION=\"/cgi-bin/hgLiftOver\""
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

void webDataFormats()
{
webNewSection("Data Formats");
puts("<LI>");
puts(
    "<A HREF=\"/goldenPath/help/customTrack.html#BED\" TARGET=_blank>"
    //"<A HREF=\"http://genome.ucsc.edu/goldenPath/help/customTrack.html#BED\" TARGET=_blank>"
    "Browser Extensible Data (BED)</A>\n");
puts("</LI>");
puts("<LI>");
puts("Genomic Coordinate Position<BR>");
puts("&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; chrN<B>:</B>start<B>-</B>end");
puts("</LI>");
}

void webDownloads()
{
webNewSection("Command Line Tool");
cgiParagraph(
"To lift genome annotations locally on Linux systems, download the "
"<A HREF=\"http://www.soe.ucsc.edu/~kent/exe/linux/liftOver.gz\">" 
"<I>liftOver</I></A> executable and the appropriate "
"<A HREF=\"http://hgdownload.cse.ucsc.edu/downloads.html#liftover\">"
"chain file</A>."
" Run <I>liftOver</I> with no arguments to see the usage message.\n");
}

/* dead code will not need this

struct liftOverChain *findLiftOverChain(struct liftOverChain *chainList, char *fromDb, char *toDb)
/ * Return TRUE if there's a chain with both fromDb and
 * toDb. * /
{
struct liftOverChain *chain;
if (!fromDb || !toDb)
    return NULL;
for (chain = chainList; chain != NULL; chain = chain->next)
    if (sameString(chain->fromDb,fromDb) && sameString(chain->toDb,toDb))
	return chain;
return NULL;
}

*/

boolean sameOk(char *a, char *b)
/* Return TRUE if the strings are the same, otherwise FALSE. 
 * Tolerates NULL pointers */
{
return differentStringNullOk(a,b) == 0;
}

int scoreLiftOverChain(struct liftOverChain *chain,
    char *fromOrg, char *fromDb, char *toOrg, char *toDb, char *orgFromDb, char *orgToDb, 
    char *cartOrg, char *cartDb, struct hash *dbRank )
/* Score the chain in terms of best match for cart settings */
{
int score = 0;

char *chainFromOrg = hArchiveOrganism(chain->fromDb);
char *chainToOrg = hArchiveOrganism(chain->toDb);
int fromRank = hashIntValDefault(dbRank, chain->fromDb, 0);
int toRank = hashIntValDefault(dbRank, chain->toDb, 0);
int maxRank = hashIntVal(dbRank, "maxRank");

if (fromRank == 0 || toRank == 0) /* not an active db in dbDb or archiveDbDb. */
    return -1;    /*  toOrg and toDb lists would not display properly */

if (sameOk(fromDb,chain->fromDb) && sameOk(toDb,chain->toDb))
    score += 1000000;

if (sameOk(fromDb,chain->fromDb)) 
    score += 200000;
if (sameOk(fromOrg,chainFromOrg)) 
    score += 100000;

if (sameOk(toDb,chain->toDb))
    score += 20000;
if (sameOk(toOrg,chainToOrg))
    score += 10000;

if (sameOk(orgFromDb,chainFromOrg)) 
    score += 20000;
if (sameOk(orgToDb,chainToOrg))
    score += 10000;

if (sameOk(cartDb,chain->fromDb)) 
    score +=  5000;
if (sameOk(cartDb,chain->toDb)) 
    score +=  3000;

if (sameOk(cartOrg,chainFromOrg)) 
    score +=  2000;
if (sameOk(cartOrg,chainToOrg)) 
    score +=  1000;

score += 10*(maxRank-fromRank);
score += (maxRank - toRank);

return score;
}


struct liftOverChain *defaultChoices(struct liftOverChain *chainList)
/* Out of a list of liftOverChains and a cart, choose a
 * list to display. */
{
char *fromOrg, *fromDb, *toOrg, *toDb, *orgFromDb, *orgToDb, *cartDb, *cartOrg;
struct liftOverChain *choice = chainList;  /* default to first one */
struct hash *dbRank = hGetDatabaseRank();
int bestScore = -1;
struct liftOverChain *this = NULL;

/* Get the initial values. */
fromOrg = cartCgiUsualString(cart, HGLFT_FROMORG_VAR, "0");
fromDb = cartCgiUsualString(cart, HGLFT_FROMDB_VAR, "0");
toOrg = cartCgiUsualString(cart, HGLFT_TOORG_VAR, "0");
toDb = cartCgiUsualString(cart, HGLFT_TODB_VAR, "0");
orgFromDb = hArchiveOrganism(fromDb); 
orgToDb = hArchiveOrganism(toDb);
cartDb = cartCgiUsualString(cart, "db", "0");
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
    int score = scoreLiftOverChain(this, fromOrg, fromDb, toOrg, toDb, orgFromDb, orgToDb, cartOrg, cartDb, dbRank);
    if (score > bestScore)
	{
	choice = this;
	bestScore = score;
	}
    }  

if (!choice)
    choice = chainList;
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
cartWebStart(cart, "Lift Genome Annotations");

getDbAndGenome(cart, &db, &organism);
previousDb = hPreviousAssembly(db);

chainList = liftOverChainListFiltered();

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
/* minSizeT here and in liftOverChain.c/h has been renamed minChainT in liftOver.c */
	ct = liftOverPositions(oldTn.forCgi, chainHash, 
			minMatch, minBlocks, 0, minSizeQ,
			minSizeT, 0, 
			fudgeThick, mapped, unmapped, multiple, NULL, &errCt);

	
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
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}

