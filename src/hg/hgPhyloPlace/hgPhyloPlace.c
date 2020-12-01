/* hgPhyloPlace - Upload SARS-CoV-2 sequence for placement in phylo tree. */

/* Copyright (C) 2020 The Regents of the University of California */

#include "common.h"
#include "botDelay.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "hash.h"
#include "hui.h"
#include "jsHelper.h"
#include "knetUdc.h"
#include "linefile.h"
#include "net.h"
#include "options.h"
#include "phyloPlace.h"
#include "portable.h"
#include "trackLayout.h"
#include "udc.h"
#include "web.h"

/* Global Variables */
struct cart *cart = NULL;      // CGI and other variables
struct hash *oldVars = NULL;   // Old contents of cart before it was updated by CGI
boolean measureTiming = FALSE; // Print out how long things take
char *leftLabelWidthForLongNames = "55";// Leave plenty of room for tree and long virus strain names

#define seqFileVar "sarsCoV2File"
#define remoteFileVar "remoteFile"

static struct lineFile *lineFileFromFileInput(struct cart *cart, char *fileVar)
/* Return a lineFile on data from an uploaded file with cart variable name fileVar.
 * If the file is binary, attempt to decompress it.  Return NULL if no data are found
 * or if there is a problem decompressing binary data.  If retFileName is not NULL */
{
struct lineFile *lf = NULL;
// Depending on whether the file is plain text or binary, different cart variables are present.
char *filePlainContents = cartOptionalString(cart, fileVar);
char cartVar[2048];
safef(cartVar, sizeof cartVar, "%s__binary", fileVar);
char *fileBinaryCoords = cartOptionalString(cart, cartVar);
// Also get the file name for error reporting.
safef(cartVar, sizeof cartVar, "%s__filename", fileVar);
char *fileName = cartOptionalString(cart, cartVar);
if (fileName == NULL)
    fileName = "<uploaded data>";
if (isNotEmpty(filePlainContents))
    {
    lf = lineFileOnString(fileName, TRUE, cloneString(trimSpaces(filePlainContents)));
    }
else if (isNotEmpty(fileBinaryCoords))
    {
    fprintf(stderr, "%s=%s fileBinaryCoords=%s\n", cartVar, fileName, fileBinaryCoords);
    char *binInfo = cloneString(fileBinaryCoords);
    char *words[2];
    char *mem;
    unsigned long size;
    chopByWhite(binInfo, words, ArraySize(words));
    mem = (char *)sqlUnsignedLong(words[0]);
    size = sqlUnsignedLong(words[1]);
    lf = lineFileDecompressMem(TRUE, mem, size);
    }
return lf;
}

static void newPageStartStuff()
{
// Copied these from hgGtexTrackSettings.c which says "// NOTE: This will likely go to web.c".
puts("<link rel='stylesheet' href='../style/gb.css'>");
puts("<link rel='stylesheet' href='../style/hgGtexTrackSettings.css'>");

//#*** TODO: move this out to a CSS (hardcoding for now because we're doing a standalone push
//#*** independent of the release cycle).
puts("<style>\n"
"#warnBox {\n"
"    border: 3px ridge DarkRed;\n"
"    width:640px;\n"
"    padding:10px; \n"
"    margin:10px;\n"
"    text-align:left;\n"
"}\n"
"\n"
"#warnHead {\n"
"    color: DarkRed;\n"
"}\n"
".readableWidth {\n"
"    max-width: 70em;\n"
"}\n"
"table.seqSummary, table.seqSummary th, table.seqSummary td {\n"
"    border: 1px gray solid;\n"
"    padding: 5px;\n"
"}\n"
".tooltip {\n"
"    position: relative;\n"
"    display: inline-block;\n"
"    border-bottom: 1px dotted black;\n"
"}\n"
"\n"
".tooltip .tooltiptext {\n"
"    visibility: hidden;\n"
"    background-color: lightgray;\n"
"    text-align: center;\n"
"    position: absolute;\n"
"    z-index: 1;\n"
"    opacity: 0;\n"
"    width: 220px;\n"
"    padding: 5px;\n"
"    left: 105%;\n"
"    transition: opacity .6s;\n"
"    line-height: 1em;\n"
"}\n"
"\n"
".tooltip:hover .tooltiptext {\n"
"    visibility: visible;\n"
"    opacity: .9;\n"
"}\n"
"td.qcExcellent {\n"
"    background-color: #44ff44;\n"
"}\n"
"td.qcGood {\n"
"    background-color: #88ff88;\n"
"}\n"
"td.qcMeh {\n"
"    background-color: #ffcc44;\n"
"}\n"
"td.qcBad {\n"
"    background-color: #ff8888;\n"
"}\n"
"td.qcFail {\n"
"    background-color: #ff6666;\n"
"}\n"
"</style>\n"
     );



// Container for bootstrap grid layout
puts(
"<div class='container-fluid'>\n");
}

static void newPageEndStuff()
{
puts(
"</div>");
jsIncludeFile("utils.js", NULL);
webIncludeFile("inc/gbFooter.html");
webEndJWest();
}

#define CHECK_FILE_INPUT_JS "{ var $fileInput = $('input[name="seqFileVar"]');  " \
    "if ($fileInput && $fileInput[0] && $fileInput[0].files && !$fileInput[0].files.length) {" \
      " alert('Please choose a file first, then click the upload button.');" \
      " return false; " \
    "} else { return true; } }"

static void inputForm()
/* Ask the user for FASTA or VCF. */
{
printf("<form action='%s' name='mainForm' method=POST enctype='multipart/form-data'>\n\n",
       "hgPhyloPlace");
cartSaveSession(cart);
cgiMakeHiddenVar("db", "wuhCor1");
puts("  <div class='gbControl col-md-12'>");
puts("<div class='readableWidth'>");
puts("<p>Upload your SARS-CoV-2 sequence (FASTA or VCF file) to find the most similar\n"
     "complete, high-coverage samples from \n"
     "<a href='https://www.gisaid.org/' target='_blank'>GISAID</a>\n"
     "and your sequence's placement in the phylogenetic tree generated by the\n"
     "<a href='https://github.com/roblanf/sarscov2phylo' target='_blank'>sarscov2phylo</a>\n"
     "pipeline.\n"
     "Placement is performed by\n"
     "<a href='https://github.com/yatisht/usher' target=_blank>"
     "Ultrafast Sample placement on Existing tRee (UShER)</a> "
     "(<a href='https://www.biorxiv.org/content/10.1101/2020.09.26.314971v1' target=_blank>"
     "Turakhia <em>et al.</em></a>).  UShER also generates local subtrees to show samples "
     "in the context of the most closely related sequences.  The subtrees can be visualized "
     "as Genome Browser custom tracks and/or using "
     "<a href='https://nextstrain.org' target=_blank>Nextstrain</a>'s interactive display "
     "which supports "
     "<a href='https://docs.nextstrain.org/projects/auspice/en/latest/advanced-functionality/drag-drop-csv-tsv.html' "
     "target=_blank>drag-and-drop</a> of local metadata that remains on your computer.</p>\n");
puts("<p><b>Note:</b> "
     "Please do not upload any files that contain "
     "<a href='https://en.wikipedia.org/wiki/Protected_health_information#United_States' "
     "target=_blank>Protected Health Information (PHI)</a> "
     "to UCSC.</p>\n"
     "<p>We do not store your information "
     "(aside from the information necessary to display results)\n"
     "and will not share it with others unless you choose to share your Genome Browser view.</p>\n"
     "<p>In order to enable rapid progress in SARS-CoV-2 research and genomic contact tracing,\n"
     "please share your SARS-CoV-2 sequences by submitting them to an "
     "<a href='https://ncbiinsights.ncbi.nlm.nih.gov/2020/08/17/insdc-covid-data-sharing/' "
     "target=_blank>INSDC</a> member institution\n"
     "(<a href='https://submit.ncbi.nlm.nih.gov/sarscov2/' target=_blank>NCBI</a> in the U.S.,\n"
     "<a href='https://www.covid19dataportal.org/submit-data' target=_blank>EMBL-EBI</a> in Europe\n"
     "and <a href='https://www.ddbj.nig.ac.jp/ddbj/websub.html' target=_blank>DDBJ</a> in Japan)\n"
     "and <a href='https://www.gisaid.org/' target=_blank>GISAID</a>.\n"
     "</p>\n");
puts("</div>");
puts("  </div>");
puts("  <div class='gbControl col-md-12'>");
printf("<input type='file' id='%s' name='%s' "
       "accept='.fa, .fasta, .vcf, .vcf.gz, .fa.gz, .fasta.gz'>",
       seqFileVar, seqFileVar);
printf("Number of samples per subtree showing sample placement: ");
int subtreeSize = cartUsualInt(cart, "subtreeSize", 50);
cgiMakeIntVarWithLimits("subtreeSize", subtreeSize,
 "Number of samples in subtree showing neighborhood of placement",
 5, 10, 1000);
cgiMakeOnClickSubmitButton(CHECK_FILE_INPUT_JS, "submit", "upload");
puts("  </div>");
puts("</form>");
}

static void exampleForm()
/* Let the user try Russ's example. */
{
printf("<form action='%s' name='exampleForm' method=POST>\n\n",
       "hgPhyloPlace");
cartSaveSession(cart);
cgiMakeHiddenVar("db", "wuhCor1");
puts("  <div class='gbControl col-md-12'>");
puts("If you don't have a local file, you can try an "
     "<a href='https://github.com/russcd/USHER_DEMO/' target=_blank>example</a>: ");
cgiMakeButton("submit", "try example");
puts("  </div>");
puts("</form>");
}

static void linkToLandingPage()
/* David asked for a link back to our covid19 landing page. */
{
puts("<div class='gbControl col-md-12'>");
puts("<div class='readableWidth'>");
puts("<p></p>");
puts("<p>\n"
     "<a href='/covid19.html'>COVID-19 Pandemic Resources at UCSC</a></p>\n");
puts("</div>");
puts("</div>");
}

static void gisaidFooter()
/* GISAID wants this on all pages that have anything to do with GISAID samples. */
{
puts("<div class='gbControl col-md-12'>");
puts("<div class='readableWidth'>");
puts("<p></p>");
puts("<p>\n"
     "GISAID data displayed in the Genome Browser are subject to GISAID's\n"
     "<a href='https://www.gisaid.org/registration/terms-of-use/' target=_blank>"
     "Terms and Conditions</a>.\n"
     "SARS-CoV-2 genome sequences and metadata are available for download from\n"
     "<a href='https://gisaid.org' target=_blank>GISAID</a> EpiCoV&trade;.\n"
     "</p>");
puts("</div>");
puts("</div>");
}

static void mainPage(char *db)
{
// Start web page with new-style header
webStartGbNoBanner(cart, db, "UShER: Upload");
newPageStartStuff();

puts("<div class='row'>"
     "  <div class='row gbSectionBanner'>\n"
     "    <div class='col-md-11'>UShER: Ultrafast Sample placement on Existing tRee</div>\n"
     "    <div class='col-md-1'></div>\n"
     "  </div>\n"
     "</div>\n"
     "<div class='row'>\n");
if (hgPhyloPlaceEnabled())
    {
    inputForm();
    exampleForm();
    linkToLandingPage();
    gisaidFooter();
    }
else
    {
    puts("  <div class='gbControl col-md-12'>");
    puts("  Sorry, this server is not configured to perform phylogenetic placement.");
    puts("  </div>");
    }
puts("</div>\n");

newPageEndStuff();
}

static void resultsPage(char *db, struct lineFile *lf)
/* QC the user's uploaded sequence(s) or VCF; if input looks valid then run usher
 * and display results. */
{
webStartGbNoBanner(cart, db, "UShER: Results");
newPageStartStuff();

hgBotDelay();

puts("<div class='row'>"
     "  <div class='row gbSectionBanner'>\n"
     "    <div class='col-md-11'>UShER: Ultrafast Sample placement on Existing tRee</div>\n"
     "    <div class='col-md-1'></div>\n"
     "  </div>\n"
     "</div>\n"
     "<div class='row'>\n");
// Form submits subtree custom tracks to hgTracks
printf("<form action='%s' name='resultsForm' method=%s>\n\n",
       hgTracksName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
puts("  <div class='gbControl col-md-12'>");

if (lf != NULL)
    {
    // Use trackLayout to get hgTracks parameters relevant to displaying trees:
    struct trackLayout tl;
    trackLayoutInit(&tl, cart);
    // Do our best to place the user's samples, make custom tracks if successful:
    int subtreeSize = cartUsualInt(cart, "subtreeSize", 50);
    char *ctFile = phyloPlaceSamples(lf, db, measureTiming, subtreeSize, tl.fontHeight);
    if (ctFile)
        {
        cgiMakeHiddenVar(CT_CUSTOM_TEXT_VAR, ctFile);
        if (tl.leftLabelWidthChars < 0 || tl.leftLabelWidthChars == leftLabelWidthDefaultChars)
            cgiMakeHiddenVar(leftLabelWidthVar, leftLabelWidthForLongNames);
        cgiMakeButton("submit", "view in Genome Browser");
        puts("  </div>");
        puts("</form>");
        }
    else
        {
        puts("  </div>");
        puts("</form>");
        // Let the user upload something else and try again:
        inputForm();
        }
    }
else
    {
    warn("Unable to read your uploaded data - please choose a file and try again, or click the "
         "&quot;try example&quot; button.");
    // Let the user try again:
    puts("  </div>");
    puts("</form>");
    inputForm();
    exampleForm();
    }
puts("</div>\n");

linkToLandingPage();
gisaidFooter();
newPageEndStuff();
}

static void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
char *db = NULL, *genome = NULL, *clade = NULL;
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

measureTiming = cartUsualBoolean(cart, "measureTiming", measureTiming);

char *submitLabel = cgiOptionalString("submit");
if (submitLabel && sameString(submitLabel, "try example"))
    {
    char *exampleFile = phyloPlaceDbSettingPath(db, "exampleFile");
    struct lineFile *lf = lineFileOpen(exampleFile, TRUE);
    resultsPage(db, lf);
    }
else if (cgiOptionalString(remoteFileVar))
    {
    char *url = cgiString(remoteFileVar);
    struct lineFile *lf = netLineFileOpen(url);
    resultsPage(db, lf);
    }
else if (cgiOptionalString(seqFileVar) || cgiOptionalString(seqFileVar "__filename"))
    {
    struct lineFile *lf = lineFileFromFileInput(cart, seqFileVar);
    resultsPage(db, lf);
    }
else
    mainPage(db);
}

#define LD_LIBRARY_PATH "LD_LIBRARY_PATH"

static void addLdLibraryPath()
/* usher requires a tbb lib that is not in the yum package tbb-devel, so for now
 * I'm adding the .so files to hgPhyloPlaceData.  Set environment variable LD_LIBRARY_PATH
 * to pick them up from there. */
{
char *oldValue = getenv(LD_LIBRARY_PATH);
struct dyString *dy = dyStringNew(0);
if (startsWith("/", PHYLOPLACE_DATA_DIR))
    dyStringAppend(dy, PHYLOPLACE_DATA_DIR);
else
    {
    char cwd[4096];
    getcwd(cwd, sizeof cwd);
    dyStringPrintf(dy, "%s/%s", cwd, PHYLOPLACE_DATA_DIR);
    }
if (isNotEmpty(oldValue))
    dyStringPrintf(dy, ":%s", oldValue);
setenv(LD_LIBRARY_PATH, dyStringCannibalize(&dy), TRUE);
}

int main(int argc, char *argv[])
/* Process command line. */
{
/* Null terminated list of CGI Variables we don't want to save to cart */
char *excludeVars[] = {"submit", "Submit",
                       seqFileVar, seqFileVar "__binary", seqFileVar "__filename",
                       NULL};
long enteredMainTime = clock1000();
cgiSpoof(&argc, argv);
oldVars = hashNew(10);
addLdLibraryPath();
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgPhyloPlace", enteredMainTime);
return 0;
}
