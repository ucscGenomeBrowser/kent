/* hgLiftOver - CGI-script to convert coordinates using chain files */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "errAbort.h"
#include "hCommon.h"
#include "jksql.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "web.h"
#include "hash.h"
#include "liftOver.h"
#include "liftOverChain.h"
#include "errCatch.h"
#include "hgConfig.h"
#include "jsHelper.h"


/* CGI Variables */
#define HGLFT_USERDATA_VAR "hglft_userData"     /* typed/pasted in data */
#define HGLFT_DATAFILE_VAR "hglft_dataFile"     /* file of data to convert */
#define HGLFT_FROMORG_VAR "hglft_fromOrg"         /* FROM organism */
#define HGLFT_FROMDB_VAR "hglft_fromDb"         /* FROM assembly */
#define HGLFT_TOORG_VAR   "hglft_toOrg"           /* TO organism */
#define HGLFT_TODB_VAR   "hglft_toDb"           /* TO assembly */
#define HGLFT_ERRORHELP_VAR "hglft_errorHelp"      /* Print explanatory text */
#define HGLFT_REFRESHONLY_VAR "hglft_doRefreshOnly"      /* Just refresh drop-down lists */
#define HGLFT_LAST_CHAIN "hglft_lastChain"
#define HGLFT_EXTRA_NAME_INFO "hglft_extranameinfo" /* Include input position in output item names */

/* liftOver options: */
#define HGLFT_MINMATCH "hglft_minMatch"          
#define HGLFT_MINSIZEQ "hglft_minSizeQ"
#define HGLFT_MINCHAINT "hglft_minChainT"
#define HGLFT_MULTIPLE "hglft_multiple"
#define HGLFT_MINBLOCKS "hglft_minBlocks"
#define HGLFT_FUDGETHICK "hglft_fudgeThick"

/* Global Variables */
struct cart *cart;	        /* CGI and other variables */
struct hash *oldVars = NULL;

/* Filename prefix */
#define HGLFT   "hglft"

/* Javascript to support New Assembly pulldown when Orig Assembly changes */
/* Copies selected value from the Original Assembly pulldown to a hidden form
*/
char *onChange = 
"document.mainForm."
HGLFT_REFRESHONLY_VAR
".value = 1;"
"document.mainForm.submit();";

char *chainStringVal(struct liftOverChain *chain)
/* keep the last chain in memory in this format */
{
char chainS[64];
safef(chainS, sizeof(chainS), "%s.%s", chain->fromDb, chain->toDb);
return cloneString(chainS);
}

void webMain(struct liftOverChain *chain, boolean multiple, boolean keepSettings, int minSizeQ, 
	     int minChainT, float minBlocks, float minMatch, boolean fudgeThick, boolean extraNameInfo)
/* set up page for entering data */
{
char *fromOrg = hOrganism(chain->fromDb), *toOrg = hOrganism(chain->toDb);
char *chainString = chainStringVal(chain);
cgiParagraph(
    "This tool converts genome coordinates and annotation files "
    "from the original to the new assembly using an alignment.&nbsp;&nbsp;"
    "The input regions can be entered into the text box or uploaded as a file.&nbsp;&nbsp;"
    "For files over 500Mb, use the command-line tool described in our "
    "<a href=\"../goldenPath/help/hgTracksHelp.html#Liftover\">LiftOver documentation</a>."
    "&nbsp;&nbsp;If a pair of assemblies cannot be selected from the pull-down menus,"
    " a sequential lift may still be possible (e.g., mm9 to mm10 to mm39).&nbsp;&nbsp;"
    "If your desired conversion is still not available, please "
    "<a href=\"../../contacts.html\">contact us</a>."
    "");

/* create HMTL form */
puts("<FORM ACTION=\"../cgi-bin/hgLiftOver\" METHOD=\"POST\" "
       " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* create HTML table for layout purposes */
puts("\n<TABLE WIDTH=\"100%\">\n");
printf("<TR>\n");
jsIncludeAutoCompleteLibs();
char *searchBarId = "fromGenomeSearch";
printf("<input name='%s' value='%s' type='hidden'></input>\n", HGLFT_FROMDB_VAR, chain->fromDb);
printf("<input name='%s' value='%s' type='hidden'></input>\n", HGLFT_FROMORG_VAR, fromOrg);
printf("<input name='formMethod' value='GET' type='hidden'></input>\n");
printf("<TD class='searchCell'>\n");
printGenomeSearchBar(searchBarId, "Search any species, genome or assembly name", NULL, TRUE, "Change original genome:", NULL);
jsInlineF(
    "setupGenomeSearchBar({\n"
    "    inputId: '%s',\n"
    "    labelElementId: 'fromGenomeLabel',\n"
    "    apiUrl: 'hubApi/findGenome?browser=mustExist&liftable=true&q=',\n"
    "    onSelect: function(item) {\n"
    "        document.mainForm."HGLFT_REFRESHONLY_VAR".value=1;\n"
    "        document.mainForm."HGLFT_FROMDB_VAR".value=item.genome;\n"
    "        document.mainForm."HGLFT_FROMORG_VAR".value=item.commonName.split('(')[0].trim();\n"
    "        document.mainForm.submit();\n"
    "    }\n"
    "});\n"
    , searchBarId
);
printf("</TD>\n");
printf("<TD class='searchCell' ALIGN=CENTER>\n");
printf("<div class='flexContainer'>\n");
printf("<span>Currently selected genome:</span>\n");
printf("<span id='fromGenomeLabel'>%s (%s)</span>\n", fromOrg, chain->fromDb);
printf("</div>\n");
printf("</TD>\n");

// print select/options for toDb, it is more intuitive than a search bar
struct dbDb *dbList = hGetLiftOverToDatabases(chain->fromDb);
printf("<TD class='searchCell'>\n");
printf("<label>Change new genome:</label>\n");
printf("<div class='flexContainer'>\n");
printLiftOverGenomeList(HGLFT_TOORG_VAR, chain->toDb, dbList, "change", onChange);
printf("</div></td>\n");
printf("<TD class='searchCell'>\n");
printf("<div class='flexContainer'>\n");
printf("<label>Change new assembly:</label>\n");
printAllAssemblyListHtmlParm(chain->toDb, dbList, HGLFT_TODB_VAR, TRUE, NULL, NULL);
printf("</div></td>\n");

cgiTableEnd();

printf("<br>");
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiTableField("Minimum ratio of bases that must remap:");
cgiSimpleTableFieldStart();
cgiMakeDoubleVar(HGLFT_MINMATCH, (keepSettings) ? minMatch : chain->minMatch,6);
puts("&nbsp;");
printInfoIcon("The minimum ratio of basepairs of the input region covered by an alignment. Regions scoring lower than this will not be lifted at all.");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableRowEnd();


cgiSimpleTableRowStart();
cgiTableField("<B>Regions defined by chrom:start-end (BED 4 to BED 6)</B>");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Keep original positions in output:");
cgiSimpleTableFieldStart();
cgiMakeCheckBox(HGLFT_EXTRA_NAME_INFO,extraNameInfo);
puts("&nbsp;");
printInfoIcon("Lifted items for BED4 and up will include their original positions as part of their output names to assist in determining what got mapped where (in case multiple items have the same name in the input).  Coordinates are 1-based fully closed, so the BED entry &quot;chr1 100 150 item1&quot; will be labeled &quot;chr1:101-150:item1&quot;.");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Allow multiple output regions:");
cgiSimpleTableFieldStart();
cgiMakeCheckBox(HGLFT_MULTIPLE,multiple);
puts("&nbsp;");
printInfoIcon("By default, input regions that map to multiple regions will not be lifted at all. When this option is checked, all targets are output.");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;&nbsp;Minimum hit size in query:");
cgiSimpleTableFieldStart();
cgiMakeIntVar(HGLFT_MINSIZEQ,(keepSettings) ? minSizeQ : chain->minSizeQ,4);
puts("&nbsp;");
printInfoIcon("In multiple output mode, repeated regions within longer input regions can lead to artifacts. The 'hit size' filter allows to keep only targets with a certain length.");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;&nbsp;Minimum chain size in target:");
cgiSimpleTableFieldStart();
cgiMakeIntVar(HGLFT_MINCHAINT,(keepSettings) ? minChainT : chain->minChainT,4);
puts("&nbsp;");
printInfoIcon("In multiple output mode, keeps only targets lifted through alignments of a certain length. At higher phylogenetic distances (e.g. human/mouse), the filters can be useful to keep all copies of the input regions but remove dozens of new small targets introduced by a repeat/transposon within a longer input region.");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("<B>Regions with an exon-intron structure (usually transcripts, BED 12)</B>");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Minimum ratio of alignment blocks or exons that must map:");
cgiSimpleTableFieldStart();
cgiMakeDoubleVar(HGLFT_MINBLOCKS,(keepSettings) ? minBlocks : chain->minBlocks,6);
puts("&nbsp;");
printInfoIcon("The minimum ratio of the number of exons (not their bases) covered by the alignment. Transcripts lower than this will not be output at all. If an exon (range thickStart-thickEnd) is not alignable at all, it will be skipped or, if the option below is checked, lifted it to the closest alignable base.");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("If exon is not mapped, use the closest mapped base:");
cgiSimpleTableFieldStart();
cgiMakeCheckBox(HGLFT_FUDGETHICK,(keepSettings) ? fudgeThick : (chain->fudgeThick[0]=='Y'));
puts("&nbsp;");
printInfoIcon("If checked, exons that are not covered by an alignment will be lifted to the closest alignable base.");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableRowEnd();

cgiTableEnd();

/* text box and two buttons (submit, reset) */
puts("<p style='margin-left:3px'>Paste your data below, using one position per line. Supported formats include "
        "<a href='../../FAQ/FAQformat.html#format1'>BED </a> (e.g. \"chr4 100000 100001\", "
        "0-based) and position box (\"chr4:100,001-100,001\", 1-based). "
        "Refer to the <a href='../goldenPath/help/hgTracksHelp.html#Liftover'>documentation</a> "
        "for detailed information. LiftOver is not recommended for SNPs with rsIDs; refer to our "
        "<a href='/FAQ/FAQreleases.html#snpConversion'>FAQ</a> for additional information. For "
        "information on errors in the LiftOver output, see our "
        "<a href='/FAQ/FAQdisplay.html#display4'>FAQ</a>.\n");

cgiSimpleTableStart();
cgiSimpleTableRowStart();

cgiSimpleTableFieldStart();
cgiMakeTextArea(HGLFT_USERDATA_VAR, cartCgiUsualString(cart, HGLFT_USERDATA_VAR, NULL), 10, 80);
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
cgiMakeClearButton("mainForm", HGLFT_USERDATA_VAR);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

/* next  row -- file upload controls */
cgiParagraph("&nbsp;Or upload data from a file (<a href=\"../FAQ/FAQformat.html#format1\" target=\"_blank\">BED</a> or chrN:start-end in plain text format):");
cgiSimpleTableStart();
cgiSimpleTableRowStart();
printf("<TD><INPUT TYPE=FILE NAME=\"%s\"></TD>\n", HGLFT_DATAFILE_VAR);
puts("<TD><INPUT TYPE=SUBMIT NAME=SubmitFile VALUE=\"Submit file\"></TD>\n");
cgiTableRowEnd();
cgiTableEnd();
printf("<input type=\"hidden\" name=\"%s\" value=\"0\">\n",
                        HGLFT_REFRESHONLY_VAR);
printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n", HGLFT_LAST_CHAIN, chainString);

cartSaveSession(cart);
puts("</FORM>");
freeMem(fromOrg);
freeMem(toOrg);
}

void webParamsUsed(float minMatch, boolean multiple, int minSizeQ, int minChainT, float minBlocks, boolean fudgeThick)
{
webNewSection("Parameters Used");
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiTableField("Minimum ratio of bases that must map:");
cgiSimpleTableFieldStart();
printf("%.2f",minMatch);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("BED 4 to BED 6 Options");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Allow multiple output regions:");
cgiSimpleTableFieldStart();
printf("%s", multiple ? "on" : "off");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;&nbsp;Minimum hit size in query:");
cgiSimpleTableFieldStart();
printf("%d",minSizeQ);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;&nbsp;Minimum chain size in target:");
cgiSimpleTableFieldStart();
printf("%d",minChainT);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("BED 12 Options");
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Min ratio of alignment blocks or exons that must map:");
cgiSimpleTableFieldStart();
printf("%.2f",minBlocks);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("If thickStart/thickEnd is not mapped, use the closest mapped base:");
cgiSimpleTableFieldStart();
printf("%s", fudgeThick ? "on" : "off");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
}

void webDownloads()
{
webNewSection("Command Line Tool");
cgiParagraph(
"To lift genome annotations locally on Linux systems, download the "
"<A HREF=\"https://genome-store.ucsc.edu\">" 
"<I>LiftOver</I></A> executable and the appropriate "
"<A HREF=\"http://hgdownload.soe.ucsc.edu/downloads.html#liftover\">"
"chain file</A>."
" Run <I>liftOver</I> with no arguments to see the usage message.\n"
"See the <a href=\"../goldenPath/help/hgTracksHelp.html#Liftover\">LiftOver documentation</a>.");
}


double scoreLiftOverChain(struct liftOverChain *chain,
    char *fromOrg, char *fromDb, char *toOrg, char *toDb,
    char *cartOrg, char *cartDb, struct hash *dbRank, 
    struct hash *dbHash)
/* Score the chain in terms of best match for cart settings */
{
double score = 0;
struct dbDb *chainFromDbDb = hashFindVal(dbHash, chain->fromDb);
struct dbDb *chainToDbDb = hashFindVal(dbHash, chain->toDb);

char *chainFromOrg = (chainFromDbDb) ? chainFromDbDb->organism : hOrganism(chain->fromDb);
char *chainToOrg = (chainToDbDb) ? chainToDbDb->organism : hOrganism(chain->toDb);
int maxRank = hashIntVal(dbRank, "maxRank"); 
int fromRank = hashIntValDefault(dbRank, chain->fromDb, maxRank);  /* values up to approx. #assemblies */
int toRank = hashIntValDefault(dbRank, chain->toDb, maxRank);

if (!chainFromOrg || !chainToOrg)
    return 0;

if (sameOk(fromOrg,chainFromOrg) &&
    sameOk(fromDb,chain->fromDb) && 
    sameOk(toOrg,chainToOrg) &&
    sameOk(toDb,chain->toDb))
    score += 10000000;

if (sameOk(fromOrg,chainFromOrg)) 
    score += 2000000;
if (sameOk(fromDb,chain->fromDb)) 
    score += 1000000;

if (sameOk(toOrg,chainToOrg))
    score += 200000;
if (sameOk(toDb,chain->toDb))
    score += 100000;

if (sameOk(cartDb,chain->fromDb)) 
    score +=  20000;
if (sameOk(cartDb,chain->toDb)) 
    score +=  10000;

if (sameOk(cartOrg,chainFromOrg)) 
    score +=  2000;
if (sameOk(cartOrg,chainToOrg)) 
    score +=  1000;

score += 10*(maxRank-fromRank);
score += (maxRank - toRank);

return score;
}


struct liftOverChain *defaultChoices(struct liftOverChain *chainList,
				     char *cartDb)
/* Out of a list of liftOverChains and a cart, choose a
 * list to display. */
{
char *fromOrg, *fromDb, *toOrg, *toDb, *cartOrg;
struct liftOverChain *choice = NULL;  
struct hash *dbRank = hGetDatabaseRank();
struct hash *dbDbHash = hDbDbHash();
double bestScore = -1;
struct liftOverChain *this = NULL;

/* Get the initial values. */
fromOrg = cartCgiUsualString(cart, HGLFT_FROMORG_VAR, "0");
fromDb = cartCgiUsualString(cart, HGLFT_FROMDB_VAR, "0");
toOrg = cartCgiUsualString(cart, HGLFT_TOORG_VAR, "0");
toDb = cartCgiUsualString(cart, HGLFT_TODB_VAR, "0");
cartOrg = hOrganism(cartDb);

if (sameWord(fromOrg,"0"))
    fromOrg = NULL;
if (sameWord(fromDb,"0"))
    fromDb = NULL;
if (sameWord(toOrg,"0"))
    toOrg = NULL;
if (sameWord(toDb,"0"))
    toDb = NULL;
if (sameWord(cartDb,"0"))
    cartDb = NULL;
if ((fromDb != NULL) && !sameWordOk(fromOrg, hOrganism(fromDb)))
    fromDb = NULL;
if ((toDb != NULL) && !sameOk(toOrg, hOrganism(toDb)))
    toDb = NULL;

for (this = chainList; this != NULL; this = this->next)
    {
    if (sameOk(this->fromDb ,fromDb) && sameOk(this->toDb, toDb))
        {
        choice = this;
        break;
        }
    double score = scoreLiftOverChain(this, fromOrg, fromDb, toOrg, toDb, cartOrg, cartDb, dbRank, dbDbHash);
    if (score > bestScore)
	{
	choice = this;
	bestScore = score;
	}
    }

return choice;
}


void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *userData;
char *organism;
char *db;
float minBlocks, minMatch;
boolean multiple, fudgeThick;
int minSizeQ, minChainT;
boolean refreshOnly = FALSE;
boolean keepSettings = FALSE;
boolean extraNameInfo = FALSE;
char *thisChain = NULL;
char *lastChain = NULL;

struct liftOverChain *chainList = NULL, *choice;

cart = theCart;

if (cgiOptionalString(HGLFT_ERRORHELP_VAR))
    {
    puts("<PRE>");
    puts(liftOverErrHelp());
    puts("</PRE>");
    return;
    }

/* Get data to convert - from userData variable, or if 
 * that is empty from a file. */

if (cartOptionalString(cart, "SubmitFile"))
    userData = cartOptionalString(cart, HGLFT_DATAFILE_VAR);
else
    userData = cartOptionalString(cart, HGLFT_USERDATA_VAR);
cartWebStart(cart, NULL, "Lift Genome Annotations");

getDbAndGenome(cart, &db, &organism, oldVars);

chainList = liftOverChainList();

choice = defaultChoices(chainList, db);
thisChain = chainStringVal(choice);
if (choice == NULL)
    errAbort("Sorry, no conversions available from this assembly\n");

minSizeQ = cartCgiUsualInt(cart, HGLFT_MINSIZEQ, choice->minSizeQ);
minChainT = cartCgiUsualInt(cart, HGLFT_MINCHAINT, choice->minChainT);
minBlocks = cartCgiUsualDouble(cart, HGLFT_MINBLOCKS, choice->minBlocks);
minMatch = cartCgiUsualDouble(cart, HGLFT_MINMATCH, choice->minMatch);
fudgeThick = cartCgiUsualBoolean(cart, HGLFT_FUDGETHICK, (choice->fudgeThick[0]=='Y') ? TRUE : FALSE);
multiple = cartCgiUsualBoolean(cart, HGLFT_MULTIPLE, (choice->multiple[0]=='Y') ? TRUE : FALSE);
refreshOnly = cartCgiUsualInt(cart, HGLFT_REFRESHONLY_VAR, 0);
lastChain = cartCgiUsualString(cart, HGLFT_LAST_CHAIN, NULL);
if (lastChain && thisChain && sameString(lastChain, thisChain))
    keepSettings = TRUE;
extraNameInfo = cartCgiUsualBoolean(cart, HGLFT_EXTRA_NAME_INFO, FALSE);

webMain(choice, multiple, keepSettings, minSizeQ, minChainT, minBlocks, minMatch, fudgeThick, extraNameInfo);
liftOverChainFreeList(&chainList);

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (!refreshOnly && userData != NULL && userData[0] != '\0')
        {
        struct hash *chainHash = newHash(0);
        char *chainFile;
        struct tempName oldTn, mappedTn, unmappedTn;
        FILE *old, *mapped, *unmapped;
        char *line;
        int lineSize;
        char *fromDb, *toDb;
        int ct = -1, errCt = 0;
        enum liftOverFileType lft;
        /* read in user data and save to file */
        makeTempName(&oldTn, HGLFT, ".user");
        old = mustOpen(oldTn.forCgi, "w");

        fputs(userData, old);
        fputs("\n", old);           /* in case user doesn't end last line */
        carefulClose(&old);
        chmod(oldTn.forCgi, 0666);

        /* setup output files -- one for converted lines, the other
         * for lines that could not be mapped */
        makeTempName(&mappedTn, HGLFT, ".bed");
        makeTempName(&unmappedTn, HGLFT, ".err");
        mapped = mustOpen(mappedTn.forCgi, "w");
        chmod(mappedTn.forCgi, 0666);
        unmapped = mustOpen(unmappedTn.forCgi, "w");
        chmod(unmappedTn.forCgi, 0666);

        fromDb = cgiString(HGLFT_FROMDB_VAR);
        toDb = cgiString(HGLFT_TODB_VAR);
        chainFile = liftOverChainFile(fromDb, toDb);
        if (chainFile == NULL)
            errAbort("ERROR: Can't convert from %s to %s: no chain file loaded",
                                    fromDb, toDb);

        readLiftOverMap(chainFile, chainHash);
        lft = liftOverSniff(oldTn.forCgi);
        if (lft == bed)
        ct = liftOverBed(oldTn.forCgi, chainHash, 
                minMatch, minBlocks, 0, minSizeQ,
                minChainT, 0,
                            fudgeThick, mapped, unmapped, multiple, FALSE, NULL, &errCt, extraNameInfo);
        else if (lft == positions)
        ct = liftOverPositions(oldTn.forCgi, chainHash, 
                minMatch, minBlocks, 0, minSizeQ,
                minChainT, 0,
                fudgeThick, mapped, unmapped, multiple, NULL, &errCt);
        if (ct == -1)
            /* programming error */
            errAbort("ERROR: Unsupported data format.\n");

        webNewSection("Results");
        if (ct > 0)
            {
            /* some records succesfully converted */
            cgiParagraph("");
            printf("Successfully converted %d record", ct);
            printf("%s: ", ct > 1 ? "s" : "");
	    printf("<a href='#' data-url='%s' class='link' id='viewLink'><BR>View conversions</a>\n", mappedTn.forCgi);
            printf("<A HREF=%s TARGET=_blank><BR>Download conversions</A>\n", mappedTn.forCgi);
            jsInlineF("document.getElementById('viewLink').addEventListener('click', function(ev) { "
                "ev.preventDefault();"
                "forceDisplayBedFile(ev.currentTarget.getAttribute('data-url'));"
                "});");
            }

        if (errCt)
            {
            /* some records not converted */
            cgiParagraph("");
            printf("Conversion failed on %d record", errCt);
            printf("%s. &nbsp;&nbsp;&nbsp;", errCt > 1 ? "s" : "");
            printf("<A HREF=%s TARGET=_blank><BR>Display failure file</A>&nbsp; &nbsp;\n",
                             unmappedTn.forCgi);
            printf("<A HREF=\"../cgi-bin/hgLiftOver?%s=1\" TARGET=_blank><BR>Explain failure messages</A>\n", HGLFT_ERRORHELP_VAR);
            puts("<P>Failed input regions:\n");
            struct lineFile *errFile = lineFileOpen(unmappedTn.forCgi, TRUE);
            puts("<BLOCKQUOTE><PRE>\n");
            while (lineFileNext(errFile, &line, &lineSize))
                puts(line);
            lineFileClose(&errFile);
            puts("</PRE></BLOCKQUOTE>\n");
            }
        if ((multiple) && (lft == positions))
        {
        puts("<BLOCKQUOTE><PRE>\n");
        puts("Note: &quot;multiple&quot; option is not supported for position format.");
        puts("</PRE></BLOCKQUOTE>\n");
        }
        webParamsUsed(minMatch, multiple, minSizeQ, minChainT, minBlocks, fudgeThick);

        carefulClose(&unmapped);
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError || errCatch->gotWarning)
    warn("%s", errCatch->message->string);
errCatchFree(&errCatch);
webDownloads();
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "SubmitFile",
                        HGLFT_USERDATA_VAR,
                        HGLFT_DATAFILE_VAR,
                        HGLFT_ERRORHELP_VAR,
                        NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgLiftOver", enteredMainTime);
return 0;
}

