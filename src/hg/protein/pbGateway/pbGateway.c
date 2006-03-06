/* pbGateway - Human Proteome Browser Gateway. */
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

void pbGateway()
/* pbGateway - Human Proteome Browser Gateway. */
{
char *oldDb = NULL;
char *oldOrg = NULL;
char *defaultPosition = hDefaultPos(db);
char *position = cloneString(cartUsualString(cart, "position", defaultPosition));

/* 
   If we are changing databases via explicit cgi request,
   then remove custom track data which will 
   be irrelevant in this new database .
   If databases were changed then use the new default position too.
*/

oldDb = hashFindVal(oldVars, dbCgiName);
oldOrg = hashFindVal(oldVars, orgCgiName);
if ((oldDb && !containsStringNoCase(oldDb, db))
|| (oldOrg && !containsStringNoCase(oldOrg, organism)))
    {
    position = defaultPosition;
    }
if (sameString(position, "proteome") || sameString(position, "hgBatch"))
    position = defaultPosition;

puts(
"<FORM ACTION=\"/cgi-bin/pbGlobal\" NAME=\"mainForm\" METHOD=\"GET\">\n"
"<CENTER>"
"<TABLE BGCOLOR=\"FFFEF3\" BORDERCOLOR=\"cccc99\" BORDER=0 CELLPADDING=1>\n"
"<TR><TD><FONT SIZE=\"2\">\n"
"<CENTER>\n"
"The UCSC Proteome Browser was created by the \n"
"<A HREF=\"/staff.html\">Genome Bioinformatics Group of UC Santa Cruz</A>.\n"
"<BR>"
"Software Copyright (c) The Regents of the University of California.\n"
"All rights reserved.\n"
"</CENTER>\n"
"</FONT></TD></TR></TABLE></CENTER>\n"
);

puts("<BR><B>Enter a gene symbol or a Swiss-Prot/TrEMBL protein ID: </B>");
puts("<input type=\"text\" name=\"proteinID\" >\n");
puts("<INPUT TYPE=SUBMIT>");

puts("</FORM>\n"
);

printf("<FORM ACTION=\"/cgi-bin/pbGateway\" METHOD=\"GET\" NAME=\"orgForm\"><input type=\"hidden\" name=\"org\" value=\"%s\">\n", organism);
printf("<input type=\"hidden\" name=\"db\" value=\"%s\">\n", db);
cartSaveSession(cart);
puts("</FORM><BR>");
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
cart = theCart;

getDbAndGenome(cart, &db, &organism);
if (! hDbIsActive(db))
    {
    db = hDefaultDb();
    organism = hGenome(db);
    }
if (hIsMgcServer())
    cartWebStart(theCart, "MGC Proteome Browser Gateway \n");
else
    cartWebStart(theCart, "UCSC Proteome Browser Gateway \n");

/* start with a clean slate */
cartResetInDb(hUserCookie());

pbGateway();
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
