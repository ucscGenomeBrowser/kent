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

static char const rcsid[] = "$Id: hgLiftOver.c,v 1.4 2004/03/22 22:43:41 kate Exp $";

/* CGI Variables */
#define HGLFT_USERDATA_VAR "hglft.userData"     /* typed/pasted in data */
#define HGLFT_DATAFILE_VAR "hglft.dataFile"     /* file of data to convert */
#define HGLFT_DATAFORMAT_VAR "hglft.dataFormat" /* format of data to convert */
#define HGLFT_CMD_PREFIX "hglft.do_"            /* prefix for commands */
#define HGLFT_SHOWPAGE_CMD "hglft.do_showPage"  /* command to display output */

/* Global Variables */
struct cart *cart;	        /* CGI and other variables */
struct hash *oldCart = NULL;

char *formatList[] = 
        //{"Position", "BED*", "MAF*", "Wiggle*", "GFF", 0};
        {"Position", "BED", 0};

char *genomeList[] = {"Human", 0};
//char *origAssemblyList[] = {"June 2002", "Nov. 2002", "April 2003", 0};
//char *newAssemblyList[] = {"Nov. 2002", "April 2003", "July 2003", 0};
char *origAssemblyList[] = {"April 2003"};
char *newAssemblyList[] = {"July 2003"};

void webMain(struct sqlConnection *conn)
/* set up page for entering data */
{
char *userData;
char *db, *organism;

cgiParagraph(
    "This tool converts genome coordinates and genome annotation files "
    "between assemblies.&nbsp;&nbsp;"
    "The input data can be pasted into the text box, or uploaded from a file."
    "");
/* Get data to convert - from userData variable, or if 
 * that is empty from a file. */
/*
userData = cartOptionalString(cart, "hglft.userData");
if(userData != 0 && userData[0] == '\0')
    {
    userData = cartOptionalString(cart, "hglft.dataFile");
    }
    */

/* create HMTL form */
printf("<FORM ACTION=\"../cgi-bin/hgLiftOver\" METHOD=\"POST\" "
       " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cgiMakeHiddenVar(HGLFT_USERDATA_VAR, "");
cgiMakeHiddenVar(HGLFT_DATAFILE_VAR, "");
cgiMakeHiddenVar(HGLFT_DATAFILE_VAR, "BED");
cgiMakeHiddenVar(HGLFT_SHOWPAGE_CMD, "true");
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
cgiMakeDropList(HGLFT_DATAFORMAT_VAR, formatList, 2, "BED");
cgiTableFieldEnd();
cgiTableRowEnd();
cgiTableEnd();

/* text box and two buttons (submit, reset) */
cgiParagraph("&nbsp;Paste in data:\n");
cgiSimpleTableStart();
cgiSimpleTableRowStart();

cgiSimpleTableFieldStart();
cgiMakeTextArea(HGLFT_USERDATA_VAR, NULL, 15, 80);
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
printf("<TD><INPUT TYPE=SUBMIT NAME=Submit VALUE=\"Submit File\"></TD>\n");
cgiTableRowEnd();
cgiTableEnd();

printf("</FORM>\n");

webNewSection("Data Formats");
printf("<UL>");
printf("<LI>");
printf("For <B>Position</B> format, enter the <I>chromosome</I>, <I>start</I>, and <I>end</I> positions, in the format <B>chrN:S-E</B>\n");
printf("<LI>");
printf("<A HREF=\"http://genome.ucsc.edu/goldenPath/help/customTrack.html#BED\">Browser Extensible Data (BED)</A>\n");
printf("</UL>");
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
struct sqlConnection *conn = NULL;
cart = theCart;
conn = hAllocConn();

//getDbAndGenome(cart, &db, &organism);
cart = theCart;
cartWebStart(cart, "Lift Genome Annotations");
webMain(conn);
cartWebEnd();

cartRemovePrefix(cart, HGLFT_CMD_PREFIX);
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", 
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

