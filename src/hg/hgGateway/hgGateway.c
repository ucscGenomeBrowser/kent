/* hgGateway - Human Genome Browser Gateway. */
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
#include "hubConnect.h"
#include "hgConfig.h"
#include "jsHelper.h"
#include "hPrint.h"
#include "suggest.h"
#include "search.h"

static char const rcsid[] = "$Id: hgGateway.c,v 1.117 2010/04/29 02:54:35 larrym Exp $";

struct cart *cart = NULL;
struct hash *oldVars = NULL;
char *clade = NULL;
char *organism = NULL;
char *db = NULL;

void hgGateway()
/* hgGateway - Human Genome Browser Gateway. */
{
char *defaultPosition = hDefaultPos(db);
char *position = cloneString(cartUsualString(cart, "position", defaultPosition));
boolean gotClade = hGotClade();
char *survey = cfgOptionEnv("HGDB_SURVEY", "survey");
char *surveyLabel = cfgOptionEnv("HGDB_SURVEY_LABEL", "surveyLabel");
boolean supportsSuggest = assemblySupportsGeneSuggest(db);

/* JavaScript to copy input data on the change genome button to a hidden form
This was done in order to be able to flexibly arrange the UI HTML
*/
char *onChangeDB = "onchange=\"document.orgForm.db.value = document.mainForm.db.options[document.mainForm.db.selectedIndex].value; document.orgForm.submit();\"";
char *onChangeOrg = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.db.value = 0; document.orgForm.submit();\"";
char *onChangeClade = "onchange=\"document.orgForm.clade.value = document.mainForm.clade.options[document.mainForm.clade.selectedIndex].value; document.orgForm.org.value = 0; document.orgForm.db.value = 0; document.orgForm.submit();\"";

/*
   If we are changing databases via explicit cgi request,
   then remove custom track data which will
   be irrelevant in this new database .
   If databases were changed then use the new default position too.
*/

if (sameString(position, "genome") || sameString(position, "hgBatch"))
    position = defaultPosition;

jsIncludeFile("jquery.js", NULL);
#ifdef NEW_JQUERY
webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("jquery-ui.js", NULL);
printf("<script type='text/javascript'>var newJQuery=true;</script>\n");
#else///ifndef NEW_JQUERY
webIncludeResourceFile("autocomplete.css");
jsIncludeFile("jquery.autocomplete.js", NULL);
printf("<script type='text/javascript'>var newJQuery=false;</script>\n");
#endif///ndef NEW_JQUERY
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("autocomplete.js", NULL);
jsIncludeFile("hgGateway.js", NULL);
jsIncludeFile("utils.js", NULL);

puts(
"<CENTER style='font-size:small;'>"
"The UCSC Genome Browser was created by the \n"
"<A HREF=\"../staff.html\">Genome Bioinformatics Group of UC Santa Cruz</A>.\n"
"<BR>"
"Software Copyright (c) The Regents of the University of California.\n"
"All rights reserved.\n"
"</CENTER>\n"
);

puts(
"<FORM ACTION='../cgi-bin/hgTracks' NAME='mainForm' METHOD='GET' style='display:inline;'>\n"
"<CENTER>"
"<table style='background-color:#FFFEF3; border: 1px solid #CCCC99;'>\n"
"<tr><td>\n");
cgiMakeHiddenVar(hgHubConnectCgiDestUrl, "../cgi-bin/hgTracks");

puts("<table><tr>");
if (gotClade)
    puts("<td align=center valign=baseline>clade</td>");
puts(
"<td align=center valign=baseline>genome</td>\n"
"<td align=center valign=baseline>assembly</td>\n"
"<td align=center valign=baseline>position or search term</td>\n");
if(supportsSuggest)
    puts("<td align=center valign=baseline><a title='click for help on gene search box' target='_blank' href='../goldenPath/help/geneSearchBox.html'>gene</a></td>\n");
puts(
"<td align=center valign=baseline> &nbsp; </td>\n"
"</tr>\n<tr>"
);

if (gotClade)
    {
    puts("<td align=center>\n");
    printCladeListHtml(organism, onChangeClade);
    puts("</td>\n");
    }

puts("<td align=center>\n");
if (gotClade)
    printGenomeListForCladeHtml(db, onChangeOrg);
else
    printGenomeListHtml(db, onChangeOrg);
puts("</td>\n");

puts("<td align=center>\n");
printAssemblyListHtml(db, onChangeDB);
puts("</td>\n");

puts("<td align=center>\n");
cgiMakeTextVar("position", addCommasToPos(db, position), 30);
printf("</td>\n");

if(supportsSuggest)
    {
    puts("<td align=center>\n");
    hPrintf("<input name='hgt.suggest' type='text' size='5' id='suggest' />\n"
            "<input type='hidden' name='hgt.suggestTrack' id='suggestTrack' value='%s'>\n", assemblyGeneSuggestTrack(db)
            );
    printf("</td>\n");
    }

cartSetString(cart, "position", position);
cartSetString(cart, "db", db);
cartSetString(cart, "org", organism);
if (gotClade)
    cartSetString(cart, "clade", clade);

freez(&defaultPosition);
position = NULL;

puts("<td align=center>");
if(supportsSuggest)
    hButtonWithOnClick("Submit", "submit", NULL, "submitButtonOnClick()");
else
    cgiMakeButton("Submit", "submit");
/* This is a clear submit button that browsers will use by default when enter is pressed in position box. FIXME: This should be done with js onchange event! */
printf("<input TYPE=\"IMAGE\" BORDER=\"0\" NAME=\"hgt.dummyEnterButton\" src=\"../images/DOT.gif\" WIDTH=1 HEIGHT=1 ALT=dot>");
cartSaveSession(cart);  /* Put up hgsid= as hidden variable. */
puts(
"</td>\n"
"</tr></table>\n"
"</td></tr>\n");

puts(
"<tr><td><CENTER><BR>\n"
"<a HREF=\"../cgi-bin/cartReset\">Click here to reset</a> the browser user interface settings to their defaults.");

#define SURVEY 1
#ifdef SURVEY
if (survey && differentWord(survey, "off"))
    printf("&nbsp;&nbsp;&nbsp;<span style='background-color:yellow;'><A HREF=\"%s\" TARGET=_BLANK><EM><B>%s</EM></B></A></span>", survey, surveyLabel ? surveyLabel : "Take survey");
#endif

puts(
"<BR>\n"
"</CENTER>\n"
"</td></tr><tr><td><CENTER>\n"
);

puts("<TABLE BORDER=\"0\">");
puts("<TR>");

if(isSearchTracksSupported(db,cart))
    {
    puts("<TD VALIGN=\"TOP\">");
    cgiMakeButtonWithMsg(TRACK_SEARCH, TRACK_SEARCH_BUTTON,TRACK_SEARCH_HINT);
    puts("</TD>");
    }

// custom track button. disable hgCustom button on GSID server, until
// necessary additional work is authorized.
puts("<TD VALIGN=\"TOP\">");

/* disable CT for CGB servers for the time being */
if (!hIsGsidServer() && !hIsCgbServer())
    {
    boolean hasCustomTracks = customTracksExist(cart, NULL);
    printf("<input TYPE=SUBMIT onclick=\"document.mainForm.action='%s';\" VALUE='%s' title='%s'>\n",
        hgCustomName(),hasCustomTracks ? CT_MANAGE_BUTTON_LABEL:CT_ADD_BUTTON_LABEL,
        hasCustomTracks ? "Manage your custom tracks" : "Add your own custom tracks"  );
    }
puts("</TD>");

if (hubConnectTableExists())
    {
    puts("<TD VALIGN=\"TOP\">");
    printf("<input TYPE=SUBMIT onclick=\"document.mainForm.action='%s';\" VALUE='%s' title='%s'>\n",
        "../cgi-bin/hgHubConnect", "track hubs", "Import tracks");
    puts("</TD>");
    }

// configure button
puts("<TD VALIGN=\"TOP\">");
cgiMakeButtonWithMsg("hgTracksConfigPage", "configure tracks and display","Configure track selections and browser display");
puts("</TD>");

// clear possition button
puts("<TD VALIGN=\"TOP\">");
if(supportsSuggest)
    cgiMakeOnClickButton("document.mainForm.position.value=''; document.getElementById('suggest').value='';", "clear position");
else
    cgiMakeOnClickButton("document.mainForm.position.value=''", "clear position");
puts("</TD>");

puts("</TR></TABLE>");

puts("</CENTER>\n"
"</td></tr></table>\n"
);
puts("</CENTER>");
#ifdef NEW_JQUERY
hPrintf("<input type='hidden' id='hgt.newJQuery' name='hgt.newJQuery' value='1'>\n");
#endif


if(!cartVarExists(cart, "pix"))
    // put a hidden input for pix on page so default value can be filled in on the client side
    hPrintf("<input type='hidden' name='pix' value=''>\n");

puts("</FORM>");
if (hIsPreviewHost())
    {
puts("<P>"
"WARNING: This is our preview site. It is a weekly mirror of our internal development server for public access.  "
"Data and tools here are under construction, have not been quality reviewed, and are subject to change "
"at any time.  We provide this site for early access, with the warning that it is less available "
"and stable than our public site.  For high-quality reviewed annotations on our production server, visit "
"      <A HREF=\"http://genome.ucsc.edu\">http://genome.ucsc.edu</A>."
"</P><BR>");
    }
else if (hIsPrivateHost())
    {
puts("<P>WARNING: This is our development and test site.  It usually works, but it is filled with tracks in various "
"stages of construction, and others of little interest to people outside of our local group. "
"It is usually slow because we are building databases on it. The documentation is poor. "
 "More data than usual is flat out wrong.  Maybe you want to go to "
	 "<A HREF=\"http://genome.ucsc.edu\">genome.ucsc.edu</A> instead.");
    }

if (hIsGsidServer())
    {
    webNewSection("%s", "Sequence View\n");
    printf("%s",
	   "Sequence View is a customized version of the UCSC Genome Browser, which is specifically tailored to provide functions needed for the GSID HIV Data Browser.\n");
    }

hgPositionsHelpHtml(organism, db);

puts("<FORM ACTION=\"../cgi-bin/hgGateway\" METHOD=\"GET\" NAME=\"orgForm\">");
cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */
if (gotClade)
    printf("<input type=\"hidden\" name=\"clade\" value=\"%s\">\n", clade);
else
    printf("<input type=\"hidden\" name=\"clade\" value=\"%s\">\n", "mammal");

printf("<input type=\"hidden\" name=\"org\" value=\"%s\">\n", organism);
printf("<input type=\"hidden\" name=\"db\" value=\"%s\">\n", db);
puts("</FORM><BR>");
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
char *scientificName = NULL;
cart = theCart;

getDbGenomeClade(cart, &db, &organism, &clade, oldVars);
if (! hDbIsActive(db))
    {
    db = hDefaultDb();
    organism = hGenome(db);
    clade = hClade(organism);
    }
scientificName = hScientificName(db);
if (hIsGsidServer())
    cartWebStart(theCart, db, "GSID %s Sequence View (UCSC Genome Browser) Gateway \n", organism);
else
    {
    char buffer[128];

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
    cartWebStart(theCart, db, "%s %s%s Gateway\n", organism, buffer, hBrowserName());
    htmlDoEscape();
    }
hgGateway();
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
