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

static char const rcsid[] = "$Id: hgLiftOver.c,v 1.2 2004/03/18 04:18:13 kate Exp $";

struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

char *formatList[] = 
        {"Position", "BED*", "MAF*", "Wiggle*", "GFF", 0};

char *genomeList[] = {"Human", 0};
char *origAssemblyList[] = {"June 2002", "Nov. 2002", "April 2003", 0};
char *newAssemblyList[] = {"Nov. 2002", "April 2003", "July 2003", 0};

void doMiddle(struct cart *theCart)
{
char *userData;
char *db, *organism;

//getDbAndGenome(cart, &db, &organism);
cart = theCart;

cartWebStart(cart, "Convert Genome Coordinates");

printf("<P>"
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
cartSaveSession(cart);

/* create HMTL form */
printf("<FORM ACTION=\"../cgi-bin/hgLiftOver\" METHOD=\"POST\" "
       " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");

/* create HTML table for layout purposes */
printf("<TABLE WIDTH=\"100%%\">\n");

/* top two rows -- genome and assembly menus */
printf("<TR>\n");
printf("<TD> Original Genome: </TD>");
printf("<TD> Assembly: </TD>");
printf("<TD> New Genome: </TD>");
printf("<TD> Assembly: </TD>");
printf("</TR>\n");

printf("<TR>\n");
printf("<TD>");
cgiMakeDropList("origGenome", genomeList, 1, "Human");
printf("</TD>");
printf("<TD>");
cgiMakeDropList("origAssembly", origAssemblyList, 3, "April 2003");
printf("</TD>");
printf("<TD>");
cgiMakeDropList("newGenome", genomeList, 1, "Human");
printf("</TD>");
printf("<TD>");
cgiMakeDropList("newAssembly", newAssemblyList, 3, "July 2003");
printf("</TD>");
printf("</TR>\n");
printf("</TABLE>\n");

/* next row -- file format menu */
printf("<P>\n");
printf("Data input formats marked with star (*) are suitable for "
        "ENCODE data submission.&nbsp;&nbsp;"
        "For the \"position\" format, enter a list of "
        "chromosome positions using the format chrN:X.\n");
printf("<TABLE>\n");
printf("<TR>\n");
printf("<TD>");
printf("Format: ");
printf("</TD>");
printf("<TD>");
cgiMakeDropList("dataFormat", formatList, 5, "BED*");
printf("</TD>");
printf("</TR>\n");
printf("</TABLE>\n");

/* text box and two buttons (submit, reset) */
printf("<P>\n");
printf("Paste in data:\n");
printf("<TABLE>\n");
printf("<TR>\n");
printf("<TD>\n");
printf("<TEXTAREA NAME=userData ROWS=14 COLS=70> </TEXTAREA>\n");
printf("</TD>\n");
printf("<TD>\n");

printf("<TABLE>\n");
printf("<TR><TD><INPUT TYPE=SUBMIT NAME=Submit VALUE=Submit></TD></TR>\n");
printf("<TR><TD><INPUT TYPE=RESET NAME=Reset VALUE=Reset></TD></TR>\n");
printf("</TABLE>\n");

printf("</TABLE>\n");

/* next  row -- file upload controls */
printf("<P>\n");
printf("Or upload data from a file:\n");
printf("<TABLE>\n");
printf("<TR>\n");
printf("<TD><INPUT TYPE=FILE NAME=\"dataFile\"></TD>\n");
printf("<TD><INPUT TYPE=SUBMIT NAME=Submit VALUE=Submit File></TD>\n");
printf("</TR>\n");
printf("</TABLE>\n");

cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", 
                        "hglft.genome", "hglft.userData", "hglft.dataFile", 
                        NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);
//htmlSetBackground("../images/floret.jpg");
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

