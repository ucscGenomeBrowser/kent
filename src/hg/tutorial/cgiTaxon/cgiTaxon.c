/* cgiTaxon - Go between taxon ID, scientific name and common name.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "spDb.h"

static char const rcsid[] = "$Id: cgiTaxon.c,v 1.1.44.1 2008/08/01 06:10:57 markd Exp $";

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

#define varPrefix "cgiTaxon."
#define typeVar  varPrefix "type"
#define textVar varPrefix "text"

char *typeNames[] = {
   "NCBI taxon",
   "Scientific Name",
   "Common Name",
   };

#define taxonType "taxon"
#define sciType "sci"
#define commonType "common"

char *types[] = {
   taxonType,
   sciType,
   commonType,
   };

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
cartWebStart(cart, database, "Go between taxon ID, scientific name and common name.");
printf("<FORM action=\"%s\">\n", "../cgi-bin/cgiTaxon");
cartSaveSession(cart);
char *type = cartUsualString(cart, typeVar, types[0]);
char *text = cartUsualString(cart, textVar, "");
cgiMakeDropListWithVals(typeVar, typeNames, types, ArraySize(types), 
	cartUsualString(cart, typeVar, type));
cgiMakeTextVar(textVar, text, 32);
cgiMakeSubmitButton();
printf("<BR>");

/* Get connection to uniprot. */
struct sqlConnection *conn = sqlConnect("uniProt");

/* Convert user input string to NCBI taxon. */
int taxon = 0;
if (sameString(type, taxonType))
    {
    taxon = atoi(text);
    }
else if (sameString(type, sciType))
    {
    taxon = spBinomialToTaxon(conn, text);
    }
else if (sameString(type, commonType))
    {
    taxon = spCommonToTaxon(conn, text);
    }
else
    errAbort("Unknown type %s", type);

/* Display taxon, scientific, common names. */
if (taxon <= 0)
    printf("Taxon %d not found<BR>\n", taxon);
else
    {
    printf("NCBI Taxon: %d<BR>\n", taxon);
    printf("Scientific names: %s<BR>\n", spTaxonToBinomial(conn, taxon));
    printf("Common name: %s<BR>\n", spTaxonToCommon(conn, taxon));
    }

sqlDisconnect(&conn);
printf("</FORM>");
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
