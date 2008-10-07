/* hgPal - URL entry point to library pal routines */
#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "web.h"
#include "hdb.h"
#include "hui.h"
#include "pal.h"

static char const rcsid[] = "$Id: hgPal.c,v 1.12 2008/10/07 17:02:13 ann Exp $";

char *excludeVars[] = {"Submit", "submit", NULL,};

void addOurButtons()
{
cgiMakeButton("Submit", "Submit");
}

void doMiddle(struct cart *cart)
/* Set up globals and make web page */
{
char *track = cartString(cart, "g");
char *chrom = cartOptionalString(cart, "c");
char *item = cartOptionalString(cart, "i");
int start = cartInt(cart, "l");
int end = cartInt(cart, "r");
char *database;
char *genome;

getDbAndGenome(cart, &database, &genome, NULL);
struct sqlConnection *conn = hAllocConn(database);
cartWebStart(cart, database, "Protein Alignments for %s %s",track,item);

/* output the option selection dialog */
palOptions(cart, conn, addOurButtons, NULL);

printf("For information about output data format see the "
  "<A HREF=\"../goldenPath/help/hgTablesHelp.html#FASTA\">User's Guide</A><BR>");

struct bed *bed;
AllocVar(bed);
bed->name = item;
bed->chromStart = start;
bed->chromEnd = end;
bed->chrom = chrom;

printf("<pre>");
/* output the alignments */
if (palOutPredsInBeds(conn, cart, bed, track) == 0)
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
