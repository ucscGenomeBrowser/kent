/* cartDump - Dump contents of cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jsHelper.h"
#include "hui.h"

static char const rcsid[] = "$Id: cartDump.c,v 1.14 2008/12/09 00:41:20 angie Exp $";

#define CART_DUMP_REMOVE_VAR "n/a"

void doMiddle(struct cart *cart)
/* cartDump - Dump contents of cart. */
{
#define MATCH_VAR  "match"

char *vName = "cartDump.varName";
char *vVal = "cartDump.newValue";
char *wildcard;
boolean asTable = cartVarExists(cart,CART_DUMP_AS_TABLE);

if (cgiVarExists("submit"))
    {
    char *varName = cgiOptionalString(vName);
    char *newValue = cgiOptionalString(vVal);
    if (isNotEmpty(varName) && isNotEmpty(newValue))
        {
	varName = skipLeadingSpaces(varName);
	eraseTrailingSpaces(varName);
	if (sameString(newValue, CART_DUMP_REMOVE_VAR) || sameString(newValue, CART_VAR_EMPTY))
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
if (asTable)
    {
    jsIncludeFile("utils.js",NULL);
    jsIncludeFile("ajax.js",NULL);
    printf("<A HREF='../cgi-bin/cartDump?%s=[]'>Show as plain text.</a><BR>",CART_DUMP_AS_TABLE);
    printf("<FORM ACTION=\"../cgi-bin/cartDump\" METHOD=GET>\n");
    cartSaveSession(cart);
    printf("<em>Variables can be altered by changing the values and then leaving the field (onchange event will use ajax).\n");
    printf("Enter </em><B><code style='color:%s'>%s</code></B><em> or </em><B><code style='color:%s'>%s</code></B><em> to remove a variable.</em>",
        COLOR_DARKBLUE,CART_DUMP_REMOVE_VAR,COLOR_DARKBLUE,CART_VAR_EMPTY);
    printf("<BR><em>Add a variable named:</em> ");
    cgiMakeTextVar(vName, "", 12);
    printf(" <em>value:</em> ");
    cgiMakeTextVar(vVal, "", 24);
    printf("<BR>");
    cgiMakeButton("submit", "submit");
    //printf("<BR><A HREF='../cgi-bin/cartDump'><INPUT TYPE='button' VALUE='Refresh'></a><BR>");
    printf("</FORM>\n");
    }
else
    {
    printf("<A HREF='../cgi-bin/cartDump?%s=1'>Show as updatable table.</a><BR>",CART_DUMP_AS_TABLE);
    }
printf("<TT><PRE>");
wildcard = cgiOptionalString(MATCH_VAR);
if (wildcard)
    cartDumpLike(cart, wildcard);
else
    cartDump(cart);
printf("</TT></PRE>");
if (!asTable)
    {
    printf("<FORM ACTION=\"../cgi-bin/cartDump\" METHOD=GET>\n");
    cartSaveSession(cart);
    printf("<em>Add/alter a variable named:</em> ");
    cgiMakeTextVar(vName, cartUsualString(cart, vName, ""), 12);
    printf(" <em>new value</em> ");
    cgiMakeTextVar(vVal, "", 24);
    printf(" ");
    cgiMakeButton("submit", "submit");
    printf("<BR>Put </em><B><code style='color:%s'>%s</code></B><em> in for the new value to clear a variable.</em>",
        COLOR_DARKBLUE,CART_DUMP_REMOVE_VAR);
    printf("</FORM>\n");
    }
printf("<P><em>Cookies passed to</em> %s:<BR>\n%s\n</P>\n",
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
