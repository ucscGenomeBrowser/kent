/* hgPal - URL entry point to library pal routines */
#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "web.h"
#include "hdb.h"
#include "hui.h"
#include "pal.h"

static char const rcsid[] = "$Id: hgPal.c,v 1.10 2008/09/26 18:48:34 braney Exp $";

char *excludeVars[] = {"Submit", "submit", NULL,};

void addOurButtons()
{
cgiMakeButton("Submit", "Submit");
}

void doMiddle(struct cart *cart)
/* Set up globals and make web page */
{
char *track = cartString(cart, "g");
char *item = cartOptionalString(cart, "i");
char *database;
char *genome;

getDbAndGenome(cart, &database, &genome, NULL);
struct sqlConnection *conn = hAllocConn(database);
cartWebStart(cart, database, "Protein Alignments for %s %s",track,item);

/* output the option selection dialog */
palOptions(cart, conn, addOurButtons, NULL);

printf("For information about output data format see "
  "<A HREF=\"../goldenPath/help/hgTablesHelp.html#FASTA\">Table Browser User's Guide</A><BR>");
struct hash *hash = newHash(1);

/* we're only showing the one name */
hashStore(hash, item);
printf("<pre>");

/* output the alignments */
if (palOutPredsInHash(conn, cart, hash, track) == 0)
    printf("<B>No coding region in gene '%s'</B><BR>",item);

cartHtmlEnd();
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
