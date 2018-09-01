/* hgGateway: make it easy to select a species and assembly
 *
 * Copyright (C) 2016 The Regents of the University of California
 *
 * This CGI has three modes of operation:
 *  - HTML output for main page (default); most HTML is in hgGateway.html
 *  - cart-based JSON responses to ajax requests from javascript (using hg/lib/cartJson.c)
 *    (if CGI param CARTJSON_COMMAND exists)
 *  - no cart; fast JSON responses to species-search autocomplete requests
 *    (if CGI param SEARCH_TERM exists)
 */
#include "common.h"
#include "cart.h"
#include "cartJson.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "googleAnalytics.h"
#include "hCommon.h"
#include "hgConfig.h"
#include "hdb.h"
#include "htmshell.h"
#include "hubConnect.h"
#include "hui.h"
#include "jsHelper.h"
#include "jsonParse.h"
#include "obscure.h"  // for readInGulp
#include "regexHelper.h"
#include "suggest.h"
#include "trackHub.h"
#include "web.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */
struct hash *oldVars = NULL;          /* Old contents of cart before it was updated by CGI */

#define SEARCH_TERM "hggw_term"

static char *maybeGetDescriptionText(char *db)
/* Slurp the description.html file for db into a string (if possible, don't die if
 * we can't read it) and return it. */
{
struct errCatch *errCatch = errCatchNew();
char *descText = NULL;
if (errCatchStart(errCatch))
    {
    char *htmlPath = hHtmlPath(db);
    if (isNotEmpty(htmlPath))
        descText = udcFileReadAll(htmlPath, NULL, 0, NULL);
    }
errCatchEnd(errCatch);
// Just ignore errors for now.
return descText;
}

static int trackHubCountAssemblies(struct trackHub *hub)
/* Return the number of hub's genomes that are hub assemblies, i.e. that have a twoBitPath. */
{
int count = 0;
struct trackHubGenome *genome;
for (genome = hub->genomeList;  genome != NULL;  genome = genome->next)
    {
    if (isNotEmpty(genome->twoBitPath))
        count++;
    }
return count;
}

static char *trackHubDefaultAssembly(struct trackHub *hub)
/* If hub->defaultDb is an assembly genome, return it; otherwise return the first assembly.
 * Don't free result. */
{
char *defaultDb = hub->defaultDb;
char *firstAssemblyDb = NULL;
struct trackHubGenome *genome;
for (genome = hub->genomeList;  genome != NULL;  genome = genome->next)
    {
    if (isNotEmpty(genome->twoBitPath))
        {
        if (sameString(defaultDb, genome->name))
            return defaultDb;
        else if (firstAssemblyDb == NULL)
            firstAssemblyDb = genome->name;
        }
    }
return firstAssemblyDb;
}

static void listAssemblyHubs(struct jsonWrite *jw)
/* Write out JSON describing assembly hubs (not track-only hubs) connected in the cart. */
{
jsonWriteListStart(jw, "hubs");
struct hubConnectStatus *status, *statusList = hubConnectStatusListFromCart(cart);
for (status = statusList;  status != NULL;  status = status->next)
    {
    struct trackHub *hub = status->trackHub;
    if (hub == NULL)
        continue;
    int assemblyCount = trackHubCountAssemblies(hub);
    if (assemblyCount > 0)
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "name", hub->name);
        jsonWriteString(jw, "shortLabel", hub->shortLabel);
        jsonWriteString(jw, "longLabel", hub->longLabel);
        jsonWriteString(jw, "defaultDb", trackHubDefaultAssembly(hub));
        jsonWriteString(jw, "hubUrl", status->hubUrl);
        jsonWriteNumber(jw, "assemblyCount", assemblyCount);
        jsonWriteString(jw, "errorMessage", status->errorMessage);
        // We might be able to do better than this for taxId, for example if defaultDb is local
        // or if hub genomes ever specify taxId...
        jsonWriteNumber(jw, "taxId", 0);
        jsonWriteObjectEnd(jw);
        }
    }
jsonWriteListEnd(jw);
}


static void writeFindPositionInfo(struct jsonWrite *jw, char *db, int taxId, char *hubUrl,
                                  char *position)
/* Write JSON for the info needed to populate the 'Find Position' section. */
{
char *genome = hGenome(db);
if (isEmpty(genome))
    {
    jsonWriteStringf(jw, "error", "No genome for db '%s'", db);
    }
else
    {
    jsonWriteString(jw, "db", db);
    jsonWriteNumber(jw, "taxId", taxId);
    jsonWriteString(jw, "genome", genome);
    struct slPair *dbOptions = NULL;
    char genomeLabel[PATH_LEN*4];
    if (isNotEmpty(hubUrl))
        {
        struct trackHub *hub = hubConnectGetHub(hubUrl);
        if (hub == NULL)
            {
            jsonWriteStringf(jw, "error", "Can't connect to hub at '%s'", hubUrl);
            return;
            }
        struct dbDb *dbDbList = trackHubGetDbDbs(hub->name);
        dbOptions = trackHubDbDbToValueLabel(dbDbList);
        safecpy(genomeLabel, sizeof(genomeLabel), hub->shortLabel);
        jsonWriteString(jw, "hubUrl", hubUrl);
        }
    else
        {
        dbOptions = hGetDbOptionsForGenome(genome);
        safecpy(genomeLabel, sizeof(genomeLabel), genome);
        }
    jsonWriteValueLabelList(jw, "dbOptions", dbOptions);
    jsonWriteString(jw, "genomeLabel", genomeLabel);
    jsonWriteString(jw, "position", position);
    char *suggestTrack = NULL;
    if (! trackHubDatabase(db))
        suggestTrack = assemblyGeneSuggestTrack(db);
    jsonWriteString(jw, "suggestTrack", suggestTrack);
    char *description = maybeGetDescriptionText(db);
    jsonWriteString(jw, "description", description);
    listAssemblyHubs(jw);
    }
}

static void setTaxId(struct cartJson *cj, struct hash *paramHash)
/* Set db and genome according to taxId (and/or db) and return the info we'll need
 * to fill in the findPosition section. */
{
char *taxIdStr = cartJsonRequiredParam(paramHash, "taxId", cj->jw, "setTaxId");
char *db = cartJsonOptionalParam(paramHash, "db");
int taxId = atoi(taxIdStr);
if (isEmpty(db))
    db = hDbForTaxon(taxId);
if (isEmpty(db))
    jsonWriteStringf(cj->jw, "error", "No db for taxId '%s'", taxIdStr);
else
    {
    writeFindPositionInfo(cj->jw, db, taxId, NULL, hDefaultPos(db));
    cartSetString(cart, "db", db);
    cartSetString(cart, "position", hDefaultPos(db));
    }
}

static void setHubDb(struct cartJson *cj, struct hash *paramHash)
/* Set db and genome according to hubUrl (and/or db and hub) and return the info we'll need
 * to fill in the findPosition section. */
{
char *hubUrl = cartJsonRequiredParam(paramHash, "hubUrl", cj->jw, "setHubDb");
char *taxIdStr = cartJsonOptionalParam(paramHash, "taxId");
int taxId = taxIdStr ? atoi(taxIdStr) : -1;
// cart's db was already set by magic handling of hub CGI variables sent along
// with this command.
char *db = cartString(cart, "db");
if (isEmpty(db))
    jsonWriteStringf(cj->jw, "error", "No db for hubUrl '%s'", hubUrl);
else
    writeFindPositionInfo(cj->jw, db, taxId, hubUrl, hDefaultPos(db));
}

static void setDb(struct cartJson *cj, struct hash *paramHash)
/* Set taxId and genome according to db and return the info we'll need to fill in
 * the findPosition section. */
{
char *db = cartJsonRequiredParam(paramHash, "db", cj->jw, "setDb");
char *hubUrl = cartJsonOptionalParam(paramHash, "hubUrl");
int taxId = hTaxId(db);
writeFindPositionInfo(cj->jw, db, taxId, hubUrl, hDefaultPos(db));
cartSetString(cart, "db", db);
cartSetString(cart, "position", hDefaultPos(db));
}

static void getUiState(struct cartJson *cj, struct hash *paramHash)
/* Write out JSON for hgGateway.js's uiState object using current cart settings. */
{
char *db = cartUsualString(cj->cart, "db", hDefaultDb());
char *position = cartUsualString(cart, "position", hDefaultPos(db));
char *hubUrl = NULL;
if (trackHubDatabase(db))
    {
    struct trackHub *hub = hubConnectGetHubForDb(db);
    hubUrl = hub->url;
    }
writeFindPositionInfo(cj->jw, db, hTaxId(db), hubUrl, position);
// If cart already has a pix setting, pass that along; otherwise the JS will
// set pix according to web browser window width.
int pix = cartUsualInt(cj->cart, "pix", 0);
if (pix)
    jsonWriteNumber(cj->jw, "pix", pix);
}

static void doCartJson()
/* Perform UI commands to update the cart and/or retrieve cart vars & metadata. */
{
struct cartJson *cj = cartJsonNew(cart);
cartJsonRegisterHandler(cj, "setTaxId", setTaxId);
cartJsonRegisterHandler(cj, "setDb", setDb);
cartJsonRegisterHandler(cj, "setHubDb", setHubDb);
cartJsonRegisterHandler(cj, "getUiState", getUiState);
cartJsonExecute(cj);
}

static void printActiveGenomes(struct dyString *dy)
/* Print out JSON for an object mapping each genome that has at least one db with active=1
 * to its taxId.  */
{
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
struct sqlConnection *conn = hConnectCentral();
// Join with defaultDb because in rare cases, different taxIds (species vs. subspecies)
// may be used for different assemblies of the same species.  Using defaultDb means that
// we send a taxId consistent with the taxId of the assembly that we'll change to when
// the species is selected from the tree.
char *query = NOSQLINJ "select dbDb.genome, taxId, dbDb.name from dbDb, defaultDb "
    "where defaultDb.name = dbDb.name and active = 1 "
    "and taxId > 1;"; // filter out experimental hgwdev-only stuff with invalid taxIds
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *genome = row[0], *db = row[2];
    int taxId = atoi(row[1]);
    if (hDbExists(db))
        jsonWriteNumber(jw, genome, taxId);
    }
hDisconnectCentral(&conn);
jsonWriteObjectEnd(jw);
dyStringAppend(dy, jw->dy->string);
jsonWriteFree(&jw);
}

static void doMainPage()
/* Send HTML with javascript to bootstrap the user interface. */
{
// Start web page with new banner
char *db = NULL, *genome = NULL, *clade = NULL;
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);
// If CGI has &lastDbPos=..., handle that here and save position to cart so it's in place for
// future cartJson calls.
char *position = cartGetPosition(cart, db, NULL);
cartSetString(cart, "position", position);
webStartJWest(cart, db, "Genome Browser Gateway");

if (cgiIsOnWeb())
    checkForGeoMirrorRedirect(cart);

#define WARNING_BOX_START "<div id=\"previewWarningRow\" class=\"jwRow\">" \
         "<div id=\"previewWarningBox\" class=\"jwWarningBox\">"

#define UNDER_DEV "Data and tools on this site are under development, have not been reviewed " \
         "for quality, and are subject to change at any time. "

#define MAIN_SITE "The high-quality, reviewed public site of the UCSC Genome Browser is " \
         "available for use at <a href=\"http://genome.ucsc.edu/\">http://genome.ucsc.edu/</a>."

#define WARNING_BOX_END "</div></div>"

if (hIsPreviewHost())
    {
    puts(WARNING_BOX_START
         "WARNING: This is the UCSC Genome Browser preview site. "
         "This website is a weekly mirror of our internal development server for public access. "
         UNDER_DEV
         "We provide this site for early access, with the warning that it is less available "
         "and stable than our public site. "
         MAIN_SITE
         WARNING_BOX_END);
    }

if (hIsPrivateHost() && !hHostHasPrefix("hgwdev-demo6"))
    {
    puts(WARNING_BOX_START
         "WARNING: This is the UCSC Genome Browser development site. "
         "This website is used for testing purposes only and is not intended for general public "
         "use. "
         UNDER_DEV
         MAIN_SITE
         WARNING_BOX_END);
    }

// The visible page elements are all in ./hgGateway.html, which is transformed into a quoted .h
// file containing a string constant that we #include and print here (see makefile).
puts(
#include "hgGateway.html.h"
);

// Set global JS variables hgsid, activeGenomes, and survey* at page load time
// We can't just use "var hgsid = " or the other scripts won't see it -- it has to be
// "window.hgsid = ".
struct dyString *dy = dyStringNew(1024);

dyStringPrintf(dy, "window.%s = '%s';\n", cartSessionVarName(), cartSessionId(cart));
dyStringPrintf(dy, "window.activeGenomes =\n");
printActiveGenomes(dy);
dyStringPrintf(dy, "\n;\n");
char *surveyLink = cfgOption("survey");
if (isNotEmpty(surveyLink) && !sameWord(surveyLink, "off"))
    {
    dyStringPrintf(dy, "window.surveyLink=\"%s\";\n", jsonStringEscape(surveyLink));
    char *surveyLabel = cfgOptionDefault("surveyLabel", "Please take our survey");
    dyStringPrintf(dy, "window.surveyLabel=\"%s\";\n", jsonStringEscape(surveyLabel));
    char *surveyLabelImage = cfgOption("surveyLabelImage");
    if (isNotEmpty(surveyLabelImage))
        dyStringPrintf(dy, "window.surveyLabelImage=\"%s\";\n", jsonStringEscape(surveyLabelImage));
    else
        dyStringPrintf(dy, "window.surveyLabelImage=null;\n");
    }
else
    {
    dyStringPrintf(dy, "window.surveyLink=null;\n");
    dyStringPrintf(dy, "window.surveyLabel=null;\n");
    dyStringPrintf(dy, "window.surveyLabelImage=null;\n");
    }
dyStringPrintf(dy, "hgGateway.init();\n");

jsInline(dy->string);
dyStringFree(&dy);

jsIncludeFile("es5-shim.4.0.3.min.js", NULL);
jsIncludeFile("es5-sham.4.0.3.min.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);

webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("jquery.watermarkinput.js", NULL);
jsIncludeFile("utils.js",NULL);

// Phylogenetic tree .js file, produced by dbDbTaxonomy.pl:
char *defaultDbDbTree = webTimeStampedLinkToResource("dbDbTaxonomy.js", FALSE);
char *dbDbTree = cfgOptionDefault("hgGateway.dbDbTaxonomy", defaultDbDbTree);
if (isNotEmpty(dbDbTree))
    printf("<script src=\"%s\"></script>\n", dbDbTree);

// Main JS for hgGateway:
jsIncludeFile("hgGateway.js", NULL);

webIncludeFile("inc/jWestFooter.html");

cartFlushHubWarnings();

webEndJWest();
}

void doMiddle(struct cart *theCart)
/* Depending on invocation, either perform a query and print out results
 * or display the main page. */
{
cart = theCart;
if (cgiOptionalString(CARTJSON_COMMAND))
    doCartJson();
else
    doMainPage();
}

// We find matches from various fields of dbDb, and prefer them in this order:
enum dbDbMatchType { ddmtDescription=0, ddmtGenome=1, ddmtDb=2, ddmtSciName=3 };

struct dbDbMatch
    // Info about a match of a search term to some field of dbDb, including info that
    // helps prioritize matches.
    {
    struct dbDbMatch *next;
    struct dbDb *dbDb;        // the row of dbDb in which a match was found
    enum dbDbMatchType type;  // which field the match was found in
    int offset;               // offset of the search term in the dbDb value
    boolean isWord;           // TRUE if the the search term matches the word at offset
    boolean isComplete;       // TRUE if the search term matches the entire target string
    };

static struct dbDbMatch *dbDbMatchNew(struct dbDb *dbDb, enum dbDbMatchType type, int offset,
                                      boolean isWord, boolean isComplete)
/* Allocate and return a new dbDbMatch.  Do not free dbDb until done with this. */
{
struct dbDbMatch *ddm;
AllocVar(ddm);
ddm->dbDb = dbDb;
ddm->type = type;
ddm->offset = offset;
ddm->isWord = isWord;
ddm->isComplete = isComplete;
return ddm;
}

static int dbDbMatchCmp(const void *va, const void *vb)
/* Compare two matches by type, orderKey, offset and genome. */
{
const struct dbDbMatch *a = *((struct dbDbMatch **)va);
const struct dbDbMatch *b = *((struct dbDbMatch **)vb);
int diff = b->isComplete - a->isComplete;
if (diff == 0)
    diff = b->isWord - a->isWord;
if (diff == 0 && a->isWord && b->isWord)
    diff = a->offset - b->offset;
// Use int values of type:
if (diff == 0)
    diff = (int)(a->type) - (int)(b->type);
if (diff == 0)
    diff = a->dbDb->orderKey - b->dbDb->orderKey;
if (diff == 0)
    diff = a->offset - b->offset;
if (diff == 0)
    diff = strcmp(a->dbDb->genome, b->dbDb->genome);
return diff;
}

INLINE void safeAddN(char **pDest, int *pSize, char *src, int len)
/* Copy len bytes of src into dest.  Subtract len from *pSize and add len to *pDest,
 * for building up a string bit by bit. */
{
safencpy(*pDest, *pSize, src, len);
*pSize -= len;
*pDest += len;
}

INLINE void safeAdd(char **pDest, int *pSize, char *src)
/* Copy src into dest.  Subtract len from *pSize and add len to *pDest,
 * for building up a string bit by bit. */
{
safeAddN(pDest, pSize, src, strlen(src));
}

static char *boldTerm(char *target, char *term, int offset, enum dbDbMatchType type)
/* Return a string with <b>term</b> swapped in for term at offset.
 * If offset is negative and type is ddmtSciName, treat term as an abbreviated species
 * name (term = "G. species" vs. target = "Genus species"): bold the first letter of the
 * genus and the matching portion of the species. */
{
int termLen = strlen(term);
int targetLen = strlen(target);
if (offset + termLen > targetLen)
    errAbort("boldTerm: invalid offset (%d) for term '%s' (length %d) in target '%s' (length %d)",
             offset, term, termLen, target, targetLen);
else if (offset < 0 && type != ddmtSciName)
    errAbort("boldTerm: negative offset (%d) given for type %d", offset, type);
// Allocate enough to have two bolded chunks:
int resultSize = targetLen + 2*strlen("<b></b>") + 1;
char result[resultSize];
char *p = result;
int size = sizeof(result);
if (offset >= 0)
    {
    // The part of target before the term:
    safeAddN(&p, &size, target, offset);
    // The bolded term:
    safeAdd(&p, &size, "<b>");
    safeAddN(&p, &size, target+offset, termLen);
    safeAdd(&p, &size, "</b>");
    // The rest of the target after the term:
    safeAdd(&p, &size, target+offset+termLen);
    // Accounting tweak -- we allocate enough for two bolded chunks, but use only one here:
    size -= strlen("<b></b>");
    }
else
    {
    // Term is abbreviated scientific name -- bold the first letter of the genus:
    safeAdd(&p, &size, "<b>");
    safeAddN(&p, &size, target, 1);
    safeAdd(&p, &size, "</b>");
    // add the rest of the genus:
    char *targetSpecies = skipLeadingSpaces(skipToSpaces(target));
    int targetOffset = targetSpecies - target;
    safeAddN(&p, &size, target+1, targetOffset-1);
    // bold the matching portion of the species:
    char *termSpecies = skipLeadingSpaces(skipToSpaces(term));
    termLen = strlen(termSpecies);
    safeAdd(&p, &size, "<b>");
    safeAddN(&p, &size, targetSpecies, termLen);
    safeAdd(&p, &size, "</b>");
    // add the rest of the species:
    safeAdd(&p, &size, targetSpecies+termLen); 
    }
if (*p != '\0' || size != 1)
    errAbort("boldTerm: bad arithmetic (size is %d, *p is '%c')", size, *p);
return cloneStringZ(result, resultSize);
}

static void writeDbDbMatch(struct jsonWrite *jw, struct dbDbMatch *match, char *term,
                           char *category)
/* Write out the JSON encoding of a match in dbDb. */
{
struct dbDb *dbDb = match->dbDb;
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "genome", dbDb->genome);
// label includes <b> tag to highlight the match for term.
char label[PATH_LEN*4];
// value is placed in the input box when user selects the item.
char value[PATH_LEN*4];
if (match->type == ddmtSciName)
    {
    safef(value, sizeof(value), "%s (%s)", dbDb->scientificName, dbDb->genome);
    char *bolded = boldTerm(dbDb->scientificName, term, match->offset, match->type);
    safef(label, sizeof(label), "%s (%s)", bolded, dbDb->genome);
    freeMem(bolded);
    }
else if (match->type == ddmtGenome)
    {
    safecpy(value, sizeof(value), dbDb->genome);
    char *bolded = boldTerm(dbDb->genome, term, match->offset, match->type);
    safecpy(label, sizeof(label), bolded);
    freeMem(bolded);
    }
else if (match->type == ddmtDb)
    {
    safecpy(value, sizeof(value), dbDb->name);
    char *bolded = boldTerm(dbDb->name, term, match->offset, match->type);
    safef(label, sizeof(label), "%s (%s %s)",
          bolded, dbDb->genome, dbDb->description);
    freeMem(bolded);
    jsonWriteString(jw, "db", dbDb->name);
    }
else if (match->type == ddmtDescription)
    {
    safef(value, sizeof(value), "%s (%s %s)",
          dbDb->name, dbDb->genome, dbDb->description);
    char *bolded = boldTerm(dbDb->description, term, match->offset, match->type);
    safef(label, sizeof(label), "%s (%s %s)",
          dbDb->name, dbDb->genome, bolded);
    freeMem(bolded);
    jsonWriteString(jw, "db", dbDb->name);
    }
else
    errAbort("writeDbDbMatch: unrecognized dbDbMatchType value %d (db %s, term %s)",
             match->type, dbDb->name, term);
jsonWriteString(jw, "label", label);
jsonWriteString(jw, "value", value);
jsonWriteNumber(jw, "taxId", dbDb->taxId);
if (isNotEmpty(category))
    jsonWriteString(jw, "category", category);
jsonWriteObjectEnd(jw);
}

int wordMatchOffset(char *term, char *target)
/* If some word of target starts with term (case insensitive), return the offset of
 * that word in target; otherwise return -1. */
{
if (startsWith(term, target))
    return 0;
int targetLen = strlen(target);
char targetClone[targetLen+1];
safecpy(targetClone, sizeof(targetClone), target);
char *p = targetClone;
while (nextWord(&p) && p != NULL)
    {
    // skip punctuation like parentheses
    while (*p != '\0' && ! isalnum(*p))
        p++;
    if (startsWith(term, p))
        return p - targetClone;
    }
return -1;
}

static void addIfFirstMatch(struct dbDb *dbDb, enum dbDbMatchType type, int offset, char *target,
                            char *term, struct hash *matchHash, struct dbDbMatch **pMatchList)
/* If target doesn't already have a match in matchHash, compute matchLength and isWord,
 * and then add the new match to pMatchList and add target to matchHash. */
{
if (dbDb->active && ! hashLookup(matchHash, target))
    {
    char *termInTarget = (offset >= 0) ? target+offset : target;
    int matchLength = countSame(term, termInTarget);
    // is the match complete up to a word boundary in termInTarget?
    boolean isWord = (matchLength == strlen(term) &&
                       (termInTarget[matchLength] == '\0' || isspace(termInTarget[matchLength])));
    boolean isComplete = sameString(term, target);
    struct dbDbMatch *match = dbDbMatchNew(dbDb, type, offset, isWord, isComplete);
    slAddHead(pMatchList, match);
    hashStore(matchHash, target);
    }
}

static void checkTerm(char *term, char *target, enum dbDbMatchType type, struct dbDb *dbDb,
                      struct hash *matchHash, struct dbDbMatch **pMatchList)
/* If target starts with term (case-insensitive), and target is not already in matchHash,
 * add target to matchHash and add a new match to pMatchList. */
{
// Make uppercase version of target for case-insensitive matching.
int targetLen = strlen(target);
char targetUpcase[targetLen + 1];
safencpy(targetUpcase, sizeof(targetUpcase), target, targetLen);
touppers(targetUpcase);
int offset = wordMatchOffset(term, targetUpcase);
if (offset >= 0)
    {
    addIfFirstMatch(dbDb, type, offset, targetUpcase, term, matchHash, pMatchList);
    }
else if (offset < 0 && type == ddmtSciName && term[0] == targetUpcase[0])
    {
    // For scientific names ("Genus species"), see if the user entered the term as 'G. species'
    // e.g. term 'P. trog' for target 'Pan troglodytes'
    regmatch_t substrArr[3];
    if (regexMatchSubstrNoCase(term, "^[a-z](\\.| ) *([a-z]+)", substrArr, ArraySize(substrArr)))
        {
        char *termSpecies = term + substrArr[2].rm_so;
        char *targetSpecies = skipLeadingSpaces(skipToSpaces(targetUpcase));
        if (targetSpecies && startsWithNoCase(termSpecies, targetSpecies))
            {
            // Keep the negative offset since we can't just bold one chunk of target...
            addIfFirstMatch(dbDb, type, offset, targetUpcase, term, matchHash, pMatchList);
            }
        }
    }
}

static struct dbDbMatch *searchDbDb(struct dbDb *dbDbList, char *term)
/* Search various fields of dbDb for matches to term and sort by relevance. */
{
struct dbDbMatch *matchList = NULL;
struct hash *matchHash = hashNew(0);
struct dbDb *dbDb;
for (dbDb = dbDbList;  dbDb != NULL; dbDb = dbDb->next)
    {
    checkTerm(term, dbDb->name, ddmtDb, dbDb, matchHash, &matchList);
    // Skip experimental stuff on hgwdev with bogus taxId unless the db name matches term.
    if (dbDb->taxId >= 2)
        {
        checkTerm(term, dbDb->genome, ddmtGenome, dbDb, matchHash, &matchList);
        checkTerm(term, dbDb->scientificName, ddmtSciName, dbDb, matchHash, &matchList);
        }
    // dbDb.description is a little too much for autocomplete ("br" would match dozens
    // of Broad assemblies), but we do need to recognize "GRC".
    if (startsWith("GR", term))
        checkTerm(term, dbDb->description, ddmtDescription, dbDb, matchHash, &matchList);
    }
slSort(&matchList, dbDbMatchCmp);
return matchList;
}

// Assembly hub match:

struct aHubMatch
    // description of an assembly hub db
    {
    struct aHubMatch *next;
    char *shortLabel;          // hub shortLabel
    char *hubUrl;              // hub url
    char *aDb;                 // assembly db hosted by hub
    };

static struct aHubMatch *aHubMatchNew(char *shortLabel, char *hubUrl, char *aDb)
/* Allocate and return a description of an assembly hub db. */
{
struct aHubMatch *match;
AllocVar(match);
match->shortLabel = cloneString(shortLabel);
match->hubUrl = cloneString(hubUrl);
match->aDb = cloneString(aDb);
return match;
}

static struct hash *unpackHubDbUrlList(struct slName *hubDbUrlList)
/* hubDbUrlList contains strings like "db,hubUrl" -- split on comma and return a hash of
 * hubUrl to one or more dbs. */
{
struct hash *hubToDb = hashNew(0);
struct slName *hubDbUrl;
for (hubDbUrl = hubDbUrlList;  hubDbUrl != NULL;  hubDbUrl = hubDbUrl->next)
    {
    char *comma = strchr(hubDbUrl->name, ',');
    if (comma)
        {
        char *db = hubDbUrl->name;
        *comma = '\0';
        char *hubUrl = comma+1;
        struct hashEl *hel = hashLookup(hubToDb, hubUrl);
        struct slName *dbList = hel ? hel->val : NULL;
        slAddHead(&dbList, slNameNew(db));
        if (hel == NULL)
            hashAdd(hubToDb, hubUrl, dbList);
        else
            hel->val = dbList;
        }
    }
return hubToDb;
}

static struct aHubMatch *filterHubSearchTextMatches(struct dbDb *dbDbList,
                                                    struct slName *hubDbUrlList)
/* Collect the assembly hub matches (not track hub matches) from a search in hubSearchText. */
{
if (hubDbUrlList == NULL)
    return NULL;
struct aHubMatch *aHubMatchList = NULL;
// Make a hash of local dbs so we can tell which hub dbs must be assembly hubs
// not track hubs.
struct hash *localDbs = hashNew(0);
struct dbDb *dbDb;
for (dbDb = dbDbList;  dbDb != NULL;  dbDb = dbDb->next)
    hashStore(localDbs, dbDb->name);
struct hash *hubToDb = unpackHubDbUrlList(hubDbUrlList);
// Build up a query to find shortLabel and dbList for each hubUrl.
struct dyString *query = sqlDyStringCreate("select shortLabel,hubUrl,dbList from %s "
                                           "where hubUrl in (",
                                           hubPublicTableName());
struct hashEl *hel;
struct hashCookie cookie = hashFirst(hubToDb);
boolean isFirst = TRUE;
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (isFirst)
        isFirst = FALSE;
    else
        dyStringAppend(query, ", ");
    dyStringPrintf(query, "'%s'", hel->name);
    }
dyStringAppendC(query, ')');
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *shortLabel = row[0];
    char *hubUrl = row[1];
    struct slName *dbName, *matchDbList = hashFindVal(hubToDb, hubUrl);
    struct slName *hubDbList = slNameListFromComma(row[2]);
    if (slCount(matchDbList) == 1 && isEmpty(matchDbList->name))
        {
        // top-level hub match, no specific db match; add all of hub's assembly dbs
        for (dbName = hubDbList;  dbName != NULL;  dbName = dbName->next)
            if (! hashLookup(localDbs, dbName->name))
                slAddHead(&aHubMatchList, aHubMatchNew(shortLabel, hubUrl, dbName->name));
        }
    else
        {
        // Add matching assembly dbs that are found in hubDbList
        for (dbName = matchDbList;  dbName != NULL;  dbName = dbName->next)
            if (! hashLookup(localDbs, dbName->name) && slNameInList(hubDbList, dbName->name))
                slAddHead(&aHubMatchList, aHubMatchNew(shortLabel, hubUrl, dbName->name));
        }
    }
slReverse(&aHubMatchList);
hDisconnectCentral(&conn);
return aHubMatchList;
}

static void writeAssemblyHubMatches(struct jsonWrite *jw, struct aHubMatch *aHubMatchList)
/* Write out JSON for each assembly in each assembly hub that matched the search term. */
{
struct aHubMatch *aHubMatch;
for (aHubMatch = aHubMatchList;  aHubMatch != NULL;  aHubMatch = aHubMatch->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "genome", aHubMatch->shortLabel);
    jsonWriteString(jw, "db", aHubMatch->aDb);
    jsonWriteString(jw, "hubUrl", aHubMatch->hubUrl);
    jsonWriteString(jw, "hubName", hubNameFromUrl(aHubMatch->hubUrl));
    // Add a category label for customized autocomplete-with-categories.
    char category[PATH_LEN*4];
    safef(category, sizeof(category), "Assembly Hub: %s", aHubMatch->shortLabel);
    jsonWriteString(jw, "category", category);
    jsonWriteString(jw, "value", aHubMatch->aDb);
    // Use just the db as label, since shortLabel is included in the category label.
    jsonWriteString(jw, "label", aHubMatch->aDb);
    jsonWriteObjectEnd(jw);
    }
}

static struct aHubMatch *searchPublicHubs(struct dbDb *dbDbList, char *term)
/* Search for term in public hubs -- return a list of matches to assembly hubs
 * (i.e. hubs that host an assembly with 2bit etc as opposed to only providing tracks.) */
{
struct aHubMatch *aHubMatchList = NULL;
char *hubSearchTableName = cfgOptionDefault("hubSearchTextTable", "hubSearchText");
struct sqlConnection *conn = hConnectCentral();
if (sqlTableExists(conn, hubSearchTableName))
    {
    char query[1024];
    sqlSafef(query, sizeof(query), "select distinct(concat(db, concat(',', hubUrl))) from %s "
             "where track = '' and "
             "(db like '%s%%' or label like '%%%s%%' or text like '%s%%')",
             hubSearchTableName, term, term, term);
    struct slName *hubDbUrlList = sqlQuickList(conn, query);
    aHubMatchList = filterHubSearchTextMatches(dbDbList, hubDbUrlList);
    if (aHubMatchList == NULL)
        {
        // Try a looser query
        sqlSafef(query, sizeof(query), "select distinct(concat(db, concat(',', hubUrl))) from %s "
                 "where track = '' and text like '%% %s%%'",
                 hubSearchTableName, term);
        hubDbUrlList = sqlQuickList(conn, query);
        aHubMatchList = filterHubSearchTextMatches(dbDbList, hubDbUrlList);
        }
    }
hDisconnectCentral(&conn);
return aHubMatchList;
}

static char *getSearchTermUpperCase()
/* If we don't have the SEARCH_TERM cgi param, exit with an HTTP Bad Request response.
 * If we do, convert it to upper case for case-insensitive matching and return it. */
{
pushAbortHandler(htmlVaBadRequestAbort);
char *term = cgiOptionalString(SEARCH_TERM);
touppers(term);
if (isEmpty(term))
    errAbort("Missing required CGI parameter %s", SEARCH_TERM);
popAbortHandler();
return term;
}

static void lookupTerm()
/* Look for matches to term in hgcentral and print as JSON for autocomplete if found. */
{
char *term = getSearchTermUpperCase();

// Write JSON response with list of matches
puts("Content-Type:text/javascript\n");

// Before accessing hubs, intialize udc cache location from hg.conf:
setUdcCacheDir();
struct dbDb *dbDbList = hDbDbList();
struct dbDbMatch *matchList = searchDbDb(dbDbList, term);
struct aHubMatch *aHubMatchList = searchPublicHubs(dbDbList, term);
struct jsonWrite *jw = jsonWriteNew();
jsonWriteListStart(jw, NULL);
// Write out JSON for dbDb matches, if any; add category if we found assembly hub matches too.
char *category = aHubMatchList ? "UCSC databases" : NULL;
struct dbDbMatch *match;
for (match = matchList;  match != NULL;  match = match->next)
    writeDbDbMatch(jw, match, term, category);
// Write out assembly hub matches, if any.
writeAssemblyHubMatches(jw, aHubMatchList);
jsonWriteListEnd(jw);
puts(jw->dy->string);
jsonWriteFree(&jw);
}

int main(int argc, char *argv[])
/* Process CGI / command line. */
{
/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {SEARCH_TERM, CARTJSON_COMMAND, NULL,};
cgiSpoof(&argc, argv);
if (cgiOptionalString(SEARCH_TERM))
    // Skip the cart for speedy searches
    lookupTerm();
else
    {
    long enteredMainTime = clock1000();
    oldVars = hashNew(10);
    cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
    cgiExitTime("hgGateway", enteredMainTime);
    }
return 0;
}
