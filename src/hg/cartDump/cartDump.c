/* cartDump - Dump contents of cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"

static char const rcsid[] = "$Id: cartDump.c,v 1.7 2003/06/23 16:27:07 kent Exp $";

void doMiddle(struct cart *cart)
/* cartDump - Dump contents of cart. */
{
char *vName = "cartDump.varName";
char *vVal = "cartDump.newValue";

if (cgiVarExists("submit"))
    {
    char *varName = cgiOptionalString(vName);
    char *newValue = cgiOptionalString(vVal);
    if (varName != NULL && varName[0] != 0 && newValue != NULL && newValue[0] != 0)
        {
	if (sameString(newValue, "n/a"))
	    cartRemove(cart, varName);
	else
	    cartSetString(cart, varName, newValue);
	}
    cartRemove(cart, vVal);
    cartRemove(cart, "submit");
    }
printf("<TT><PRE>");
cartDump(cart);
printf("</TT></PRE>");
printf("<FORM ACTION=\"../cgi-bin/cartDump\" METHOD=GET>\n");
printf("alter variable named: ");
cgiMakeTextVar(vName, cartUsualString(cart, vName, ""), 12);
printf("new value");
cgiMakeTextVar(vVal, "", 24);
cgiMakeButton("submit", "submit");
printf("<BR>\n");
printf("Put n/a in for the new value to clear a variable.");
}

char *excludeVars[] = { "submit", "Submit", NULL };

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartHtmlShell("Cart Dump", doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
