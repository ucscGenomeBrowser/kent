/* hgPhyloPlace - Upload SARS-CoV-2 or MPXV sequence for placement in phylo tree. */

/* Copyright (C) 2020-2024 The Regents of the University of California */

#include "common.h"
#include "botDelay.h"
#include "cart.h"
#include "cgiApoptosis.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "hash.h"
#include "hgConfig.h"
#include "htmshell.h"
#include "hui.h"
#include "jsHelper.h"
#include "knetUdc.h"
#include "linefile.h"
#include "md5.h"
#include "net.h"
#include "options.h"
#include "phyloPlace.h"
#include "portable.h"
#include "trackLayout.h"
#include "udc.h"
#include "web.h"
#include "wikiLink.h"

/* Global Variables */
struct cart *cart = NULL;      // CGI and other variables
struct hash *oldVars = NULL;   // Old contents of cart before it was updated by CGI
boolean measureTiming = FALSE; // Print out how long things take

/* for botDelay call, 10 second for warning, 20 second for immediate exit */
#define delayFraction   0.25
static boolean issueBotWarning = FALSE;
static long enteredMainTime = 0;

#define orgVar "hgpp_org"
#define seqFileVar "sarsCoV2File"
#define pastedIdVar "namesOrIds"
#define remoteFileVar "remoteFile"
#define serverCommandVar "hgpp_serverCommand"
#define serverCommentVar "hgpp_serverComment"
#define serverPlainVar "hgpp_serverPlain"
#define serverSaltyVar "hgpp_serverSalty"

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

static void selectOrg(char **pOrg, char **pLabel)
/* Search for config files in hgPhyloPlaceData.  If there is more than one
 * supported organism, then make a menu / select input for supported organisms;
 * reload the page on change. */
{
struct slPair *orgLabelList = phyloPlaceOrgList(cart);
if (orgLabelList == NULL)
    errAbort("Sorry, this server is not configured to perform phylogenetic placement.");
if (!slPairFind(orgLabelList, *pOrg))
    {
    *pOrg = cloneString(orgLabelList->name);
    }
*pLabel = phyloPlaceOrgSetting(*pOrg, "name");
if (isEmpty(*pLabel))
    *pLabel = *pOrg;
char *selectVar = orgVar;
int orgCount = slCount(orgLabelList);
if (orgCount > 1)
    {
    char *labels[orgCount];
    char *values[orgCount];
    struct slPair *orgLabel;
    int i;
    for (orgLabel = orgLabelList, i = 0;  i < orgCount;  orgLabel = orgLabel->next, i++)
        {
        values[i] = orgLabel->name;
        labels[i] = orgLabel->val;
        }
    struct dyString *dy = jsOnChangeStart();
    jsDropDownCarryOver(dy, selectVar);
    char *js = jsOnChangeEnd(&dy);
    puts("<p>Choose your pathogen: ");
    cgiMakeDropListFull(selectVar, labels, values, orgCount, *pOrg, "change", js);
    puts("</p>");
    }
else
    cgiMakeHiddenVar(selectVar, *pOrg);
slPairFreeList(&orgLabelList);
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
".gbSectionBannerLarge {\n"
"    padding: 10px;\n"
"    margin-top: 6px;\n"
"    margin-right: 0;\n"
"    background-color: #4c759c;  /* light blue */\n"
"    color: white;\n"
"    font-weight: bold;\n"
"    font-size: 22px;\n"
"}\n"
"h2 { font-size: 18px; }\n"
"h3 { font-size: 16px; }\n"
"table.invisalign {\n"
"    border: 0px;\n"
"}\n"
"table.invisalign td {\n "
"    padding: 5px;\n"
"}\n"
"button.fullwidth {\n "
"    width: 100%;\n"
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

#define CHECK_FILE_OR_PASTE_INPUT_JS(fileVarName, pasteVarName) \
    "{ var $fileInput = $('input[name="fileVarName"]');" \
    "  var $pasteInput = $('textarea[name="pasteVarName"]');" \
    "  if ($fileInput && $fileInput[0] && $fileInput[0].files && !$fileInput[0].files.length &&" \
    "      $pasteInput && !$pasteInput.val()) {" \
    "     alert('Please either choose a file or paste in sequence names/IDs first, ' +" \
    "           'and then click the upload button.');" \
    "     return false; " \
    "   } else if ($fileInput && $fileInput[0] && $fileInput[0].files && " \
    "              !!$fileInput[0].files.length &&" \
    "              $pasteInput && !!$pasteInput.val()) {" \
    "     alert('Sorry, unable to process both a file and pasted-in sequence names/IDs at the ' +" \
    "            'same time.  Please clear one or the other and then click the upload button.');" \
    "     return false; " \
    "   } else { loadingImage.run(); return true; } }"

static void inputForm(char *org)
/* Ask the user for FASTA or VCF. */
{
printf("<form action='%s' name='mainForm' method=POST enctype='multipart/form-data'>\n\n",
       "hgPhyloPlace");
cartSaveSession(cart);
puts("<div class='readableWidth'>");
puts("  <div class='gbControl col-md-12'>");
puts("<div style='font-size: 20px; font-weight: 500; margin-top: 15px; margin-bottom: 10px;'>"
     "Place your sequences in a global phylogenetic tree</div>");
char *label = NULL;
selectOrg(&org, &label);
printf("<p>Select your FASTA, VCF or list of sequence names/IDs: ");
printf("<input type='file' id='%s' name='%s'>",
       seqFileVar, seqFileVar);
printf("</p><p>or paste in sequence names/IDs:<br>\n");
cgiMakeTextArea(pastedIdVar, "", 10, 70);
if (phyloPlaceOrgSetting(org, "nextcladeIndex") == NULL)
    {
    // This is not a multi-reference organism, this is an old-style single-reference setup for
    // which the user can directly choose the tree (i.e. SARS-CoV-2).
    struct treeChoices *treeChoices = loadTreeChoices(org, org);
    puts("</p><p>");
    printf("Phylogenetic tree version: ");
    char *phyloPlaceTree = cartOptionalString(cart, "phyloPlaceTree");
    cgiMakeDropListWithVals("phyloPlaceTree", treeChoices->descriptions, treeChoices->protobufFiles,
                            treeChoices->count, phyloPlaceTree);
    }
puts("</p><p>");
printf("Number of samples per subtree showing sample placement: ");
int subtreeSize = cartUsualInt(cart, "subtreeSize", 50);
struct dyString *dy = dyStringCreate("Number of samples in subtree showing neighborhood of "
                                     "placement (max: %d", MAX_SUBTREE_SIZE);
if (microbeTraceHost() != NULL)
    dyStringPrintf(dy, "; max for MicrobeTrace: %d)", MAX_MICROBETRACE_SUBTREE_SIZE);
else
    dyStringAppend(dy, ")");
cgiMakeIntVarWithLimits("subtreeSize", subtreeSize, dy->string, 5, 10, MAX_SUBTREE_SIZE);
puts("</p><p>");
char *sessionDataDir = cfgOption("sessionDataDir");
if (isNotEmpty(sessionDataDir))
    {
    puts("Prevent subtree Auspice JSON files from expiring after two days: ");
    boolean subtreePersist = cartUsualBoolean(cart, "subtreePersist", FALSE);
    cgiMakeCheckBox("subtreePersist", subtreePersist);
    puts("</p><p>");
    }
cgiMakeOnClickSubmitButton(CHECK_FILE_OR_PASTE_INPUT_JS(seqFileVar, pastedIdVar),
                           "submit", "Upload");
char *exampleFile = phyloPlaceOrgSettingPath(org, "exampleFile");
if (isNotEmpty(exampleFile))
    {
    puts("&nbsp;&nbsp;");
    cgiMakeOnClickSubmitButton("{ loadingImage.run(); return true; }",
                               "exampleButton", "Upload Example File");
    if (sameString(org, "wuhCor1"))
        {
        puts("&nbsp;&nbsp;");
        puts("<a href='https://github.com/russcd/USHER_DEMO/' target=_blank>More example files</a>");
        }
    }
puts("</p>");
// Add a loading image to reassure people that we're working on it when they upload a big file
printf("<div><img id='loadingImg' src='../images/loading.gif' />\n");
printf("<span id='loadingMsg'></span></div>\n");
jsInline("$(document).ready(function() {\n"
         "    loadingImage.init($('#loadingImg'), $('#loadingMsg'), "
         "'<p style=\"color: red; font-style: italic;\">Uploading and processing your sequences "
         "may take some time. Please leave this window open while we work on your sequences.</p>');"
         "});\n");

puts("  </div>");
puts("</div>");
puts("<div class='readableWidth'>");
puts("  <div class='gbControl col-md-12'>");
puts("<h2>More information</h2>");
printf("<p>Upload your %s sequence (FASTA or VCF file) to find the most similar\n"
       "complete, high-coverage samples from \n", label);
if (sameString(org, "wuhCor1"))
    {
    puts("<a href='https://www.gisaid.org/' target='_blank'>GISAID</a>\n"
         "or from public sequence databases (INSDC: GenBank/ENA/DDBJ accessed using "
         "<a href='https://www.ncbi.nlm.nih.gov/labs/virus/vssi/#/virus?SeqType_s=Nucleotide&VirusLineage_ss=SARS-CoV-2,%20taxid:2697049' "
         "target=_blank>NCBI Virus</a>,\n"
         "<a href='https://www.cogconsortium.uk/data/' target=_blank>COG-UK</a> and the\n"
         "<a href='https://bigd.big.ac.cn/ncov/release_genome' "
         "target=_blank>China National Center for Bioinformation</a>), "
         "and your sequence's placement in the phylogenetic tree generated by the\n"
         "<a href='https://github.com/roblanf/sarscov2phylo' target='_blank'>sarscov2phylo</a>\n"
         "pipeline.\n");
    }
else
    {
    //#*** TODO get NCBI Virus link that is not hardcoded to MPXV
    puts("public sequence databases (INSDC: GenBank/ENA/DDBJ accessed using "
         "<a href='https://www.ncbi.nlm.nih.gov/labs/virus/vssi/#/virus?SeqType_s=Nucleotide&VirusLineage_ss=Monkeypox%20virus%20(monkeypox),%20taxid:10244' "
         "target=_blank>NCBI Virus</a>)\n"
         "and your sequence's placement in a global phylogenetic tree.\n"
         );
    }
puts("Placement is performed by\n"
     "<a href='https://github.com/yatisht/usher' target=_blank>"
     "Ultrafast Sample placement on Existing tRee (UShER)</a> "
     "(<a href='https://www.nature.com/articles/s41588-021-00862-7' target=_blank>"
     "Turakhia <em>et al.</em></a>).  UShER also generates local subtrees to show samples "
     "in the context of the most closely related sequences.  The subtrees can be visualized "
     "as Genome Browser custom tracks and/or using "
     "<a href='https://nextstrain.org' target=_blank>Nextstrain</a>'s interactive display "
     "which supports "
     "<a href='"NEXTSTRAIN_DRAG_DROP_DOC"' "
     "target=_blank>drag-and-drop</a> of local metadata that remains on your computer.\n");
if (microbeTraceHost())
    printf("If the subtree size is set to %d or smaller, then subtrees can also be visualized in "
           "<a href='https://github.com/CDCgov/MicrobeTrace/wiki' target=_blank>MicrobeTrace</a>, "
           "a network visualization tool that integrates and overlays genomic, laboratory, and "
           "epidemiologic data and offers multiple visualization options of your combined data.\n",
           MAX_MICROBETRACE_SUBTREE_SIZE);
puts("</p>");
if (sameString(org, "wuhCor1"))
    {
    puts("<p>\n"
         "GISAID data displayed in the Genome Browser are subject to GISAID's\n"
         "<a href='https://www.gisaid.org/registration/terms-of-use/' target=_blank>"
         "Terms and Conditions</a>.\n"
         "SARS-CoV-2 genome sequences and metadata are available for download from\n"
         "<a href='https://gisaid.org' target=_blank>GISAID</a> EpiCoV&trade;.\n"
         "</p>");
    puts("<p>\n"
         "<a href='/covid19.html'>COVID-19 Pandemic Resources at UCSC</a></p>\n");
    }
puts("</div>");
puts("</div>");
// If org directory includes non-empty download.html file then make a Download section
char *orgSkipHub = trackHubSkipHubName(org);
char downloadHtmlFile[1024];
safef(downloadHtmlFile, sizeof downloadHtmlFile, PHYLOPLACE_DATA_DIR "/%s/download.html", orgSkipHub);
struct lineFile *lf = lineFileMayOpen(downloadHtmlFile, TRUE);
if (lf != NULL)
    {
    char *line = NULL;
    int size;
    lineFileNext(lf, &line, &size);
    if (isNotEmpty(line))
        {
        puts("<div class='readableWidth'>");
        puts("  <div class='gbControl col-md-12'>");
        puts("<h2>Download public tree files</h2>");
        puts(line);
        while (lineFileNext(lf, &line, &size))
            puts(line);
        puts("  </div>");
        puts("</div>");
        }
    lineFileClose(&lf);
    }
puts("<div class='readableWidth'>");
puts("  <div class='gbControl col-md-12'>");
puts("<h2>Privacy and sharing</h2>");
puts("<h3>Please do not upload "
     "<a href='https://en.wikipedia.org/wiki/Protected_health_information#United_States' "
     "target=_blank>Protected Health Information (PHI)</a>.</h3>\n"
     "If even virus sequence files must remain local on your computer, then you can try "
     "<a href='https://shusher.gi.ucsc.edu/' target=_blank>ShUShER</a> "
     "which runs entirely in your web browser so that no files leave your computer."
     "</p>\n"
     "<p>We do not store your information "
     "(aside from the information necessary to display results)\n"
     "and will not share it with others unless you choose to share your Genome Browser view.</p>\n"
     "<p>In order to enable rapid progress in pandemic research and genomic contact tracing,\n"
     "please share your sequences by submitting them to an "
     "<a href='https://ncbiinsights.ncbi.nlm.nih.gov/2020/08/17/insdc-covid-data-sharing/' "
     "target=_blank>INSDC</a> member institution\n"
     "(<a href='https://submit.ncbi.nlm.nih.gov/sarscov2/' target=_blank>NCBI</a>,\n"
     "<a href='https://www.covid19dataportal.org/submit-data' target=_blank>EMBL-EBI</a>\n"
     "or <a href='https://www.ddbj.nig.ac.jp/ddbj/websub.html' target=_blank>DDBJ</a>)\n");
if (sameString(org, "wuhCor1"))
    puts("and <a href='https://www.gisaid.org/' target=_blank>GISAID</a>\n");
puts(".</p>\n");
puts("</div>");
puts("  </div>");
puts("<div class='readableWidth'>");
puts("<div class='gbControl col-md-12'>");
puts("<h2>Tutorial</h2>");
puts("<iframe width='950' height='535' src='https://www.youtube.com/embed/humQ1NyZOUM' "
     "frameborder='0' allow='accelerometer; autoplay; clipboard-write; encrypted-media; "
     "gyroscope; picture-in-picture' allowfullscreen></iframe>\n"
     "<h3><a href='https://www.cdc.gov/amd/pdf/slidesets/ToolkitModule_3.3-508C.pdf' "
     "target=_blank>Slides for tutorial</a></h3>\n"
     "<h3><a href='https://www.cdc.gov/amd/training/covid-19-gen-epi-toolkit.html' target=_blank>"
     "More tutorials from CDC COVID-19 Genomic Epidemiology Toolkit</a></h3>\n"
     "</p>"
     );
puts("</div>");
puts("</div>");
puts("</form>");
}

static void mainPage(char *org)
{
// Start web page with new-style header
webStartGbNoBanner(cart, org, "UShER: Upload");
jsInit();
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("ajax.js", NULL);
newPageStartStuff();

// Hidden form for reloading page when hpp_org select is changed
static char *saveVars[] = { orgVar };
jsCreateHiddenForm(cart, cgiScriptName(), saveVars, ArraySize(saveVars));

puts("<div class='row'>"
     "  <div class='row gbSectionBannerLarge'>\n"
     "    <div class='col-md-11'>UShER: Ultrafast Sample placement on Existing tRee</div>\n"
     "    <div class='col-md-1'></div>\n"
     "  </div>\n"
     "</div>\n"
     "<div class='row'>\n");
if (hgPhyloPlaceEnabled())
    {
    inputForm(org);
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

static void resultsPage(char *db, char *org, struct lineFile *lf)
/* QC the user's uploaded sequence(s) or VCF; if input looks valid then run usher
 * and display results. */
{
// If org is a real database or hub then set db to org.
if (hDbExists(org))
    db = org;
else
    {
    // Not a db -- see if it's a hub that is already connected:
    struct trackHubGenome *hubGenome = trackHubGetGenomeUndecorated(org);
    if (hubGenome != NULL)
        db = org;
    // Otherwise we're counting on the config to specify a .2bit file and we won't make CTs.
    }
webStartGbNoBanner(cart, db, "UShER: Results");
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("ajax.js", NULL);
newPageStartStuff();

if (issueBotWarning)
    {
    char *ip = getenv("REMOTE_ADDR");
    botDelayMessage(ip, botDelayMillis);
    }

// Allow 10 minutes for big sets of sequences
lazarusLives(15 * 60);

puts("<div class='row'>"
     "  <div class='row gbSectionBannerLarge'>\n"
     "    <div class='col-md-11'>UShER: Ultrafast Sample placement on Existing tRee</div>\n"
     "    <div class='col-md-1'></div>\n"
     "  </div>\n"
     "</div>\n"
     "<div class='row'>\n");
puts("  <div class='gbControl col-md-12'>");
fflush(stdout);

if (lf != NULL)
    {
    // Use trackLayout to get hgTracks parameters relevant to displaying trees:
    struct trackLayout tl;
    trackLayoutInit(&tl, cart);
    // Do our best to place the user's samples, make custom tracks if successful:
    char *phyloPlaceTree = cartOptionalString(cart, "phyloPlaceTree");
    int subtreeSize = cartUsualInt(cart, "subtreeSize", 50);
    boolean success = phyloPlaceSamples(lf, db, org, phyloPlaceTree, measureTiming, subtreeSize,
                                        &tl, cart);
    if (! success)
        {
        puts("<p></p>");
        puts("  </div>");
        // Let the user upload something else and try again:
        inputForm(org);
        }
    }
else
    {
    warn("Unable to read your uploaded data - please choose a file and try again, or click the "
         "&quot;try example&quot; button.");
    // Let the user try again:
    puts("  </div>");
    inputForm(org);
    }
puts("</div>\n");

newPageEndStuff();
}

static boolean serverAuthOk(char *plain, char *salty)
/* Construct a salted hash of plain and compare it to salty. */
{
char *salt = cfgOption(CFG_LOGIN_COOKIE_SALT);
if (! salt)
    salt = "";
char *plainMd5 = md5HexForString(plain);
struct dyString *dySalted = dyStringCreate("%s-%s", salt, plainMd5);
char *rightSalty = md5HexForString(dySalted->string);
boolean ok = sameOk(salty, rightSalty);
dyStringFree(&dySalted);
return ok;
}

INLINE void maybeComment(char *comment)
/* If comment is nonempty, append it to stderr.  Then print a newline regardless of comment. */
{
if (isNotEmpty(comment))
    fprintf(stderr, ": %s", comment);
fputc('\n', stderr);
}

#define CONTENT_TYPE "Content-Type: text/plain\n\n"

static void sendServerCommand(char *org)
/* If a recognized server command is requested (with minimal auth to prevent DoS), and usher server
 * is configured, then send the command to the usher server's manager fifo. */
{
pushWarnHandler(htmlVaBadRequestAbort);
pushAbortHandler(htmlVaBadRequestAbort);
char *plain = cgiOptionalString(serverPlainVar);
char *salty = cgiOptionalString(serverSaltyVar);
if (isNotEmpty(plain) && isNotEmpty(salty) && serverAuthOk(plain, salty))
    {
    if (serverIsConfigured(org))
        {
        char *command = cgiString(serverCommandVar);
        char *comment = cgiOptionalString(serverCommentVar);
        struct tempName tnCheckServer;
        trashDirFile(&tnCheckServer, "ct", "usher_check_server", ".txt");
        FILE *errFile = mustOpen(tnCheckServer.forCgi, "w");
        boolean serverUp = serverIsRunning(org, errFile);
        carefulClose(&errFile);
        if (sameString(command, "start"))
            {
            // This one is really a command for the CGI not the server manager fifo (because the
            // server is not yet running and needs to be started at this point), but uses the
            // same CGI interface.

            //#*** TODO implement this at the org level, descending into ref subdirs.  For now
            //#*** this is working because only SARS-CoV-2 has a server and org==ref for it.

            struct treeChoices *treeChoices = loadTreeChoices(org, org);
            if (treeChoices != NULL)
                {
                if (serverUp)
                    errAbort("Server is already running for org %s, see %s",
                             org, tnCheckServer.forCgi);
                struct tempName tnServerStartup;
                trashDirFile(&tnServerStartup, "ct", "usher_server_startup", ".txt");
                errFile = mustOpen(tnServerStartup.forCgi, "w");
                fprintf(stderr, "Usher server start for %s", org);
                maybeComment(comment);
                boolean success = startServer(org, treeChoices, errFile);
                carefulClose(&errFile);
                if (success)
                    {
                    fprintf(stderr, "Spawned usher server background process, details in %s",
                            tnServerStartup.forCgi);
                    printf(CONTENT_TYPE"Started server for %s\n", org);
                    }
                else
                    errAbort("Unable to spawn usher server background process, details in %s",
                             tnServerStartup.forCgi);
                }
            else
                errAbort("No treeChoices for org=%s", org);
            }
        else if (serverUp)
            {
            if (sameString(command, "reload"))
                {
                struct treeChoices *treeChoices = loadTreeChoices(org, org);
                fprintf(stderr, "Usher server reload for %s", org);
                maybeComment(comment);
                serverReloadProtobufs(org, treeChoices);
                printf(CONTENT_TYPE"Sent reload command for %s\n", org);
                }
            else if (sameString(command, "stop"))
                {
                fprintf(stderr, "Usher server stop for %s", org);
                maybeComment(comment);
                serverStop(org);
                printf(CONTENT_TYPE"Sent stop command for %s\n", org);
                }
            else
                {
                char commandCopy[16];
                safecpy(commandCopy, sizeof commandCopy, command);
                char *words[3];
                int wordCount = chopLine(commandCopy, words);
                int val;
                if (wordCount == 2 && (val = atol(words[1])) > 0)
                    {
                    if (sameString(words[0], "thread"))
                        {
                        fprintf(stderr, "Usher server thread count set to %d", val);
                        maybeComment(comment);
                        serverSetThreadCount(org, val);
                        printf(CONTENT_TYPE"Sent thread %d command for %s\n", val, org);
                        }
                    else if (sameString(words[0], "timeout"))
                        {
                        fprintf(stderr, "Usher server timeout set to %d", val);
                        maybeComment(comment);
                        serverSetTimeout(org, val);
                        printf(CONTENT_TYPE"Sent timeout %d command for %s\n", val, org);
                        }
                    else
                        errAbort("Unrecognized command '%s'", command);
                    }
                else
                    errAbort("Unrecognized command '%s'", command);
                }
            }
        else
            errAbort("Server for %s is down (see %s), cannot send command '%s'",
                     org, tnCheckServer.forCgi, command);
        }
    else
        errAbort("Usher server mode not configured for org=%s", org);
    }
else
    errAbort("Bad request");
popWarnHandler();
popAbortHandler();
}

static void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
char *db = NULL, *genome = NULL;
// Get the current db from the cart
getDbAndGenome(cart, &db, &genome, oldVars);
// The currently selected organism may or may not be a db/hub.
char *org = cartOptionalString(cart, orgVar);
if (isEmpty(org))
    {
    // If orgVar is not found but old cart var is set, use it and then remove it to tidy up.
    org = cartOptionalString(cart, "hpp_ref");
    if (isNotEmpty(org))
        cartRemove(cart, "hpp_ref");
    }
if (isEmpty(org))
    {
    // Default to db
    org = cloneString(db);
    }

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

measureTiming = cartUsualBoolean(cart, "measureTiming", measureTiming);

char *submitLabel = cgiOptionalString("submit");
char *newExampleButton = cgiOptionalString("exampleButton");
if ((submitLabel && sameString(submitLabel, "try example")) ||
    (newExampleButton && sameString(newExampleButton, "Upload Example File")))
    {
    char *exampleFile = phyloPlaceOrgSettingPath(org, "exampleFile");
    struct lineFile *lf = lineFileOpen(exampleFile, TRUE);
    resultsPage(db, org, lf);
    }
else if (cgiOptionalString(remoteFileVar))
    {
    char *url = cgiString(remoteFileVar);
    struct lineFile *lf = netLineFileOpen(url);
    resultsPage(db, org, lf);
    }
else if (isNotEmpty(trimSpaces(cgiOptionalString(pastedIdVar))))
    {
    char *pastedIds = cgiString(pastedIdVar);
    struct lineFile *lf = lineFileOnString("pasted names/IDs", TRUE, pastedIds);
    resultsPage(db, org, lf);
    }
else if (cgiOptionalString(seqFileVar) || cgiOptionalString(seqFileVar "__filename"))
    {
    struct lineFile *lf = lineFileFromFileInput(cart, seqFileVar);
    resultsPage(db, org, lf);
    }
else if (isNotEmpty(cgiOptionalString(serverCommandVar)))
    {
    sendServerCommand(org);
    }
else
    mainPage(org);
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
                       pastedIdVar, remoteFileVar,
                       serverCommandVar, serverCommentVar, serverPlainVar, serverSaltyVar,
                       NULL};
enteredMainTime = clock1000();
issueBotWarning = earlyBotCheck(enteredMainTime, "hgPhyloPlace", delayFraction, 0, 0, "html");

cgiSpoof(&argc, argv);
oldVars = hashNew(10);
addLdLibraryPath();

cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgPhyloPlace", enteredMainTime);
return 0;
}
