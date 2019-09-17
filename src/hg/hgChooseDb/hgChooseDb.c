/* hgChooseDb - bootstrapper / back end for demo app: auto-complete db/organism search
 * This CGI has three modes of operation:
 *  - HTML output for minimal main page with a <div> container to be filled in by javascript
 *    (default, in the absence of special CGI params)
 *  - cart-based JSON responses to ajax requests from javascript (using hg/lib/cartJson.c)
 *    (if CGI param CARTJSON_COMMAND exists)
 *  - no cart; fast JSON responses to species-search autocomplete requests
 *    (if CGI param SEARCH_TERM exists)
 * The UI view top level is in ../js/react/hgChooseDb/hgChooseDb.jsx
 * The UI model top level is in ../js/model/hgChooseDb/hgChooseDbModel.js
 */
#include "common.h"
#include "cart.h"
#include "cartJson.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "hdb.h"
#include "hui.h"
#include "jsHelper.h"
#include "jsonParse.h"
#include "obscure.h"  // for readInGulp
#include "trackHub.h"
#include "web.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */
struct hash *oldVars = NULL;          /* Old contents of cart before it was updated by CGI */

#define SEARCH_TERM "hgcd_term"

static char *getPhotoForGenome(char *genome, char *db)
/* If the expected file for this genome's description page photo exists, return the filename. */
{
if (db == NULL)
    db = hDefaultDbForGenome(genome);
char baseName[PATH_LEN];
if (sameString("Human", genome))
    safecpy(baseName, sizeof(baseName), "human.jpg");
else
    {
    safef(baseName, sizeof(baseName), "%s.jpg", hScientificName(db));
    subChar(baseName, ' ', '_');
    }
char fileName[PATH_LEN];
safef(fileName, sizeof(fileName), "%s/images/%s", hDocumentRoot(), baseName);
if (fileExists(fileName))
    {
    // Reformat for URL:
    safef(fileName, sizeof(fileName), "../images/%s", baseName);
    return cloneString(fileName);
    }
else
    return NULL;
}

static void writeGenomeInfo(struct jsonWrite *jw, char *genome, char *db)
// Write menu options, selected db etc. for genome and db.  If db is NULL use default for genome.
{
if (db == NULL)
    db = hDefaultDbForGenome(genome);
struct slPair *dbOptions = hGetDbOptionsForGenome(genome);
jsonWriteValueLabelList(jw, "dbOptions", dbOptions);
jsonWriteString(jw, "db", db);
jsonWriteString(jw, "genome", genome);
jsonWriteString(jw, "img", getPhotoForGenome(genome, db));
}

static void writeDbMenuData(struct jsonWrite *jw, char *genome, char *db)
// Set dbMenuData to use genome's info.
{
jsonWriteObjectStart(jw, "dbMenuData");
writeGenomeInfo(jw, genome, db);
jsonWriteObjectEnd(jw);
}

static void getDbMenu(struct cartJson *cj, struct hash *paramHash)
/* Write a dbMenu for the selected search term match.  */
{
char *genome = cartJsonRequiredParam(paramHash, "genome", cj->jw, "getDbMenu");
char *db = cartJsonOptionalParam(paramHash, "db");
writeDbMenuData(cj->jw, genome, db);
}

static char *popularSpecies[] =
    { "Human", "Mouse", "Rat", "D. melanogaster", "C. elegans", NULL };

static void writeOnePopularSpecies(struct jsonWrite *jw, char *genome, char *db)
// Write a nameless object with info about this species.
{
jsonWriteObjectStart(jw, NULL);
writeGenomeInfo(jw, genome, db);
jsonWriteObjectEnd(jw);
}

static void getPopularSpecies(struct cartJson *cj, struct hash *paramHash)
/* Return a list of popular species' names, images and db menu info. */
{
jsonWriteListStart(cj->jw, "popularSpecies");
char *currentDb = cartOptionalString(cart, "db");
char *currentGenome = currentDb ? hGenome(currentDb) : NULL;
boolean gotCurrent = FALSE;
int i;
for (i = 0;  popularSpecies[i] != NULL;  i++)
    {
    char *db = NULL;
    if (sameOk(popularSpecies[i], currentGenome))
        {
        gotCurrent = TRUE;
        db = currentDb;
        }
    writeOnePopularSpecies(cj->jw, popularSpecies[i], db);
    }
if (! gotCurrent)
    writeOnePopularSpecies(cj->jw, currentGenome, currentDb);
jsonWriteListEnd(cj->jw);
if (currentGenome != NULL)
    writeDbMenuData(cj->jw, currentGenome, currentDb);
}

static void getDescriptionHtml(struct cartJson *cj, struct hash *paramHash)
/* Return assembly description html for the given db. */
{
char *db = cartJsonRequiredParam(paramHash, "db", cj->jw, "getDescriptionHtml");
char *htmlPath = hHtmlPath(db);
char *htmlString = NULL;
if (htmlPath != NULL)
    {
    if (fileExists(htmlPath))
	readInGulp(htmlPath, &htmlString, NULL);
    else if (   startsWith("http://" , htmlPath) ||
		startsWith("https://", htmlPath) ||
		startsWith("ftp://"  , htmlPath))
	{
	struct lineFile *lf = udcWrapShortLineFile(htmlPath, NULL, 256*1024);
	htmlString =  lineFileReadAll(lf);
	lineFileClose(&lf);
	}
    }
if (isNotEmpty(htmlString))
    {
    jsonWriteObjectStart(cj->jw, "assemblyDescription");
    jsonWriteString(cj->jw, "db", db);
    jsonWriteString(cj->jw, "description", htmlString);
    jsonWriteObjectEnd(cj->jw);
    }
}

static void doCartJson()
/* Perform UI commands to update the cart and/or retrieve cart vars & metadata. */
{
struct cartJson *cj = cartJsonNew(cart);
cartJsonRegisterHandler(cj, "getPopularSpecies", getPopularSpecies);
cartJsonRegisterHandler(cj, "getDbMenu", getDbMenu);
cartJsonRegisterHandler(cj, "getDescriptionHtml", getDescriptionHtml);
cartJsonExecute(cj);
}

static void doMainPage()
/* Send HTML with javascript to bootstrap the user interface. */
{
//#*** A lot of this is copied from hgIntegrator... libify!

char *db = NULL, *genome = NULL, *clade = NULL;
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);
webStartWrapperDetailedNoArgs(cart, trackHubSkipHubName(db),
                              "", "UCSC Genome Browser Databases",
                              TRUE, FALSE, TRUE, TRUE);

// Ideally these would go in the <HEAD>
puts("<link rel=\"stylesheet\" href=\"//code.jquery.com/ui/1.10.3/themes/smoothness/jquery-ui.css\">");
puts("<link rel=\"stylesheet\" href=\"//netdna.bootstrapcdn.com/font-awesome/4.0.3/css/font-awesome.css\">");

puts("<div id=\"appContainer\">Loading...</div>");

// Set a global JS variable hgsid.
// Plain old "var ..." doesn't work (other scripts can't see it), it has to belong to window.
printf("<script>window.%s='%s';</script>\n", cartSessionVarName(), cartSessionId(cart));

jsIncludeReactLibs();
jsIncludeFile("reactHgChooseDb.js", NULL);
jsIncludeFile("hgChooseDbModel.js", NULL);

// Invisible form for jumping to hgTracks
printf("\n<form action=\"%s\" method=%s id='mainForm'>\n",
       hgTracksName(), cartUsualString(cart, "formMethod", "GET"));
cartSaveSession(cart);
cgiMakeHiddenVar("db", db);
puts("</form>");
webEnd();
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

static void fail(char *msg)
//#*** Copied from hgSuggest... libify to cheapCgi?
{
puts("Status: 400\n\n");
puts(msg);
exit(-1);
}

INLINE void addIfStartsWithNoCase(char *term, char *target, struct dbDb *dbDb,
                                  struct hash *matchHash)
{
if (startsWithNoCase(term, target))
    {
    if (! hashLookup(matchHash, target))
        hashAdd(matchHash, target, dbDb);
    }
}

static void lookupTerm()
/* Look for matches to term in hgCentral and print as JSON if found. */
{
char *term = cgiOptionalString(SEARCH_TERM);
if (isEmpty(term))
    fail("Missing search term parameter");

// Write JSON response with list of matches
puts("Content-Type:text/javascript\n");
struct jsonWrite *jw = jsonWriteNew();
jsonWriteListStart(jw, NULL);

// Search dbDb for matches
struct hash *matchHash = hashNew(0);
struct dbDb *dbDbList = hDbDbList(), *dbDb;
for (dbDb = dbDbList;  dbDb != NULL; dbDb = dbDb->next)
    {
    if (startsWithNoCase(term, dbDb->name))
        {
        jsonWriteObjectStart(jw, NULL);
        char description[PATH_LEN];
        safef(description, sizeof(description), "%s (%s %s)",
              dbDb->name, dbDb->genome, dbDb->description);
        jsonWriteString(jw, "value", description);
        jsonWriteString(jw, "genome", dbDb->genome);
        jsonWriteString(jw, "db", dbDb->name);
        jsonWriteObjectEnd(jw);
        }
    addIfStartsWithNoCase(term, dbDb->genome, dbDb, matchHash);
    addIfStartsWithNoCase(term, dbDb->scientificName, dbDb, matchHash);
    addIfStartsWithNoCase(term, dbDb->sourceName, dbDb, matchHash);
    }

struct hashEl *hel;
struct hashCookie cookie = hashFirst(matchHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    dbDb = hel->val;
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "value", hel->name);
    jsonWriteString(jw, "genome", dbDb->genome);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
puts(jw->dy->string);
}

int main(int argc, char *argv[])
/* Process CGI / command line. */
{
/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {SEARCH_TERM, CARTJSON_COMMAND, NULL,};
cgiSpoof(&argc, argv);
setUdcCacheDir();
oldVars = hashNew(10);
if (cgiOptionalString(SEARCH_TERM))
    // Skip the cart for speedy searches
    lookupTerm();
else
    cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
