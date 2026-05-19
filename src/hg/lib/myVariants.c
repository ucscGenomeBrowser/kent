#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "myVariants.h"
#include "myVariantsShare.h"
#include "customTrack.h"
#include "hdb.h"
#include "hgConfig.h"
#include "cheapcgi.h"
#include "trashDir.h"
#include "obscure.h"
#include "wikiLink.h"

boolean isMyVariantsType(char *type)
/* TRUE if type names the myVariants custom-track type. NULL-safe. */
{
return sameOk(type, MYVARIANTS_TYPE);
}

boolean isMyVariantsTrack(char *trackName)
/* TRUE if trackName is a myVariants custom track (own or shared). NULL-safe. */
{
return trackName != NULL && startsWith(MYVARIANTS_TRACK_PREFIX, trackName);
}

boolean isMyVariantsSharedTrack(char *trackName)
/* TRUE if trackName is a myVariants shared custom track. NULL-safe. */
{
return trackName != NULL && startsWith(MYVARIANTS_SHARED_TRACK_PREFIX, trackName);
}

void myVariantsStaticLoad(char **row, struct myVariants *ret)
/* Load a row from myVariants table into ret. The contents of ret will be replaced at the next call to this function. */
{
int sizeOne;
ret->bin = sqlUnsigned(row[0]);
ret->chrom = row[1];
ret->chromStart = sqlUnsigned(row[2]);
ret->chromEnd = sqlUnsigned(row[3]);
ret->name = row[4];
ret->score = sqlUnsigned(row[5]);
safecpy(ret->strand, sizeof(ret->strand), row[6]);
ret->thickStart = sqlUnsigned(row[7]);
ret->thickEnd = sqlUnsigned(row[8]);
ret->itemRgb = sqlUnsigned(row[9]);
ret->blockCount = sqlUnsigned(row[10]);
sqlSignedDynamicArray(row[11], &ret->blockSizes, &sizeOne);
assert(sizeOne == ret->blockCount);
sqlSignedDynamicArray(row[12], &ret->chromStarts, &sizeOne);
assert(sizeOne == ret->blockCount);
ret->description = row[13];
ret->db = row[14];
ret->ref = row[15];
ret->alt = row[16];
ret->project = row[17];
ret->mouseover = row[18];
ret->id = sqlUnsigned(row[19]);
}

struct myVariants *myVariantsLoadByQuery(struct sqlConnection *conn, char *query)
/* Load all myVariants from table that satisfy the query given. Dispose of this with myVariantsFreeList(). */
{
struct myVariants *list = NULL, *el;
struct sqlResult *sr;
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = myVariantsLoad(row);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
return list;
}

static char *commaIntList(int *arr, int n)
/* Build a "n1,n2,...,nN," string from an int array.  Caller frees. */
{
struct dyString *dy = dyStringNew(n * 8);
int i;
for (i = 0; i < n; i++)
    dyStringPrintf(dy, "%d,", arr[i]);
return dyStringCannibalize(&dy);
}

void myVariantsSaveToDb(struct sqlConnection *conn, struct myVariants *el, char *tableName, int updateSize)
/* Save myVariants as a row to the table specified by tableName.
 * Uses explicit column names so custom fields in el->customFields are included.
 * If el->name is NULL or empty, fills it in post-INSERT as "Variant N" using
 * the row's auto-increment id; sqlLastAutoId wraps MariaDB's mysql_insert_id,
 * which is per-connection and unaffected by concurrent INSERTs on other
 * connections. */
{
struct dyString *update = dyStringNew(updateSize);
sqlDyStringPrintf(update, "insert into %s (bin,chrom,chromStart,chromEnd,name,score,strand,thickStart,thickEnd,itemRgb,blockCount,blockSizes,chromStarts,description,db,ref,alt,project,mouseover", tableName);

/* Append custom field column names */
struct slPair *cf;
for (cf = el->customFields; cf != NULL; cf = cf->next)
    sqlDyStringPrintf(update, ",%s", cf->name);

char *insertName = isEmpty(el->name) ? "" : el->name;
char *blockSizesStr = commaIntList(el->blockSizes, el->blockCount);
char *chromStartsStr = commaIntList(el->chromStarts, el->blockCount);
sqlDyStringPrintf(update, ") values (%u,'%s',%u,%u,'%s',%u,'%s',%u,%u,%u,%u,'%s','%s','%s','%s','%s','%s','%s','%s'",
    el->bin, el->chrom, el->chromStart, el->chromEnd, insertName, el->score, el->strand,
    el->thickStart, el->thickEnd, el->itemRgb, el->blockCount, blockSizesStr, chromStartsStr,
    el->description, el->db, el->ref, el->alt, el->project, el->mouseover);
freeMem(blockSizesStr);
freeMem(chromStartsStr);

/* Append custom field values */
for (cf = el->customFields; cf != NULL; cf = cf->next)
    sqlDyStringPrintf(update, ",'%s'", (char *)cf->val);

sqlDyStringPrintf(update, ")");
sqlUpdate(conn, update->string);
dyStringFree(&update);

if (isEmpty(el->name))
    {
    unsigned int newId = sqlLastAutoId(conn);
    struct dyString *nameUpdate = sqlDyStringCreate(
        "update %s set name = 'Variant %u' where id = %u",
        tableName, newId, newId);
    sqlUpdate(conn, dyStringCannibalize(&nameUpdate));
    el->id = newId;
    freez(&el->name);
    struct dyString *dy = dyStringNew(0);
    dyStringPrintf(dy, "Variant %u", newId);
    el->name = dyStringCannibalize(&dy);
    }
}

struct myVariants *myVariantsLoad(char **row)
/* Load a myVariants from row fetched with select * from myVariants from database. Dispose of this with myVariantsFree(). */
{
struct myVariants *ret;
AllocVar(ret);
int sizeOne;
ret->bin = sqlUnsigned(row[0]);
ret->chrom = cloneString(row[1]);
ret->chromStart = sqlUnsigned(row[2]);
ret->chromEnd = sqlUnsigned(row[3]);
ret->name = cloneString(row[4]);
ret->score = sqlUnsigned(row[5]);
safecpy(ret->strand, sizeof(ret->strand), row[6]);
ret->thickStart = sqlUnsigned(row[7]);
ret->thickEnd = sqlUnsigned(row[8]);
ret->itemRgb = sqlUnsigned(row[9]);
ret->blockCount = sqlUnsigned(row[10]);
sqlSignedDynamicArray(row[11], &ret->blockSizes, &sizeOne);
assert(sizeOne == ret->blockCount);
sqlSignedDynamicArray(row[12], &ret->chromStarts, &sizeOne);
assert(sizeOne == ret->blockCount);
ret->description = cloneString(row[13]);
ret->db = cloneString(row[14]);
ret->ref = cloneString(row[15]);
ret->alt = cloneString(row[16]);
ret->project = cloneString(row[17]);
ret->mouseover = cloneString(row[18]);
ret->id = sqlUnsigned(row[19]);
return ret;
}

struct myVariants *myVariantsLoadAll(char *fileName)
/* Load all myVariants from a whitespace-separated file. Dispose of this with myVariantsFreeList(). */
{
struct myVariants *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[MYVARIANTS_NUM_COLS];
while (lineFileRow(lf, row))
    {
    el = myVariantsLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct myVariants *myVariantsLoadAllByChar(char *fileName, char chopper)
/* Load all myVariants from a chopper separated file. Dispose of this with myVariantsFreeList(). */
{
struct myVariants *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[MYVARIANTS_NUM_COLS];
while (lineFileNextCharRow(lf, chopper, row, ArraySize(row)))
    {
    el = myVariantsLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct myVariants *myVariantsCommaIn(char **pS, struct myVariants *ret)
/* Create a myVariants out of a comma separated string. This will fill in ret if non-null, otherwise will return a new myVariants */
{
char *s = *pS;
if (ret == NULL)
    AllocVar(ret);
ret->bin = sqlUnsignedComma(&s);
ret->chrom = sqlStringComma(&s);
ret->chromStart = sqlUnsignedComma(&s);
ret->chromEnd = sqlUnsignedComma(&s);
ret->name = sqlStringComma(&s);
ret->score = sqlUnsignedComma(&s);
sqlFixedStringComma(&s, ret->strand, sizeof(ret->strand));
ret->thickStart = sqlUnsignedComma(&s);
ret->thickEnd = sqlUnsignedComma(&s);
ret->itemRgb = sqlUnsignedComma(&s);
ret->blockCount = sqlUnsignedComma(&s);
    {
    int i;
    s = sqlEatChar(s, '{');
    if (ret->blockCount > 0)
        AllocArray(ret->blockSizes, ret->blockCount);
    for (i=0; i<ret->blockCount; ++i)
        ret->blockSizes[i] = sqlSignedComma(&s);
    s = sqlEatChar(s, '}');
    s = sqlEatChar(s, ',');
    }
    {
    int i;
    s = sqlEatChar(s, '{');
    if (ret->blockCount > 0)
        AllocArray(ret->chromStarts, ret->blockCount);
    for (i=0; i<ret->blockCount; ++i)
        ret->chromStarts[i] = sqlSignedComma(&s);
    s = sqlEatChar(s, '}');
    s = sqlEatChar(s, ',');
    }
ret->description = sqlStringComma(&s);
ret->db = sqlStringComma(&s);
ret->ref = sqlStringComma(&s);
ret->alt = sqlStringComma(&s);
ret->project = sqlStringComma(&s);
ret->mouseover = sqlStringComma(&s);
ret->id = sqlUnsignedComma(&s);
*pS = s;
return ret;
}

void myVariantsFree(struct myVariants **pEl)
/* Free a single dynamically allocated myVariants such as created with myVariantsLoad(). */
{
struct myVariants *el;
if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->name);
freeMem(el->blockSizes);
freeMem(el->chromStarts);
freeMem(el->description);
freeMem(el->db);
freeMem(el->ref);
freeMem(el->alt);
freeMem(el->project);
freeMem(el->mouseover);
slPairFreeValsAndList(&el->customFields);
freez(pEl);
}

void myVariantsFreeList(struct myVariants **pList)
/* Free a list of dynamically allocated myVariants's */
{
struct myVariants *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    myVariantsFree(&el);
    }
*pList = NULL;
}

void myVariantsOutput(struct myVariants *el, FILE *f, char sep, char lastSep)
/* Print out myVariants. Separate fields with sep. Follow last field with lastSep. */
{
fprintf(f, "%u", el->bin);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->chrom);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->chromStart);
fputc(sep,f);
fprintf(f, "%u", el->chromEnd);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->name);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->score);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->strand);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->thickStart);
fputc(sep,f);
fprintf(f, "%u", el->thickEnd);
fputc(sep,f);
fprintf(f, "%u", el->itemRgb);
fputc(sep,f);
fprintf(f, "%u", el->blockCount);
fputc(sep,f);
    {
    int i;
    if (sep == ',') fputc('{',f);
    for (i=0; i<el->blockCount; ++i)
        {
        fprintf(f, "%d", el->blockSizes[i]);
        fputc(',', f);
        }
    if (sep == ',') fputc('}',f);
    }
fputc(sep,f);
    {
    int i;
    if (sep == ',') fputc('{',f);
    for (i=0; i<el->blockCount; ++i)
        {
        fprintf(f, "%d", el->chromStarts[i]);
        fputc(',', f);
        }
    if (sep == ',') fputc('}',f);
    }
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->description);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->db);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->ref);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->alt);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->project);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->mouseover);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->id);
fputc(lastSep,f);
}

static char *myVariantsAutoSqlString =
"table myVariants\n"
"\"An item in a myVariants type track.\"\n"
"    (\n"
"    uint bin;         \"Bin for range index\"\n"
"    string chrom;     \"Reference sequence chromosome or scaffold\"\n"
"    uint   chromStart;\"Start position in chromosome\"\n"
"    uint   chromEnd;  \"End position in chromosome\"\n"
"    string name;      \"Name of item - up to 16 chars\"\n"
"    uint  score;      \"0-1000.  Higher numbers are darker.\"\n"
"    char[1] strand;   \"+ or - for strand\"\n"
"    uint   thickStart;\"Start of thick part\"\n"
"    uint   thickEnd;  \"End position of thick part\"\n"
"    uint itemRgb;     \"RGB 8 bits each as in bed\"\n"
"    uint blockCount;  \"Number of blocks\"\n"
"    int[blockCount] blockSizes; \"Comma separated list of block sizes\"\n"
"    int[blockCount] chromStarts; \"Start positions relative to chromStart\"\n"
"    lstring description; \"Longer item description\"\n"
"    string db;        \"database name of this annotation\"\n"
"    string ref;       \"reference allele\"\n"
"    string alt;       \"alternate allele\"\n"
"    string project;    \"project name for grouping variants\"\n"
"    string mouseover;  \"short mouseover text for hover display\"\n"
"    uint id;          \"Unique ID for item\"\n"
"    )\n"
;

struct asObject *myVariantsAsObj()
/* Return asObject describing fields of myVariants */
{
return asParseText(myVariantsAutoSqlString);
}

char *myVariantsGetDatabaseForUser(char *userName)
/* Hash the userName and map it to 1..31 inclusive for deciding what
 * database the table should be created in */
{
if (!userName)
    return NULL;

unsigned hashed = hashString(userName);
unsigned clamped = (hashed % 31) + 1;
struct dyString *dbName = dyStringCreate("customData%02u", clamped);
return dyStringCannibalize(&dbName);
}

static char *encodeUserNameForIdentifier(char *userName)
/* Return a SQL-identifier-safe encoding of userName.  Alphanumeric and
 * underscore characters are kept verbatim; every other byte becomes _XX where
 * XX is its lowercase two-digit hex value.  This keeps the encoding
 * deterministic, reversible, and free of collisions for distinct inputs that
 * differ only by special characters.  Caller frees. */
{
if (isEmpty(userName))
    return cloneString("");
struct dyString *dy = dyStringNew(strlen(userName) + 8);
char *p;
for (p = userName; *p; p++)
    {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '_')
        dyStringAppendC(dy, c);
    else
        dyStringPrintf(dy, "_%02x", c);
    }
return dyStringCannibalize(&dy);
}

char *myVariantsGetTableName(char *userName)
/* Build the SQL table name for this user's myVariants.  Encodes the user
 * name so non-identifier characters (e.g. '@' in email-style logins) don't
 * fail sqlCheckIdentifier. */
{
if (!userName)
    return NULL;
char *encoded = encodeUserNameForIdentifier(userName);
struct dyString *tableName = dyStringCreate(MYVARIANTS_TRACK_PREFIX "%s", encoded);
freeMem(encoded);
return dyStringCannibalize(&tableName);
}

char *myVariantsGetDbTable(char *userName)
/* Return the string db.tableName based on the userName for use in sql statements
 * without specifying the database */
{
char *db = myVariantsGetDatabaseForUser(userName);
char *tbl = myVariantsGetTableName(userName);
if (isNotEmpty(db) && isNotEmpty(tbl))
    {
    struct dyString *ret = dyStringCreate("%s.%s", db, tbl);
    return dyStringCannibalize(&ret);
    }
return NULL;
}

void myVariantsDeleteForDb(char *userName, char *targetDb)
/* Delete the user's myVariants items for the given assembly.  No-op if the
 * table doesn't exist. */
{
if (isEmpty(userName) || isEmpty(targetDb))
    return;
char *dbTable = myVariantsTableExists(userName);
if (isEmpty(dbTable))
    return;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
struct dyString *del = sqlDyStringCreate("DELETE FROM %s WHERE db='%s'",
    dbTable, targetDb);
sqlUpdate(conn, del->string);
dyStringFree(&del);
hFreeConn(&conn);
}

struct myVariantsShare *myVariantsResolveSharedTrack(char *trackName, struct cart *cart)
/* For a "myVariants_shared_*" custom-track name, look up and revalidate the
 * share record from hgcentral. Returns NULL if the track is not a shared
 * track, the cart cookie is missing, the share has been revoked, or the
 * current user is not authorized (target user mismatch). The returned share
 * carries the validated owner/db/project/permission; callers should use these
 * (not the cart-supplied values) for authorization or scoping decisions.
 * Caller frees with myVariantsShareFree. */
{
if (isEmpty(trackName) || !isMyVariantsSharedTrack(trackName))
    return NULL;
char *token = trackName + strlen(MYVARIANTS_SHARED_TRACK_PREFIX);
char cartVar[256];
safef(cartVar, sizeof(cartVar), MYVAR_SHARED_CART_PREFIX "%s", token);
/* The cart-cookie presence gate is belt-and-suspenders: it ensures the share
 * was once accepted into this session before we hit hgcentral. The real
 * authorization is the targetUser check below against the live share row. */
if (cart == NULL || !cartVarExists(cart, cartVar))
    return NULL;
struct sqlConnection *conn = hConnectCentral();
if (!sqlTableExists(conn, "myVariantsShares"))
    {
    hDisconnectCentral(&conn);
    return NULL;
    }
struct myVariantsShare *share = myVariantsGetShareByToken(conn, token);
hDisconnectCentral(&conn);
if (share == NULL)
    return NULL;
if (isNotEmpty(share->targetUser))
    {
    char *userName = getUserName();
    if (isEmpty(userName) || !sameString(share->targetUser, userName))
        {
        myVariantsShareFree(&share);
        return NULL;
        }
    }
return share;
}

char *myVariantsResolveDbTableForCustomTrack(char *trackName, struct cart *cart)
/* For a custom-track name of the form "myVariants_*", return the fully
 * qualified SQL table (db.tableName) holding the items.  Handles both own
 * tracks and shared tracks. For shared tracks, revalidates the share against
 * hgcentral; returns NULL if the share has been revoked, downgraded out of
 * scope, or is not for the current user. */
{
if (isEmpty(trackName))
    return NULL;
if (isMyVariantsSharedTrack(trackName))
    {
    struct myVariantsShare *share = myVariantsResolveSharedTrack(trackName, cart);
    if (share == NULL)
        return NULL;
    char *dbTable = myVariantsGetDbTable(share->ownerUser);
    myVariantsShareFree(&share);
    return dbTable;
    }
if (isMyVariantsTrack(trackName))
    {
    /* trackName is the SQL-identifier-encoded form "myVariants_<encoded>".
     * Resolve via the current logged-in user (an own track is only viewable
     * by its owner) and verify the trackName matches the encoded form for
     * that user before returning their db.tableName. */
    char *userName = getUserName();
    if (isEmpty(userName))
        return NULL;
    char *expected = myVariantsGetTableName(userName);
    boolean match = sameOk(expected, trackName);
    freeMem(expected);
    if (!match)
        return NULL;
    return myVariantsGetDbTable(userName);
    }
return NULL;
}

char *myVariantsSharedScopeWhere(char *trackName, struct cart *cart)
/* For a "myVariants_shared_*" custom-track, return a SQL WHERE-clause
 * fragment that limits a query to the share's authorized project and db
 * (e.g. "db='hg38' and project='Variants'", or "db='hg38'" alone when the
 * share's project is "*"). Returns NULL for non-shared tracks or revoked
 * shares. Memoized per-process: callers receive a fresh cloneString that
 * they own. */
{
static struct hash *cache = NULL;
if (cache == NULL)
    cache = hashNew(0);
if (isEmpty(trackName) || !isMyVariantsSharedTrack(trackName))
    return NULL;
char *cached = hashFindVal(cache, trackName);
if (cached != NULL)
    return cached[0] ? cloneString(cached) : NULL;

char *result = NULL;
struct myVariantsShare *share = myVariantsResolveSharedTrack(trackName, cart);
if (share != NULL && isNotEmpty(share->db))
    {
    struct dyString *dy = sqlDyStringCreate("db='%s'", share->db);
    if (isNotEmpty(share->project) && !sameString(share->project, "*"))
        sqlDyStringPrintf(dy, " and project='%s'", share->project);
    result = dyStringCannibalize(&dy);
    }
myVariantsShareFree(&share);
hashAdd(cache, trackName, cloneString(result ? result : ""));
return result;
}

void myVariantsStripHiddenFields(struct slName **pFieldList)
/* Remove any field whose name starts with "_hidden_" from the list in place.
 * Handles bare names ("_hidden_foo") and dotted/qualified names
 * ("db.table._hidden_foo") by checking the segment after the last '.'.
 * Used by hgTables for myVariants_shared_* tables so a recipient does not
 * see columns the owner has hidden. (Custom non-hidden columns are kept,
 * since they are part of the data the owner intentionally shared.) */
{
struct slName *kept = NULL, *fld, *next;
for (fld = *pFieldList; fld != NULL; fld = next)
    {
    next = fld->next;
    char *bare = strrchr(fld->name, '.');
    bare = bare ? bare + 1 : fld->name;
    if (startsWith("_hidden_", bare))
        freeMem(fld);
    else
        slAddHead(&kept, fld);
    }
slReverse(&kept);
*pFieldList = kept;
}

char *myVariantsTableExists(char *userName)
/* See if we already have a table for this user. If so, return the name
 * of the table (in db.tableName format), else NULL */
{
if (!userName)
    return NULL;
char *dbTable = myVariantsGetDbTable(userName);
if (!dbTable)
    return NULL;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
boolean exists = sqlTableExists(conn, dbTable);
hFreeConn(&conn);
if (exists)
    return dbTable;
return NULL;
}

char *myVariantsCreateTable(char *userName)
/* Return name of myVariants table (in db.tableName format) for user if it exists or
 * we can create, NULL otherwise */
{
if (!userName)
    return NULL;
char *db = myVariantsGetDatabaseForUser(userName);
char *tableName = myVariantsGetTableName(userName);
if (!tableName)
    return NULL;
char *dbTable = myVariantsGetDbTable(userName);
struct sqlConnection *existConn = hAllocConn(CUSTOM_TRASH);
boolean exists = sqlTableExists(existConn, dbTable);
hFreeConn(&existConn);
if (exists)
    return dbTable;
else
    {
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct dyString *createTable = sqlDyStringCreate(
            "CREATE TABLE %s.%s (\n"
            "    bin int unsigned not null,\n"
            "    chrom varchar(255) not null,\n"
            "    chromStart int unsigned not null,\n"
            "    chromEnd int unsigned not null,\n"
            "    name varchar(255) not null,\n"
            "    score int unsigned not null,\n"
            "    strand char(1) not null,\n"
            "    thickStart int unsigned not null,\n"
            "    thickEnd int unsigned not null,\n"
            "    itemRgb int unsigned not null,\n"
            "    blockCount int unsigned not null,\n"
            "    blockSizes longblob not null,\n"
            "    chromStarts longblob not null,\n"
            "    description longblob not null,\n"
            "    db varchar(255) not null,\n"
            "    ref varchar(255) not null,\n"
            "    alt varchar(255) not null,\n"
            "    project varchar(255) not null,\n"
            "    mouseover varchar(255) not null,\n"
            "    id int auto_increment,\n"
            "    PRIMARY KEY(id),\n"
            "    INDEX(chrom(16),bin),\n"
            "    INDEX(db),\n"
            "    INDEX(project)\n"
            ") ENGINE=InnoDB;", db, tableName);
    sqlUpdate(conn, dyStringCannibalize(&createTable));
    return myVariantsGetDbTable(userName);
    }
} 


static void readLabelsFromCtFile(char *path, char *trackName,
                                 char **retShort, char **retLong)
/* Parse the matching track line in an existing ctfile and pull shortLabel
 * and longLabel via hashVarLine.  Leaves *ret as NULL when not found or
 * path is missing. */
{
*retShort = NULL;
*retLong = NULL;
if (isEmpty(path) || isEmpty(trackName) || !fileExists(path))
    return;
struct lineFile *lf = lineFileOpen(path, TRUE);
char *line = NULL;
while (lineFileNext(lf, &line, NULL))
    {
    if (!startsWith("track ", line))
        continue;
    struct hash *settings = hashVarLine(line + strlen("track "), lf->lineIx);
    char *name = hashFindVal(settings, "name");
    if (name != NULL && sameString(name, trackName))
        {
        char *s = hashFindVal(settings, "shortLabel");
        char *l = hashFindVal(settings, "longLabel");
        if (s != NULL)
            *retShort = cloneString(s);
        if (l != NULL)
            *retLong = cloneString(l);
        hashFree(&settings);
        break;
        }
    hashFree(&settings);
    }
lineFileClose(&lf);
}

static char *sanitizeLabel(char *raw, int maxLen)
/* Strip both quote characters, newlines and carriage returns, then cap to
 * maxLen characters.  Returns NULL for NULL/empty input or when sanitizing
 * leaves nothing behind.  Matches the precedent in hgCustom.c for
 * user-supplied CT labels written into the trackline. */
{
if (isEmpty(raw))
    return NULL;
char *s = cloneString(raw);
stripChar(s, '"');
stripChar(s, '\'');
stripChar(s, '\\');
stripChar(s, '\n');
stripChar(s, '\r');
if ((int)strlen(s) > maxLen)
    s[maxLen] = '\0';
if (isEmpty(s))
    {
    freeMem(s);
    return NULL;
    }
return s;
}

static void myVariantsCtFilePath(struct tempName *tn,
                                 char *encodedTableName, char *targetDb)
/* Build the per-user-per-db ctfile path.  Prefers
 * cfgOption("myVariantsDataDir") so the file lives outside trash and the
 * trashCleaner doesn't expire it; falls back to trashDirReusableFile when
 * the dir isn't configured. */
{
char *persistentDir = cfgOption("myVariantsDataDir");
if (isNotEmpty(persistentDir))
    {
    /* Group files per user so one user's tracks live together:
     * ${persistentDir}/<encodedTableName>/<db>.bed. */
    char *subdir = isNotEmpty(encodedTableName) ? encodedTableName : "shared";
    char *sep = endsWith(persistentDir, "/") ? "" : "/";
    char path[PATH_LEN];
    safef(path, sizeof path, "%s%s%s/%s.bed", persistentDir, sep, subdir, targetDb);
    char dirPart[PATH_LEN];
    splitPath(path, dirPart, NULL, NULL);
    makeDirsOnPath(dirPart);
    safef(tn->forCgi, sizeof tn->forCgi, "%s", path);
    safef(tn->forHtml, sizeof tn->forHtml, "%s", path);
    }
else
    {
    char base[PATH_LEN];
    char *hostPort = cgiServerNamePort();
    safef(base, sizeof base, MYVARIANTS_TRACK_PREFIX "%s_%s_%s",
        hostPort ? hostPort : "localhost", targetDb,
        isNotEmpty(encodedTableName) ? encodedTableName : "shared");
    for (char *p = base; *p; p++) if (*p == '/') *p = '_';
    trashDirReusableFile(tn, "ct", base, ".bed");
    }
}

void myVariantsUnlinkCtFile(char *userName, char *targetDb)
/* Delete the on-disk ctfile for this user+assembly if it exists.  Uses the
 * same path resolution as myVariantsWriteCtFile so it targets either the
 * persistent dir (myVariantsDataDir) or the trash fallback. */
{
if (isEmpty(userName) || isEmpty(targetDb))
    return;
char *encodedTableName = myVariantsGetTableName(userName);
if (isEmpty(encodedTableName))
    return;
struct tempName tn;
myVariantsCtFilePath(&tn, encodedTableName, targetDb);
if (fileExists(tn.forCgi))
    unlink(tn.forCgi);
freeMem(encodedTableName);
}

char *myVariantsWriteCtFile(char *userName, char *targetDb, struct cart *cart)
/* Write a Custom Track file for user's myVariants in targetDb and any shared
 * tracks found in cart. Return filename or NULL if nothing to write.
 * If cfgOption("myVariantsDataDir") is set the file is placed there so the
 * trashCleaner doesn't expire it; otherwise it goes in trash/ct as before. */
{
if (isEmpty(targetDb))
    return NULL;

/* Identifier-safe form of userName for the CT track name and trash filename.
 * The raw userName may contain non-ASCII or characters unsafe in trackDb
 * syntax, filesystem paths, or SQL (e.g. '@', ';', spaces, quotes). */
char *encodedTableName = NULL;
if (isNotEmpty(userName))
    encodedTableName = myVariantsGetTableName(userName);

/* Check if user has their own items */
boolean hasOwnItems = FALSE;
if (isNotEmpty(userName))
    {
    char *dbTable = myVariantsGetDbTable(userName);
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    if (isNotEmpty(dbTable) && sqlTableExists(conn, dbTable))
        {
        char countQuery[512];
        sqlSafef(countQuery, sizeof countQuery,
            "select count(*) from %s where db='%s'", dbTable, targetDb);
        hasOwnItems = (sqlQuickNum(conn, countQuery) > 0);
        }
    hFreeConn(&conn);
    }

/* Collect shared track lines from cart. All authoritative metadata
 * (owner/db/project/label) comes from the live share row, never from
 * cart-parsed values. */
struct dyString *sharedLines = dyStringNew(0);
if (cart != NULL)
    {
    struct hashEl *shareVars = cartFindPrefix(cart, MYVAR_SHARED_CART_PREFIX);
    struct hashEl *el;
    for (el = shareVars; el != NULL; el = el->next)
        {
        char *token = el->name + strlen(MYVAR_SHARED_CART_PREFIX);
        char trackName[512];
        safef(trackName, sizeof(trackName), MYVARIANTS_SHARED_TRACK_PREFIX "%s", token);
        struct myVariantsShare *share = myVariantsResolveSharedTrack(trackName, cart);
        if (share == NULL)
            continue;
        if (!sameString(share->db, targetDb))
            {
            myVariantsShareFree(&share);
            continue;
            }
        /* Skip if the sharer is the current user - they already see their own track */
        if (isNotEmpty(userName) && sameString(share->ownerUser, userName))
            {
            myVariantsShareFree(&share);
            continue;
            }
        /* Verify the owner's table exists */
        char *ownerDbTable = myVariantsGetDbTable(share->ownerUser);
        struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
        boolean tableOk = (isNotEmpty(ownerDbTable) && sqlTableExists(conn, ownerDbTable));
        hFreeConn(&conn);
        if (!tableOk)
            {
            myVariantsShareFree(&share);
            continue;
            }
        /* Strip double quotes from owner-controlled values to prevent injection
         * of additional trackDb settings via the CT file track line. */
        char *owner = cloneString(share->ownerUser);
        char *project = cloneString(share->project);
        char *label = isNotEmpty(share->label) ? cloneString(share->label) : NULL;
        stripChar(owner, '"');
        stripChar(project, '"');
        if (label != NULL)
            stripChar(label, '"');
        char *projectLabel = sameString(project, "*") ? "All" : project;
        char shortLabel[64];
        if (isNotEmpty(label))
            safef(shortLabel, sizeof(shortLabel), "%s", label);
        else
            safef(shortLabel, sizeof(shortLabel), "%s's %s", owner, projectLabel);
        dyStringPrintf(sharedLines,
            "track name=\"" MYVARIANTS_SHARED_TRACK_PREFIX "%s\""
            " type=\"" MYVARIANTS_TYPE "\" itemRgb=\"on\""
            " visibility=\"pack\""
            " shortLabel=\"%s\""
            " longLabel=\"Shared annotations: %s (from %s)\"\n",
            token, shortLabel, projectLabel, owner);
        freeMem(owner);
        freeMem(project);
        freeMem(label);
        myVariantsShareFree(&share);
        }
    hashElFreeList(&shareVars);
    }

if (!hasOwnItems && dyStringLen(sharedLines) == 0)
    {
    /* Nothing to write: remove any stale on-disk file so it doesn't leak
     * when the user empties their table.  Under the old trash-only path
     * the trashCleaner would have done this for us. */
    if (isNotEmpty(encodedTableName))
        {
        struct tempName stale;
        myVariantsCtFilePath(&stale, encodedTableName, targetDb);
        if (fileExists(stale.forCgi))
            unlink(stale.forCgi);
        }
    dyStringFree(&sharedLines);
    freeMem(encodedTableName);
    return NULL;
    }

/* Reusable, stable filename per user+db - always rewrite since shares are dynamic */
struct tempName tn;
myVariantsCtFilePath(&tn, encodedTableName, targetDb);

/* Resolve display labels for the user's own track: cart vars set by the
 * rename UI win, then the existing on-disk file (so a rename survives a
 * cart reset), then the default. */
char *shortLabel = NULL, *longLabel = NULL;
if (hasOwnItems)
    {
    if (cart != NULL)
        {
        char cartVar[256];
        safef(cartVar, sizeof cartVar, "%s.%s.shortLabel",
            encodedTableName, targetDb);
        shortLabel = sanitizeLabel(cartOptionalString(cart, cartVar), 80);
        safef(cartVar, sizeof cartVar, "%s.%s.longLabel",
            encodedTableName, targetDb);
        longLabel = sanitizeLabel(cartOptionalString(cart, cartVar), 200);
        }
    if (shortLabel == NULL || longLabel == NULL)
        {
        char *diskShort = NULL, *diskLong = NULL;
        readLabelsFromCtFile(tn.forCgi, encodedTableName, &diskShort, &diskLong);
        if (shortLabel == NULL)
            shortLabel = sanitizeLabel(diskShort, 80);
        if (longLabel == NULL)
            longLabel = sanitizeLabel(diskLong, 200);
        freeMem(diskShort);
        freeMem(diskLong);
        }
    if (shortLabel == NULL)
        shortLabel = cloneString("My Annotations");
    if (longLabel == NULL)
        longLabel = cloneString("My Annotations");
    }

FILE *f = mustOpen(tn.forCgi, "w");
if (hasOwnItems)
    fprintf(f, "track name=\"%s\" type=\"" MYVARIANTS_TYPE "\" itemRgb=\"on\""
        " visibility=\"pack\" shortLabel=\"%s\""
        " longLabel=\"%s\"\n", encodedTableName, shortLabel, longLabel);
if (dyStringLen(sharedLines) > 0)
    fprintf(f, "%s", dyStringContents(sharedLines));
carefulClose(&f);
dyStringFree(&sharedLines);
freeMem(encodedTableName);
freeMem(shortLabel);
freeMem(longLabel);
return cloneString(tn.forCgi);
}

boolean myVariantsHandleCtRemoval(struct customTrack *ct, struct cart *cart,
                                  char *database)
/* If ct is a myVariants own track: delete the user's rows for the current
 * assembly, unlink the persistent ctfile, drop the per-db renamed labels
 * and the visibility var.  If ct is a myVariants shared track: drop the
 * share-acceptance cart var and the visibility var.  Returns TRUE when ct
 * was a myVariants track and this function fully handled the cart cleanup
 * (caller must skip its own per-track cart cleanup so other-assembly
 * labels are preserved); FALSE otherwise. */
{
if (ct == NULL || ct->tdb == NULL || isEmpty(ct->tdb->track))
    return FALSE;
char *trackName = ct->tdb->track;
if (!isMyVariantsTrack(trackName))
    return FALSE;

/* Cleanup common to any myVariants removal: drop the track's visibility var
 * and invalidate the on-disk ctfile pointer so the next entry-point visit
 * regenerates from current SQL/share state. */
cartRemove(cart, trackName);
char mvVar[256];
safef(mvVar, sizeof mvVar, MYVARIANTS_FILE_VAR_PREFIX "%s", database);
cartRemove(cart, mvVar);

if (isMyVariantsSharedTrack(trackName))
    {
    /* Drop the share-acceptance var so the share isn't re-imported. */
    char *token = trackName + strlen(MYVARIANTS_SHARED_TRACK_PREFIX);
    char shareCartVar[256];
    safef(shareCartVar, sizeof shareCartVar,
        MYVAR_SHARED_CART_PREFIX "%s", token);
    cartRemove(cart, shareCartVar);
    return TRUE;
    }

/* Own track: delete the user's SQL rows for this assembly, unlink the
 * persisted ctfile, and drop any renamed-label cart vars so a freshly
 * created track on this assembly starts at "My Annotations" again. */
char *userName = wikiLinkUserName();
if (isNotEmpty(userName))
    {
    myVariantsDeleteForDb(userName, database);
    myVariantsUnlinkCtFile(userName, database);
    }
char labelPrefix[256];
safef(labelPrefix, sizeof labelPrefix, "%s.%s.", trackName, database);
cartRemovePrefix(cart, labelPrefix);
return TRUE;
}

struct slName *myVariantsGetProjects(char *userName)
/* Return list of distinct non-empty project values for this user's myVariants table.
 * Caller must slFreeList the result. Returns NULL if no projects or table doesn't exist. */
{
if (isEmpty(userName))
    return NULL;

char *dbTable = myVariantsTableExists(userName);
if (isEmpty(dbTable))
    return NULL;

struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
struct slName *projects = NULL;
char query[512];
sqlSafef(query, sizeof(query),
    "SELECT DISTINCT project FROM %s WHERE project IS NOT NULL AND project != '' ORDER BY project",
    dbTable);
projects = sqlQuickList(conn, query);
hFreeConn(&conn);
return projects;
}

/* Built-in column names from myVariants.as - any column NOT in this list is a
 * user-added custom column.  Filtering by name (rather than index) is robust
 * against column reordering or future schema changes. */
static const char *builtInColumns[] = {
    "bin", "chrom", "chromStart", "chromEnd", "name", "score", "strand",
    "thickStart", "thickEnd", "itemRgb", "blockCount", "blockSizes",
    "chromStarts", "description", "db", "ref", "alt", "project",
    "mouseover", "id",
};

static boolean isBuiltInColumn(char *name)
{
int i;
for (i = 0;  i < ArraySize(builtInColumns);  i++)
    if (sameString(name, builtInColumns[i]))
        return TRUE;
return FALSE;
}

struct slName *myVariantsGetCustomFields(char *userName)
/* Return list of user-added custom column names for this user's myVariants table.
 * Excludes built-in columns and _hidden_ prefixed columns.
 * Caller must slFreeList the result. Returns NULL if no custom fields or table doesn't exist. */
{
if (isEmpty(userName))
    return NULL;

char *dbTable = myVariantsTableExists(userName);
if (isEmpty(dbTable))
    return NULL;

struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
struct slName *allFields = sqlListFields(conn, dbTable);
hFreeConn(&conn);
if (allFields == NULL)
    return NULL;

struct slName *customFields = NULL;
struct slName *field;
for (field = allFields; field != NULL; field = field->next)
    {
    if (isBuiltInColumn(field->name))
        continue;
    if (startsWith("_hidden_", field->name))
        continue;
    slNameAddHead(&customFields, field->name);
    }
slFreeList(&allFields);
slReverse(&customFields);
return customFields;
}

struct slName *myVariantsGetHiddenFields(char *userName)
/* Return list of hidden custom column names (with _hidden_ prefix stripped) for this user's
 * myVariants table. Caller must slFreeList the result. Returns NULL if none. */
{
if (isEmpty(userName))
    return NULL;

char *dbTable = myVariantsTableExists(userName);
if (isEmpty(dbTable))
    return NULL;

struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
struct slName *allFields = sqlListFields(conn, dbTable);
hFreeConn(&conn);
if (allFields == NULL)
    return NULL;

struct slName *hiddenFields = NULL;
struct slName *field;
for (field = allFields; field != NULL; field = field->next)
    {
    if (isBuiltInColumn(field->name))
        continue;
    if (startsWith("_hidden_", field->name))
        slNameAddHead(&hiddenFields, field->name + strlen("_hidden_"));
    }
slFreeList(&allFields);
slReverse(&hiddenFields);
return hiddenFields;
}
