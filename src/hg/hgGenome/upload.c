/* Upload - put up upload pages and sub-pages. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "ra.h"
#include "portable.h"
#include "cheapcgi.h"
#include "localmem.h"
#include "cart.h"
#include "web.h"
#include "chromInfo.h"
#include "chromGraph.h"
#include "chromGraphFactory.h"
#include "errCatch.h"
#include "hgGenome.h"


static char *markerNames[] = {
    cgfMarkerGuess,
    cgfMarkerGenomic,
    cgfMarkerSts,
    cgfMarkerSnp,
    cgfMarkerAffy100,
    cgfMarkerAffy500,
    cgfMarkerHumanHap300,
    };

char *formatNames[] = {
    cgfFormatGuess,
    cgfFormatTab,
    cgfFormatComma,
    cgfFormatSpace,
    };

char *colLabelNames[] = {
    cgfColLabelGuess,
    cgfColLabelNumbered,
    cgfColLabelFirstRow,
    };

void uploadPage()
/* Put up initial upload page. */
{
char *oldFileName = cartUsualString(cart, hggUploadFile "__filename", "");
cartWebStart(cart, "Upload Data to Genome Association View");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">");
cartSaveSession(cart);
hPrintf("Name of data set: ");
cartMakeTextVar(cart, hggDataSetName, "", 16);
hPrintf("<i>Only first 16 letters are displayed in some contexts.</i>");
hPrintf("<BR>");

hPrintf("Description: ");
cartMakeTextVar(cart, hggDataSetDescription, "", 64);
hPrintf(" <i>Optional - used in Genome Browser</i> ");
hPrintf("<BR>\n");


hPrintf("File format: ");
cgiMakeDropList(hggFormatType, formatNames, ArraySize(formatNames), 
    	cartUsualString(cart, hggFormatType, formatNames[0]));
hPrintf(" <i>Guess is usually accurate, override if problems.</i> ");
hPrintf("<BR>\n");

hPrintf(" Markers are: ");
cgiMakeDropList(hggMarkerType, markerNames, ArraySize(markerNames), 
    	cartUsualString(cart, hggMarkerType, markerNames[0]));
hPrintf("<i> This says how values are mapped to chromosomes.</i>");
hPrintf("<BR>\n");

hPrintf(" Column labels: ", colLabelNames, ArraySize(colLabelNames),
	cartUsualString(cart, hggColumnLabels, colLabelNames[0]));
cgiMakeDropList(hggColumnLabels, colLabelNames, ArraySize(colLabelNames), 
	cartUsualString(cart, hggColumnLabels, colLabelNames[0]));
hPrintf("<i> Optionally include first row of file in labels.</i>");
hPrintf("<BR>\n");

hPrintf("Display Min Value: ");
cartMakeTextVar(cart, hggMinVal, "", 5);
hPrintf(" Max Value: ");
cartMakeTextVar(cart, hggMaxVal, "", 5);
hPrintf(" <i>Leave min/max blank to take scale from data itself.</i><BR>\n");

hPrintf("Label Values: ");
cartMakeTextVar(cart, hggLabelVals, "", 32);
hPrintf(" <i>Comma-separated numbers for axis label</i><BR>\n");
hPrintf("Draw connecting lines between markers separated by up to ");
cartMakeIntVar(cart, hggMaxGapToFill, 25000000, 8);
hPrintf(" bases.<BR>");
hPrintf("File name: <INPUT TYPE=FILE NAME=\"%s\" VALUE=\"%s\">", hggUploadFile,
	oldFileName);
hPrintf(" ");
// cgiMakeButton(hggSubmitUpload, "Submit");
cgiMakeButton(hggSubmitUpload2, "Submit");
hPrintf("</FORM>\n");
hPrintf("<i>note: If you are uploading more than one data set please give them ");
hPrintf("different names.  Only the most recent data set of a given name is ");
hPrintf("kept.  Data sets will be kept for at least 8 hours.  After that time ");
hPrintf("you may have to upload them again.</i>");

/* Put up section that describes file formats. */
webNewSection("Upload file formats");
hPrintf("%s", 
"<P>The upload file is a table in some format.  In all formats there is "
"a single line for each marker. "
"Each line starts with information on the marker, and ends with "
"the numerical values associated with that marker. The exact format "
"of the line depends on what is selected from the locations drop "
"down menu.  If this is <i>chromosome base</i> then the line will "
"contain the tab or space-separated fields:  chromosome, position, "
"and value(s).  The first base in a chromosome is considered position 0. "
"An example <i>chromosome base</i> type line is is:<PRE><TT>\n"
"chrX 100000 1.23\n"
"</TT></PRE>The lines for other location type contain two fields: "
"marker and value(s).  For dbSNP rsID's an example is:<PRE><TT>\n"
"rs10218492 0.384\n"
"</TT></PRE>"
"</P><P>"
"The file can contain multiple value fields.  In this case a "
"separate graph will be available for each value column in the input "
"table. It's a "
"good idea to set the display min/max values above if you want the "
"graphs to share the same scale."
"</P>"
);

cartWebEnd();
}

static void addIfNonempty(struct hash *hash, char *cgiVar, char *trackVar)
/* If cgiVar exists and is non-empty, add it to ra. */
{
char *val = skipLeadingSpaces(cartUsualString(cart, cgiVar, ""));
if (val[0] != 0)
    hashAdd(hash, trackVar, val);
}

void updateCustomTracks(struct customTrack *upList) 
/* Update custom tracks file with current upload data */
{
struct customTrack *outList = NULL;
struct tempName tempName;
char *fileName = cartOptionalString(cart, "ct");
if (fileName == NULL || !fileExists(fileName))
    {
    makeTempName(&tempName, "hggUp", ".ra");
    fileName = tempName.forCgi;
    outList = upList;
    }
else
    {
    struct customTrack *ct, *next, *oldList = customTracksParseCart(cart, NULL, NULL);

    /* Create hash with new tracks. */
    struct hash *upHash = hashNew(0);
    for (ct = upList; ct != NULL; ct = ct->next)
	 hashAdd(upHash, ct->tdb->tableName, ct);

    /* Make list with all old tracks that are not being replaced */
    for (ct = oldList; ct != NULL; ct = next)
        {
	next = ct->next;
	if (!hashLookup(upHash, ct->tdb->tableName))
	    {
	    slAddHead(&outList, ct);
	    }
	}

    /* Add new tracks to list. */
    for (ct = upList; ct != NULL; ct = next)
        {
	next = ct->next;
	slAddHead(&outList, ct);
	}
    slReverse(&outList);
    hashFree(&upHash);
    }

uglyf("Saving customTrack to %s<BR>\n", fileName);
customTrackSave(outList, fileName);
cartSetString(cart, "ct", fileName);

hPrintf("This data is now available in the drop down menus on the ");
hPrintf("main page for graphing.<BR>");
}

void trySubmitUpload2(struct sqlConnection *conn, char *rawText)
/* Called when they've submitted from uploads page */
{
struct lineFile *lf = lineFileOnString("uploaded data", TRUE, rawText);
struct customPp *cpp = customPpNew(lf);
struct hash *settings = hashNew(8);
addIfNonempty(settings, hggMinVal, "minVal");
addIfNonempty(settings, hggMaxVal, "maxVal");
addIfNonempty(settings, hggMaxGapToFill, "maxGapToFill");
addIfNonempty(settings, hggLabelVals, "linesAt");

struct customTrack *trackList = chromGraphParser(cpp,
	cartUsualString(cart, hggFormatType, formatNames[0]),
	cartUsualString(cart, hggMarkerType, cgfMarkerGenomic),
	cartUsualString(cart, hggColumnLabels, colLabelNames[0]), 
	nullIfAllSpace(cartUsualString(cart, hggDataSetName, NULL)),
	nullIfAllSpace(cartUsualString(cart, hggDataSetDescription, NULL)),
	settings, TRUE);
cartRemove(cart, hggUploadFile);
updateCustomTracks(trackList);

hPrintf("<CENTER>");
cgiMakeButton("submit", "OK");
hPrintf("</CENTER>");
}

void submitUpload2(struct sqlConnection *conn)
/* Called when they've submitted from uploads page */
{
char *rawText = cartUsualString(cart, hggUploadFile, NULL);
int rawTextSize = strlen(rawText);
// struct errCatch *errCatch = errCatchNew();
cartWebStart(cart, "Data Upload2 Complete (%d bytes)", rawTextSize);
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\">");
cartSaveSession(cart);
// if (errCatchStart(errCatch))
     trySubmitUpload2(conn, rawText);
// errCatchFinish(&errCatch);
hPrintf("</FORM>");
cartWebEnd();
}
