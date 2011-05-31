/* great - stuff related to GREAT. */

#include "common.h"
#include "cheapcgi.h"
#include "hgTables.h"
#include "cart.h"
#include "dystring.h"
#include "textOut.h"
#include "trashDir.h"

static char const rcsid[] = "$Id: great.c,v 1.3 2010/06/01 20:09:36 galt Exp $";

static char* greatData = "greatData/supportedAssemblies.txt";

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

void addAssemblyToSupportedList(struct dyString* dy, char* assembly)
{
char* organism = hOrganism(assembly);
char* freezeDate = hFreezeDate(assembly);
dyStringPrintf(dy, "%s %s", organism, freezeDate);
freeMem(organism);
freeMem(freezeDate);
}

void verifyGreatAssemblies()
{
// First read in the assembly name and description information into name lists
struct slName* supportedAssemblies = NULL;
struct lineFile *lf = lineFileOpen(greatData, TRUE);
int fieldCount = 1;
char* row[fieldCount];
int wordCount;
while ((wordCount = lineFileChopTab(lf, row)) != 0)
	{
	if (wordCount != fieldCount)
		errAbort("The %s file is not properly formatted.\n", greatData);
	slNameAddHead(&supportedAssemblies, row[0]);
	}
lineFileClose(&lf);

boolean invalidAssembly = TRUE;
struct slName* currAssembly;
for (currAssembly = supportedAssemblies; currAssembly != NULL; currAssembly = currAssembly->next)
	{
	if (!hDbIsActive(currAssembly->name))
		{
		errAbort("Assembly %s in supported assembly file is not an active assembly.\n", currAssembly->name);
		}
	if (sameOk(database, currAssembly->name))
		{
		invalidAssembly = FALSE;
		break;
		}
	}

if (invalidAssembly)
    {
	slReverse(&supportedAssemblies);
	currAssembly = supportedAssemblies;
	struct dyString* dy = dyStringNew(0);
	addAssemblyToSupportedList(dy, currAssembly->name);

	currAssembly = currAssembly->next;
	while (currAssembly != NULL)
		{
		dyStringAppend(dy, ", ");
		if (currAssembly->next == NULL)
			dyStringAppend(dy, "and ");
		addAssemblyToSupportedList(dy, currAssembly->name);
		currAssembly = currAssembly->next;
		}

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
    errAbort("GREAT only supports the %s assemblies."
    "\nPlease go back and ensure that one of those assemblies is chosen.",
	dyStringContents(dy));
    htmlClose();
	dyStringFree(&dy);
    }

slNameFreeList(&supportedAssemblies);
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
hPrintf("<div style='height:.9em;'></div>\n");
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
struct dyString *requestURL = dyStringCreate("http://%s/%s", cgiServerNamePort(), path);
struct dyString *greatRequest;

greatRequest = dyStringCreate(
    "<meta http-equiv='refresh' content='0;url=http://great.stanford.edu/public/cgi-bin/greatStart.php?requestURL=%s&requestSpecies=%s&requestName=%s&requestSender=UCSC%20Table%20Browser'>",
    dyStringContents(requestURL), database, dyStringContents(requestName));

hPrintf("<b>GREAT</b> is processing BED data from \"%s\"...please wait.\n", dyStringContents(requestName));
hWrites(dyStringContents(greatRequest));
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

