/* hgConvert - CGI-script to convert browser window coordinates
 * using chain files */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "hash.h"
#include "errAbort.h"
#include "jksql.h"
#include "linefile.h"
#include "hCommon.h"
#include "fa.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "web.h"
#include "chain.h"
#include "liftOver.h"
#include "liftOverChain.h"
#include "chromInfo.h"
#include "net.h"
#include "genark.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "quickLift.h"
#include "chromAlias.h"
#include "jsHelper.h"
#include "hPrint.h"
#ifdef NOTNOW
#include "bigChain.h"
#include "bigLink.h"
#endif


/* CGI Variables */
#define HGLFT_TOORG_VAR   "hglft_toOrg"           /* TO organism */
#define HGLFT_TODB_VAR   "hglft_toDb"           /* TO assembly */
#define HGLFT_DO_CONVERT "hglft_doConvert"	/* Do the actual conversion */

/* Global Variables */
static struct cart *cart;	        /* CGI and other variables */
static struct hash *oldVars = NULL;
static char *organism = NULL;
static char *database = NULL;

/* Javascript to support New Assembly pulldown when New Genome changes. */
/* Copies selected values to a hidden form */
char *onChangeToOrg = "document.mainForm.submit();";

static struct dbDb *matchingDb(struct dbDb *list, char *name)
/* Find database of given name in list or die trying. */
{
struct dbDb *db;
for (db = list; db != NULL; db = db->next)
    {
    if (sameString(name, db->name))
        return db;
    }

struct dbDb *toDb =  genarkLiftOverDb(name);
if (toDb == NULL)
    errAbort("Can't find %s in matchingDb", name);
return toDb;
}

static void askForDestination(struct liftOverChain *liftOver, char *fromPos,
	struct dbDb *fromDb, struct dbDb *toDb)
/* set up page for entering data */
{
struct dbDb *dbList;
boolean askAboutQuickLift = FALSE;
boolean quickLift = FALSE;

if (quickLiftEnabled(cart))
    {
    askAboutQuickLift = TRUE;
    quickLift = cartUsualBoolean(cart, "doQuickLift", FALSE);
    }

cartWebStart(cart, database, "Convert %s to New Assembly", fromPos);

/* Include autocomplete libraries */
jsIncludeAutoCompleteLibs();

/* create HTML form */
puts("<FORM ACTION=\"../cgi-bin/hgConvert\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* CSS for two-section layout */
puts("<style>\n"
     ".convertGrid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; max-width: 800px; }\n"
     ".convertSection { padding: 15px; border: 1px solid #ddd; border-radius: 4px; background: #fafafa; }\n"
     ".sectionLabel { font-weight: bold; margin-bottom: 10px; border-bottom: 1px solid #ccc; padding-bottom: 5px; }\n"
     ".fieldRow { margin: 8px 0; }\n"
     ".fieldLabel { display: inline-block; width: 80px; font-weight: bold; }\n"
     ".currentSelection { margin-top: 8px; color: #333; }\n"
     ".currentSelection::before { content: 'Selected: '; font-weight: bold; }\n"
     "</style>\n");

puts("<div class='convertGrid'>\n");

/* SOURCE SECTION (read-only) */
puts("<div class='convertSection'>\n");
puts("<div class='sectionLabel'>Source</div>\n");
hPrintf("<div class='fieldRow'><span class='fieldLabel'>Genome:</span> %s</div>\n", fromDb->organism);
hPrintf("<div class='fieldRow'><span class='fieldLabel'>Assembly:</span> %s</div>\n", fromDb->description);
puts("</div>\n");

/* DESTINATION SECTION (editable) */
puts("<div class='convertSection'>\n");
puts("<div class='sectionLabel'>Destination</div>\n");

/* Hidden fields for form submission */
hPrintf("<input name='%s' value='%s' type='hidden'>\n", HGLFT_TOORG_VAR, toDb->organism);
hPrintf("<input name='%s' value='%s' type='hidden'>\n", HGLFT_TODB_VAR, liftOver->toDb);

/* Search bar */
char *searchBarId = "toGenomeSearch";
puts("<div class='fieldRow'>\n");
puts("<span class='fieldLabel'>Search:</span>\n");
printGenomeSearchBar(searchBarId, "Search for target genome...", NULL, TRUE, NULL, NULL);
puts("</div>\n");

/* Current selection display */
char *selectedLabel = getCurrentGenomeLabel(liftOver->toDb);
hPrintf("<div class='currentSelection' id='toGenomeLabel'>%s</div>\n", selectedLabel);

/* Assembly dropdown (updates based on genome selection) */
puts("<div class='fieldRow'>\n");
puts("<span class='fieldLabel'>Assembly:</span>\n");
dbList = hGetLiftOverToDatabases(liftOver->fromDb);
printAllAssemblyListHtmlParm(liftOver->toDb, dbList, HGLFT_TODB_VAR, TRUE, "change", onChangeToOrg);
puts("</div>\n");

/* QuickLift option */
if (askAboutQuickLift)
    {
    puts("<div class='fieldRow' style='margin-top: 15px;'>\n");
    cgiMakeCheckBoxWithId("doQuickLift", quickLift, "doQuickLift");
    puts(" <label for='doQuickLift'>QuickLift tracks</label>\n");
    puts(" <a href='https://docs.google.com/document/d/1wecESHUpgTlE6U_Mj0OnfHeSZBrTX9hkZRN5jlJS8ZQ/edit?usp=sharing' "
         "target='ucscHelp' title='QuickLift is in beta testing' "
         "style='color:#8A2BE2;font-weight:bold;text-transform:uppercase;font-size:smaller;padding:2px "
         "4px;background:lavender;border-radius:3px;text-decoration:none;margin-left:6px;'>beta</a>\n");
    puts("</div>\n");
    }

puts("</div>\n");  /* end destination section */
puts("</div>\n");  /* end grid */

/* Submit button centered below */
puts("<div style='text-align: center; margin-top: 20px;'>\n");
cgiMakeButton(HGLFT_DO_CONVERT, "Submit");
puts("</div>\n");

/* JavaScript initialization for autocomplete with liftOver filtering */
jsInlineF(
    "let validTargets = new Set();\n"
    "\n"
    "fetch('../cgi-bin/hubApi/liftOver/listExisting?fromGenome=%s')\n"
    "    .then(response => response.json())\n"
    "    .then(data => {\n"
    "        if (data.existingLiftOvers) {\n"
    "            data.existingLiftOvers.forEach(chain => validTargets.add(chain.toDb));\n"
    "        }\n"
    "\n"
    "        // Custom onServerReply that processes results and filters to valid targets\n"
    "        function processAndFilterResults(result, term) {\n"
    "            let processed = processFindGenome(result, term);\n"
    "            let filtered = processed.filter(item => validTargets.has(item.genome));\n"
    "            if (filtered.length === 0 && processed.length > 0) {\n"
    "                // Found genomes but none have liftOver from source\n"
    "                return [{label: 'No liftOver available for matching genomes', value: '', genome: '', disabled: true}];\n"
    "            } else if (filtered.length === 0) {\n"
    "                // No genomes matched the search at all\n"
    "                return [{label: 'No genomes found', value: '', genome: '', disabled: true}];\n"
    "            }\n"
    "            return filtered;\n"
    "        }\n"
    "\n"
    "        // Error handler for API failures (e.g. HTTP 400)\n"
    "        function onSearchError(jqXHR, textStatus, errorThrown, term) {\n"
    "            return [{label: 'No genomes found', value: '', genome: '', disabled: true}];\n"
    "        }\n"
    "\n"
    "        function onGenomeSelect(selectEle, item) {\n"
    "            // Ignore disabled/placeholder items\n"
    "            if (item.disabled || !item.genome) {\n"
    "                return;\n"
    "            }\n"
    "            selectEle.textContent = item.label;\n"
    "            document.mainForm.%s.value = item.commonName.split('(')[0].trim();\n"
    "            document.mainForm.%s.value = item.genome;\n"
    "            document.mainForm.submit();\n"
    "        }\n"
    "\n"
    "        let selectEle = document.getElementById('toGenomeLabel');\n"
    "        initSpeciesAutoCompleteDropdown('%s', onGenomeSelect.bind(null, selectEle), null, null, processAndFilterResults, onSearchError);\n"
    "    });\n"
    "\n"
    "document.addEventListener('DOMContentLoaded', () => {\n"
    "    let btn = document.getElementById('%sButton');\n"
    "    if (btn) {\n"
    "        btn.addEventListener('click', () => {\n"
    "            let val = document.getElementById('%s').value;\n"
    "            $('[id=\\x27%s\\x27]').autocompleteCat('search', val);\n"
    "        });\n"
    "    }\n"
    "});\n"
    , liftOver->fromDb
    , HGLFT_TOORG_VAR
    , HGLFT_TODB_VAR
    , searchBarId, searchBarId, searchBarId, searchBarId
);

puts("</FORM>\n");

cartWebEnd();
}

static double scoreLiftOverChain(struct liftOverChain *chain,
    char *fromOrg, char *fromDb, char *toOrg, char *toDb, struct hash *dbRank )
/* Score the chain in terms of best match for cart settings */
{
double score = 0;

char *chainFromOrg = hOrganism(chain->fromDb);
char *chainToOrg = hOrganism(chain->toDb);
int fromRank = hashIntValDefault(dbRank, chain->fromDb, 0);
int maxRank = hashIntVal(dbRank, "maxRank");
int toRank = hashIntValDefault(dbRank, chain->toDb, maxRank);

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

// at the moment we aren't ranking genark db's
//if (toRank == 0)  // chains to db's that are not active shouldn't be considered
    //return 0;
score += 10*(maxRank-fromRank);
score += (maxRank - toRank);

return score;
}


static struct liftOverChain *defaultChoices(struct liftOverChain *chainList, char *fromOrg, char *fromDb)
/* Out of a list of liftOverChains and a cart, choose a
 * list to display. */
{
char *toOrg, *toDb;
struct liftOverChain *choice = NULL;
struct hash *dbRank = hGetDatabaseRank();
double bestScore = -1;
struct liftOverChain *this = NULL;

/* Get the initial values. */
toOrg = cartCgiUsualString(cart, HGLFT_TOORG_VAR, "0");
toDb = cartCgiUsualString(cart, HGLFT_TODB_VAR, "0");

if (sameWord(toOrg,"0"))
    toOrg = NULL;
if (sameWord(toDb,"0"))
    toDb = NULL;
if ((toDb != NULL) && !sameOk(toOrg, hOrganism(toDb)))
    toDb = NULL;

if (toOrg == NULL)
    toOrg = "Human";

for (this = chainList; this != NULL; this = this->next)
    {
    double score = scoreLiftOverChain(this, fromOrg, fromDb, toOrg, toDb, dbRank);
    if (score > bestScore)
	{
	choice = this;
	bestScore = score;
	}
    }

return choice;
}

static char *skipWord(char *s)
/* Skip word, and any leading spaces before next word. */
{
return skipLeadingSpaces(skipToSpaces(s));
}

static long chainTotalBlockSize(struct chain *chain)
/* Return sum of sizes of all blocks in chain */
{
struct cBlock *block;
long total = 0;
for (block = chain->blockList; block != NULL; block = block->next)
    total += block->tEnd - block->tStart;
return total;
}

static struct chain *chainLoadIntersecting(char *fileName,
	char *chrom, int start, int end)
/* Load the chains that intersect given region. */
{
struct lineFile *lf = netLineFileOpen(fileName);
char *line;
int chromNameSize = strlen(chrom);
struct chain *chainList = NULL, *chain;
#ifdef SOON	/* Put in if we index. */
boolean gotChrom = FALSE;
#endif  /* SOON */
int chainCount = 0;

while (lineFileNextReal(lf, &line))
    {
    if (startsWith("chain", line) && isspace(line[5]))
        {
	++chainCount;
	line = skipWord(line);	/* Skip over 'chain' */
	line = skipWord(line);	/* Skip over chain score */
	if (startsWith(chrom, line) && isspace(line[chromNameSize]))
	    {
#ifdef SOON	/* Put in if we index. */
	    gotChrom = TRUE;
#endif  /* SOON */
	    lineFileReuse(lf);
	    chain = chainReadChainLine(lf);
	    if (rangeIntersection(chain->tStart, chain->tEnd, start, end) > 0)
		{
		chainReadBlocks(lf, chain);
		slAddHead(&chainList, chain);
		}
	    else
	        chainFree(&chain);
	    }
#ifdef SOON	/* Put in if we index. */
	else if (gotChrom)
	    break;	/* We assume file is sorted by chromosome, so we're done. */
#endif /* SOON */
	}
    }
lineFileClose(&lf);
slReverse(&chainList);
return chainList;
}

static struct chain *chainLoadAndTrimIntersecting(char *fileName,
	char *chrom, int start, int end)
/* Load the chains that intersect given region, and trim them
 * to fit region. */
{
struct chain *rawList, *chainList = NULL, *chain, *next;
rawList = chainLoadIntersecting(fileName, chrom, start, end);
for (chain = rawList; chain != NULL; chain = next)
    {
    struct chain *subChain, *chainToFree;
    next = chain->next;
    chainSubsetOnT(chain, start, end, &subChain, &chainToFree);
    if (subChain != NULL)
	slAddHead(&chainList, subChain);
    if (chainToFree != NULL)
        chainFree(&chain);
    }
slSort(&chainList, chainCmpScore);
return chainList;
}

#ifdef NOTNOW
struct segment
{
struct segment *next;
unsigned start, end;
char strand;
};

void compressSegs(struct segment *segs)
{
struct segment *nextSeg = NULL;
struct segment *growSeg = NULL;

for(; segs; segs = nextSeg)
    {
    nextSeg = segs->next;
    if (nextSeg == NULL)
        break;

    if (growSeg == NULL)
        growSeg = segs;

    if ((nextSeg->start < growSeg->end + 10000) || (nextSeg->strand != growSeg->strand))
        {
        growSeg->end = nextSeg->end;
        //nextSeg = nextSeg->next;
        }
    else
        {
        growSeg->next = segs;
        growSeg = NULL;
        }
    }
}

static void getSegs(char *fromDb, char *toDb, struct segment **fromSegs, struct segment **toSegs, char *chrom, unsigned seqStart, unsigned seqEnd)
{
char *quickLiftFile = quickLiftGetChainPath(cart, fromDb, toDb);
struct lm *lm = lmInit(0);
//struct bigBedInterval *bbChain, *bbChainList =  bigBedIntervalQuery(bbiChain, chrom, seqStart, seqEnd, 0, lm);
struct bbiFile *bbiChain = bigBedFileOpenAlias(quickLiftFile, chromAliasFindAliases);
struct bigBedInterval *bbChain, *bbChainList =  bigBedIntervalQuery(bbiChain, chrom, seqStart, seqEnd, 0, lm);

char *links = bigChainGetLinkFile(quickLiftFile);
struct bbiFile *bbiLink = bigBedFileOpenAlias(links, chromAliasFindAliases);
struct bigBedInterval  *bbLink, *bbLinkList =  bigBedIntervalQuery(bbiLink, chrom, seqStart, seqEnd, 0, lm);

char *chainRow[1024];
char *linkRow[1024];
char startBuf[16], endBuf[16];

bigBedIntervalToRow(bbChainList, chrom, startBuf, endBuf, chainRow, ArraySize(chainRow));
struct bigChain *fbc = bigChainLoad(chainRow);

for (bbChain = bbChainList; bbChain != NULL; bbChain = bbChain->next)
    {
    bigBedIntervalToRow(bbChain, chrom, startBuf, endBuf, chainRow, ArraySize(chainRow));
    struct bigChain *bc = bigChainLoad(chainRow);
    if (differentString(bc->chrom, fbc->chrom))
        continue;

   for (bbLink = bbLinkList; bbLink != NULL; bbLink = bbLink->next)
        {
        bigBedIntervalToRow(bbLink, chrom, startBuf, endBuf, linkRow, ArraySize(linkRow));
        struct bigLink *bl = bigLinkLoad(linkRow);

        if (!sameString(bl->name, bc->name))
            continue;

        boolean isFlipped = fbc->strand[0] != bc->strand[0];
        struct segment *seg;
        AllocVar(seg);
        seg->start = bl->chromStart;
        seg->end = bl->chromEnd;
        seg->strand = 0;
        slAddHead(fromSegs, seg);

        unsigned width = bl->chromEnd - bl->chromStart;
        unsigned qStart = bl->qStart;
        unsigned qEnd = bl->qStart + width;
        if (bc->strand[0] == '-')
            {
            qStart = bc->qSize - qStart;
            qEnd = bc->qSize - qEnd;
            }

        AllocVar(seg);
        seg->start = qStart;
        seg->end = qEnd;
        seg->strand = isFlipped;
        slAddHead(toSegs, seg);
        }
    }
slReverse(toSegs);
//compressSegs(*toSegs);
slReverse(fromSegs);
//compressSegs(*fromSegs);

}

static void printSegs(struct segment *segs)
{
unsigned segNum = 0;

jsInline("segments: [\n");
for(; segs; segs = segs->next)
    jsInlineF("{ name: '%d', start: %d, end: %d },\n", segNum++, segs->start, segs->end);
jsInline("]\n");
}

static void drawSegments(char *fromDb, char *toDb, struct chain *chain)
{
struct segment *fromSegs = NULL;
struct segment *toSegs = NULL;
/*
unsigned seqStart = chain->tStart;
unsigned seqEnd = chain->tEnd;
char *chrom = chain->tName;
*/

unsigned qStart = chain->qStart;
unsigned qEnd = chain->qEnd;
if (chain->qStrand == '-')
    {
    unsigned saveQStart = qStart;
    qStart = chain->qSize - qEnd;
    qEnd = chain->qSize - saveQStart;
    }

getSegs(fromDb, toDb, &fromSegs, &toSegs, chain->qName, chain->qStart, chain->qEnd);

printf("<canvas id='canvas' width='900' height='500'></canvas>");
jsIncludeFile("parallelSegments.js", NULL);

jsInline("function drawCustom() {\n");
jsInline("const line1Data = {\n");
jsInlineF("range: { start: %d, end: %d },\n", chain->tStart, chain->tEnd);
jsInlineF("label: \"%s %s:%d-%d\",\n", fromDb, chain->tName, chain->tStart, chain->tEnd);
printSegs(toSegs);
jsInline("}\n");

jsInline("const line2Data = {\n");
jsInlineF("range: { start: %d, end: %d },\n", qStart, qEnd);
jsInlineF("label: \"%s %s:%d-%d\",\n", fromDb, chain->qName, qStart, qEnd);
printSegs(fromSegs);
jsInline("}\n");

jsInline("ParallelSegments.drawCoordinateBasedSegments(line1Data, line2Data);\n");
jsInline("}\n");
jsInline("drawCustom();\n");
}
#endif

static void doConvert(char *fromPos)
/* Actually do the conversion */
{
struct dbDb *fromDb = hDbDb(trackHubSkipHubName(database)), *toDb = hDbDb(cartString(cart, HGLFT_TODB_VAR));
#ifdef NOTNOW
boolean doSegments = TRUE;
#endif

if (fromDb == NULL)
    {
    char buffer[4096];
    safef(buffer, sizeof buffer, "'%s'", trackHubSkipHubName(database));
    fromDb =  genarkLiftOverDbs(buffer);
    }
if (toDb == NULL)
    toDb =  genarkLiftOverDb(cartString(cart, HGLFT_TODB_VAR));

if (!fromDb || !toDb)
    errAbort("Early error - unable to find matching database records in dbDb - please contact support");

chromAliasSetup(database);
cartWebStart(cart, database, "%s %s %s to %s %s", fromDb->organism, fromDb->description,
	fromPos, toDb->organism, toDb->description);

char *fileName = liftOverChainFile(trackHubSkipHubName(fromDb->name), trackHubSkipHubName(toDb->name));
if (isEmpty(fileName))
    errAbort("Unable to find a chain file from %s to %s - please contact support", fromDb->name, toDb->name);

fileName = hReplaceGbdbMustDownload(fileName);
char *chrom;
int start, end;
int origSize;
struct chain *chainList, *chain;
struct dyString *visDy = NULL;

if (!hgParseChromRange(database, fromPos, &chrom, &start, &end))
    errAbort("position %s is not in chrom:start-end format", fromPos);
origSize = end - start;

boolean doQuickLift = cartUsualBoolean(cart, "doQuickLift", FALSE);
cartRemove(cart, "doQuickLift");

unsigned quickChain = 0;
unsigned quickHub = 0;
struct trackDb *badList = NULL;

if (doQuickLift)
    {
    quickChain = quickLiftGetChainId(cart, trackHubSkipHubName(fromDb->name), trackHubSkipHubName(toDb->name));

    if (quickChain == 0)
        errAbort("can't find quickChain from %s to %s", fromDb->name, toDb->name);

    visDy = newDyString(1024);
    char *newHub = trackHubBuild(fromDb->name, cart, visDy, &badList);
    char *error = NULL;
    quickHub = hubFindOrAddUrlInStatusTable(cart, newHub, &error);
    if (error != NULL)
        errAbort("can't add quickLift hub (error %s)",error);
    }

chainList = chainLoadAndTrimIntersecting(fileName, chrom, start, end);
if (chainList == NULL)
    printf("Sorry this position couldn't be found in new assembly");
else
    {
    for (chain = chainList; chain != NULL; chain = chain->next)
        {
	int blockSize;
	int qStart, qEnd;
	if (chain->qStrand == '-')
	    {
	    qStart = chain->qSize - chain->qEnd;
	    qEnd = chain->qSize - chain->qStart;
	    }
	else
	    {
	    qStart = chain->qStart;
	    qEnd = chain->qEnd;
	    }
	blockSize = chainTotalBlockSize(chain);
        /* Check if the toDb database exists and if the chromosome
           sequence file (of the hgConvert result) exists in the location
           specified in chromInfo for the toDb. */

        boolean chromSeqExists = (sqlDatabaseExists(toDb->name) &&
				  chromSeqFileExists(toDb->name, chain->qName));
        /* Check if the toDb has active set to 1 in dbDb if the toDb
           database exists.
           If these conditions are met then print position link to
           browser for toDb, otherwise just print position without link. */
        boolean startedAnchor = FALSE;
        visDy = newDyString(20);
        if ((hDbIsActive(toDb->name) && chromSeqExists) || startsWith("hub:",toDb->nibPath) || sameString(toDb->nibPath, "genark"))
            {
            if (quickChain)
                printf("<A HREF=\"%s?db=%s&position=%s:%d-%d&quickLift.%d.%s=%d\">",
                   hgTracksName(),  toDb->name,  chain->qName, qStart+1, qEnd, quickHub, toDb->name, quickChain);
            else
                printf("<A HREF=\"%s?db=%s&position=%s:%d-%d\">",
		   hgTracksName(), toDb->name, chain->qName, qStart+1, qEnd);
            startedAnchor = TRUE;
            }
	printf("%s:%d-%d",  chain->qName, qStart+1, qEnd);
        if (startedAnchor)
	    printf("</A>");
	printf(" (%3.1f%% of bases, %3.1f%% of span)<BR>\n",
	    100.0 * blockSize/origSize,
	    100.0 * (chain->tEnd - chain->tStart) / origSize);
#ifdef NTONOW
        if (doSegments)
            drawSegments(fromDb->name, toDb->name, chain);
        break;
#endif
	}
    }
if (badList)
    {
    printf("<BR>Some of your tracks failed to lift because the type is not supported by quickLift.<BR><BR>");
    printf("<TABLE><TR><TD><B>Short label<TD><B>Type</TD></TR>");
    for(; badList; badList = badList->next)
        {
        printf("<TR><TD>%s</TD><TD>%s</TD></TR>", badList->shortLabel, badList->type);
        }
    printf("</TABLE>");
    }
cartWebEnd();
}

static struct liftOverChain *cleanLiftOverList(struct liftOverChain *list)
/* eliminate from the list where toDb doesn't exist in dbDb */
{
struct liftOverChain *cleanList = NULL;
struct hash *dbDbHash = hDbDbHash();
struct liftOverChain *this = NULL;
struct liftOverChain *next = NULL;
for (this = list; this != NULL; this = next)
    {
    next = this->next;
    if (hashLookup(dbDbHash, this->toDb) || startsWith("GC", this->toDb))
        slAddHead(&cleanList, this);
    else
        liftOverChainFree(&this);
    }
slReverse(&cleanList);
return cleanList;
}

static void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *fromPos = cartString(theCart, "position");

cart = theCart;
getDbAndGenome(cart, &database, &organism, oldVars);

// Try to deal with virt chrom position used by hgTracks.
if (startsWith(    MULTI_REGION_CHROM, cartUsualString(cart, "position", ""))
 || startsWith(OLD_MULTI_REGION_CHROM, cartUsualString(cart, "position", "")))
    cartSetString(cart, "position", cartUsualString(cart, "nonVirtPosition", ""));

if (cartVarExists(cart, HGLFT_DO_CONVERT))
    doConvert(fromPos);
else
    {
    struct liftOverChain *checkLiftOverList = liftOverChainListForDbFiltered(trackHubSkipHubName(database));
    struct liftOverChain *liftOverList = cleanLiftOverList(checkLiftOverList);
    struct liftOverChain *choice = defaultChoices(liftOverList, organism, trackHubSkipHubName(database));
    if (choice == NULL)
	errAbort("Sorry, no conversions available from this assembly.");
    struct dbDb *dbList, *fromDb, *toDb;
    dbList = hDbDbListMaybeCheck(FALSE);
    fromDb = matchingDb(dbList, choice->fromDb);
    toDb = matchingDb(dbList, choice->toDb);
    askForDestination(choice, fromPos, fromDb, toDb);
    liftOverChainFreeList(&liftOverList);
    }
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = { "submit", HGLFT_DO_CONVERT, NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgConvert", enteredMainTime);
return 0;
}

