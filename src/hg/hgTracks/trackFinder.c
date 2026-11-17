/* trackFinder - Track lists in JSON for the Find Tracks dialog. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "ra.h"
#include "cheapcgi.h"
#include "cart.h"
#include "cartJson.h"
#include "cartTrackDb.h"
#include "hdb.h"
#include "hui.h"
#include "grp.h"
#include "trackDb.h"
#include "jsonWrite.h"
#include "jksql.h"
#include "hgConfig.h"
#include "htmlColor.h"
#include "errCatch.h"
#include "trackHub.h"
#include "recTrackSets.h"
#include "hgTracks.h"

#define TRACK_FACETS_RA "trackFacets.ra"
#define TRACK_FACETS_RA_GBDB "/gbdb/hgFixed/trackFacets.ra"

struct trackFacetDef
/* A track finder facet from trackFacets.ra.  Tracks give values with facet.<name>. */
    {
    struct trackFacetDef *next;
    char *name;         /* Facet name, the part after "facet." in trackDb */
    char *label;        /* Facet heading shown in the track finder */
    char *description;  /* Optional text for the info icon next to the heading */
    double priority;    /* Facets are listed in increasing priority */
    };

struct finderGroup
/* A group and the tracks the finder lists in it. */
    {
    struct finderGroup *next;
    struct grp *grp;            /* Group name and label */
    struct slRef *tdbList;      /* Tracks in the group, supertracks included */
    };

static boolean isValidTag(char *tag)
/* Return TRUE if tag is non-empty and only has [A-Za-z0-9] characters. */
{
if (isEmpty(tag))
    return FALSE;
char *c;
for (c = tag; *c != 0; c++)
    if (!isalnum((unsigned char)*c))
        return FALSE;
return TRUE;
}

static void jsonWriteCommaList(struct jsonWrite *jw, char *name, char *setting,
                               boolean tagsOnly)
/* Write a list named name from a comma separated setting.  Empty values are dropped,
 * and so are values that are not valid tags if tagsOnly. */
{
jsonWriteListStart(jw, name);
struct slName *valList = slNameListFromComma(setting);
struct slName *val;
for (val = valList; val != NULL; val = val->next)
    {
    char *s = trimSpaces(val->name);
    if (isEmpty(s))
        continue;
    if (tagsOnly && !isValidTag(s))
        continue;
    jsonWriteString(jw, NULL, s);
    }
slFreeList(&valList);
jsonWriteListEnd(jw);
}

static void jsonWriteTrackTags(struct jsonWrite *jw, struct trackDb *tdb)
/* Write the "tags" list from the tdb's own tags setting. Bad tags are dropped. */
{
char *setting = trackDbLocalSetting(tdb, "tags");
if (setting != NULL)
    jsonWriteCommaList(jw, "tags", setting, TRUE);
}

static int trackFacetDefCmp(const void *va, const void *vb)
/* Compare two trackFacetDefs by priority. */
{
const struct trackFacetDef *a = *((struct trackFacetDef **)va);
const struct trackFacetDef *b = *((struct trackFacetDef **)vb);
if (a->priority < b->priority)
    return -1;
if (a->priority > b->priority)
    return 1;
return 0;
}

static char *tagColorCode(char *color, struct lineFile *lf)
/* Return color, a #rrggbb code or an HTML color name, as a #rrggbb code. */
{
unsigned value;
if (!htmlColorForCode(color, &value) && !htmlColorForName(color, &value))
    errAbort("bad color '%s' in stanza ending line %d of %s", color, lf->lineIx, lf->fileName);
return htmlColorToCode(value);
}

static void readTrackFacetsRa(struct trackFacetDef **retFacets, struct hash **retTagColors)
/* Read the facet and tag definitions, from /gbdb/hgFixed if there is a copy there, else
 * from the copy installed with the CGIs.  Facets come back sorted by priority, and tag
 * colors as a hash of tag name to #rrggbb code.  Both are empty if there is no file. */
{
struct trackFacetDef *defList = NULL;
struct hash *tagColors = hashNew(0);
char *fileName = TRACK_FACETS_RA;
if (fileExists(TRACK_FACETS_RA_GBDB))
    fileName = TRACK_FACETS_RA_GBDB;
struct lineFile *lf = lineFileMayOpen(fileName, TRUE);
if (lf != NULL)
    {
    struct hash *ra;
    while ((ra = raNextRecord(lf)) != NULL)
        {
        char *tag = hashFindVal(ra, "tag");
        if (hashFindVal(ra, "facet") != NULL)
            {
            struct trackFacetDef *def;
            AllocVar(def);
            def->name = cloneString(hashMustFindVal(ra, "facet"));
            def->label = cloneString(hashMustFindVal(ra, "label"));
            def->description = cloneString(hashFindVal(ra, "description"));
            def->priority = sqlDouble(hashMustFindVal(ra, "priority"));
            slAddHead(&defList, def);
            }
        else if (tag != NULL)
            hashAdd(tagColors, tag, tagColorCode(hashMustFindVal(ra, "color"), lf));
        else
            errAbort("stanza ending line %d of %s is not a facet or a tag",
                     lf->lineIx, lf->fileName);
        hashFree(&ra);
        }
    lineFileClose(&lf);
    }
slSort(&defList, trackFacetDefCmp);
*retFacets = defList;
*retTagColors = tagColors;
}

static void jsonWriteTrackFacets(struct jsonWrite *jw, struct trackDb *tdb,
                                 struct trackFacetDef *defList)
/* Write a "facets" object with a list of values for each facet.<name> setting
 * the tdb has itself. */
{
jsonWriteObjectStart(jw, "facets");
struct trackFacetDef *def;
for (def = defList; def != NULL; def = def->next)
    {
    char settingName[256];
    safef(settingName, sizeof settingName, "facet.%s", def->name);
    char *setting = trackDbLocalSetting(tdb, settingName);
    if (setting != NULL)
        jsonWriteCommaList(jw, def->name, setting, FALSE);
    }
jsonWriteObjectEnd(jw);
}

static void writeRecTrackSetsJson(struct jsonWrite *jw, char *db)
/* Write recommended track sets as a JSON array named "recTrackSets".
 * Each entry has label, userName, sessionName, description. */
{
jsonWriteListStart(jw, "recTrackSets");
if (recTrackSetsEnabled(db))
    {
    struct recTrackSet *rts, *rtsList = loadRecTrackSets(db);
    for (rts = rtsList; rts != NULL; rts = rts->next)
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "label", rts->label);
        jsonWriteString(jw, "userName", rts->userName);
        jsonWriteString(jw, "sessionName", rts->sessionName);
        jsonWriteString(jw, "description", rts->description);
        jsonWriteObjectEnd(jw);
        }
    }
jsonWriteListEnd(jw);
}

static int grpCmpLabel(const void *va, const void *vb)
/* Compare two finderGroups by label. */
{
const struct finderGroup *a = *((struct finderGroup **)va);
const struct finderGroup *b = *((struct finderGroup **)vb);
return strcmp(a->grp->label, b->grp->label);
}

static int tdbRefCmpPriority(const void *va, const void *vb)
/* Compare two slRefs to trackDbs by priority. */
{
const struct slRef *aa = *((struct slRef **)va);
const struct slRef *bb = *((struct slRef **)vb);
return trackDbCmp(&aa->val, &bb->val);
}

static boolean anyTableNamed(struct trackDb *tdbList, char *table)
/* Return TRUE if a track or subtrack in tdbList has this table. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (sameOk(tdb->table, table) || anyTableNamed(tdb->subtracks, table))
        return TRUE;
    }
return FALSE;
}

static boolean dropGcOnFly(struct trackDb *tdbList)
/* Return TRUE if hgTracks leaves out the gcOnFly track: it is turned off in hg.conf,
 * or the assembly has a gc5Base track. */
{
return !cfgOptionBooleanDefault("gcOnTheFly", TRUE) ||
    anyTableNamed(tdbList, "gc5Base") || anyTableNamed(tdbList, "gc5BaseBw");
}

static struct finderGroup *groupTracks(struct trackDb *tdbList, struct grp *grpList,
                                       struct hash *superChildren)
/* Put the tracks into their groups, and add each supertrack to the group of its first
 * child.  Fill superChildren with supertrack name to slRef list of its children.
 * Return the groups that have tracks, sorted by label. */
{
struct hash *groupHash = hashNew(0);
struct finderGroup *fgList = NULL, *fg;
struct grp *grp;
for (grp = grpList; grp != NULL; grp = grp->next)
    {
    AllocVar(fg);
    fg->grp = grp;
    hashAdd(groupHash, grp->name, fg);
    slAddHead(&fgList, fg);
    }
struct hash *supersAdded = hashNew(0);
boolean noGcOnFly = dropGcOnFly(tdbList);
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    fg = hashFindVal(groupHash, tdb->grp);
    if (fg == NULL)
        continue;
    if (noGcOnFly && sameOk(trackHubSkipHubName(tdb->table), GC_ON_FLY_TRACK_NAME))
        continue;
    if (tdbIsSuperTrack(tdb))
        {
        if (hashLookup(supersAdded, tdb->track))
            continue;
        hashAdd(supersAdded, tdb->track, NULL);
        }
    refAdd(&fg->tdbList, tdb);
    if (tdbIsSuperTrackChild(tdb) && tdb->parent != NULL)
        {
        struct trackDb *super = tdb->parent;
        struct hashEl *hel = hashLookup(superChildren, super->track);
        if (hel == NULL)
            hel = hashAdd(superChildren, super->track, NULL);
        refAdd((struct slRef **)&hel->val, tdb);
        if (!hashLookup(supersAdded, super->track))
            {
            hashAdd(supersAdded, super->track, NULL);
            refAdd(&fg->tdbList, super);
            }
        }
    }
hashFree(&supersAdded);
hashFree(&groupHash);

struct finderGroup *withTracks = NULL, *next;
for (fg = fgList; fg != NULL; fg = next)
    {
    next = fg->next;
    if (fg->tdbList != NULL)
        {
        slSort(&fg->tdbList, tdbRefCmpPriority);
        slAddHead(&withTracks, fg);
        }
    }
slSort(&withTracks, grpCmpLabel);
return withTracks;
}

static enum trackVisibility containerVis(struct cart *cart, struct trackDb *tdb,
                                         struct slRef *superChildList,
                                         enum trackVisibility vis)
/* Return hide for a visible container with no visible children, else vis.  For a
 * composite the children are its leaf subtracks, under its views if it has any. */
{
if (vis == tvHide || !(tdbIsSuperTrack(tdb) || tdbIsComposite(tdb)))
    return vis;
if (tdbIsSuperTrack(tdb))
    {
    struct slRef *ref;
    for (ref = superChildList; ref != NULL; ref = ref->next)
        if (tdbVisLimitedByAncestors(cart, ref->val, TRUE, FALSE) != tvHide)
            return vis;
    }
else
    {
    struct slRef *ref, *leafList = trackDbListGetRefsToDescendantLeaves(tdb->subtracks);
    boolean anyOn = FALSE;
    for (ref = leafList; ref != NULL && !anyOn; ref = ref->next)
        anyOn = (tdbVisLimitedByAncestors(cart, ref->val, TRUE, FALSE) != tvHide);
    slFreeList(&leafList);
    if (anyOn)
        return vis;
    }
return tvHide;
}

static void jsonWriteTrackMetadata(struct jsonWrite *jw, struct trackDb *tdb)
/* Write the "metadata" object for a track.  If its metadata cannot be read, for
 * example from a dead metaTab url, write it empty so the other tracks still load. */
{
struct slPair *pairs = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    pairs = trackDbMetaPairs(tdb);
errCatchEnd(errCatch);
if (errCatch->gotError)
    pairs = NULL;
errCatchFree(&errCatch);
jsonWriteObjectStart(jw, "metadata");
struct slPair *pair;
for (pair = pairs; pair != NULL; pair = pair->next)
    jsonWriteString(jw, pair->name, (char *)pair->val);
jsonWriteObjectEnd(jw);
}

static void jsonWriteFinderTrack(struct jsonWrite *jw, struct cart *cart, char *db,
                                 boolean viewing, struct trackDb *tdb, struct grp *grp,
                                 struct trackFacetDef *facetDefs, struct hash *superChildren)
/* Write one track object.  Visibility is only written for the cart's database. */
{
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "track", tdb->track);
jsonWriteString(jw, "shortLabel", tdb->shortLabel);
jsonWriteString(jw, "longLabel", tdb->longLabel);
jsonWriteString(jw, "group", grp->name);
jsonWriteString(jw, "groupLabel", grp->label);

char *trackType = cloneFirstWord(tdb->type);
jsonWriteString(jw, "type", trackType);
freeMem(trackType);

boolean isComposite = tdbIsComposite(tdb);
jsonWriteBoolean(jw, "isComposite", isComposite);
boolean isSuperTrack = tdbIsSuperTrack(tdb);
jsonWriteBoolean(jw, "isSuperTrack", isSuperTrack);
if ((tdbIsSuperTrackChild(tdb) || tdbIsCompositeChild(tdb)) && tdb->parent)
    jsonWriteString(jw, "parentTrack", tdb->parent->track);

jsonWriteTrackTags(jw, tdb);
jsonWriteTrackFacets(jw, tdb, facetDefs);

struct slRef *superChildList = NULL;
if (isSuperTrack)
    superChildList = hashFindVal(superChildren, tdb->track);
int subCount = 0;
if (isComposite)
    subCount = trackDbCountDescendantLeaves(tdb);
else if (isSuperTrack)
    subCount = slCount(superChildList);
jsonWriteNumber(jw, "subtrackCount", subCount);

if (viewing)
    {
    // Track visibility limited by ancestors (what's actually showing on screen)
    enum trackVisibility vis = tdbVisLimitedByAncestors(cart, tdb, TRUE, TRUE);
    // Track's own visibility ignoring supertrack folder state
    enum trackVisibility ownVis = tdbVisLimitedByAncestors(cart, tdb, TRUE, FALSE);
    vis = containerVis(cart, tdb, superChildList, vis);
    jsonWriteString(jw, "visibility", hStringFromTv(vis));
    jsonWriteString(jw, "ownVisibility", hStringFromTv(ownVis));
    }

jsonWriteTrackMetadata(jw, tdb);

jsonWriteObjectEnd(jw);
}

static void jsonWriteRecentTracks(struct jsonWrite *jw, struct cart *cart)
/* Write the recently used tracks from the cart. */
{
jsonWriteListStart(jw, "recentTracks");
char *recentStr = cartOptionalString(cart, "hgt.recentTracks");
if (recentStr)
    {
    struct slName *recentList = slNameListFromComma(recentStr);
    struct slName *n;
    for (n = recentList; n != NULL; n = n->next)
        jsonWriteString(jw, NULL, n->name);
    slNameFreeList(&recentList);
    }
jsonWriteListEnd(jw);
}

static void jsonWriteRelatedTracks(struct jsonWrite *jw, struct cart *cart, char *db,
                                   struct finderGroup *fgList)
/* Write the tracks from the relatedTrack table that are related to a visible track
 * and are not visible themselves.  Only tracks in the finder's list are written. */
{
jsonWriteListStart(jw, "relatedTracks");
if (!trackHubDatabase(db))
    {
    char *relTable = cfgOptionDefault("db.relatedTrack", "relatedTrack");
    struct sqlConnection *conn = hAllocConn(db);
    if (sqlTableExists(conn, relTable))
        {
        struct hash *tdbByName = hashNew(0);
        struct hash *visibleTracks = hashNew(6);
        struct hash *suggested = hashNew(6);
        struct finderGroup *fg;
        for (fg = fgList; fg != NULL; fg = fg->next)
            {
            struct slRef *ref;
            for (ref = fg->tdbList; ref != NULL; ref = ref->next)
                {
                struct trackDb *tdb = ref->val;
                hashAdd(tdbByName, tdb->track, tdb);
                if (tdbVisLimitedByAncestors(cart, tdb, TRUE, TRUE) != tvHide)
                    hashStore(visibleTracks, tdb->track);
                }
            }
        // Query related tracks for all visible tracks at once
        struct hashEl *hel, *helList = hashElListHash(visibleTracks);
        if (helList != NULL)
            {
            struct dyString *query = sqlDyStringCreate(
                "select track1, track2, why from %s where track1 in (", relTable);
            for (hel = helList; hel != NULL; hel = hel->next)
                {
                if (hel != helList)
                    sqlDyStringPrintf(query, ",");
                sqlDyStringPrintf(query, "'%s'", hel->name);
                }
            sqlDyStringPrintf(query, ")");
            struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
            char **row;
            while ((row = sqlNextRow(sr)) != NULL)
                {
                char *relatedTo = row[0];
                char *relTrack = row[1];
                char *why = row[2];
                struct trackDb *relTdb = hashFindVal(tdbByName, relTrack);
                if (relTdb == NULL || hashLookup(visibleTracks, relTrack)
                    || hashLookup(suggested, relTrack))
                    continue;
                hashStore(suggested, relTrack);
                jsonWriteObjectStart(jw, NULL);
                jsonWriteString(jw, "track", relTrack);
                jsonWriteString(jw, "shortLabel", relTdb->shortLabel);
                jsonWriteString(jw, "why", why);
                jsonWriteString(jw, "relatedTo", relatedTo);
                jsonWriteObjectEnd(jw);
                }
            sqlFreeResult(&sr);
            dyStringFree(&query);
            }
        hashElFreeList(&helList);
        hashFree(&visibleTracks);
        hashFree(&suggested);
        hashFree(&tdbByName);
        }
    hFreeConn(&conn);
    }
jsonWriteListEnd(jw);
}

static void getTrackFinderData(struct cartJson *cj, struct hash *paramHash)
/* Write the "trackFinderData" object for the db param, or the cart's db if there is no
 * db param.  Visibility, recent and related tracks, track sets and position are only
 * written for the cart's db. */
{
struct cart *cart = cj->cart;
struct jsonWrite *jw = cj->jw;
char *cartDb = cartString(cart, "db");
char *db = cartJsonOptionalParam(paramHash, "db");
if (isEmpty(db))
    db = cartDb;
boolean viewing = sameString(db, cartDb);
if (!viewing && !hDbIsActive(db))
    errAbort("Unknown database: %s", db);

struct trackDb *tdbList = NULL;
struct grp *grpList = NULL;
cartTrackDbInitForApi(cart, db, &tdbList, &grpList, FALSE);
struct hash *superChildren = hashNew(0);
struct finderGroup *fgList = groupTracks(tdbList, grpList, superChildren);

jsonWriteObjectStart(jw, "trackFinderData");
jsonWriteString(jw, "db", db);
jsonWriteBoolean(jw, "viewing", viewing);

jsonWriteListStart(jw, "groups");
struct finderGroup *fg;
for (fg = fgList; fg != NULL; fg = fg->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "name", fg->grp->name);
    jsonWriteString(jw, "label", fg->grp->label);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);

// Facet definitions in display order, and the tag colors
struct trackFacetDef *facetDefs, *def;
struct hash *tagColors;
readTrackFacetsRa(&facetDefs, &tagColors);
jsonWriteListStart(jw, "facetDefs");
for (def = facetDefs; def != NULL; def = def->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "name", def->name);
    jsonWriteString(jw, "label", def->label);
    if (def->description != NULL)
        jsonWriteString(jw, "description", def->description);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
jsonWriteObjectStart(jw, "tagColors");
struct hashEl *tagEl;
struct hashCookie tagCookie = hashFirst(tagColors);
while ((tagEl = hashNext(&tagCookie)) != NULL)
    jsonWriteString(jw, tagEl->name, (char *)tagEl->val);
jsonWriteObjectEnd(jw);

jsonWriteListStart(jw, "tracks");
for (fg = fgList; fg != NULL; fg = fg->next)
    {
    struct slRef *ref;
    for (ref = fg->tdbList; ref != NULL; ref = ref->next)
        jsonWriteFinderTrack(jw, cart, db, viewing, ref->val, fg->grp, facetDefs,
                             superChildren);
    }
jsonWriteListEnd(jw);

if (viewing)
    {
    writeRecTrackSetsJson(jw, db);
    jsonWriteRecentTracks(jw, cart);
    jsonWriteRelatedTracks(jw, cart, db, fgList);
    // Current position, for building rtsLoad links
    char *pos = cartOptionalString(cart, "position");
    if (pos)
        jsonWriteString(jw, "position", pos);
    }
jsonWriteObjectEnd(jw);
}

static void printJsonError(char *message)
/* Print the Content-Type and a JSON object with an "error" message. */
{
cgiPrintContentType("text/javascript");
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "error", message);
jsonWriteObjectEnd(jw);
puts(jw->dy->string);
jsonWriteFree(&jw);
}

void trackFinderCartJson()
/* Answer the Find Tracks dialog's cartJson request.  The cart is read but never saved. */
{
struct cart *cart = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    cart = cartForSession(hUserCookie(), NULL, NULL);
errCatchEnd(errCatch);
if (errCatch->gotError)
    printJsonError(errCatch->message->string);
else
    {
    struct cartJson *cj = cartJsonNew(cart);
    cartJsonRegisterHandler(cj, "getTrackFinderData", getTrackFinderData);
    cartJsonExecute(cj);
    }
errCatchFree(&errCatch);
}
