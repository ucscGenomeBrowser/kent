/* hgSeqSearch - CGI-script to manage fast human genome sequence searching. */
#include "common.h"
#include "errabort.h"
#include "hCommon.h"
#include "jksql.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "dbDb.h"
#include "blatServers.h"
#include "web.h"
#include "hash.h"
#include "botDelay.h"
#include "liftOver.h"

static char const rcsid[] = "$Id: hgLiftOver.c,v 1.8 2004/03/24 04:15:41 kate Exp $";

/* CGI Variables */
#define HGLFT_USERDATA_VAR "hglft.userData"     /* typed/pasted in data */
#define HGLFT_DATAFILE_VAR "hglft.dataFile"     /* file of data to convert */
#define HGLFT_DATAFORMAT_VAR "hglft.dataFormat" /* format of data to convert */
#define HGLFT_SHOWPAGE_CMD "hglft.do_showPage"  /* command to display output */

/* Global Variables */
struct cart *cart;	        /* CGI and other variables */
struct hash *oldCart = NULL;

char *formatList[] = 
        //{"Position", "BED*", "MAF*", "Wiggle*", "GFF", 0};
        //{"Position", "BED", 0};
        {"BED", 0};

char *genomeList[] = {"Human", 0};
//char *origAssemblyList[] = {"June 2002", "Nov. 2002", "April 2003", 0};
//char *newAssemblyList[] = {"Nov. 2002", "April 2003", "July 2003", 0};
char *origAssemblyList[] = {"April 2003"};
char *newAssemblyList[] = {"July 2003"};

void webMain(struct sqlConnection *conn, char *err)
/* set up page for entering data */
{
if (err != NULL)
    printf("<H4 ALIGN=CENTER>ERROR: %s</H4>\n", err); 

cgiParagraph(
    "This tool converts genome coordinates and genome annotation files "
    "between assemblies.&nbsp;&nbsp;"
    "The input data can be pasted into the text box, or uploaded from a file."
    "");

/* create HMTL form */
printf("<FORM ACTION=\"../cgi-bin/hgLiftOver\" METHOD=\"POST\" "
       " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* create HTML table for layout purposes */
printf("\n<TABLE WIDTH=\"100%%\">\n");

/* top two rows -- genome and assembly menus */
cgiSimpleTableRowStart();
cgiTableField("Genome: ");
cgiTableField("Original Assembly: ");
cgiTableField("New Assembly: ");
cgiTableRowEnd();

cgiSimpleTableRowStart();

cgiSimpleTableFieldStart();
cgiMakeDropList("genome", genomeList, 1, "Human");
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
cgiMakeDropList("origAssembly", origAssemblyList, 1, "April 2003");
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
cgiMakeDropList("newAssembly", newAssemblyList, 1, "July 2003");
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
cgiMakeDropList(HGLFT_DATAFORMAT_VAR, formatList, 1, "BED");
cgiTableFieldEnd();
cgiTableRowEnd();
cgiTableEnd();

/* text box and two buttons (submit, reset) */
cgiParagraph("&nbsp;Paste in data:\n");
cgiSimpleTableStart();
cgiSimpleTableRowStart();

cgiSimpleTableFieldStart();
cgiMakeTextArea(HGLFT_USERDATA_VAR, NULL, 10, 80);
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
cgiMakeResetButton();
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
printf("<TD><INPUT TYPE=SUBMIT NAME=SubmitFile VALUE=\"Submit File\"></TD>\n");
cgiTableRowEnd();
cgiTableEnd();
printf("</FORM>\n");

cgiParagraph("Results will appear below.");
}

void webDataFormats()
{
webNewSection("Data Formats");
printf("<UL>");
/*
printf("<LI>");
printf("For <B>Position</B> format, enter the <I>chromosome</I>, <I>start</I>, and <I>end</I> positions, in the format <B>chrN:S-E</B>\n");
*/
printf("<LI>");
printf(
    "<A HREF=\"/goldenPath/help/customTrack.html#BED\" TARGET=_blank>"
    //"<A HREF=\"http://genome.ucsc.edu/goldenPath/help/customTrack.html#BED\" TARGET=_blank>"
    "Browser Extensible Data (BED)</A>\n");
printf("</UL>");
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *userData;
char *dataFile;
char *db, *organism;    
char *showPage = FALSE;
char *err = NULL;
struct sqlConnection *conn = NULL;
cart = theCart;
conn = hAllocConn();

//getDbAndGenome(cart, &db, &organism);

/* Get data to convert - from userData variable, or if 
 * that is empty from a file. */

if (cartOptionalString(cart, "SubmitFile"))
    userData = cartOptionalString(cart, HGLFT_DATAFILE_VAR);
else
    userData = cartOptionalString(cart, HGLFT_USERDATA_VAR);
showPage = cartOptionalString(cart, "showPage");
cartWebStart(cart, "Lift Genome Annotations");
webMain(conn, err);

if (showPage || userData == NULL || userData[0] == '\0')
    {
    /* display main form to enter input annotation data */
    webDataFormats();
    }
else 
    {
    struct hash *chainHash = newHash(0);
    struct tempName oldTn, mappedTn, unmappedTn;
    FILE *old, *mapped, *unmapped;
    char *line;
    int lineSize;
    struct lineFile *errFile;
    int ct;
    int errCt;

    /* read in user data and save to file */
    makeTempName(&oldTn, "hglft", ".user");
    old = mustOpen(oldTn.forCgi, "w");
    fputs(userData, old);
    carefulClose(&old);
    chmod(oldTn.forCgi, 0666);

    /* setup output files -- one for converted lines, the other
     * for lines that could not be mapped */
    makeTempName(&mappedTn, "hglft", ".bed");
    makeTempName(&unmappedTn, "hglft", ".err");
    mapped = mustOpen(mappedTn.forCgi, "w");
    chmod(mappedTn.forCgi, 0666);
    unmapped = mustOpen(unmappedTn.forCgi, "w");
    chmod(unmappedTn.forCgi, 0666);

    //readLiftOverTable("hg16", "chainHg15", chainHash);
    readLiftOverMap("/usr/local/apache/htdocs/encode/hg15toHg16.chain", 
                        chainHash);
    ct = liftOverBed(
                oldTn.forCgi, chainHash, LIFTOVER_MINMATCH, LIFTOVER_MINBLOCKS,
                                 mapped, unmapped, &errCt);
    webNewSection("Results");
    if (ct)
        {
        /* some records succesfully converted */
        cgiParagraph("");
        printf("Successfully converted %d record", ct);
        printf("%s: ", ct > 1 ? "s" : "");
        printf("<A HREF=%s TARGET=_blank>View File</A>\n", mappedTn.forCgi);
        }
    if (errCt)
        {
        /* some records not converted */
        cgiParagraph("");
        printf("Conversion failed on %d record", errCt);
        printf("%s: &nbsp;&nbsp;&nbsp;", errCt > 1 ? "s" : "");
        printf("<A HREF=%s TARGET=_blank>View File</A>\n",
                         unmappedTn.forCgi);
        fclose(unmapped);
        errFile = lineFileOpen(unmappedTn.forCgi, TRUE);
        printf("<BLOCKQUOTE>\n");
        printf("<PRE>\n");
        while (lineFileNext(errFile, &line, &lineSize))
            {
            puts(line);
            }
        printf("</PRE>\n");
        printf("</BLOCKQUOTE>\n");
        }
    webDataFormats();
    /* remove temp files */
    //remove(oldTn.forCgi);
    cartRemove(cart, HGLFT_USERDATA_VAR);
    }
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "SubmitFile",
                        HGLFT_USERDATA_VAR,
                        HGLFT_DATAFILE_VAR,
                        HGLFT_SHOWPAGE_CMD,
                        NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
//htmlSetBackground("../images/floret.jpg");
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}

