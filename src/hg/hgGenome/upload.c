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
#include "hPrint.h"
#include "customTrack.h"
#include "hgGenome.h"
#include "trashDir.h"


static char *markerNames[] = {
    cgfMarkerGuess,
    cgfMarkerGenomic,
    cgfMarkerSts,
    cgfMarkerSnp,
    // cgfMarkerAffy100,
    cgfMarkerAffy500,
    cgfMarkerAffy6,
    cgfMarkerAffy6SV,

    cgfMarkerHumanHap300,
    cgfMarkerHumanHap550,
    cgfMarkerHumanHap650,

    cgfMarkerAgilentCgh244A,

    };

static char *formatNames[] = {
    cgfFormatGuess,
    cgfFormatTab,
    cgfFormatComma,
    cgfFormatSpace,
    };

static char *colLabelNames[] = {
    cgfColLabelGuess,
    cgfColLabelNumbered,
    cgfColLabelFirstRow,
    };

void uploadPage()
/* Put up initial upload page. */
{
char *oldFileName = cartUsualString(cart, hggUploadFile "__filename", "");
cartWebStart(cart, database, "Upload Data to Genome Graphs");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">");
cartSaveSession(cart);
hPrintf("name of data set: ");
cartMakeTextVar(cart, hggDataSetName, "", 16);
hPrintf("<BR>");

hPrintf("description: ");
cartMakeTextVar(cart, hggDataSetDescription, "", 64);
hPrintf("<BR>\n");


hPrintf("file format: ");
cgiMakeDropList(hggFormatType, formatNames, ArraySize(formatNames), 
    	cartUsualString(cart, hggFormatType, formatNames[0]));
hPrintf("<BR>\n");

hPrintf(" markers are: ");
cgiMakeDropList(hggMarkerType, markerNames, ArraySize(markerNames), 
    	cartUsualString(cart, hggMarkerType, markerNames[0]));
hPrintf("<BR>\n");

hPrintf(" column labels: ");
cgiMakeDropList(hggColumnLabels, colLabelNames, ArraySize(colLabelNames), 
	cartUsualString(cart, hggColumnLabels, colLabelNames[0]));
hPrintf("<BR>\n");

hPrintf("display min value: ");
cartMakeTextVar(cart, hggMinVal, "", 5);
hPrintf(" max value: ");
cartMakeTextVar(cart, hggMaxVal, "", 5);
hPrintf("<BR>\n");

hPrintf("label values: ");
cartMakeTextVar(cart, hggLabelVals, "", 32);
hPrintf("<BR>\n");
hPrintf("draw connecting lines between markers separated by up to ");
cartMakeIntVar(cart, hggMaxGapToFill, 25000000, 8);
hPrintf(" bases.<BR>");
hPrintf("<BR>\n");
hPrintf("file name: <INPUT TYPE=FILE NAME=\"%s\" VALUE=\"%s\">", hggUploadFile,
	oldFileName);
hPrintf("<BR>\n");
hPrintf("&nbsp;or<BR>\n");
hPrintf("Paste URLs or data:<br>");
cgiMakeTextArea(hggUploadUrl, "", 7, 73);
hPrintf("<BR>\n");
hPrintf("<BR>\n");
cgiMakeButton(hggSubmitUpload, "submit");
hPrintf("</FORM>\n");
hPrintf("<i>Note: If you are uploading more than one data set please give them ");
hPrintf("different names.  Only the most recent data set of a given name is ");
hPrintf("kept.  Otherwise data sets will be kept for at least 48 hours from ");
hPrintf("last use.  After that time you may have to upload them again.</i>");

/* Put up section that describes file formats. */
webNewSection("Upload file formats");
hPrintf("%s", 
"<P>The upload file is a table in some format.  In all formats there is "
"a single line for each marker. "
"Each line starts with information on the marker, and ends with "
"the numerical values associated with that marker. The exact format "
"of the line depends on what is selected from the markers drop "
"down menu.  If this is <i>chromosome base</i> then the line will "
"contain the tab or space-separated fields:  chromosome, position, "
"and value(s).  The first base in a chromosome is considered position 0. "
"An example <i>chromosome base</i> type line is:<PRE><TT>\n"
"chrX 100000 1.23\n"
"</TT></PRE>The lines for other marker types contain two fields: "
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

webNewSection("Using the upload page");
hPrintf(
"To upload a file in any of the supported formats, "
"locate the file on your computer using the controls next to <B>file name</B>, "
"or supply a <B>URL</B> to the data, "
"and then submit. The other controls on this form are optional, though "
"filling them out will sometimes enhance the display. In general "
"the controls that default to <i>best guess</i> can be left alone, "
"since the guess is almost always correct. The controls for display min and "
"max values and connecting lines can be set later via the configuration "
"page as well. Here is a description of each control."
"<UL>"
"<LI><B>name of data set:</B> Displayed in graph drop-down in Genome Graphs "
" and as trackname in Genome Browser. Only the first 16 characters are "
" visible in some contexts. For data sets with multiple graphs, this is the "
" first part of the name, shared with all members of the data set.</LI>"
"<LI><B>description:</B> A short sentence describing the data set. Displayed in "
"the Genome Graphs and Genome Browser configuration pages, and as the center "
"label in the Genome Browser.</LI>"
"<LI><B>file format:</B> Controls whether the upload file is a tab-separated, "
" comma-separated, or space separated table.</LI>"
"<LI><B>markers are:</B> Describes how to map the data to chromosomes. The choices "
" are either the first column of the file is an ID of some sort, or the first column "
" is a chromosome and the next a base. The IDs can be SNP rs numbers, STS marker names "
" or ID's from any of the supported genotyping platforms.</LI>"
"<LI><B>column labels:</B> Controls whether the first row of the upload file is "
" interpreted as labels rather than data. If the first row contains text in the "
" numerical fields, or if the mapping fields are empty, it is interpreted by "
" \"best guess\" as labels. This is generally correct, but you can override this "
" interpretation by explicitly setting the control. </LI>"
"<LI><B>display min value/max value:</B> Set the range of the data set that will "
" be plotted. If left blank, the range will be taken from the min/max values in the "
" data set itself. For all data sets to share the same scale usually you'll need "
" to set this.</LI>"
"<LI><B>label values:</B> A comma-separated list of numbers for the vertical axis. "
" If left blank the axis will be labeled at the 1/3 and 2/3 point. </LI>"
"<LI><B>draw connecting lines:</B> Lines connecting data points separated by "
" no more than this number of bases are drawn.  </LI>"
"<LI><B>file name, or Paste URLs or data:</B> Specify the uploaded data --"
" enter either a file on your local computer; or a URL at which the data file can be"
" found; or simply paste-in the data. If entries are made in both fields, the file name will take"
" precedence. </LI>");
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
struct customTrack *oldList = customTracksParseCart(database, cart, NULL, NULL);
struct customTrack *outList = customTrackAddToList(oldList, upList, NULL, FALSE);
customTracksSaveCart(database, cart, outList);
hPrintf("These data are now available in the drop-down menus on the ");
hPrintf("main page for graphing.<BR>");
}

void trySubmitUpload(struct sqlConnection *conn, char *rawText)
/* Called when they've submitted from uploads page */
{
struct lineFile *lf = lineFileOnString("uploaded data", TRUE, rawText);
struct customPp *cpp = customPpNew(lf);
struct hash *settings = hashNew(8);
addIfNonempty(settings, hggMinVal, "minVal");
addIfNonempty(settings, hggMaxVal, "maxVal");
addIfNonempty(settings, hggMaxGapToFill, "maxGapToFill");
addIfNonempty(settings, hggLabelVals, "linesAt");

struct customTrack *trackList = chromGraphParser(database, cpp,
	cartUsualString(cart, hggFormatType, formatNames[0]),
	cartUsualString(cart, hggMarkerType, cgfMarkerGenomic),
	cartUsualString(cart, hggColumnLabels, colLabelNames[0]), 
	nullIfAllSpace(cartUsualString(cart, hggDataSetName, NULL)),
	nullIfAllSpace(cartUsualString(cart, hggDataSetDescription, NULL)),
	settings, TRUE);
updateCustomTracks(trackList);
}

void submitUpload(struct sqlConnection *conn)
/* Called when they've submitted from uploads page */
{
char *urlText = cartUsualString(cart, hggUploadUrl, NULL);
char *fileText = cartUsualString(cart, hggUploadFile, NULL);
char *rawText = urlText;
if (isNotEmpty(fileText))
  rawText = fileText; 
int rawTextSize = strlen(rawText);
struct errCatch *errCatch = errCatchNew();
cartWebStart(cart, database, "Data Upload Complete (%d bytes)", rawTextSize);
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\">");
cartSaveSession(cart);
if (errCatchStart(errCatch))
     trySubmitUpload(conn, rawText);
errCatchFinish(&errCatch);
cartRemove(cart, hggUploadFile);
hPrintf("<CENTER>");
cgiMakeButton("submit", "OK");
hPrintf("</CENTER>");
hPrintf("</FORM>");
cartWebEnd();
}
