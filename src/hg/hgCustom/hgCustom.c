/* hgCustom - Custom track management CGI. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cart.h"
#include "cheapcgi.h"
#include "web.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "hCommon.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: hgCustom.c,v 1.6 2006/05/12 23:55:01 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCustom - Custom track management CGI\n"
  "usage:\n"
  "   hgCustom <CGI settings>\n"
  );
}

/* Global variables */
struct cart *cart;
struct hash *oldCart = NULL;
char *excludeVars[] = {"Submit", "submit", NULL};
char *database;
char *organism;

void hgCustom()
/* hgCustom - Custom track management CGI. */
{
char option[64];
struct customTrack *ct, *ctList;
struct slName *browserLines = NULL;
char *ctFileName;
int i;

cartWebStart(cart, "Add Custom Track");
puts("Display your own custom annotation tracks in the browser"
     " using the procedure described in the custom tracks"
"<A TARGET=_BLANK HREF=\"../goldenPath/help/customTrack.html\"> user's guide </A>."
     " For information on upload procedures and supported formats, see "
     "the \"Loading Custom Annotation Tracks\" section, below.");

/* create HMTL form */
puts("<FORM ACTION=\"/cgi-bin/hgCustom\" METHOD=\"POST\" "
               " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");

/* add your own */
/* text box and two buttons (submit, reset) */
cgiParagraph("&nbsp;Paste in data or URL:\n");
cgiSimpleTableStart();
cgiSimpleTableRowStart();

cgiSimpleTableFieldStart();
cgiMakeTextArea("hgt.customText", "", 10, 60);
cgiTableFieldEnd();

/* right element of table is a nested table
    * with two buttons stacked on top of each other */
cgiSimpleTableFieldStart();
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeSubmitButton();
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeResetButton("mainForm", "hgt.customText");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

/* next  row -- file upload controls */
cgiSimpleTableStart();
cgiSimpleTableRowStart();
puts("<TD>&nbsp;Or upload data from a file:</TD>");
printf("<TD><INPUT TYPE=FILE NAME=\"%s\"></TD>\n", "hgt.customFile");
puts("<TD><INPUT TYPE=SUBMIT NAME=SubmitFile VALUE=\"Submit File\"></TD>\n");
cgiTableRowEnd();

cgiSimpleTableRowStart();
puts("<TD>&nbsp;Optional description file (HTML):</TD>");
printf("<TD><INPUT TYPE=FILE NAME=\"%s\"></TD>\n", "hgt.customHtmlFile");
cgiTableRowEnd();
cgiTableEnd();

/* list of tracks to allow removing */
ctList = customTracksParseCart(cart, &browserLines, &ctFileName);
if (ctList != NULL)
    {
    webNewSection("Remove Custom Tracks");
    hTableStart();
    puts("<TR><TH align=LEFT colspan=3 BGCOLOR=#536ED3>");
    printf("<B>&nbsp;%s</B> ", wrapWhiteFont("Custom Tracks"));
    for (i = 0; i < 50; i++)
        puts("&nbsp;");
    cgiMakeButton("delete", "Delete");
    puts("</TH></TR>");
    ctList = customTracksParseCart(cart, &browserLines, &ctFileName);
    for (ct = ctList; ct != NULL; ct = ct->next)
        {
        printf("<TR><TD> %s </TD><TD> %s </TD><TD>", 
                ct->tdb->shortLabel, ct->tdb->longLabel);
        safef(option, sizeof(option), "%s", ct->tdb->tableName);
        cgiMakeCheckBox(option, FALSE);
        puts("</TD></TR>");
        }
    hTableEnd();
    webEndSection();
    }
puts("</FORM>\n");

webNewSection("Loading Custom Tracks");

puts("<P>A data file in one of the supported custom track \n"
    "<A HREF=\"../goldenPath/help/customTrack.html#format\" \n"
    "TARGET=_blank>formats</A> may be uploaded \n"
    "by any of the following methods: \n"
    "<UL> \n"
    "<LI>Pasting the custom annotation text directly into the large text box above\n"
    "<LI>Clicking the &quot;browse&quot; button and choosing a custom annotation \n"
    "located on your computer\n"
    "<LI>Entering a URL for the custom annotation in the large text box above \n"
    "</UL></P>\n"
    "<P>Multiple URLs may be entered into the text box, one per line.\n"
    "The Genome Browser supports the HTTP and FTP (passive only) URL protocols.\n"
    "</P> \n"
    "<P>Data compressed by any of the following programs may be loaded: \n"
    "gzip (<em>.gz</em>), compress (<em>.Z</em>) or bzip2 (<em>.bz2</em>). \n"
    "The filename must include the extension indicated. </P>\n"
    "<P>If a login and password is required to access data loaded through \n"
    "a URL, this information can be included in the URL using the format \n"
    "<em>protocol://user:password@server.com/somepath</em>. Only Basic \n"
    "Authentication is \n"
    "supported for HTTP. Note that passwords included in URLs are <B>not</B> \n"
    "protected. If a password contains a non-alphanumeric character, such as \n"
    "@, the character must be replaced by the hexidecimal \n"
    "representation for that character. For example, in the password \n"
    "<em>mypwd@wk</em>, the @ character should be replaced by \n"
    "%40, resulting in the modified password <em>mypwd%40wk</em>. \n"
    "</P>\n");

webEndSection();
cartWebEnd(cart);
}

void doMiddle(struct cart *theCart)
/* Print the body of an html file.   */
{
cart = theCart;
getDbAndGenome(cart, &database, &organism);
saveDbAndGenome(cart, database, organism);
hSetDb(database);

hgCustom();
}

int main(int argc, char *argv[])
/* Process command line. */
{
//htmlPushEarlyHandlers();
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
/*
cartHtmlShell("Custom Track Manager", doMiddle, hUserCookie(), 
        excludeVars, oldCart);
        */
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}
