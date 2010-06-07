/* great - stuff related to GREAT. */

#include "common.h"
#include "cheapcgi.h"
#include "hgTables.h"
#include "cart.h"
#include "dystring.h"
#include "textOut.h"
#include "trashDir.h"

static char const rcsid[] = "$Id: great.c,v 1.3 2010/06/01 20:09:36 galt Exp $";

static struct dyString *getRequestName()
{
char *track = cartString(cart, hgtaTrack);
char *table = cartString(cart, hgtaTable);
struct dyString *name;

if (differentStringNullOk(track, table))
    name = dyStringCreate("%s:%s", track, table);
else
    name = dyStringCreate("%s", track);
return name;
}

void verifyGreatFormat(const char *output)
{
if (!sameString(output, outBed) && !sameString(output, outWigBed))
    {
    htmlOpen("Sorry!");
    errAbort("GREAT requires that the output format be BED format.\n"
         "Some tracks are not available in BED format and thus cannot be passed to GREAT.\n"
         "Please go back and ensure that BED format is chosen.");
    htmlClose();
    }
}

void verifyGreatAssemblies()
{
if (differentStringNullOk(database, "hg18") && differentStringNullOk(database, "mm9"))
    {
    hPrintf("<script type='text/javascript'>\n");
    hPrintf("function logSpecies() {\n");
    hPrintf("try {\n");
    hPrintf("var r = new XMLHttpRequest();\n");
    hPrintf("r.open('GET', 'http://great.stanford.edu/public/cgi-bin/logSpecies.php?species=%s');\n", database);
    hPrintf("r.send(null);\n");
    hPrintf("} catch (err) { }\n");
    hPrintf("}\n");
    hPrintf("window.onload = logSpecies;\n");
    hPrintf("</script>\n");
    errAbort("GREAT only supports the Human, Mar. 2006 (NCBI36/hg18) and Mouse, July 2007 (NCBI37/mm9) assemblies."
    "\nPlease go back and ensure that one of those assemblies is chosen.");
    htmlClose();
    }
}

void doGreatTopLevel()
/* intermediate page for formats printed directly from top form */
{
struct dyString *name = getRequestName();

htmlOpen("Send BED data to GREAT as %s", dyStringContents(name));
freeDyString(&name);
verifyGreatAssemblies();
startGreatForm();
cgiMakeHiddenVar(hgtaDoTopSubmit, "get output");
printGreatSubmitButtons();
htmlClose();
}

void startGreatForm()
/* Start form for GREAT */
{
hPrintf("<FORM ACTION=\"%s\" NAME='greatForm' METHOD=POST>\n", getScriptName());
}

void printGreatSubmitButtons()
/* print submit button to create data and then send query results to GREAT. */
{
cartSetString(cart, hgtaCompressType, textOutCompressNone);
cartSetString(cart, hgtaOutFileName, "");

cgiMakeHiddenVar(hgtaCompressType, textOutCompressNone);
cgiMakeHiddenVar(hgtaOutFileName, "");

cgiMakeHiddenVar(hgtaDoGreatOutput, "on");
cgiMakeHiddenVar(hgtaPrintCustomTrackHeaders, "on");
cgiMakeButton(hgtaDoGreatQuery, "Send query to GREAT");
hPrintf("</FORM>\n");
hPrintf(" ");
/* new form as action is different */
hPrintf("<FORM ACTION=\"%s\" METHOD=GET>\n", cgiScriptName());
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>\n");
}

boolean doGreat()
/* has the send to GREAT checkbox been selected? */
{
return cartUsualBoolean(cart, "sendToGreat", FALSE);
}

void doSubmitToGreat(const char *path)
/* Send a URL to GREAT that it can use to retrieve the results. */
{
struct dyString *requestName = getRequestName();
struct dyString *requestURL = dyStringCreate("http://%s/%s", cgiServerName(), path);
struct dyString *greatRequest;

greatRequest = dyStringCreate(
    "<meta http-equiv='refresh' content='0;url=http://great.stanford.edu/public/cgi-bin/greatStart.php?requestURL=%s&requestSpecies=%s&requestName=%s&requestSender=UCSC%20Table%20Browser'>",
    dyStringContents(requestURL), database, dyStringContents(requestName));

hPrintf("<b>GREAT</b> is processing BED data from \"%s\"...please wait.\n", dyStringContents(requestName));
hPrintf(dyStringContents(greatRequest));
freeDyString(&greatRequest);
freeDyString(&requestName);
freeDyString(&requestURL);
}

void doGetGreatOutput(void (*dispatch)())
{
char hgsid[64];
struct tempName tn;
int saveStdout;
FILE *f;

safef(hgsid, sizeof(hgsid), "%u", cartSessionId(cart));
trashDirFile(&tn, "great", hgsid, ".bed");
f = fopen(tn.forCgi, "w");

/* We want to capture hgTables stdout output to a trash file
 * which will later be fetched by Great via URL. */
/* Workaround because stdout stream is not assignable on some operating systems */
fflush(stdout);
saveStdout = dup(STDOUT_FILENO);
dup2(fileno(f),STDOUT_FILENO);   /* closes STDOUT before setting it again */
fclose(f);

dispatch();  /* this writes to stdout */

/* restore stdout */
fflush(stdout);
dup2(saveStdout ,STDOUT_FILENO);  /* closes STDOUT before setting it back to saved descriptor */
close(saveStdout);

cartRemove(cart, hgtaDoGreatOutput);
htmlOpen("Table Browser integration with GREAT");
doSubmitToGreat(tn.forCgi);
htmlClose();
}

