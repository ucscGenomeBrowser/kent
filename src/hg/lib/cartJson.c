/* cartJson - parse and execute JSON commands to update cart and/or return cart data as JSON. */
#include "common.h"
#include "cartJson.h"
#include "cartTrackDb.h"
#include "cheapcgi.h"
#include "grp.h"
#include "hdb.h"
#include "hgFind.h"
#include "htmshell.h"
#include "hubConnect.h"
#include "hui.h"
#include "jsonParse.h"
#include "obscure.h"
#include "regexHelper.h"
#include "suggest.h"
#include "trackHub.h"
#include "web.h"

char *cartJsonOptionalParam(struct hash *paramHash, char *name)
/* Convenience function for a CartJsonHandler function: Look up name in paramHash.
 * Return the string contained in its jsonElement value, or NULL if not found. */
{
struct jsonElement *jel = hashFindVal(paramHash, name);
if (jel)
    return jsonStringVal(jel, name);
return NULL;
}

char *cartJsonRequiredParam(struct hash *paramHash, char *name, struct jsonWrite *jw, char *context)
/* Convenience function for a CartJsonHandler function: Look up name in paramHash.
 * If found, return the string contained in its jsonElement value.
 * If not, write an error message (using context) and return NULL. */
{
char *param = cartJsonOptionalParam(paramHash, name);
if (param == NULL)
    jsonWriteStringf(jw, "error",
		     "%s: required param %s is missing", context, name);
return param;
}

static void printClade(struct jsonWrite *jw, char *clade)
/* Print out clade menu's option list and selected item */
{
struct slPair *cladeOptions = hGetCladeOptions();
jsonWriteValueLabelList(jw, "cladeOptions", cladeOptions);
jsonWriteString(jw, "clade", clade);
}

static void printOrg(struct jsonWrite *jw, char *clade, char *org)
/* Print out org menu's option list and selected item */
{
struct slPair *orgOptions = hGetGenomeOptionsForClade(clade);
jsonWriteValueLabelList(jw, "orgOptions", orgOptions);
jsonWriteString(jw, "org", org);
}

static void printDb(struct jsonWrite *jw, char *org, char *db)
/* Print out db menu's option list and selected item */
{
struct slPair *dbOptions = hGetDbOptionsForGenome(org);
jsonWriteValueLabelList(jw, "dbOptions", dbOptions);
jsonWriteString(jw, "db", db);
}

static void printPosition(struct jsonWrite *jw, char *position)
/* Print the position as given (it's up to caller to ensure this is consistent w/cart). */
{
jsonWriteString(jw, "position", position);
}

static char *stripAnchor(char *textIn)
/* If textIn contains an HTML anchor tag, strip it out (and its end tag). */
{
regmatch_t matches[3];
if (regexMatchSubstrNoCase(textIn, "<a href[^>]+>", matches, ArraySize(matches)))
    {
    char *textOut = cloneString(textIn);
    memmove(textOut+matches[0].rm_so, textOut+matches[0].rm_eo,
	    strlen(textOut) + 1 - matches[0].rm_eo);
    if (regexMatchSubstrNoCase(textOut, "</a>", matches, ArraySize(matches)))
	memmove(textOut+matches[0].rm_so, textOut+matches[0].rm_eo,
		strlen(textOut) + 1 - matches[0].rm_eo);
    return textOut;
    }
return textIn;
}

static void hgPositionsJson(struct jsonWrite *jw, char *db, struct hgPositions *hgp, struct cart *cart)
/* Write out JSON description of multiple position matches. */
{
struct hgPosTable *table;
jsonWriteListStart(jw, "positionMatches");
struct trackDb *tdbList = NULL;
for (table = hgp->tableList; table != NULL; table = table->next)
    {
    if (table->posList != NULL)
	{
	char *tableName = table->name;
	// clear the tdb cache if this track is a hub track
	if (isHubTrack(tableName))
	    tdbList = NULL;
	struct trackDb *tdb = tdbForTrack(db, tableName, &tdbList);
	if (!tdb && startsWith("all_", tableName))
            tdb = tdbForTrack(db, tableName+strlen("all_"), &tdbList);
        if (!tdb)
            errAbort("no track for table \"%s\" found via a findSpec", tableName);
	char *trackName = tdb->track;
	jsonWriteObjectStart(jw, NULL);
	jsonWriteString(jw, "name", table->name);
	jsonWriteString(jw, "trackName", trackName);
	jsonWriteString(jw, "description", table->description);
	jsonWriteString(jw, "vis", hCarefulTrackOpenVis(db, trackName));
	jsonWriteListStart(jw, "matches");
	struct hgPos *pos;
	for (pos = table->posList; pos != NULL; pos = pos->next)
	    {
	    char *encMatches = cgiEncode(pos->browserName);
	    jsonWriteObjectStart(jw, NULL); // begin one match
	    if (pos->chrom != NULL)
		jsonWriteStringf(jw, "position", "%s:%d-%d",
				 pos->chrom, pos->chromStart+1, pos->chromEnd);
	    else
		// GenBank results set position to GB accession instead of chr:s-e position.
		jsonWriteString(jw, "position", pos->name);
	    // this is magic to tell the browser to make the
	    // composite and this subTrack visible
	    if (tdb->parent)
		{
		if (tdbIsSuperTrackChild(tdb))
		    jsonWriteStringf(jw, "extraSel", "%s=show&", tdb->parent->track);
		else
		    {
		    // tdb is a subtrack of a composite or a view
		    jsonWriteStringf(jw, "extraSel", "%s_sel=1&%s_sel=1&",
				     trackName, tdb->parent->track);
		    }
		}
	    jsonWriteString(jw, "hgFindMatches", encMatches);
	    jsonWriteString(jw, "posName", htmlEncodeText(pos->name, FALSE));
	    if (pos->description)
		{
		stripString(pos->description, "\n");
		jsonWriteString(jw, "description", stripAnchor(pos->description));
		}
	    jsonWriteObjectEnd(jw); // end one match
	    }
	jsonWriteListEnd(jw); // end matches
	jsonWriteObjectEnd(jw); // end one table
	}
    }
    jsonWriteListEnd(jw); // end positionMatches
}

static struct hgPositions *genomePosCJ(struct jsonWrite *jw,
				       char *db, char *spec, char **retChromName,
				       int *retWinStart, int *retWinEnd, struct cart *cart)
/* Search for positions in genome that match user query.
 * Return an hgp unless there is a problem.  hgp->singlePos will be set if a single
 * position matched.
 * Otherwise display list of positions, put # of positions in retWinStart,
 * and return NULL. */
{
char *hgAppName = "cartJson";
struct hgPositions *hgp = NULL;
char *chrom = NULL;
int start = BIGNUM;
int end = 0;

char *terms[16];
int termCount = chopByChar(cloneString(spec), ';', terms, ArraySize(terms));
boolean multiTerm = (termCount > 1);

int i = 0;
for (i = 0;  i < termCount;  i++)
    {
    trimSpaces(terms[i]);
    if (isEmpty(terms[i]))
	continue;
    hgp = hgPositionsFind(db, terms[i], "", hgAppName, cart, multiTerm);
    if (hgp == NULL || hgp->posCount == 0)
	{
	jsonWriteStringf(jw, "error",
			 "Sorry, couldn't locate %s in genome database", htmlEncode(terms[i]));
	if (multiTerm)
	    jsonWriteStringf(jw, "error",
			     "%s not uniquely determined -- can't do multi-position search.",
			     terms[i]);
	*retWinStart = 0;
	return NULL;
	}
    if (hgp->singlePos != NULL)
	{
	if (chrom != NULL && !sameString(chrom, hgp->singlePos->chrom))
	    {
	    jsonWriteStringf(jw, "error",
			     "Sites occur on different chromosomes: %s, %s.",
			     chrom, hgp->singlePos->chrom);
	    return NULL;
	    }
	chrom = hgp->singlePos->chrom;
	if (hgp->singlePos->chromStart < start)
	    start = hgp->singlePos->chromStart;
	if (hgp->singlePos->chromEnd > end)
	    end = hgp->singlePos->chromEnd;
	}
    else
	{
	hgPositionsJson(jw, db, hgp, cart);
	if (multiTerm && hgp->posCount != 1)
	    {
	    jsonWriteStringf(jw, "error",
			     "%s not uniquely determined (%d locations) -- "
			     "can't do multi-position search.",
			     terms[i], hgp->posCount);
	    return NULL;
	    }
	break;
	}
    }
if (hgp != NULL)
    {
    *retChromName = chrom;
    *retWinStart  = start;
    *retWinEnd    = end;
    }
return hgp;
}

static void changePosition(struct cartJson *cj, char *newPosition)
/* Update position in cart, after performing lookup if necessary.
 * Usually we don't report what we just changed, but since we might modify it,
 * print out the final value. */
{
char *db = cartString(cj->cart, "db");
char *chrom = NULL;
int start=0, end=0;
struct hgPositions *hgp = genomePosCJ(cj->jw, db, newPosition, &chrom, &start, &end, cj->cart);
// If it resolved to a single position, update the cart; otherwise the app can
// present the error (or list of matches) to the user.
if (hgp && hgp->singlePos)
    {
    char newPosBuf[128];
    safef(newPosBuf, sizeof(newPosBuf), "%s:%d-%d", chrom, start+1, end);
    cartSetString(cj->cart, "position", newPosBuf);
    printPosition(cj->jw, newPosBuf);
    }
else
    // Search failed; restore position from cart
    jsonWriteString(cj->jw, "position", cartUsualString(cj->cart, "position", hDefaultPos(db)));
}

static void changePositionHandler(struct cartJson *cj, struct hash *paramHash)
/* Update position in cart, after performing lookup if necessary.
 * Usually we don't report what we just changed, but since we might modify it,
 * print out the final value. */
{
char *newPosition = cartJsonRequiredParam(paramHash, "newValue", cj->jw, "changePosition");
if (newPosition)
    changePosition(cj, newPosition);
}

static void printGeneSuggestTrack(struct cartJson *cj, char *db)
/* Get the gene track used by hgSuggest for db (defaulting to cart db), or null if
 * there is none for this assembly. */
{
if (isEmpty(db))
    db = cartString(cj->cart, "db");
char *track = assemblyGeneSuggestTrack(db);
jsonWriteString(cj->jw, "geneSuggestTrack", track);
}

static void changeDb(struct cartJson *cj, char *newDb)
/* Change db to new value, update cart and print JSON of new position & gene suggest track. */
{
cartSetString(cj->cart, "db", newDb);
jsonWriteString(cj->jw, "db", newDb);
char *defaultPosition = hDefaultPos(newDb);
changePosition(cj, defaultPosition);
printGeneSuggestTrack(cj, newDb);
}

void cartJsonChangeDb(struct cartJson *cj, struct hash *paramHash)
/* Change db to new value, update cart and print JSON of new position & gene suggest track. */
{
char *newDb = cartJsonRequiredParam(paramHash, "newValue", cj->jw, "changeDb");
if (newDb)
    changeDb(cj, newDb);
}

static void changeOrg(struct cartJson *cj, char *newOrg)
/* Change org to new value, update cart variables and print JSON of new db menu etc. */
{
char *newDb = hDefaultDbForGenome(newOrg);
if (isEmpty(newDb))
    {
    jsonWriteStringf(cj->jw, "error",
		     "changeOrg: can't find default db for '%s'", newOrg);
    return;
    }
cartSetString(cj->cart, "org", newOrg);
jsonWriteString(cj->jw, "org", newOrg);
printDb(cj->jw, newOrg, newDb);
changeDb(cj, newDb);
}

void cartJsonChangeOrg(struct cartJson *cj, struct hash *paramHash)
/* Change org to new value, update cart and print JSON of new db menu, new position etc. */
{
char *newOrg = cartJsonRequiredParam(paramHash, "newValue", cj->jw, "changeOrg");
if (newOrg)
    changeOrg(cj, newOrg);
}

void cartJsonChangeClade(struct cartJson *cj, struct hash *paramHash)
/* Change clade to new value, update cart, and print JSON of new org & db menus, new position etc */
{
char *newClade = cartJsonRequiredParam(paramHash, "newValue", cj->jw, "changeClade");
if (! newClade)
    return;
char *newOrg = hDefaultGenomeForClade(newClade);
if (isEmpty(newOrg))
    {
    jsonWriteStringf(cj->jw, "error",
		     "changeClade: can't find default genome for '%s'", newClade);
    return;
    }
cartSetString(cj->cart, "clade", newClade);
jsonWriteString(cj->jw, "clade", newClade);
printOrg(cj->jw, newClade, newOrg);
changeOrg(cj, newOrg);
}

static void getVar(struct cartJson *cj, struct hash *paramHash)
/* Print out the requested cart var(s). varString may be a comma-separated list.
 * If a var is a list variable, prints out a list of values for that var. */
{
char *varString = cartJsonRequiredParam(paramHash, "var", cj->jw, "get");
if (! varString)
    return;
struct slName *varList = slNameListFromComma(varString), *var;
for (var = varList;  var != NULL;  var = var->next)
    {
    if (cartListVarExists(cj->cart, var->name))
	{
	// Use cartOptionalSlNameList and return a list:
	struct slName *valList = cartOptionalSlNameList(cj->cart, var->name);
	jsonWriteSlNameList(cj->jw, var->name, valList);
	slFreeList(&valList);
	}
    else
	{
	// Regular single-value variable (or not in the cart):
	char *val = cartOptionalString(cj->cart, var->name);
	//#*** TODO: move jsonStringEscape inside jsonWriteString
	char *encoded = jsonStringEscape(val);
	jsonWriteString(cj->jw, var->name, encoded);
	freeMem(encoded);
	}
    }
slFreeList(&varList);
}

static struct slPair *trackLabelPairsFromTdbRefs(struct slRef *tdbRefList)
/* For each tdb in tdbRefList, make a pair of {track, shortLabel}. */
{
struct slPair *pairList = NULL;
struct slRef *slr;
for (slr = tdbRefList;  slr != NULL;  slr = slr->next)
    {
    struct trackDb *tdb = slr->val;
    slAddHead(&pairList, slPairNew(tdb->track, tdb->shortLabel));
    }
slReverse(&pairList);
return pairList;
}

static int trackDbRefCmpShortLabel(const void *va, const void *vb)
/* Case-insensitive comparison of max 32 chars of tdb->shortLabel in slRefs. */
{
const struct slRef *aRef = *((struct slRef **)va);
const struct slRef *bRef = *((struct slRef **)vb);
const struct trackDb *tdbA = aRef->val, *tdbB = bRef->val;
char shortLabelA[33], shortLabelB[33];
strncpy(shortLabelA, tdbA->shortLabel, 32);
shortLabelA[32] = '\0';
strncpy(shortLabelB, tdbB->shortLabel, 32);
shortLabelB[32] = '\0';
tolowers(shortLabelA);
tolowers(shortLabelB);
return strcmp(shortLabelA, shortLabelB);
}

static void printAllTracks(struct cartJson *cj, struct trackDb *trackList)
/* Print a mapping of "allTracks" to {track, shortLabel} for all tracks,
 * alphabetized by shortLabel. */
{
struct slRef *refList = refListFromSlList(trackList);
slSort(&refList, trackDbRefCmpShortLabel);
struct slPair *trackLabelPairs = trackLabelPairsFromTdbRefs(refList);
jsonWriteValueLabelList(cj->jw, "allTracks", trackLabelPairs);
}

static void printOneGroupTracks(struct cartJson *cj, char *groupName, struct slRef *tdbRefList)
/* Given a group and value is an slRef list of trackDbs, sort the list by trackDb priority
 * and print out JSON mapping the group name to list of trackDb {value, label} objects. */
{
slSort(&tdbRefList, trackDbRefCmp);
struct slPair *trackLabelPairs = trackLabelPairsFromTdbRefs(tdbRefList);
jsonWriteValueLabelList(cj->jw, groupName, trackLabelPairs);
}

static void printGroupTracks(struct cartJson *cj, struct trackDb *trackList)
/* Hash group names to lists of tracks in those groups, and print out the structure as JSON. */
{
struct hash *hash = hashNew(0);
struct trackDb *tdb;
for (tdb = trackList;  tdb != NULL;  tdb = tdb->next)
    {
    struct hashEl *hel = hashLookup(hash, tdb->grp);
    struct slRef *slr = slRefNew(tdb);
    if (hel)
	slAddHead(&(hel->val), slr);
    else
	hashAdd(hash, tdb->grp, slr);
    }
jsonWriteObjectStart(cj->jw, "groupTracks");
printAllTracks(cj, trackList);
struct hashCookie cookie = hashFirst(hash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    char *groupName = hel->name;
    struct slRef *tdbRefList = hel->val;
    printOneGroupTracks(cj, groupName, tdbRefList);
    }
jsonWriteObjectEnd(cj->jw);
hashFree(&hash);
}

static void printTrackTables(struct cartJson *cj, struct trackDb *trackList)
/* Hash track names to lists of tables in those tracks, and print out the structure as JSON. */
{
jsonWriteObjectStart(cj->jw, "trackTables");
char *db = cartString(cj->cart, "db");
struct trackDb *tdb;
for (tdb = trackList;  tdb != NULL;  tdb = tdb->next)
    {
    struct slName *tableList = cartTrackDbTablesForTrack(db, tdb, FALSE); // no useJoiner for now
    jsonWriteSlNameList(cj->jw, tdb->track, tableList);
    slFreeList(&tableList);
    }
jsonWriteObjectEnd(cj->jw);
}

void cartJsonGetGroupsTracksTables(struct cartJson *cj, struct hash *paramHash)
/* Print info necessary for group/track/table menus. */
{
struct trackDb *fullTrackList = NULL;
struct grp *fullGroupList = NULL;
cartTrackDbInit(cj->cart, &fullTrackList, &fullGroupList, /* useAccessControl=*/TRUE);
jsonWriteObjectStart(cj->jw, "trackDbInfo");
// Print out options for the track group menu:
jsonWriteValueLabelList(cj->jw, "groupOptions", (struct slPair *)fullGroupList);
// Print out an object that maps group names to their tracks:
printGroupTracks(cj, fullTrackList);
// Print out an object that maps track names to tables:
printTrackTables(cj, fullTrackList);
jsonWriteObjectEnd(cj->jw);
}

static char *hAssemblyDescription(char *db)
/* Return a string containing db's description.html, or NULL if not found. */
//#*** LIBIFY: Code lifted from hgFind.c's hgPositionsHelpHtml.
{
char *htmlPath = hHtmlPath(db);
char *htmlString = NULL;
if (htmlPath != NULL)
    {
    if (fileExists(htmlPath))
	readInGulp(htmlPath, &htmlString, NULL);
    else if (startsWith("http://" , htmlPath) ||
	     startsWith("https://", htmlPath) ||
	     startsWith("ftp://"  , htmlPath))
	{
	struct lineFile *lf = udcWrapShortLineFile(htmlPath, NULL, 256*1024);
	htmlString = lineFileReadAll(lf);
	lineFileClose(&lf);
	}
    }
return htmlString;
}

static void getAssemblyInfo(struct cartJson *cj, struct hash *paramHash)
/* Return useful things from dbDb (or track hub) and assembly description html (possibly NULL).
 * If db param is NULL, use db from cart. */
{
char *db = cartJsonOptionalParam(paramHash, "db");
if (db == NULL)
    db = cartString(cj->cart, "db");
jsonWriteString(cj->jw, "db", db);
jsonWriteString(cj->jw, "commonName", hGenome(db));
jsonWriteString(cj->jw, "scientificName", hScientificName(db));
jsonWriteString(cj->jw, "dbLabel", hFreezeDate(db));
//#*** TODO: move jsonStringEscape inside jsonWriteString
jsonWriteString(cj->jw, "assemblyDescription", jsonStringEscape(hAssemblyDescription(db)));
}

static void getHasCustomTracks(struct cartJson *cj, struct hash *paramHash)
/* Tell whether cart has custom tracks for db.  If db param is NULL, use db from cart. */
{
char *db = cartJsonOptionalParam(paramHash, "db");
if (db == NULL)
    db = cartString(cj->cart, "db");
jsonWriteBoolean(cj->jw, "hasCustomTracks", customTracksExistDb(cj->cart, db, NULL));
}

static void getIsSpecialHost(struct cartJson *cj, struct hash *paramHash)
/* Tell whether we're on a development host, preview, gsid etc. */
{
jsonWriteBoolean(cj->jw, "isPrivateHost", hIsPrivateHost());
jsonWriteBoolean(cj->jw, "isBetaHost", hIsBetaHost());
jsonWriteBoolean(cj->jw, "isBrowserbox", hIsBrowserbox());
jsonWriteBoolean(cj->jw, "isPreviewHost", hIsPreviewHost());
}

static void getHasHubTable(struct cartJson *cj, struct hash *paramHash)
/* Tell whether central database has a hub table (i.e. server can do hubs). */
{
jsonWriteBoolean(cj->jw, "hasHubTable", hubConnectTableExists());
}

static void setIfUnset(struct cartJson *cj, struct hash *paramHash)
/* For each name in paramHash, if that cart variable doesn't already have a non-empty
 * value, set it to the value. */
{
struct hashCookie cookie = hashFirst(paramHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (isEmpty(cartOptionalString(cj->cart, hel->name)))
	{
	char *val = jsonStringVal((struct jsonElement *)(hel->val), hel->name);
	if (val)
	    cartSetString(cj->cart, hel->name, val);
	}
    }
}

static void getCladeOrgDbPos(struct cartJson *cj, struct hash *paramHash)
/* Get cart's current clade, org, db, position and geneSuggest track. */
{
char *db = cartString(cj->cart, "db");
char *genome = hGenome(db);
char *clade = hClade(genome);
printClade(cj->jw, clade);
printOrg(cj->jw, clade, genome);
printDb(cj->jw, genome, db);
char *position = cartOptionalString(cj->cart, "position");
if (isEmpty(position))
    position = hDefaultPos(db);
printPosition(cj->jw, position);
printGeneSuggestTrack(cj, cartString(cj->cart, "db"));
}

static void getStaticHtml(struct cartJson *cj, struct hash *paramHash)
/* Read HTML text from a relative path under browser.documentRoot and
 * write it as an encoded JSON string */
{
char *tag = cartJsonOptionalParam(paramHash, "tag");
if (isEmpty(tag))
    tag = "html";
char *file = cartJsonRequiredParam(paramHash, "file", cj->jw, "getStaticHtml");
char *html = hFileContentsOrWarning(file);
//#*** TODO: move jsonStringEscape inside jsonWriteString
char *encoded = jsonStringEscape(html);
jsonWriteString(cj->jw, tag, encoded);
}

void cartJsonRegisterHandler(struct cartJson *cj, char *command, CartJsonHandler *handler)
/* Associate handler with command; when javascript sends command, handler will be executed. */
{
hashAdd(cj->handlerHash, command, handler);
}

struct cartJson *cartJsonNew(struct cart *cart)
/* Allocate and return a cartJson object with default handlers. */
{
struct cartJson *cj;
AllocVar(cj);
cj->handlerHash = hashNew(0);
cj->jw = jsonWriteNew();
cj->cart = cart;
cartJsonRegisterHandler(cj, "getCladeOrgDbPos", getCladeOrgDbPos);
cartJsonRegisterHandler(cj, "changeClade", cartJsonChangeClade);
cartJsonRegisterHandler(cj, "changeOrg", cartJsonChangeOrg);
cartJsonRegisterHandler(cj, "changeDb", cartJsonChangeDb);
cartJsonRegisterHandler(cj, "changePosition", changePositionHandler);
cartJsonRegisterHandler(cj, "get", getVar);
cartJsonRegisterHandler(cj, "getGroupsTracksTables", cartJsonGetGroupsTracksTables);
cartJsonRegisterHandler(cj, "getAssemblyInfo", getAssemblyInfo);
cartJsonRegisterHandler(cj, "getHasCustomTracks", getHasCustomTracks);
cartJsonRegisterHandler(cj, "getIsSpecialHost", getIsSpecialHost);
cartJsonRegisterHandler(cj, "getHasHubTable", getHasHubTable);
cartJsonRegisterHandler(cj, "setIfUnset", setIfUnset);
cartJsonRegisterHandler(cj, "getStaticHtml", getStaticHtml);
return cj;
}

void cartJsonFree(struct cartJson **pCj)
/* Close **pCj's contents and nullify *pCj. */
{
if (*pCj == NULL)
    return;
struct cartJson *cj = *pCj;
jsonWriteFree(&cj->jw);
hashFree(&cj->handlerHash);
freez(pCj);
}

static void doOneCommand(struct cartJson *cj, char *command,
                         struct jsonElement *paramObject)
/* Dispatch command by name, checking for required parameters. */
{
CartJsonHandler *handler = hashFindVal(cj->handlerHash, command);
if (handler)
    {
    struct hash *paramHash = jsonObjectVal(paramObject, command);
    handler(cj, paramHash);
    }
else
    {
    jsonWriteStringf(cj->jw, "error",
		     "cartJson: unrecognized command '%s'\"", command);
    return;
    }
}

static int commandCmp(const void *pa, const void *pb)
/* Comparison function to put "change" commands ahead of other commands. */
{
struct slPair *cmdA = *((struct slPair **)pa);
struct slPair *cmdB = *((struct slPair **)pb);
boolean aIsChange = startsWith("change", cmdA->name);
boolean bIsChange = startsWith("change", cmdB->name);
if (aIsChange && !bIsChange)
    return -1;
if (!aIsChange && bIsChange)
    return 1;
return strcmp(cmdA->name, cmdB->name);
}

// Accumulate warnings so they can be JSON-ified:
static struct dyString *dyWarn = NULL;

static void cartJsonVaWarn(char *format, va_list args)
/* Save warnings for later. */
{
dyStringVaPrintf(dyWarn, format, args);
}

static void cartJsonPrintWarnings(struct jsonWrite *jw)
/* If there are warnings, write them out as JSON: */
{
if (dyWarn && dyStringLen(dyWarn) > 0)
    {
    //#*** TODO: move jsonStringEscape inside jsonWriteString
    char *encoded = jsonStringEscape(dyWarn->string);
    jsonWriteString(jw, "warning", encoded);
    freeMem(encoded);
    }
}

static void cartJsonAbort()
/* Print whatever warnings we have accumulated and exit. */
{
if (dyWarn)
    puts(dyWarn->string);
exit(0);
}

static void cartJsonPushErrHandlers()
/* Push warn and abort handlers for errAbort. */
{
if (dyWarn == NULL)
    dyWarn = dyStringNew(0);
else
    dyStringClear(dyWarn);
pushWarnHandler(cartJsonVaWarn);
pushAbortHandler(cartJsonAbort);
}

static void cartJsonPopErrHandlers()
/* Pop warn and abort handlers for errAbort. */
{
popWarnHandler();
popAbortHandler();
}

void cartJsonExecute(struct cartJson *cj)
/* Get commands from cgi, print Content-type, execute commands, print results as JSON. */
{
cartJsonPushErrHandlers();
puts("Content-Type:text/javascript\n");

// Initialize response JSON object:
jsonWriteObjectStart(cj->jw, NULL);
// Always send back hgsid:
jsonWriteString(cj->jw, cartSessionVarName(), cartSessionId(cj->cart));

char *commandJson = cgiOptionalString(CARTJSON_COMMAND);
if (commandJson)
    {
    struct jsonElement *commandObj = jsonParse(commandJson);
    struct hash *commandHash = jsonObjectVal(commandObj, "commandObj");
    // change* commands need to go first!  Really we need an ordered map type here...
    // for now, just make a list and sort to put change commands at the front.
    struct slPair *commandList = NULL, *cmd;
    struct hashCookie cookie = hashFirst(commandHash);
    struct hashEl *hel;
    while ((hel = hashNext(&cookie)) != NULL)
        slAddHead(&commandList, slPairNew(hel->name, hel->val));
    slSort(&commandList, commandCmp);
    for (cmd = commandList;  cmd != NULL;  cmd = cmd->next)
	doOneCommand(cj, cmd->name, (struct jsonElement *)cmd->val);
    }

cartJsonPrintWarnings(cj->jw);
jsonWriteObjectEnd(cj->jw);
puts(cj->jw->dy->string);
cartJsonPopErrHandlers();
}
