/* cartDump - Dump contents of cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"

static char const rcsid[] = "$Id: cartDump.c,v 1.14 2008/12/09 00:41:20 angie Exp $";

void doMiddle(struct cart *cart)
/* cartDump - Dump contents of cart. */
{
#define MATCH_VAR  "match"

char *vName = "cartDump.varName";
char *vVal = "cartDump.newValue";
char *wildcard;

if (cgiVarExists("submit"))
    {
    char *varName = cgiOptionalString(vName);
    char *newValue = cgiOptionalString(vVal);
    if (isNotEmpty(varName) && isNotEmpty(newValue))
        {
	varName = skipLeadingSpaces(varName);
	eraseTrailingSpaces(varName);
	if (sameString(newValue, "n/a"))
	    cartRemove(cart, varName);
	else
	    cartSetString(cart, varName, newValue);
	}
    cartRemove(cart, vVal);
    cartRemove(cart, "submit");
    }
if (cgiVarExists("noDisplay"))
    {
    return;
    }
printf("<TT><PRE>");
wildcard = cgiOptionalString(MATCH_VAR);
if (wildcard)
    cartDumpLike(cart, wildcard);
else
    cartDump(cart);
printf("</TT></PRE>");
printf("<FORM ACTION=\"../cgi-bin/cartDump\" METHOD=GET>\n");
cartSaveSession(cart);
printf("alter variable named: ");
cgiMakeTextVar(vName, cartUsualString(cart, vName, ""), 12);
printf(" new value ");
cgiMakeTextVar(vVal, "", 24);
printf(" ");
cgiMakeButton("submit", "submit");
printf("<BR>\n");
printf("Put n/a in for the new value to clear a variable.");
printf("<P>Cookies passed to %s:<BR>\n%s\n</P>\n",
       cgiServerName(), getenv("HTTP_COOKIE"));
}

char *excludeVars[] = { "submit", "Submit", "noDisplay", MATCH_VAR, NULL };

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartHtmlShell("Cart Dump", doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
