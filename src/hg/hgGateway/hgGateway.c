/* hgGateway - Human Genome Browser Gateway. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "dbDb.h"
#include "web.h"
#include "hgFind.h"

struct cart *cart = NULL;
struct hash *oldVars = NULL;
static char * const orgCgiName = "org";
static char * const dbCgiName = "db";
char *organism = NULL;
char *db = NULL;

/*
  Remove any custom track data from the cart.
*/
void removeCustomTrackData()
{
cartRemove(cart, "hgt.customText");
cartRemove(cart, "hgt.customFile");
cartRemove(cart, "ct");
}

void hgGateway()
/* hgGateway - Human Genome Browser Gateway. */
{
char *oldDb = NULL;
char *defaultPosition = hDefaultPos(db);
char *position = cartUsualString(cart, "position", defaultPosition);

/* JavaScript to copy input data on the change genome button to a hidden form
This was done in order to be able to flexibly arrange the UI HTML
*/
char *onChangeText = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.submit();\"";
/* 
   If we are changing databases via explicit cgi request,
   then remove custom track data which will 
   be irrelevant in this new database .
   If databases were changed then use the new default position too.
*/
oldDb = hashFindVal(oldVars, dbCgiName);
if (!strstrNoCase(oldDb, db))
    {
    position = defaultPosition;
    removeCustomTrackData();
    }

puts(
"<CENTER>"
"Web tool created by "
"<A HREF=\"http://www.soe.ucsc.edu/~kent\">Jim Kent</A>, "
"<A HREF=\"http://www.soe.ucsc.edu/~sugnet\">Charles Sugnet</A>, "
"<A HREF=\"http://www.soe.ucsc.edu/~booch\">Terry Furey</A> and "
"<A HREF=\"http://www.soe.ucsc.edu/~haussler\">David Haussler</A> "
"of UC Santa Cruz.<P></CENTER><P>\n");

puts(
"<center>\n"
"<table bgcolor=\"cccc99\" border=\"0\" CELLPADDING=1 CELLSPACING=0>\n"
"<tr><td>\n"
"<table BGCOLOR=\"FEFDEF\" BORDERCOLOR=\"CCCC99\" BORDER=0 CELLPADDING=0 CELLSPACING=0>\n"  
"<tr><td>\n"
);

puts(
"<table bgcolor=\"FFFEF3\" border=0>\n"
"<tr>\n"
"<td>\n"
"<FORM ACTION=\"/cgi-bin/hgTracks\" NAME=\"mainForm\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">\n"
"<input TYPE=\"IMAGE\" BORDER=\"0\" NAME=\"hgt.dummyEnterButton\" src=\"/images/DOT.gif\">\n"
"<table><tr>\n"
"<td align=center valign=baseline>genome</td>\n"
"<td align=center valign=baseline>assembly</td>\n"
"<td align=center valign=baseline>position</td>\n"
"<td align=center valign=baseline>image width</td>\n"
);

puts("<tr><td align=center>\n");
printGenomeListHtml(db, onChangeText);
puts("</td>\n");

puts("<td align=center>\n");
printAssemblyListHtml(db);
puts("</td>\n");

puts("<td align=center>\n");
cgiMakeTextVar("position", position, 30);
printf("</td>\n");

freez(&defaultPosition);
position = NULL;

puts("<td align=center>\n");
cgiMakeIntVar("pix", cartUsualInt(cart, "pix", 610), 4);
printf("</td>\n");
printf("<td align=center>");
cgiMakeButton("Submit", "Submit");
cartSaveSession(cart);
printf("</td>\n");

puts(
"</tr></table>\n"
"</td></tr><tr><td><center>\n"
"<a HREF=\"../cgi-bin/cartReset\">Click here to reset</a> the browser user interface settings to their defaults.\n"
"</center>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
"</center>\n"
);

hgPositionsHelpHtml(organism);

webNewSection("Add Your Own Tracks");

puts(
"<P>You can add your own annotation tracks to the browser using \n"
"the <A HREF=\"../goldenPath/help/customTrack.html\"> \n"
"procedure described here</A>.  Annotations can be stored in files or\n"
"pasted in. You can also paste in a URL or list of URLs which refer to \n"
"files in one of the supported formats.</P>\n"
"\n"
"	Annotation File: <INPUT TYPE=FILE NAME=\"hgt.customFile\"><BR>\n"
"	<TEXTAREA NAME=\"hgt.customText\" ROWS=14 COLS=80></TEXTAREA>\n"
"	</FORM>\n"
);

printf("<FORM ACTION=\"/cgi-bin/hgGateway\" METHOD=\"GET\" NAME=\"orgForm\"><input type=\"hidden\" name=\"org\" value=\"%s\">\n", organism);
cartSaveSession(cart);
puts("</FORM>"
"	<BR></TD><TD WIDTH=15>&nbsp;</TD></TR></TABLE>\n"
"	</TD></TR></TABLE>\n"
"			\n"
"</TD></TR></TABLE>\n");
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
cart = theCart;

getDbAndGenome(cart, &db, &organism);
cartWebStart(theCart, "%s Genome Browser Gateway \n", organism);
hgGateway();
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);

cartEmptyShell(doMiddle, "hguid", excludeVars, oldVars);
return 0;
}
