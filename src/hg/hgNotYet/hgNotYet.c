/* hgNotYet - Human Genome Browser. */
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
#include "customTrack.h"

static char const rcsid[] = "$Id: hgNotYet.c,v 1.2 2007/01/24 18:28:30 fanhsu Exp $";

boolean isPrivateHost;		/* True if we're on genome-test. */
struct cart *cart = NULL;
struct hash *oldVars = NULL;
char *clade = NULL;
char *organism = NULL;
char *db = NULL;

void hgNotYet()
/* hgNotYet - Human Genome Browser. */
{
printf("This function is currently under construction.<BR>Please visit us again in the future.\n");
return;
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
char *scientificName = NULL;
cart = theCart;

getDbGenomeClade(cart, &db, &organism, &clade);
if (! hDbIsActive(db))
    {
    db = hDefaultDb();
    organism = hGenome(db);
    clade = hClade(organism);
    }
scientificName = hScientificName(db);
if (hIsMgcServer())
    cartWebStart(theCart, "MGC/ORFeome %s Genome Browser \n", organism);
else if (hIsGsidServer())
    cartWebStart(theCart, "GSID %s Genome Browser \n", organism);
else
    {
    char buffer[128];
    char *browserName = (isPrivateHost ? "TEST Genome Browser" : "Genome Browser");

    /* tell html routines *not* to escape htmlOut strings*/
    htmlNoEscape();
    buffer[0] = 0;
    if (*scientificName != 0)
	{
	if (sameString(clade,"ancestor"))
	    safef(buffer, sizeof(buffer), "(<I>%s</I> Ancestor) ", scientificName);
	else
	    safef(buffer, sizeof(buffer), "(<I>%s</I>) ", scientificName);
	}
    cartWebStart(theCart, "%s %s%s Gateway\n", organism, buffer, browserName);
    htmlDoEscape();
    }
hgNotYet();
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
isPrivateHost = hIsPrivateHost();
oldVars = hashNew(8);
cgiSpoof(&argc, argv);

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
