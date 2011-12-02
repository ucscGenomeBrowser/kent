/* testCart - Test cart routines.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"



void doMiddle(struct cart *cart)
/* Print out middle parts. */
{
char *old;

printf("<FORM ACTION=\"../cgi-bin/testCart\" METHOD=GET>\n");
cartSaveSession(cart, "testCart");

printf("<H3>Just a Test</H3>\n");
printf("<B>Filter:</B> ");
old = cartUsualString(cart, "filter", "red");
cgiMakeRadioButton("filter", "red", sameString(old, "red"));
printf("red ");
cgiMakeRadioButton("filter", "green", sameString(old, "green"));
printf("green ");
cgiMakeRadioButton("filter", "blue", sameString(old, "blue"));
printf("blue ");
cgiMakeButton("submit", "Submit");
printf("<BR>\n");

printf("<B>Font Attributes:</B> ");
cgiMakeCheckBox("fBold", cartUsualBoolean(cart, "fBold", FALSE, "testCart"));
printf("bold ");
cgiMakeCheckBox("fItalic", cartUsualBoolean(cart, "fItalic", FALSE, "testCart"));
printf("italic ");
cgiMakeCheckBox("fUnderline", cartUsualBoolean(cart, "fUnderline", FALSE, "testCart"));
printf("underline ");
printf("<BR>\n");

printf("</FORM>");
printf("<TT><PRE>");
cartDump(cart);
}

char *exclude[] = {"hgsid", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartHtmlShell("testCart", doMiddle, "hguid", exclude);
return 0;
}
