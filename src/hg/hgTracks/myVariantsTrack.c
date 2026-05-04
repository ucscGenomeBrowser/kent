/* myVariantsTrack.c - supports tracks of type myVariants.  Users can drag to create an item
 * and click to edit one. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bed.h"
#include "binRange.h"
#include "myVariants.h"
#include "myVariantsShare.h"
#include "jsonParse.h"
#include "sqlNum.h"
#include "customFactory.h"
#include "hgConfig.h"
#include "htmlColor.h"
#include "wikiLink.h"
#include "hgFind.h"
#include "jsonWrite.h"

static char *reservedFieldNames[] = {
    "bin", "chrom", "chromStart", "chromEnd", "name", "score", "strand",
    "thickStart", "thickEnd", "itemRgb", "description", "db", "ref", "alt",
    "project", "mouseover", "id", NULL
};

static boolean isValidFieldName(char *name)
/* Validate that name matches [a-zA-Z_][a-zA-Z0-9_]*, is not a reserved column name,
 * and does not start with _hidden_ */
{
if (isEmpty(name))
    return FALSE;
if (startsWith("_hidden_", name))
    return FALSE;
/* Check first char is letter or underscore */
if (!isalpha(name[0]) && name[0] != '_')
    return FALSE;
/* Check remaining chars are alphanumeric or underscore */
int i;
for (i = 1; name[i] != '\0'; i++)
    {
    if (!isalnum(name[i]) && name[i] != '_')
        return FALSE;
    }
/* Check against reserved names */
int j;
for (j = 0; reservedFieldNames[j] != NULL; j++)
    {
    if (sameString(name, reservedFieldNames[j]))
        return FALSE;
    }
return TRUE;
}

void myVariantsJsCommand(char *command, struct track *trackList, struct hash *trackHash)
/* Execute some command sent to us from the javaScript.  All we know for sure is that
 * the first word of the command is "myVariants."  We expect it to be of format:
 *    myVariants <trackName> <jsonData>
 * where jsonData is a JSON string containing the item details */
{
if (!cfgOptionBooleanDefault("doMyVariants", FALSE))
    return;
char *userName = getUserName();
if (userName == NULL)
    return;

/* Parse out command into local variables. */
char *words[3];
char *dupeCommand = cloneString(command);	/* For parsing. */
int wordCount = chopByWhiteRespectDoubleQuotes(dupeCommand, words, ArraySize(words));
if (wordCount != 3)
   errAbort("Expecting %d words in jsCommand '%s'", wordCount, command);
char *jsonData = words[2];

/* Parse JSON data */
struct jsonElement *json = jsonParse(jsonData);
if (json == NULL)
    errAbort("Invalid JSON data in command: %s", jsonData);

/* Handle hideField command: rename column with _hidden_ prefix */
char *hideFieldName = jsonOptionalStringField(json, "hideField", NULL);
if (isNotEmpty(hideFieldName))
    {
    if (isValidFieldName(hideFieldName))
        {
        char *tableName = myVariantsTableExists(userName);
        if (tableName)
            {
            struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
            if (sqlFieldIndex(conn, tableName, hideFieldName) >= 0)
                {
                struct dyString *alterSql = sqlDyStringCreate(
                    "ALTER TABLE %s CHANGE COLUMN %s _hidden_%s varchar(255) DEFAULT NULL",
                    tableName, hideFieldName, hideFieldName);
                sqlUpdate(conn, dyStringContents(alterSql));
                dyStringFree(&alterSql);
                }
            hFreeConn(&conn);
            }
        }
    freez(&dupeCommand);
    return;
    }

/* Create a new item based on command. */
struct myVariants *item;
AllocVar(item);

/* Extract values from JSON */
struct jsonElement *hgvsJson = jsonFindNamedField(json, "jsonData", "hgvsInput");
if (hgvsJson)
    {
    char *hgvs = jsonStringVal(hgvsJson, "hgvsInput");
    if (hgvs)
        {
        struct hgPositions *hgp = NULL;
        AllocVar(hgp);
        hgp->query = cloneString(hgvs);
        hgp->database = database;
        hgp->useAlias = FALSE;
        if (matchesHgvs(cart, database, hgvs, hgp, measureTiming))
            {
            fixSinglePos(hgp);
            if (hgp->singlePos)
                {
                // fill out an item AND redirect to that location somehow?
                item->chrom = hgp->singlePos->chrom;
                item->chromStart = item->thickStart = hgp->singlePos->chromStart;
                item->chromEnd = item->thickEnd = hgp->singlePos->chromEnd;
                item->strand[0] = '.';
                item->score = 0;
                item->bin = binFromRange(item->chromStart, item->chromEnd);
                item->name = hgp->singlePos->name;
                item->ref = cloneString("");
                item->alt = cloneString("");
                item->db = database;
                item->description = cloneString(hgp->singlePos->description);
                item->project = cloneString("");
                item->mouseover = cloneString("");
                }
            }
        else
            {
            /* HGVS didn't match - try hgFind for position/gene resolution */
            struct hgPositions *hgpFind = hgPositionsFind(database, hgvs,
                "", "myVariants", cart, FALSE, FALSE, NULL);
            if (hgpFind != NULL && hgpFind->posCount == 1)
                {
                struct hgPos *pos = hgpFind->singlePos;
                item->chrom = cloneString(pos->chrom);
                item->chromStart = item->thickStart = pos->chromStart;
                item->chromEnd = item->thickEnd = pos->chromEnd;
                item->strand[0] = '.';
                item->score = 0;
                item->bin = binFromRange(item->chromStart, item->chromEnd);
                item->name = cloneString(pos->name ? pos->name : hgvs);
                item->ref = cloneString("");
                item->alt = cloneString("");
                item->db = database;
                item->description = cloneString(pos->description ? pos->description : "");
                item->project = cloneString("");
                item->mouseover = cloneString("");
                }
            else if (hgpFind != NULL && hgpFind->posCount > 1)
                {
                warn("Position '%s' matches %d locations - please be more specific", hgvs, hgpFind->posCount);
                return;
                }
            else
                {
                warn("Position '%s' not found", hgvs);
                return;
                }
            }
        }
    }
else
    {
    char *chrom = jsonStringVal(jsonFindNamedField(json, "jsonData", "chrom"), "chrom");
    int chromStart = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "start"), "start"));
    int chromEnd = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "end"), "end"));
    char *name = jsonStringVal(jsonFindNamedField(json, "jsonData", "name"), "name");
    int score = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "score"), "score"));
    char *strand = jsonStringVal(jsonFindNamedField(json, "jsonData", "strand"), "strand");
    int thickStart = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "thickStart"), "thickStart"));
    int thickEnd = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "thickEnd"), "thickEnd"));
    char *colorCode = jsonStringVal(jsonFindNamedField(json, "jsonData", "color"), "color");
    char *description = jsonStringVal(jsonFindNamedField(json, "jsonData", "description"), "description");
    char *ref = jsonOptionalStringField(json, "ref", "");
    char *alt = jsonOptionalStringField(json, "alt", "");
    char *project = jsonOptionalStringField(json, "project", "");
    char *mouseover = jsonOptionalStringField(json, "mouseover", "");
    unsigned color;
    htmlColorForCode(colorCode, &color);

    item->bin = binFromRange(chromStart, chromEnd);
    item->chrom = cloneString(chrom);
    item->chromStart = chromStart;
    item->chromEnd = chromEnd;
    item->thickStart = thickStart;
    item->thickEnd = thickEnd;
    item->name = cloneString(name);
    item->score = score;
    item->strand[0] = strand[0];
    item->itemRgb = color;
    item->description = cloneString(description);
    item->ref = cloneString(ref);
    item->alt = cloneString(alt);
    item->db = database;
    item->project = cloneString(project);
    item->mouseover = cloneString(mouseover);
    }

if (!item)
    return;

/* Parse custom fields from JSON payload and ALTER TABLE as needed */
char *tableName = myVariantsCreateTable(userName);
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);

struct jsonElement *extraFieldsJson = jsonFindNamedField(json, "jsonData", "extraFields");
if (extraFieldsJson && extraFieldsJson->type == jsonList)
    {
    struct slRef *el;
    for (el = extraFieldsJson->val.jeList; el != NULL; el = el->next)
        {
        struct jsonElement *cfObj = el->val;
        char *cfName = jsonOptionalStringField(cfObj, "name", NULL);
        char *cfValue = jsonOptionalStringField(cfObj, "value", "");
        if (isEmpty(cfName) || !isValidFieldName(cfName))
            continue;
        /* Add column if it doesn't already exist; un-hide if hidden */
        if (sqlFieldIndex(conn, tableName, cfName) < 0)
            {
            char hiddenName[256];
            safef(hiddenName, sizeof(hiddenName), "_hidden_%s", cfName);
            if (sqlFieldIndex(conn, tableName, hiddenName) >= 0)
                {
                /* Un-hide: rename _hidden_X back to X */
                struct dyString *alterSql = sqlDyStringCreate(
                    "ALTER TABLE %s CHANGE COLUMN %s %s varchar(255) DEFAULT NULL",
                    tableName, hiddenName, cfName);
                sqlUpdate(conn, dyStringContents(alterSql));
                dyStringFree(&alterSql);
                }
            else
                {
                struct dyString *alterSql = sqlDyStringCreate(
                    "ALTER TABLE %s ADD COLUMN %s varchar(255) DEFAULT NULL", tableName, cfName);
                sqlUpdate(conn, dyStringContents(alterSql));
                dyStringFree(&alterSql);
                }
            }
        slPairAdd(&item->customFields, cfName, cloneString(cfValue));
        }
    slReverse(&item->customFields);
    }

/* Add item to database. */
myVariantsSaveToDb(conn, item, tableName, 0);

/* Trigger CT import by writing a concrete CT file to trash and pointing ctfile_<db> to it */
char *ctFile = myVariantsWriteCtFile(userName, database, cart);
if (isNotEmpty(ctFile))
    {
    char varName[256];
    safef(varName, sizeof varName, CT_FILE_VAR_PREFIX "%s", database);
    cartSetString(cart, varName, ctFile);
    freeMem(ctFile);
    }

/* Output new item coordinates for JavaScript to use for navigation */
jsInlineF("// START newItemPos\n");
jsInlineF("var newItemPos = {\"chrom\": \"%s\", \"start\": %d, \"end\": %d};\n",
          item->chrom, item->chromStart, item->chromEnd);
jsInlineF("// END newItemPos\n");

hFreeConn(&conn);

freez(&dupeCommand);
}

static int myVariantsExtraHeight(struct track *track)
/* Return extra height of track. */
{
return tl.fontHeight+2;
}

static void updateTextField(char *trackName, struct sqlConnection *conn,
	char *tableName, char *fieldName, int id)
/* Update text valued field with new val. */
{
char varName[128];
safef(varName, sizeof(varName), "%s_%s", trackName, fieldName);
char *newVal = cartOptionalString(cart, varName);
if (newVal == NULL)
    return;
if (endsWith(varName, "itemRgb"))
    {
    unsigned color;
    if (htmlColorForCode(newVal, &color))
        {
        char sql[256];
        sqlSafef(sql, sizeof(sql), "update %s set %s='%d' where id=%d",
            tableName, fieldName, color, id);
        sqlUpdate(conn, sql);
        cartRemove(cart, varName);
        }
    return;
    }
struct dyString *sql = sqlDyStringCreate("update %s set %s='%s' where id=%d",
    tableName, fieldName, newVal, id);
sqlUpdate(conn, sql->string);
dyStringFree(&sql);
cartRemove(cart, varName);
}

static void myVariantsEditOrDelete(char *trackName, struct sqlConnection *conn, char *tableName)
/* Troll through cart variables looking for things that indicate user edited item
 * or deleted it in hgc,  and carry out edits. See hgc/myVariantsClick.c. */
{
char varName[128];
char sql[256];
safef(varName, sizeof(varName), "%s_%s", trackName, "id");
char *idString = cartOptionalString(cart, varName);
if (idString != NULL)
    {
    int id = sqlUnsigned(idString);
    idString = NULL; // Will be no good after cartRemove
    cartRemove(cart, varName); // Remove so only do edits once.

    /* Handle cancel. */
    safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
    if (cartVarExists(cart, varName))
        {
        cartRemove(cart, varName); // Only want to do cancels once
        return;
        }

    /* Handle delete. */
    safef(varName, sizeof(varName), "%s_%s", trackName, "delete");
    if (cartVarExists(cart, varName))
        {
        cartRemove(cart, varName);	// Especially only want to do deletes once!
        sqlSafef(sql, sizeof(sql), "delete from %s where id=%d", tableName, id);
        sqlUpdate(conn, sql);
        /* Trigger CT refresh */
        char *userName = getUserName();
        if (userName)
            {
            char *ctFile = myVariantsWriteCtFile(userName, database, cart);
            if (isNotEmpty(ctFile))
                {
                char ctVar[256];
                safef(ctVar, sizeof ctVar, CT_FILE_VAR_PREFIX "%s", database);
                cartSetString(cart, ctVar, ctFile);
                freeMem(ctFile);
                }
            }
        return;
        }

    /* Handle edits. */
    updateTextField(trackName, conn, tableName, "name", id);
    updateTextField(trackName, conn, tableName, "description", id);
    updateTextField(trackName, conn, tableName, "itemRgb", id);
    updateTextField(trackName, conn, tableName, "chromStart", id);
    updateTextField(trackName, conn, tableName, "chromEnd", id);
    updateTextField(trackName, conn, tableName, "ref", id);
    updateTextField(trackName, conn, tableName, "alt", id);
    updateTextField(trackName, conn, tableName, "project", id);
    updateTextField(trackName, conn, tableName, "mouseover", id);
    /* Update any custom fields */
        {
        char *editUserName = getUserName();
        struct slName *customCols = myVariantsGetCustomFields(editUserName);
        struct slName *col;
        for (col = customCols; col != NULL; col = col->next)
            updateTextField(trackName, conn, tableName, col->name, id);
        slFreeList(&customCols);
        }
    /* Trigger CT refresh after edits */
    {
        char *userName = getUserName();
        if (userName)
            {
            char *ctFile = myVariantsWriteCtFile(userName, database, cart);
            if (isNotEmpty(ctFile))
                {
                char ctVar[256];
                safef(ctVar, sizeof ctVar, CT_FILE_VAR_PREFIX "%s", database);
                cartSetString(cart, ctVar, ctFile);
                freeMem(ctFile);
                }
            }
    }
    }
}

static struct linkedFeatures *loadMyVariantsItems(struct sqlConnection *conn,
    char *tableName, struct trackDb *tdb, char *whereExtra)
/* Query tableName for myVariants items in the current window matching the
 * whereExtra clause (e.g. "db='hg38' and project='test'") and return them
 * as a linkedFeatures list. */
{
struct linkedFeatures *lf, *lfList = NULL;
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, tableName, chromName, winStart, winEnd,
    whereExtra, &rowOffset);
char **row;
struct dyString *mouseover = dyStringNew(0);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct myVariants *item = myVariantsLoad(row);
    struct bed *bed;
    AllocVar(bed);
    char buf[64];
    safef(buf, sizeof(buf), "%u %s", item->id, item->name);
    bed->chrom = item->chrom;
    bed->chromStart = item->chromStart;
    bed->chromEnd = item->chromEnd;
    bed->name = cloneString(buf);
    bed->score = item->score;
    bed->strand[0] = item->strand[0];
    bed->thickStart = item->thickStart;
    bed->thickEnd = item->thickEnd;
    bed->itemRgb = item->itemRgb;
    lf = bedMungToLinkedFeatures(&bed, tdb, 9, 0, 1000, TRUE);
    dyStringClear(mouseover);
    if (item->mouseover && isNotEmpty(item->mouseover))
        lf->mouseOver = cloneString(item->mouseover);
    else
        {
        dyStringPrintf(mouseover, "%s", buf);
        if (item->ref != NULL && isNotEmpty(item->ref))
            dyStringPrintf(mouseover, "<br>Ref: %s", item->ref);
        if (item->alt != NULL && isNotEmpty(item->alt))
            dyStringPrintf(mouseover, "<br>Alt: %s", item->alt);
        lf->mouseOver = cloneString(dyStringContents(mouseover));
        }
    slAddHead(&lfList, lf);
    }
sqlFreeResult(&sr);
dyStringFree(&mouseover);
return lfList;
}

void myVariantsLoadItems(struct track *track)
/* Load up items in track already.  Also make up a pseudo-item that is
 * where you drag to create an item. */
{
struct linkedFeatures *lfList = NULL;
boolean isShared = startsWith("myVariants_shared_", track->track);

if (isShared)
    {
    /* Extract the token and look up the share info from the cart */
    char *token = track->track + strlen("myVariants_shared_");
    char cartVar[256];
    safef(cartVar, sizeof(cartVar), MYVAR_SHARED_CART_PREFIX "%s", token);
    char *cartVal = cartOptionalString(cart, cartVar);
    if (isEmpty(cartVal))
        return;
    char *owner = NULL, *project = NULL, *db = NULL;
    int permission = 0;
    if (!myVariantsParseShareCartValue(cartVal, &owner, &project, &db, &permission, NULL))
        return;
    char *tableName = myVariantsGetDbTable(owner);
    if (isEmpty(tableName))
        {
        freeMem(owner);
        freeMem(project);
        freeMem(db);
        return;
        }
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct dyString *whereClause = sqlDyStringCreate("db='%s'", database);
    if (!sameString(project, "*"))
        sqlDyStringPrintf(whereClause, " and project='%s'", project);
    lfList = loadMyVariantsItems(conn, tableName, track->tdb,
        dyStringCannibalize(&whereClause));
    hFreeConn(&conn);
    freeMem(owner);
    freeMem(project);
    freeMem(db);
    }
else
    {
    /* Load user's own items */
    char *userName = getUserName();
    char *db = myVariantsGetDatabaseForUser(userName);
    if (!db)
        return;
    char *tableName = myVariantsGetDbTable(userName);
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    myVariantsEditOrDelete(track->track, conn, tableName);
    struct dyString *whereClause = sqlDyStringCreate("db='%s'", database);
    lfList = loadMyVariantsItems(conn, tableName, track->tdb,
        dyStringCannibalize(&whereClause));
    hFreeConn(&conn);
    }
slReverse(&lfList);
track->items = lfList;
}

char *myVariantsName(struct track *track, void *item)
/* Return name of one of an item to display on left side. */
{
struct linkedFeatures *lf = item;
char *name = lf->name;
return name;
}

void myVariantsDrawLeftLabels(struct track *track, int seqStart, int seqEnd,
    struct hvGfx *hvg, int xOff, int yOff, int width, int height,
    boolean withCenterLabels, MgFont *font,
    Color color, enum trackVisibility vis)
/* Draw left label - just in dense or full mode. Needed to cope with empty space at top of track. */
{
int y = yOff + myVariantsExtraHeight(track);
int fontHeight = mgFontLineHeight(font);
if (withCenterLabels)
    y += mgFontLineHeight(font);
if (vis == tvDense)
    hvGfxTextRight(hvg, xOff, y, width-1, fontHeight, color, font, track->shortLabel);
else if (vis == tvFull)
    {
    struct bed *bed, *bedList = track->items;
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	if (track->itemLabelColor != NULL)
	    color = track->itemLabelColor(track, bed, hvg);
	int itemHeight = track->itemHeight(track, bed);
	hvGfxTextRight(hvg, xOff, y, width - 1,
	    itemHeight, color, font, track->itemName(track, bed));
	y += itemHeight;
	}
    }
/* In pack mode the draw routine takes care of the labels. */
}


int myVariantsTotalHeight(struct track *track, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height
 * they use. */
{
track->height = tgFixedTotalHeightOptionalOverflow(track, vis, tl.fontHeight+1, tl.fontHeight, FALSE) + 
	myVariantsExtraHeight(track);
return track->height;
}

void myVariantsMethods(struct track *track)
/* Set up special methods for myVariants type tracks. */
{
linkedFeaturesMethods(track);
track->totalHeight = myVariantsTotalHeight;
track->drawLeftLabels = myVariantsDrawLeftLabels;
track->loadItems = myVariantsLoadItems;
track->itemName = myVariantsName;
track->nextItemButtonable = TRUE;
}

struct trackDb *myVariantsFakeTdb()
/* Construct a trackDb record for myVariants track. */
{
char *userName = getUserName();
struct trackDb *tdb = customTrackTdbDefault();
char *tableName = myVariantsCreateTable(userName);
if (!tableName)
    errAbort("Error creating myVariants table for user '%s'", userName);
tdb->track = "myVariants";
tdb->table = cloneString(tableName);
tdb->type = "myVariants";
trackDbAddSetting(tdb, "mouseOverField", "description");
return tdb;
}

struct track *myVariantsTg()
/* Make track that will ultimately have the data from:
 * CUSTOM_TRASH.myVariants_userName table */
{
struct trackDb *tdb = myVariantsFakeTdb();
struct track *tg = trackFromTrackDb(tdb);
myVariantsMethods(tg);
return tg;
}

static void apiSuccess(struct jsonWrite *jw)
/* Send 200 JSON response and exit. */
{
printf("Content-Type: application/json\n\n");
puts(jw->dy->string);
jsonWriteFree(&jw);
exit(0);
}

static void apiError(int httpStatus, char *message)
/* Send error JSON response with given HTTP status and exit. */
{
printf("Status: %d\n", httpStatus);
printf("Content-Type: application/json\n\n");
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "error", message);
jsonWriteObjectEnd(jw);
puts(jw->dy->string);
jsonWriteFree(&jw);
exit(0);
}

static void shareToJson(struct jsonWrite *jw, struct myVariantsShare *share)
/* Write a single share record as a JSON object into jw. */
{
jsonWriteObjectStart(jw, NULL);
jsonWriteNumber(jw, "id", share->id);
jsonWriteString(jw, "ownerUser", share->ownerUser);
jsonWriteString(jw, "shareToken", share->shareToken);
jsonWriteString(jw, "project", share->project);
jsonWriteString(jw, "db", share->db);
jsonWriteNumber(jw, "permission", share->permission);
jsonWriteString(jw, "targetUser", share->targetUser);
jsonWriteString(jw, "label", share->label);
jsonWriteString(jw, "createdAt", share->createdAt);
jsonWriteObjectEnd(jw);
}

static boolean isStateChangingShareAction(char *action)
/* Returns TRUE for actions that modify the share table.  Those require an
 * hgsid match to defend against CSRF; read-only actions don't, because the
 * response only goes to the originating cookie-bearing browser. */
{
return sameString(action, "createShare") || sameString(action, "revokeShare");
}

void myVariantsShareApiHandler(char *action)
/* Handle share API requests. Outputs JSON to stdout and calls exit(0).
 * Called from main() before cartHtmlShell. */
{
char *userName = getUserName();
if (userName == NULL)
    apiError(401, "not logged in");

/* CSRF protection for state-changing actions: require the request to carry
 * an hgsid matching the user's session.  An attacker page can fire requests
 * with the user's cookies but cannot read or guess the session id, so this
 * blocks cross-site forgery of share creation/revocation. */
if (isStateChangingShareAction(action))
    {
    char *suppliedHgsid = cgiOptionalString("hgsid");
    char *expectedHgsid = cartSessionId(cart);
    if (isEmpty(suppliedHgsid) || isEmpty(expectedHgsid)
        || !sameString(suppliedHgsid, expectedHgsid))
        apiError(403, "session token missing or invalid");
    }

char *db = cgiOptionalString("db");
if (db == NULL)
    apiError(400, "missing required parameter: db");

struct sqlConnection *conn = hConnectCentral();

if (!sqlTableExists(conn, "myVariantsShares"))
    {
    hDisconnectCentral(&conn);
    apiError(500, "myVariantsShares table not found - contact genome-www@ucsc.edu");
    }

if (sameString(action, "createShare"))
    {
    char *project = cgiOptionalString("project");
    if (project == NULL)
        apiError(400, "missing required parameter: project");
    int permission = cgiOptionalInt("permission", MYVAR_PERM_READONLY);
    if (permission != MYVAR_PERM_READONLY && permission != MYVAR_PERM_READWRITE)
        apiError(400, "permission must be 0 or 1");
    char *targetUser = cgiOptionalString("targetUser");
    char *label = cgiOptionalString("label");
    /* Reject overlong fields before MySQL would (varchar(255)).  Use 200 to
     * leave room for any prefixing/quoting we add downstream. */
    if (strlen(project) > 200
        || (targetUser && strlen(targetUser) > 200)
        || (label && strlen(label) > 200))
        apiError(400, "project, targetUser, and label must each be <= 200 chars");
    /* Project must be "*" (all) or one of the owner's existing project values.
     * Prevents creating misleading share URLs that filter on a non-existent
     * project name. */
    if (!sameString(project, "*"))
        {
        struct slName *projects = myVariantsGetProjects(userName);
        boolean found = slNameInList(projects, project);
        slFreeList(&projects);
        if (!found)
            apiError(400, "project does not exist for current user");
        }
    struct myVariantsShare *share = myVariantsCreateShare(conn, userName,
        project, db, permission, targetUser, label);
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteString(jw, "token", share->shareToken);
    jsonWriteNumber(jw, "id", share->id);
    jsonWriteObjectEnd(jw);
    myVariantsShareFree(&share);
    hDisconnectCentral(&conn);
    apiSuccess(jw);
    }
else if (sameString(action, "getShares"))
    {
    struct myVariantsShare *shares = myVariantsGetSharesForOwner(conn, userName, db);
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteListStart(jw, "shares");
    struct myVariantsShare *share;
    for (share = shares; share != NULL; share = share->next)
        shareToJson(jw, share);
    jsonWriteListEnd(jw);
    jsonWriteObjectEnd(jw);
    myVariantsShareFreeList(&shares);
    hDisconnectCentral(&conn);
    apiSuccess(jw);
    }
else if (sameString(action, "getSharesForMe"))
    {
    struct myVariantsShare *shares = myVariantsGetSharesForUser(conn, userName, db);
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteListStart(jw, "shares");
    struct myVariantsShare *share;
    for (share = shares; share != NULL; share = share->next)
        shareToJson(jw, share);
    jsonWriteListEnd(jw);
    jsonWriteObjectEnd(jw);
    myVariantsShareFreeList(&shares);
    hDisconnectCentral(&conn);
    apiSuccess(jw);
    }
else if (sameString(action, "revokeShare"))
    {
    char *token = cgiOptionalString("shareToken");
    if (token == NULL)
        apiError(400, "missing required parameter: shareToken");
    boolean revoked = myVariantsRevokeShare(conn, token, userName);
    hDisconnectCentral(&conn);
    if (!revoked)
        apiError(404, "share not found");
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteObjectEnd(jw);
    apiSuccess(jw);
    }
else
    {
    hDisconnectCentral(&conn);
    apiError(400, "unknown action");
    }
}

void myVariantsProcessShareParam()
/* Check for myVarShare CGI param, validate the share token, and store
 * the share reference in the cart. Must be called before track loading. */
{
char *token = cgiOptionalString(MYVAR_SHARE_CGI_VAR);
if (isEmpty(token))
    return;

if (strlen(token) != MYVAR_TOKEN_LENGTH)
    {
    notify("A share link in the URL couldn't be read. The shared track hasn't been added, but the rest of the browser works normally.", "myVarShareInvalidToken");
    return;
    }

char *userName = getUserName();

struct sqlConnection *conn = hConnectCentral();
if (!sqlTableExists(conn, "myVariantsShares"))
    {
    hDisconnectCentral(&conn);
    return;
    }

struct myVariantsShare *share = myVariantsGetShareByToken(conn, token);
hDisconnectCentral(&conn);
if (share == NULL)
    {
    notify("A share link is no longer valid (it may have been revoked). The shared track isn't available, but the rest of the browser works normally.", "myVarShareRevoked");
    return;
    }

/* Targeted shares require the matching logged-in user. Shares with no targetUser
 * ("anyone with link") are accessible to anon users too. */
if (isNotEmpty(share->targetUser))
    {
    if (userName == NULL)
        {
        notify("A shared track in this view is limited to a specific user. You can keep browsing.", "myVarShareNeedLogin");
        myVariantsShareFree(&share);
        return;
        }
    if (!sameString(share->targetUser, userName))
        {
        notify("A shared track in this view wasn't shared with your account. You can keep browsing; the track just won't appear.", "myVarShareWrongUser");
        myVariantsShareFree(&share);
        return;
        }
    }

/* Store in cart so the shared track loader can find it during track loading */
char cartVar[256];
safef(cartVar, sizeof(cartVar), MYVAR_SHARED_CART_PREFIX "%s", token);
char *cartVal = myVariantsShareCartValue(share);
cartSetString(cart, cartVar, cartVal);
freeMem(cartVal);
myVariantsShareFree(&share);
}

void myVariantsProcessSharedEdits()
/* Process pending edits for shared tracks before any track loading.
 * This ensures edits are committed to the database before both the
 * owner's track and the shared track query for items. */
{
/* Anon users cannot edit shared items, regardless of share permission. */
char *userName = getUserName();
if (userName == NULL)
    return;
struct hashEl *shareVars = cartFindPrefix(cart, MYVAR_SHARED_CART_PREFIX);
struct hashEl *el;
for (el = shareVars; el != NULL; el = el->next)
    {
    char *token = el->name + strlen(MYVAR_SHARED_CART_PREFIX);
    char trackName[512];
    safef(trackName, sizeof(trackName), "myVariants_shared_%s", token);

    /* Check if there's a pending edit for this shared track */
    char varName[256];
    safef(varName, sizeof(varName), "%s_%s", trackName, "id");
    char *idString = cartOptionalString(cart, varName);
    if (idString == NULL)
        continue;

    /* Re-validate the share against hgcentral on every edit. The cart-cached
     * permission is not authoritative: the owner may have revoked the share
     * or downgraded it from read-write to read-only since the recipient's
     * cart was last set, and we don't want stale cart contents to grant
     * writes that the live share record no longer permits. */
    struct sqlConnection *centralConn = hConnectCentral();
    if (!sqlTableExists(centralConn, "myVariantsShares"))
        {
        hDisconnectCentral(&centralConn);
        continue;
        }
    struct myVariantsShare *liveShare = myVariantsGetShareByToken(centralConn, token);
    hDisconnectCentral(&centralConn);
    if (liveShare == NULL)
        continue;
    if (liveShare->permission != MYVAR_PERM_READWRITE)
        {
        myVariantsShareFree(&liveShare);
        continue;
        }
    if (isNotEmpty(liveShare->targetUser) && !sameString(liveShare->targetUser, userName))
        {
        myVariantsShareFree(&liveShare);
        continue;
        }
    char *owner = cloneString(liveShare->ownerUser);
    myVariantsShareFree(&liveShare);

    char *tableName = myVariantsGetDbTable(owner);
    if (isEmpty(tableName))
        {
        freeMem(owner);
        continue;
        }

    int id = sqlUnsigned(idString);
    cartRemove(cart, varName);

    /* Handle cancel */
    safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
    if (cartVarExists(cart, varName))
        {
        cartRemove(cart, varName);
        freeMem(owner);
        continue;
        }

    /* Apply field edits — no delete, no project change */
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    updateTextField(trackName, conn, tableName, "name", id);
    updateTextField(trackName, conn, tableName, "description", id);
    updateTextField(trackName, conn, tableName, "itemRgb", id);
    updateTextField(trackName, conn, tableName, "chromStart", id);
    updateTextField(trackName, conn, tableName, "chromEnd", id);
    updateTextField(trackName, conn, tableName, "ref", id);
    updateTextField(trackName, conn, tableName, "alt", id);
    updateTextField(trackName, conn, tableName, "mouseover", id);
    struct slName *customCols = myVariantsGetCustomFields(owner);
    struct slName *col;
    for (col = customCols; col != NULL; col = col->next)
        updateTextField(trackName, conn, tableName, col->name, id);
    slFreeList(&customCols);
    hFreeConn(&conn);
    freeMem(owner);
    }
hashElFreeList(&shareVars);
}

boolean myVariantsTrackEnabled()
/* Return TRUE if the "My Variants" feature is enabled in hg.conf
 * and the current request comes from a valid user */
{
return cfgOptionBooleanDefault("doMyVariants", FALSE) && (getUserName() != NULL);
}
