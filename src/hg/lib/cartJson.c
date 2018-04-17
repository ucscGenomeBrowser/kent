/* cartJson - parse and execute JSON commands to update cart and/or return cart data as JSON. */
#include "common.h"
#include "cartJson.h"
#include "cartTrackDb.h"
#include "cheapcgi.h"
#include "errCatch.h"
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
#include "trackDb.h"
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

char *cartJsonParamDefault(struct hash *paramHash, char *name, char *defaultVal)
/* Convenience function for a CartJsonHandler function: Look up name in paramHash.
 * Return the string contained in its jsonElement value, or defaultVal if not found. */
{
struct jsonElement *jel = hashFindVal(paramHash, name);
if (jel)
    return jsonStringVal(jel, name);
return defaultVal;
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
	    jsonWriteString(jw, "posName", htmlEncode(pos->name));
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
			 "Sorry, couldn't locate %s in %s %s", htmlEncode(terms[i]),
                         trackHubSkipHubName(hOrganism(db)), hFreezeDate(db));
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
    jsonWriteString(cj->jw, "position", newPosBuf);
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
char *track = NULL;
if (! trackHubDatabase(db))
    track = assemblyGeneSuggestTrack(db);
jsonWriteString(cj->jw, "geneSuggestTrack", track);
}

static void getGeneSuggestTrack(struct cartJson *cj, struct hash *paramHash)
/* Get the gene track used by hgSuggest for db (defaulting to cart db), or null if
 * there is none for this assembly. */
{
char *db = cartJsonOptionalParam(paramHash, "db");
printGeneSuggestTrack(cj, db);
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
	jsonWriteString(cj->jw, var->name, val);
	}
    }
slFreeList(&varList);
}

INLINE boolean nameIsTdbField(char *name)
/* Return TRUE if name is a tdb->{field}, e.g. "track" or "shortLabel" etc. */
{
static char *tdbFieldNames[] =
    { "track", "table", "shortLabel", "longLabel", "type", "priority", "grp", "parent",
      "subtracks", "visibility" };
return (stringArrayIx(name, tdbFieldNames, ArraySize(tdbFieldNames)) >= 0);
}

INLINE boolean fieldOk(char *field, struct hash *fieldHash)
/* Return TRUE if fieldHash is NULL or field exists in fieldHash. */
{
return (fieldHash == NULL || hashLookup(fieldHash, field));
}

static void writeTdbSimple(struct jsonWrite *jw, struct trackDb *tdb, struct hash *fieldHash)
/* Write JSON for the non-parent/child fields of tdb */
{
if (fieldOk("track", fieldHash))
    jsonWriteString(jw, "track", tdb->track);
if (fieldOk("table", fieldHash))
    jsonWriteString(jw, "table", tdb->table);
if (fieldOk("shortLabel", fieldHash))
    jsonWriteString(jw, "shortLabel", tdb->shortLabel);
if (fieldOk("longLabel", fieldHash))
    jsonWriteString(jw, "longLabel", tdb->longLabel);
if (fieldOk("type", fieldHash))
    jsonWriteString(jw, "type", tdb->type);
if (fieldOk("priority", fieldHash))
    jsonWriteDouble(jw, "priority", tdb->priority);
if (fieldOk("grp", fieldHash))
    jsonWriteString(jw, "grp", tdb->grp);
// NOTE: if you add a new field here, then also add it to nameIsTdbField above.
if (tdb->settingsHash)
    {
    struct hashEl *hel;
    struct hashCookie cookie = hashFirst(tdb->settingsHash);
    while ((hel = hashNext(&cookie)) != NULL)
        {
        if (! nameIsTdbField(hel->name) && fieldOk(hel->name, fieldHash))
            jsonWriteString(jw, hel->name, (char *)hel->val);
        }
    if (fieldOk("noGenome", fieldHash))
        {
        if ((hel = hashLookup(tdb->settingsHash, "tableBrowser")) != NULL)
            {
            if (startsWithWord("noGenome", (char *)(hel->val)))
                jsonWriteBoolean(jw, "noGenome", TRUE);
            }
        }
    }
}

static int trackDbViewCmp(const void *va, const void *vb)
/* *** I couldn't find anything like this in hgTracks or hui, but clearly views end up
 * *** being ordered alphabetically somehow.  Anyway...
 * Compare two trackDbs, first by view if both have a view setting, then the usual way
 * (by priority, then by shortLabel). */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
// The casts are necessary here unless one adds const to trackDbSetting decl
char *viewA = trackDbSetting((struct trackDb *)a, "view");
char *viewB = trackDbSetting((struct trackDb *)b, "view");
int diff = 0;
if (isNotEmpty(viewA) && isNotEmpty(viewB))
    diff = strcmp(viewA, viewB);
if (diff != 0)
    return diff;
return trackDbCmp(va, vb);
}

static struct jsonWrite *rTdbToJw(struct trackDb *tdb, struct hash *fieldHash,
                                  struct hash *excludeTypesHash, int depth, int maxDepth)
/* Recursively build and return a new jsonWrite object with JSON for tdb and its children,
 * or NULL if tdb or all children have been filtered out by excludeTypesHash.
 * If excludeTypesHash is non-NULL, omit any tracks/views/subtracks with type in excludeTypesHash.
 * If fieldHash is non-NULL, include only the field names indexed in fieldHash. */
{
if (maxDepth >= 0 && depth > maxDepth)
    return NULL;
boolean doSubtracks = (tdb->subtracks && fieldOk("subtracks", fieldHash));
// If excludeTypesHash is given and tdb is a leaf track/subtrack, look up the first word
// of tdb->type in excludeTypesHash; if found, return NULL.
if (excludeTypesHash && !doSubtracks)
    {
    char typeCopy[PATH_LEN];
    safecpy(typeCopy, sizeof(typeCopy), tdb->type);
    if (hashLookup(excludeTypesHash, firstWordInLine(typeCopy)))
        return NULL;
    }
boolean gotSomething = !doSubtracks;
struct jsonWrite *jwNew = jsonWriteNew();
jsonWriteObjectStart(jwNew, NULL);
writeTdbSimple(jwNew, tdb, fieldHash);
if (tdb->parent && fieldOk("parent", fieldHash))
    {
    // We can't link to an object in JSON and better not recurse here or else infinite loop.
    if (tdbIsSuperTrackChild(tdb))
        {
        // Supertracks have been omitted from fullTrackList, so add the supertrack object's
        // non-parent/child info here.
        jsonWriteObjectStart(jwNew, "parent");
        writeTdbSimple(jwNew, tdb->parent, fieldHash);
        jsonWriteObjectEnd(jwNew);
        }
    else
        // Just the name so we don't have infinite loops.
        jsonWriteString(jwNew, "parent", tdb->parent->track);
    }
if (doSubtracks)
    {
    jsonWriteListStart(jwNew, "subtracks");
    slSort(&tdb->subtracks, trackDbViewCmp);
    struct trackDb *subTdb;
    for (subTdb = tdb->subtracks;  subTdb != NULL;  subTdb = subTdb->next)
        {
        struct jsonWrite *jwSub = rTdbToJw(subTdb, fieldHash, excludeTypesHash, depth+1, maxDepth);
        if (jwSub)
            {
            gotSomething = TRUE;
            jsonWriteAppend(jwNew, NULL, jwSub);
            jsonWriteFree(&jwSub);
            }
        }
    jsonWriteListEnd(jwNew);
    }
jsonWriteObjectEnd(jwNew);
if (! gotSomething)
    // All children were excluded; clean up and null out jwNew.
    jsonWriteFree(&jwNew);
return jwNew;
}

static struct hash *hashFromCommaString(char *commaString)
/* Return a hash that stores words in comma-separated string, or NULL if string is NULL or empty. */
{
struct hash *hash = NULL;
if (isNotEmpty(commaString))
    {
    hash = hashNew(0);
    char *words[1024];
    int i, wordCount = chopCommas(commaString, words);
    for (i = 0;  i < wordCount;  i++)
        hashStoreName(hash, words[i]);
    }
return hash;
}

static struct hash *hashTracksByGroup(struct trackDb *trackList)
/* Hash group names to lists of tracks in those groups; sort each list by trackDb priority. */
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
struct hashCookie cookie = hashFirst(hash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    slSort(&hel->val, trackDbRefCmp);
return hash;
}

int trackDbRefCmpShortLabel(const void *va, const void *vb)
/* Do trackDbCmpShortLabel on list of references as opposed to actual trackDbs. */
{
const struct slRef *aRef = *((struct slRef **)va);
const struct slRef *bRef = *((struct slRef **)vb);
struct trackDb *a = aRef->val, *b = bRef->val;
return trackDbCmpShortLabel(&a, &b);
}

static struct slRef *sortedAllTracks(struct trackDb *trackList)
/* Return an slRef list containing all tdbs in track list, sorted by shortLabel. */
{
struct slRef *trackRefList = NULL;
struct trackDb *tdb;
for (tdb = trackList;  tdb != NULL;  tdb = tdb->next)
    slAddHead(&trackRefList, slRefNew(tdb));
slSort(&trackRefList, trackDbRefCmpShortLabel);
return trackRefList;
}

static boolean writeGroupedTrack(struct jsonWrite *jw, char *name, char *label,
                                 struct hash *fieldHash, struct hash *excludeTypesHash,
                                 int maxDepth, struct slRef *tdbRefList)
/* If tdbRefList is empty after excluding tracks/views/subtracks whose types are
 * in excludeTypesHash, then return FALSE and write nothing.  Otherwise write a group
 * and its tracks/views/subtracks and return TRUE. */
{
// Make a new jsonWrite object in case this group turns out to have no children after filtering.
struct jsonWrite *jwNew = jsonWriteNew();
jsonWriteObjectStart(jwNew, NULL);
jsonWriteString(jwNew, "name", name);
jsonWriteString(jwNew, "label", label);
jsonWriteListStart(jwNew, "tracks");
boolean gotSomething = FALSE;
struct slRef *tdbRef;
for (tdbRef = tdbRefList;  tdbRef != NULL;  tdbRef = tdbRef->next)
    {
    struct trackDb *tdb = tdbRef->val;
    // First see if there are any tracks to show for this group:
    struct jsonWrite *jwTrack = rTdbToJw(tdb, fieldHash, excludeTypesHash, 1, maxDepth);
    if (jwTrack)
        {
        gotSomething = TRUE;
        jsonWriteAppend(jwNew, NULL, jwTrack);
        jsonWriteFree(&jwTrack);
        }
    }
if (gotSomething)
    {
    // Group has at least one track, so append it to jw.
    jsonWriteListEnd(jwNew);
    jsonWriteObjectEnd(jwNew);
    jsonWriteAppend(jw, NULL, jwNew);
    }
jsonWriteFree(&jwNew);
return gotSomething;
}

void cartJsonGetGroupedTrackDb(struct cartJson *cj, struct hash *paramHash)
/* Translate trackDb list (only a subset of the fields) into JSON array of track group objects;
 * each group contains an array of track objects that may have subtracks.  Send it in a wrapper
 * object that includes the database from which it was taken; it's possible that by the time
 * this reaches the client, the user might have switched to a new db. */
{
struct jsonWrite *jw = cj->jw;
struct trackDb *fullTrackList = NULL;
struct grp *fullGroupList = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    cartTrackDbInit(cj->cart, &fullTrackList, &fullGroupList, /* useAccessControl=*/TRUE);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    warn("%s", errCatch->message->string);
    jsonWriteObjectStart(jw, "groupedTrackDb");
    jsonWriteString(jw, "db", cartString(cj->cart, "db"));
    jsonWriteListStart(jw, "groupedTrackDb");
    jsonWriteListEnd(jw);
    jsonWriteObjectEnd(jw);
    return;
    }
errCatchFree(&errCatch);
struct hash *groupedTrackRefList = hashTracksByGroup(fullTrackList);
// If the optional param 'fields' is given, hash the field names that should be returned.
char *fields = cartJsonOptionalParam(paramHash, "fields");
struct hash *fieldHash = hashFromCommaString(fields);
char *excludeTypes = cartJsonOptionalParam(paramHash, "excludeTypes");
struct hash *excludeTypesHash = hashFromCommaString(excludeTypes);
// Also check for optional parameter 'maxDepth':
int maxDepth = -1;
char *maxDepthStr = cartJsonOptionalParam(paramHash, "maxDepth");
if (isNotEmpty(maxDepthStr))
    maxDepth = atoi(maxDepthStr);
jsonWriteObjectStart(jw, "groupedTrackDb");
jsonWriteString(jw, "db", cartString(cj->cart, "db"));
jsonWriteListStart(jw, "groupedTrackDb");
int nonEmptyGroupCount = 0;
struct grp *grp;
for (grp = fullGroupList;  grp != NULL;  grp = grp->next)
    {
    struct slRef *tdbRefList = hashFindVal(groupedTrackRefList, grp->name);
    if (writeGroupedTrack(jw, grp->name, grp->label, fieldHash, excludeTypesHash,
                          maxDepth, tdbRefList))
        {
        nonEmptyGroupCount++;
        }
    }
if (nonEmptyGroupCount == 0)
    {
    // Catch-all for assembly hubs that don't declare groups for their tracks: add All Tracks
    struct slRef *allTracks = sortedAllTracks(fullTrackList);
    (void)writeGroupedTrack(jw, "allTracks", "All Tracks", fieldHash, excludeTypesHash,
                            maxDepth, allTracks);
    }
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
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
jsonWriteString(cj->jw, "assemblyDescription", hAssemblyDescription(db));
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

static void jsonWriteValueLabel(struct jsonWrite *jw, char *value, char *label)
/* Assuming we're already in an object, write out value and label tags & strings. */
{
jsonWriteString(jw, "value", value);
jsonWriteString(jw, "label", label);
}

static void printCladeOrgDbTree(struct jsonWrite *jw)
/* Print out the tree of clades, organisms and dbs as JSON.  Each node has value and label
 * for menu options; clade nodes and org nodes also have children and default. */
{
jsonWriteListStart(jw, "cladeOrgDb");
struct slPair *clade, *cladeOptions = hGetCladeOptions();
struct dbDb *centralDbDbList = hDbDbList();
for (clade = cladeOptions;  clade != NULL;  clade = clade->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteValueLabel(jw, clade->name, clade->val);
    jsonWriteListStart(jw, "children");
    struct slPair *org, *orgOptions = hGetGenomeOptionsForClade(clade->name);
    for (org = orgOptions;  org != NULL;  org = org->next)
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteValueLabel(jw, org->name, org->val);
        jsonWriteListStart(jw, "children");
        struct dbDb *dbDb, *dbDbList;
        if (isHubTrack(org->name))
            dbDbList = trackHubGetDbDbs(clade->name);
        else
            dbDbList = centralDbDbList;
        for (dbDb = dbDbList;  dbDb != NULL;  dbDb = dbDb->next)
            {
            if (sameString(org->name, dbDb->genome))
                {
                jsonWriteObjectStart(jw, NULL);
                jsonWriteValueLabel(jw, dbDb->name, dbDb->description);
                jsonWriteString(jw, "defaultPos", dbDb->defaultPos);
                jsonWriteObjectEnd(jw);
                }
            }
        jsonWriteListEnd(jw);   // children (dbs)
        jsonWriteString(jw, "default", trimSpaces(hDefaultDbForGenome(org->name)));
        jsonWriteObjectEnd(jw); // org
        }
    jsonWriteListEnd(jw);   // children (orgs)
    jsonWriteString(jw, "default", trimSpaces(hDefaultGenomeForClade(clade->name)));
    jsonWriteObjectEnd(jw); // clade
    }
jsonWriteListEnd(jw);
}

static void getCladeOrgDbPos(struct cartJson *cj, struct hash *paramHash)
/* Get cart's current clade, org, db, position and geneSuggest track. */
{
jsonWriteObjectStart(cj->jw, "cladeOrgDb");
printCladeOrgDbTree(cj->jw);
char *db = cartString(cj->cart, "db");
jsonWriteString(cj->jw, "db", db);
char *org = cartUsualString(cj->cart, "org", hGenome(db));
jsonWriteString(cj->jw, "org", org);
char *clade = cartUsualString(cj->cart, "clade", hClade(org));
jsonWriteString(cj->jw, "clade", clade);
jsonWriteObjectEnd(cj->jw);
char *position = cartOptionalString(cj->cart, "position");
if (isEmpty(position))
    position = hDefaultPos(db);
jsonWriteString(cj->jw, "position", position);
printGeneSuggestTrack(cj, db);
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
jsonWriteString(cj->jw, tag, html);
}

void cartJsonRegisterHandler(struct cartJson *cj, char *command, CartJsonHandler *handler)
/* Associate handler with command; when javascript sends command, handler will be executed. */
{
hashAdd(cj->handlerHash, command, handler);
}

struct cartJson *cartJsonNew(struct cart *cart)
/* Allocate and return a cartJson object with default handlers.
 * cart must have "db" set already. */
{
struct cartJson *cj;
AllocVar(cj);
cj->handlerHash = hashNew(0);
cj->jw = jsonWriteNew();
cj->cart = cart;
cartJsonRegisterHandler(cj, "getCladeOrgDbPos", getCladeOrgDbPos);
cartJsonRegisterHandler(cj, "changePosition", changePositionHandler);
cartJsonRegisterHandler(cj, "get", getVar);
cartJsonRegisterHandler(cj, "getGroupedTrackDb", cartJsonGetGroupedTrackDb);
cartJsonRegisterHandler(cj, "getAssemblyInfo", getAssemblyInfo);
cartJsonRegisterHandler(cj, "getGeneSuggestTrack", getGeneSuggestTrack);
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
    jsonWriteString(jw, "warning", dyWarn->string);
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
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
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
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        jsonWritePopToLevel(cj->jw, 1);
        jsonWriteString(cj->jw, "error", errCatch->message->string);
        }
    errCatchFree(&errCatch);
    }

cartJsonPrintWarnings(cj->jw);
jsonWriteObjectEnd(cj->jw);
puts(cj->jw->dy->string);
cartJsonPopErrHandlers();
}
