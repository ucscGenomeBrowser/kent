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

static char const rcsid[] = "$Id: hgLiftOver.c,v 1.29 2004/10/06 23:03:14 kate Exp $";

/* CGI Variables */
#define HGLFT_USERDATA_VAR "hglft_userData"     /* typed/pasted in data */
#define HGLFT_DATAFILE_VAR "hglft_dataFile"     /* file of data to convert */
#define HGLFT_DATAFORMAT_VAR "hglft_dataFormat" /* format of data to convert */
#define HGLFT_FROMDB_VAR "hglft_fromDb"         /* FROM assembly */
#define HGLFT_TODB_VAR   "hglft_toDb"           /* TO assembly */
#define HGLFT_ERRORHELP_VAR "hglft_errorHelp"      /* Print explanatory text */


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
char *onChangeFromDb = 
"onchange=\"document.dbForm.hglft_fromDb.value = "
"document.mainForm.hglft_fromDb.options[document.mainForm.hglft_fromDb.selectedIndex].value;"
"document.dbForm.submit();\"";

void webMain(char *organism, char *fromDb, char *toDb, char *dataFormat)
    /* set up page for entering data */
{
    struct dbDb *dbList;

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
cgiTableField("Genome: ");
cgiTableField("Original Assembly: ");
cgiTableField("New Assembly: ");
cgiTableRowEnd();

cgiSimpleTableRowStart();

/* genome */
cgiSimpleTableFieldStart();
dbList = hGetLiftOverFromDatabases();
printSomeGenomeListHtml(fromDb, dbList, "");
cgiTableFieldEnd();

/* from assembly */
cgiSimpleTableFieldStart();
printAllAssemblyListHtmlParm(fromDb, dbList, HGLFT_FROMDB_VAR, 
                                TRUE, onChangeFromDb);
cgiTableFieldEnd();

/* to assembly */
cgiSimpleTableFieldStart();
if (dbList)
    dbDbFreeList(&dbList);
dbList = hGetLiftOverToDatabases(fromDb);
printAllAssemblyListHtmlParm(toDb, dbList, HGLFT_TODB_VAR, TRUE, "");
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
       " METHOD=\"GET\" NAME=\"dbForm\">"
       "<input type=\"hidden\" name=\"%s\" value=\"%s\">\n", 
                        HGLFT_FROMDB_VAR, fromDb);
printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n",
                        HGLFT_TODB_VAR, toDb);
cartSaveSession(cart);
puts("</FORM>");
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

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *userData;
char *dataFile;
char *dataFormat;
char *organism;
char *db, *previousDb;    
char *fromDb, *toDb;
char *err = NULL;
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
fromDb = cartCgiUsualString(cart, HGLFT_FROMDB_VAR, previousDb);
toDb = cartCgiUsualString(cart, HGLFT_TODB_VAR, db);
webMain(organism, fromDb, toDb, dataFormat);

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
                        LIFTOVER_MINMATCH, LIFTOVER_MINBLOCKS,
                        FALSE, mapped, unmapped, &errCt);
        }
    else if (sameString(dataFormat, BED_FORMAT))
        {
        ct = liftOverBed(oldTn.forCgi, chainHash, 
                        LIFTOVER_MINMATCH, LIFTOVER_MINBLOCKS,
                        0, 0,
                        FALSE, mapped, unmapped, FALSE, NULL, &errCt);
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
        printf("%s: &nbsp;&nbsp;&nbsp;", errCt > 1 ? "s" : "");
        printf("<A HREF=%s TARGET=_blank>View Failure File</A>\n",
                         unmappedTn.forCgi);
        fclose(unmapped);
        errFile = lineFileOpen(unmappedTn.forCgi, TRUE);
        puts("<BLOCKQUOTE>\n");
        puts("<PRE>\n");
        while (lineFileNext(errFile, &line, &lineSize))
            {
            puts(line);
            }
        puts("</PRE>\n");
        puts("</BLOCKQUOTE>\n");
        printf("<A HREF=\"/cgi-bin/hgLiftOver?%s=1\" TARGET=_blank>Failure Messages</A>\n", HGLFT_ERRORHELP_VAR);
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

