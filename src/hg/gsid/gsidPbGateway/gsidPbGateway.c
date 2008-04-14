/* gsidPbGateway - GSID Protein View Gateway. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "obscure.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "dbDb.h"
#include "hgFind.h"
#include "hCommon.h"
#include "hui.h"

struct cart *cart = NULL;
struct hash *oldVars = NULL;
static char * const orgCgiName = "org";
static char * const dbCgiName = "db";
char *organism = NULL;
char *db = NULL;

void gsidPbGateway()
/* gsidPbGateway - GSID Protein View. */
{
char *defaultPosition = hDefaultPos(db);
char *position = cloneString(cartUsualString(cart, "position", defaultPosition));

if (sameString(position, "proteome") || sameString(position, "genome") ||
    sameString(position, "hgBatch"))
    position = defaultPosition;

puts(
"<FORM ACTION=\"../cgi-bin/pbGsid\" NAME=\"mainForm\" METHOD=\"GET\">\n"
"<CENTER>"
"<TABLE BGCOLOR=\"FFFEF3\" BORDERCOLOR=\"cccc99\" BORDER=0 CELLPADDING=1>\n"
"<TR><TD><FONT SIZE=\"2\">\n"
"<CENTER>\n"
"The GSID Protein View was created by the \n"
"<A HREF=\"../staff.html\">Genome Bioinformatics Group of UC Santa Cruz</A>.\n"
"<BR>"
"Software Copyright (c) The Regents of the University of California.\n"
"All rights reserved.\n"
"</CENTER>\n"
"</FONT></TD></TR></TABLE></CENTER>\n"
);

puts("<BR><B>Enter a VAX003 or VAX004 protein ID: </B>");
puts("<input type=\"text\" name=\"proteinID\" >\n");
puts("<INPUT TYPE=SUBMIT>");

puts("</FORM>\n"
);

printf("<FORM ACTION=\"../cgi-bin/pbGsidGateway\" METHOD=\"GET\" NAME=\"orgForm\"><input type=\"hidden\" name=\"org\" value=\"%s\">\n", organism);
printf("<input type=\"hidden\" name=\"db\" value=\"%s\">\n", db);
cartSaveSession(cart);
puts("</FORM><BR>");
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
cart = theCart;

getDbAndGenome(cart, &db, &organism, oldVars);
if (! hDbIsActive(db))
    {
    db = hDefaultDb();
    organism = hGenome(db);
    }

if (hIsGsidServer())
    {
    cartWebStart(theCart, "GSID Protein View Gateway \n");
    }
else
    {
    errAbort("This Customized Proteome Browser should be used on a GSID server only.");
    }

/* start with a clean slate */
cartResetInDb(hUserCookie());  
/* This cartResetInDb does nothing since database will be overwritten
 * with memory by cartWebEnd ... unless pgGateway crashes, in which
 * case along with the crash it will wipe out all the users settings.
 * Is that a nice thing to do?  -Jim */

gsidPbGateway();
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
