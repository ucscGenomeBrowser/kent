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
#include "hgHgvs.h"
#include "jsonWrite.h"

static char *reservedFieldNames[] = {
    "bin", "chrom", "chromStart", "chromEnd", "name", "score", "strand",
    "thickStart", "thickEnd", "itemRgb", "blockCount", "blockSizes",
    "chromStarts", "description", "db", "ref", "alt", "project",
    "mouseover", "itemType", "cnvType", "id", NULL
};

static char *truncateSeq(char *seq, int maxLen)
/* Return seq cloned and truncated to maxLen with "..." appended if longer.
 * NULL or empty input returns NULL. */
{
if (isEmpty(seq))
    return NULL;
if (strlen(seq) <= (size_t)maxLen)
    return cloneString(seq);
struct dyString *dy = dyStringNew(maxLen + 4);
dyStringAppendN(dy, seq, maxLen);
dyStringAppend(dy, "...");
return dyStringCannibalize(&dy);
}

static char *changeTypePrefix(enum hgvsChangeType t)
/* Map a non-substitution HGVS change type to its short label. */
{
switch (t)
    {
    case hgvsctDel: return "Del";
    case hgvsctDup: return "Dup";
    case hgvsctIns: return "Ins";
    case hgvsctInv: return "Inv";
    case hgvsctCon: return "Con";
    default: return NULL;
    }
}

static char *synthesizeItemName(char *ref, char *alt,
                                enum hgvsChangeType changeType, char *changeSeq)
/* Build a default name for a new myVariants item from its ref/alt or HGVS
 * change.  Returns a cloned string, or NULL when the row's auto-increment
 * id is needed (the SQL layer fills in "Variant N" after the INSERT). */
{
boolean haveRef = !isEmpty(ref);
boolean haveAlt = !isEmpty(alt);
struct dyString *dy = dyStringNew(0);
if (haveRef && haveAlt)
    dyStringPrintf(dy, "%s>%s", ref, alt);
else if (haveRef)
    dyStringPrintf(dy, "Ref: %s", ref);
else if (haveAlt)
    dyStringPrintf(dy, "Alt: %s", alt);
else
    {
    char *prefix = changeTypePrefix(changeType);
    if (prefix != NULL && !isEmpty(changeSeq))
        {
        char *trunc = truncateSeq(changeSeq, 10);
        dyStringPrintf(dy, "%s: %s", prefix, trunc);
        freeMem(trunc);
        }
    }
if (dyStringIsEmpty(dy))
    {
    dyStringFree(&dy);
    return NULL;
    }
return dyStringCannibalize(&dy);
}

static void extractHgvsChange(char *hgvsTerm,
                              char **retRef, char **retAlt,
                              enum hgvsChangeType *retType, char **retSeq)
/* Parse an HGVS term and pull either ref/alt (substitution) or the change
 * type plus its asserted sequence (del/dup/ins/inv/con).  Each out parameter
 * is set to a freshly cloned string or to NULL/hgvsctUndefined when the
 * piece is unavailable. */
{
*retRef = NULL;
*retAlt = NULL;
*retType = hgvsctUndefined;
*retSeq = NULL;
if (isEmpty(hgvsTerm))
    return;
struct hgvsVariant *hgvs = hgvsParseTerm(hgvsTerm);
if (hgvs == NULL || isEmpty(hgvs->changes))
    return;
struct dyString *dyError = dyStringNew(0);
struct hgvsChange *change = hgvsParseNucleotideChange(hgvs->changes, hgvs->type, dyError);
dyStringFree(&dyError);
if (change == NULL)
    return;
if (change->type == hgvsctSubst &&
    change->value.refAlt.altType == hgvsstSimple)
    {
    if (!isEmpty(change->value.refAlt.refSequence))
        *retRef = cloneString(change->value.refAlt.refSequence);
    char *altSeq = change->value.refAlt.altValue.seq;
    if (!isEmpty(altSeq))
        *retAlt = cloneString(altSeq);
    }
else
    {
    *retType = change->type;
    switch (change->type)
        {
        case hgvsctDel:
        case hgvsctDup:
        case hgvsctInv:
            if (!isEmpty(change->value.refAlt.refSequence))
                *retSeq = cloneString(change->value.refAlt.refSequence);
            break;
        case hgvsctIns:
        case hgvsctCon:
            if (change->value.refAlt.altType == hgvsstSimple &&
                !isEmpty(change->value.refAlt.altValue.seq))
                *retSeq = cloneString(change->value.refAlt.altValue.seq);
            break;
        default:
            break;
        }
    }
}

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

static boolean hasTabOrNewline(char *s)
/* TRUE if s contains \t, \n, or \r. */
{
return s != NULL && (strchr(s, '\t') != NULL ||
                     strchr(s, '\n') != NULL ||
                     strchr(s, '\r') != NULL);
}

static void validateItemBlocks(struct myVariants *item)
/* Format item as a 12-column BED row and run it through loadAndValidateBed
 * (isCt=TRUE) for the canonical block checks.  lineFileAbort fires errAbort
 * on failure, the same error path used elsewhere in this file. */
{
/* Reject embedded tab/newline in chrom and name: lineFileNext would
 * truncate the row and chopByChar would leave trailing row[] slots
 * uninitialized for loadAndValidateBed to dereference. */
if (hasTabOrNewline(item->chrom))
    errAbort("chrom contains illegal whitespace");
if (hasTabOrNewline(item->name))
    errAbort("name contains illegal whitespace");

struct dyString *dy = dyStringNew(256);
int i;
dyStringPrintf(dy, "%s\t%u\t%u\t%s\t%u\t%s\t%u\t%u\t%u\t%u\t",
    item->chrom, item->chromStart, item->chromEnd,
    isEmpty(item->name) ? "x" : item->name,
    item->score, item->strand, item->thickStart, item->thickEnd,
    item->itemRgb, item->blockCount);
for (i = 0; i < item->blockCount; i++)
    dyStringPrintf(dy, "%d,", item->blockSizes[i]);
dyStringAppendC(dy, '\t');
for (i = 0; i < item->blockCount; i++)
    dyStringPrintf(dy, "%d,", item->chromStarts[i]);

/* lineFileOnString takes the buffer by pointer, but lineFileClose does
 * not free it (the fd<0 branch skips the free).  Hold the pointer
 * separately and free it after lineFileClose. */
char *buf = dyStringCannibalize(&dy);
struct lineFile *lf = lineFileOnString("myVariantsBlocks", TRUE, buf);
char *row[12];
char *line = NULL;
lineFileNext(lf, &line, NULL);
int got = chopByChar(line, '\t', row, ArraySize(row));
if (got != 12)
    errAbort("validateItemBlocks: chopped %d fields, expected 12", got);
struct bed tmp;
ZeroVar(&tmp);
loadAndValidateBed(row, 12, 12, lf, &tmp, NULL, TRUE);
lineFileClose(&lf);
freeMem(buf);
freeMem(tmp.blockSizes);
freeMem(tmp.chromStarts);
}

static void ensureItemBlocks(struct myVariants *item)
/* If item has no blocks, synthesize a single full-span block. */
{
if (item->blockCount > 0)
    return;
item->blockCount = 1;
AllocArray(item->blockSizes, 1);
AllocArray(item->chromStarts, 1);
item->blockSizes[0] = item->chromEnd - item->chromStart;
item->chromStarts[0] = 0;
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
    {
    warn("You must be logged in to add an annotation.");
    return;
    }

/* This command runs mysql commands so require the hgsids match to prevent CSRF. */
char *suppliedHgsid = cgiOptionalString("hgsid");
char *expectedHgsid = cartSessionId(cart);
if (isEmpty(suppliedHgsid) || isEmpty(expectedHgsid)
    || !sameString(suppliedHgsid, expectedHgsid))
    errAbort("session token missing or invalid");

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
                // matchesHgvs widens the position by HGVS_FIND_PADDING on each
                // side for visual context; the saved variant should be the exact
                // mapped span.
                item->chromStart = item->thickStart =
                    hgp->singlePos->chromStart + HGVS_FIND_PADDING;
                item->chromEnd = item->thickEnd =
                    hgp->singlePos->chromEnd - HGVS_FIND_PADDING;
                item->strand[0] = '.';
                item->score = 0;
                item->bin = binFromRange(item->chromStart, item->chromEnd);
                char *hgvsRef = NULL, *hgvsAlt = NULL, *changeSeq = NULL;
                enum hgvsChangeType changeType = hgvsctUndefined;
                extractHgvsChange(hgvs, &hgvsRef, &hgvsAlt, &changeType, &changeSeq);
                item->ref = hgvsRef ? hgvsRef : cloneString("");
                item->alt = hgvsAlt ? hgvsAlt : cloneString("");
                item->name = synthesizeItemName(item->ref, item->alt, changeType, changeSeq);
                if (item->name == NULL)
                    item->name = cloneString("");
                freeMem(changeSeq);
                item->db = database;
                item->description = cloneString(hgp->singlePos->description);
                item->project = cloneString("");
                item->mouseover = cloneString("");
                item->itemType = cloneString("snv");
                item->cnvType = cloneString("");
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
                item->ref = cloneString("");
                item->alt = cloneString("");
                item->name = cloneString("");
                item->db = database;
                item->description = cloneString(pos->description ? pos->description : "");
                item->project = cloneString("");
                item->mouseover = cloneString("");
                item->itemType = cloneString("snv");
                item->cnvType = cloneString("");
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
    char *itemType = myVariantsCanonicalItemType(
        jsonOptionalStringField(json, "itemType", "snv"));
    if (itemType == NULL)
        errAbort("invalid itemType");
    char *cnvType = "";
    if (sameString(itemType, "cnv"))
        {
        cnvType = myVariantsCanonicalCnvType(jsonOptionalStringField(json, "cnvType", ""));
        if (cnvType == NULL)
            errAbort("invalid cnvType");
        }
    unsigned color;
    htmlColorForCode(colorCode, &color);

    item->bin = binFromRange(chromStart, chromEnd);
    item->chrom = cloneString(chrom);
    item->chromStart = chromStart;
    item->chromEnd = chromEnd;
    /* SNV/CNV items don't carry a thick range or BED12 blocks; force defaults
     * so a stale value from a view toggle in the client can't leak through. */
    if (sameString(itemType, "snv") || sameString(itemType, "cnv"))
        {
        item->thickStart = chromStart;
        item->thickEnd = chromEnd;
        }
    else
        {
        item->thickStart = thickStart;
        item->thickEnd = thickEnd;
        }
    if (isEmpty(name))
        {
        item->name = synthesizeItemName(ref, alt, hgvsctUndefined, NULL);
        if (item->name == NULL)
            item->name = cloneString("");
        }
    else
        item->name = cloneString(name);
    item->score = score;
    item->strand[0] = strand[0];
    item->itemRgb = color;
    item->description = cloneString(description);
    if (sameString(itemType, "transcript"))
        {
        item->ref = cloneString("");
        item->alt = cloneString("");
        }
    else if (sameString(itemType, "cnv"))
        {
        /* CNV stores the inserted/duplicated sequence in alt; ref is unused. */
        item->ref = cloneString("");
        item->alt = cloneString(alt);
        }
    else
        {
        item->ref = cloneString(ref);
        item->alt = cloneString(alt);
        }
    item->db = database;
    item->project = cloneString(project);
    item->mouseover = cloneString(mouseover);
    item->itemType = cloneString(itemType);
    item->cnvType = cloneString(cnvType);
    }

if (!item)
    return;

/* Parse blocks from JSON if present. Empty / missing means single full-span
 * block; ensureItemBlocks synthesizes that below. */
struct jsonElement *blockSizesJson = jsonFindNamedField(json, "jsonData", "blockSizes");
struct jsonElement *chromStartsJson = jsonFindNamedField(json, "jsonData", "chromStarts");
if (blockSizesJson && blockSizesJson->type == jsonList &&
    chromStartsJson && chromStartsJson->type == jsonList)
    {
    int sizeN = slCount(blockSizesJson->val.jeList);
    int startN = slCount(chromStartsJson->val.jeList);
    if (sizeN != startN)
        errAbort("blockSizes and chromStarts lengths differ (%d vs %d)", sizeN, startN);
    if (sizeN > 0)
        {
        item->blockCount = sizeN;
        AllocArray(item->blockSizes, sizeN);
        AllocArray(item->chromStarts, sizeN);
        struct slRef *el;
        int i = 0;
        for (el = blockSizesJson->val.jeList; el != NULL; el = el->next, i++)
            {
            struct jsonElement *child = (struct jsonElement *)el->val;
            if (child->type != jsonNumber)
                errAbort("blockSizes[%d] is not a number", i);
            item->blockSizes[i] = child->val.jeNumber;
            }
        i = 0;
        for (el = chromStartsJson->val.jeList; el != NULL; el = el->next, i++)
            {
            struct jsonElement *child = (struct jsonElement *)el->val;
            if (child->type != jsonNumber)
                errAbort("chromStarts[%d] is not a number", i);
            item->chromStarts[i] = child->val.jeNumber;
            }
        }
    }
ensureItemBlocks(item);
validateItemBlocks(item);

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

/* Refresh the on-disk myVariants ctfile and point mvCtfile_<db> at it. */
char *ctFile = myVariantsWriteCtFile(userName, database, cart);
if (isNotEmpty(ctFile))
    {
    char varName[256];
    safef(varName, sizeof varName, MYVARIANTS_FILE_VAR_PREFIX "%s", database);
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
	char *tableName, char *fieldName, int id, char *scopeProject, char *scopeDb)
/* Update text valued field with new val. If scopeProject/scopeDb are non-NULL,
 * include them in the WHERE clause so a recipient cannot edit rows outside
 * the share's authorized project/db. scopeProject of "*" means all projects. */
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
        struct dyString *sql = sqlDyStringCreate(
            "update %s set %s='%d' where id=%d", tableName, fieldName, color, id);
        if (isNotEmpty(scopeDb))
            sqlDyStringPrintf(sql, " and db='%s'", scopeDb);
        if (isNotEmpty(scopeProject) && !sameString(scopeProject, "*"))
            sqlDyStringPrintf(sql, " and project='%s'", scopeProject);
        sqlUpdate(conn, sql->string);
        dyStringFree(&sql);
        cartRemove(cart, varName);
        }
    return;
    }
if (sameString(fieldName, "cnvType"))
    {
    if (isNotEmpty(newVal) && myVariantsCanonicalCnvType(newVal) == NULL)
        errAbort("invalid cnvType");
    newVal = isEmpty(newVal) ? "" : myVariantsCanonicalCnvType(newVal);
    }
struct dyString *sql = sqlDyStringCreate("update %s set %s='%s' where id=%d",
    tableName, fieldName, newVal, id);
if (isNotEmpty(scopeDb))
    sqlDyStringPrintf(sql, " and db='%s'", scopeDb);
if (isNotEmpty(scopeProject) && !sameString(scopeProject, "*"))
    sqlDyStringPrintf(sql, " and project='%s'", scopeProject);
sqlUpdate(conn, sql->string);
dyStringFree(&sql);
cartRemove(cart, varName);
}

static void updateBlocksFields(char *trackName, struct sqlConnection *conn,
        char *tableName, int id, char *scopeProject, char *scopeDb)
/* Pull <trackName>_blockCount, _blockSizes, _chromStarts from the cart,
 * validate jointly against the row's current chromStart/chromEnd via
 * loadAndValidateBed, and UPDATE all three columns in one statement.
 * No-op if any of the three cart vars are missing. */
{
char vC[128], vS[128], vT[128];
safef(vC, sizeof vC, "%s_blockCount", trackName);
safef(vS, sizeof vS, "%s_blockSizes", trackName);
safef(vT, sizeof vT, "%s_chromStarts", trackName);
char *cartCount = cartOptionalString(cart, vC);
char *cartSizes = cartOptionalString(cart, vS);
char *cartStarts = cartOptionalString(cart, vT);
if (cartCount == NULL || cartSizes == NULL || cartStarts == NULL)
    return;
/* Clone the cart strings before removing the cart vars: if validation
 * errAborts below, the cart vars must not survive to corrupt the next
 * edit on a different row. */
char *blockCountStr = cloneString(cartCount);
char *blockSizesStr = cloneString(cartSizes);
char *chromStartsStr = cloneString(cartStarts);
cartRemove(cart, vC);
cartRemove(cart, vS);
cartRemove(cart, vT);

struct dyString *q = sqlDyStringCreate(
    "select chrom,chromStart,chromEnd,name,score,strand,thickStart,thickEnd,itemRgb"
    " from %s where id=%d", tableName, id);
if (isNotEmpty(scopeDb))
    sqlDyStringPrintf(q, " and db='%s'", scopeDb);
if (isNotEmpty(scopeProject) && !sameString(scopeProject, "*"))
    sqlDyStringPrintf(q, " and project='%s'", scopeProject);
struct sqlResult *sr = sqlGetResult(conn, q->string);
dyStringFree(&q);
char **row = sqlNextRow(sr);
if (row == NULL)
    {
    sqlFreeResult(&sr);
    freeMem(blockCountStr);
    freeMem(blockSizesStr);
    freeMem(chromStartsStr);
    return;
    }
struct myVariants item;
ZeroVar(&item);
item.chrom = cloneString(row[0]);
item.chromStart = sqlUnsigned(row[1]);
item.chromEnd = sqlUnsigned(row[2]);
item.name = cloneString(row[3]);
item.score = sqlUnsigned(row[4]);
safecpy(item.strand, sizeof(item.strand), row[5]);
item.thickStart = sqlUnsigned(row[6]);
item.thickEnd = sqlUnsigned(row[7]);
item.itemRgb = sqlUnsigned(row[8]);
sqlFreeResult(&sr);

item.blockCount = sqlUnsigned(blockCountStr);
char *sizesDup = cloneString(blockSizesStr);
char *startsDup = cloneString(chromStartsStr);
int n;
sqlSignedDynamicArray(sizesDup, &item.blockSizes, &n);
if (n != item.blockCount)
    errAbort("blockSizes count %d != blockCount %u", n, item.blockCount);
sqlSignedDynamicArray(startsDup, &item.chromStarts, &n);
if (n != item.blockCount)
    errAbort("chromStarts count %d != blockCount %u", n, item.blockCount);
freeMem(sizesDup);
freeMem(startsDup);

validateItemBlocks(&item);

struct dyString *upd = sqlDyStringCreate(
    "update %s set blockCount=%u,blockSizes='%s',chromStarts='%s' where id=%d",
    tableName, item.blockCount, blockSizesStr, chromStartsStr, id);
if (isNotEmpty(scopeDb))
    sqlDyStringPrintf(upd, " and db='%s'", scopeDb);
if (isNotEmpty(scopeProject) && !sameString(scopeProject, "*"))
    sqlDyStringPrintf(upd, " and project='%s'", scopeProject);
sqlUpdate(conn, upd->string);
dyStringFree(&upd);

freeMem(item.chrom);
freeMem(item.name);
freeMem(item.blockSizes);
freeMem(item.chromStarts);
freeMem(blockCountStr);
freeMem(blockSizesStr);
freeMem(chromStartsStr);
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
        /* Refresh the on-disk myVariants ctfile after a delete. */
        char *userName = getUserName();
        if (userName)
            {
            char *ctFile = myVariantsWriteCtFile(userName, database, cart);
            if (isNotEmpty(ctFile))
                {
                char ctVar[256];
                safef(ctVar, sizeof ctVar, MYVARIANTS_FILE_VAR_PREFIX "%s", database);
                cartSetString(cart, ctVar, ctFile);
                freeMem(ctFile);
                }
            }
        return;
        }

    /* Handle edits. Owner edits their own table, no project/db scope filter. */
    updateTextField(trackName, conn, tableName, "name", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "description", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "itemRgb", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "chromStart", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "chromEnd", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "thickStart", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "thickEnd", id, NULL, NULL);
    updateBlocksFields(trackName, conn, tableName, id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "ref", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "alt", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "project", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "mouseover", id, NULL, NULL);
    updateTextField(trackName, conn, tableName, "cnvType", id, NULL, NULL);
    /* Update any custom fields */
        {
        char *editUserName = getUserName();
        struct slName *customCols = myVariantsGetCustomFields(editUserName);
        struct slName *col;
        for (col = customCols; col != NULL; col = col->next)
            updateTextField(trackName, conn, tableName, col->name, id, NULL, NULL);
        slFreeList(&customCols);
        }
    /* Refresh the on-disk myVariants ctfile after edits. */
    {
        char *userName = getUserName();
        if (userName)
            {
            char *ctFile = myVariantsWriteCtFile(userName, database, cart);
            if (isNotEmpty(ctFile))
                {
                char ctVar[256];
                safef(ctVar, sizeof ctVar, MYVARIANTS_FILE_VAR_PREFIX "%s", database);
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
struct dyString *query = sqlDyStringCreate("select * from %s where ", tableName);
hAddBinToQueryGeneral("bin", winStart, winEnd, query);
sqlDyStringPrintf(query, " chrom='%s' and chromStart < %d and chromEnd > %d",
        chromName, winEnd, winStart);
if (isNotEmpty(whereExtra))
    sqlDyStringPrintf(query, " and (%-s)", whereExtra);
struct sqlResult *sr = sqlGetResult(conn, query->string);
dyStringFree(&query);
char **row;
struct dyString *mouseover = dyStringNew(0);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct myVariants *item = myVariantsLoad(row);
    struct bed *bed;
    AllocVar(bed);
    /* bed->name is the item identifier passed to hgc, which parses "id name"
     * to look up the row by primary key.  myVariantsName() strips the "id "
     * prefix for the displayed label. */
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
    bed->blockCount = item->blockCount;
    if (item->blockCount > 0)
        {
        bed->blockSizes = cloneMem(item->blockSizes,
                                   item->blockCount * sizeof(int));
        bed->chromStarts = cloneMem(item->chromStarts,
                                    item->blockCount * sizeof(int));
        }
    lf = bedMungToLinkedFeatures(&bed, tdb, 12, 0, 1000, TRUE);
    dyStringClear(mouseover);
    if (item->mouseover && isNotEmpty(item->mouseover))
        lf->mouseOver = cloneString(item->mouseover);
    else
        {
        dyStringPrintf(mouseover, "%s", item->name);
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
boolean isShared = isMyVariantsSharedTrack(track->track);

if (isShared)
    {
    /* Resolve via hgcentral so revoked/downgraded shares stop returning data. */
    struct myVariantsShare *share = myVariantsResolveSharedTrack(track->track, cart);
    if (share == NULL)
        return;
    /* Shared tracks are per-assembly; only load when the browser's current
     * db matches the share's db. */
    if (!sameString(share->db, database))
        {
        myVariantsShareFree(&share);
        return;
        }
    char *tableName = myVariantsGetDbTable(share->ownerUser);
    if (isEmpty(tableName))
        {
        myVariantsShareFree(&share);
        return;
        }
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct dyString *whereClause = sqlDyStringCreate("db='%s'", share->db);
    if (!sameString(share->project, "*"))
        sqlDyStringPrintf(whereClause, " and project='%s'", share->project);
    lfList = loadMyVariantsItems(conn, tableName, track->tdb,
        dyStringCannibalize(&whereClause));
    hFreeConn(&conn);
    myVariantsShareFree(&share);
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
/* Return the display label, stripping the "id " prefix that lf->name carries
 * for hgc's lookup. */
{
struct linkedFeatures *lf = item;
char *name = lf->name;
if (name == NULL)
    return name;
char *space = strchr(name, ' ');
if (space != NULL)
    return space + 1;
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

static char *cleanShareTargets(struct sqlConnection *conn, char *raw)
/* Parse a comma-separated recipient string into a normalized comma-no-space
 * list: trim each name, drop empties, de-dup, and verify each exists in
 * gbMembers. Calls apiError(400,...) (does not return) on an unknown name or
 * an overlong list. Returns a string the caller must freeMem, or NULL for an
 * empty list (anyone with link). */
{
if (isEmpty(raw))
    return NULL;
struct slName *names = slNameListFromComma(raw);
struct slName *name;
struct slName *cleanList = NULL;
for (name = names; name != NULL; name = name->next)
    {
    trimSpaces(name->name);
    if (isNotEmpty(name->name))
        slNameStore(&cleanList, name->name);
    }
slNameFreeList(&names);
if (cleanList == NULL)
    return NULL;
slReverse(&cleanList);  /* slNameStore prepends; restore input order */

struct dyString *dy = dyStringNew(256);
struct slName *c;
for (c = cleanList; c != NULL; c = c->next)
    {
    char gbq[512];
    sqlSafef(gbq, sizeof(gbq),
        "select count(*) from gbMembers where userName = BINARY '%s'", c->name);
    if (sqlQuickNum(conn, gbq) == 0)
        {
        char msg[512];
        safef(msg, sizeof(msg),
            "user '%s' not found (the username is case-sensitive) - check the"
            " spelling, or leave the field blank to make an 'anyone with link' share",
            c->name);
        apiError(400, msg);
        }
    if (dy->stringSize > 0)
        dyStringAppendC(dy, ',');
    dyStringAppend(dy, c->name);
    }
slNameFreeList(&cleanList);
if (dy->stringSize > 500)
    {
    dyStringFree(&dy);
    apiError(400, "too many recipients - the username list is too long");
    }
return dyStringCannibalize(&dy);
}

static boolean isStateChangingShareAction(char *action)
/* Returns TRUE for actions that modify the share table.  Those require an
 * hgsid match to defend against CSRF; read-only actions don't, because the
 * response only goes to the originating cookie-bearing browser. */
{
return sameString(action, "createShare") || sameString(action, "revokeShare")
    || sameString(action, "setSharePermission") || sameString(action, "setShareTargets");
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
    /* Reject overlong fields before MySQL would.  Use 200 to leave room for any
     * prefixing/quoting we add downstream; the recipient list is checked in
     * cleanShareTargets against the wider targetUser column. */
    if (strlen(project) > 200 || (label && strlen(label) > 200))
        apiError(400, "project and label must each be <= 200 chars");
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
    char *cleanTargets = cleanShareTargets(conn, targetUser);
    struct myVariantsShare *share = myVariantsCreateShare(conn, userName,
        project, db, permission, cleanTargets, label);
    freeMem(cleanTargets);
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
else if (sameString(action, "setSharePermission"))
    {
    char *token = cgiOptionalString("shareToken");
    if (token == NULL)
        apiError(400, "missing required parameter: shareToken");
    int permission = cgiOptionalInt("permission", MYVAR_PERM_READONLY);
    if (permission != MYVAR_PERM_READONLY && permission != MYVAR_PERM_READWRITE)
        apiError(400, "permission must be 0 or 1");
    boolean updated = myVariantsSetSharePermission(conn, token, userName, permission);
    hDisconnectCentral(&conn);
    if (!updated)
        apiError(404, "share not found");
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteObjectEnd(jw);
    apiSuccess(jw);
    }
else if (sameString(action, "setShareTargets"))
    {
    char *token = cgiOptionalString("shareToken");
    if (token == NULL)
        apiError(400, "missing required parameter: shareToken");
    char *cleanTargets = cleanShareTargets(conn, cgiOptionalString("targetUser"));
    boolean updated = myVariantsSetShareTargets(conn, token, userName, cleanTargets);
    freeMem(cleanTargets);
    hDisconnectCentral(&conn);
    if (!updated)
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

/* Targeted shares require a logged-in user in the share's recipient list.
 * Shares with no targetUser ("anyone with link") are accessible to anon users too. */
if (isNotEmpty(share->targetUser))
    {
    if (userName == NULL)
        {
        notify("A shared track in this view is limited to a specific user. You can keep browsing.", "myVarShareNeedLogin");
        myVariantsShareFree(&share);
        return;
        }
    if (!myVariantsShareAllowsUser(share, userName))
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
    safef(trackName, sizeof(trackName), MYVARIANTS_SHARED_TRACK_PREFIX "%s", token);

    /* Check if there's a pending edit for this shared track */
    char varName[256];
    safef(varName, sizeof(varName), "%s_%s", trackName, "id");
    char *idString = cartOptionalString(cart, varName);
    if (idString == NULL)
        continue;

    /* Revalidate via hgcentral so revoked/downgraded shares can't write. */
    struct myVariantsShare *liveShare = myVariantsResolveSharedTrack(trackName, cart);
    if (liveShare == NULL)
        continue;
    if (liveShare->permission != MYVAR_PERM_READWRITE)
        {
        myVariantsShareFree(&liveShare);
        continue;
        }
    char *tableName = myVariantsGetDbTable(liveShare->ownerUser);
    if (isEmpty(tableName))
        {
        myVariantsShareFree(&liveShare);
        continue;
        }

    int id = sqlUnsigned(idString);
    cartRemove(cart, varName);

    /* Handle cancel */
    safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
    if (cartVarExists(cart, varName))
        {
        cartRemove(cart, varName);
        myVariantsShareFree(&liveShare);
        continue;
        }

    /* Apply field edits, scoped to the share's project/db so a recipient
     * cannot edit rows in projects/assemblies they were not granted. */
    char *sp = liveShare->project;
    char *sd = liveShare->db;
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    updateTextField(trackName, conn, tableName, "name", id, sp, sd);
    updateTextField(trackName, conn, tableName, "description", id, sp, sd);
    updateTextField(trackName, conn, tableName, "itemRgb", id, sp, sd);
    updateTextField(trackName, conn, tableName, "chromStart", id, sp, sd);
    updateTextField(trackName, conn, tableName, "chromEnd", id, sp, sd);
    updateTextField(trackName, conn, tableName, "thickStart", id, sp, sd);
    updateTextField(trackName, conn, tableName, "thickEnd", id, sp, sd);
    updateBlocksFields(trackName, conn, tableName, id, sp, sd);
    updateTextField(trackName, conn, tableName, "ref", id, sp, sd);
    updateTextField(trackName, conn, tableName, "alt", id, sp, sd);
    updateTextField(trackName, conn, tableName, "mouseover", id, sp, sd);
    updateTextField(trackName, conn, tableName, "cnvType", id, sp, sd);
    struct slName *customCols = myVariantsGetCustomFields(liveShare->ownerUser);
    struct slName *col;
    for (col = customCols; col != NULL; col = col->next)
        updateTextField(trackName, conn, tableName, col->name, id, sp, sd);
    slFreeList(&customCols);
    hFreeConn(&conn);
    myVariantsShareFree(&liveShare);
    }
hashElFreeList(&shareVars);
}

boolean myVariantsTrackEnabled()
/* Return TRUE if the "My Variants" feature is enabled in hg.conf
 * and the current request comes from a valid user */
{
return cfgOptionBooleanDefault("doMyVariants", FALSE) && (getUserName() != NULL);
}
