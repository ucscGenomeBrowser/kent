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

void myVariantsStaticLoad(char **row, struct myVariants *ret)
/* Load a row from myVariants table into ret. The contents of ret will be replaced at the next call to this function. */
{
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
ret->description = row[10];
ret->db = row[11];
ret->ref = row[12];
ret->alt = row[13];
ret->project = row[14];
ret->mouseover = row[15];
ret->id = sqlUnsigned(row[16]);
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

void myVariantsSaveToDb(struct sqlConnection *conn, struct myVariants *el, char *tableName, int updateSize)
/* Save myVariants as a row to the table specified by tableName.
 * Uses explicit column names so custom fields in el->customFields are included. */
{
struct dyString *update = dyStringNew(updateSize);
sqlDyStringPrintf(update, "insert into %s (bin,chrom,chromStart,chromEnd,name,score,strand,thickStart,thickEnd,itemRgb,description,db,ref,alt,project,mouseover", tableName);

/* Append custom field column names */
struct slPair *cf;
for (cf = el->customFields; cf != NULL; cf = cf->next)
    sqlDyStringPrintf(update, ",%s", cf->name);

sqlDyStringPrintf(update, ") values (%u,'%s',%u,%u,'%s',%u,'%s',%u,%u,%u,'%s','%s','%s','%s','%s','%s'",
    el->bin, el->chrom, el->chromStart, el->chromEnd, el->name, el->score, el->strand,
    el->thickStart, el->thickEnd, el->itemRgb, el->description, el->db, el->ref, el->alt,
    el->project, el->mouseover);

/* Append custom field values */
for (cf = el->customFields; cf != NULL; cf = cf->next)
    sqlDyStringPrintf(update, ",'%s'", (char *)cf->val);

sqlDyStringPrintf(update, ")");
sqlUpdate(conn, update->string);
dyStringFree(&update);
}

struct myVariants *myVariantsLoad(char **row)
/* Load a myVariants from row fetched with select * from myVariants from database. Dispose of this with myVariantsFree(). */
{
struct myVariants *ret;
AllocVar(ret);
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
ret->description = cloneString(row[10]);
ret->db = cloneString(row[11]);
ret->ref = cloneString(row[12]);
ret->alt = cloneString(row[13]);
ret->project = cloneString(row[14]);
ret->mouseover = cloneString(row[15]);
ret->id = sqlUnsigned(row[16]);
return ret;
}

struct myVariants *myVariantsLoadAll(char *fileName)
/* Load all myVariants from a whitespace-separated file. Dispose of this with myVariantsFreeList(). */
{
struct myVariants *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[17];
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
char *row[17];
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
struct dyString *tableName = dyStringCreate("myVariants_%s", encoded);
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

char *myVariantsResolveDbTableForCustomTrack(char *trackName, struct cart *cart)
/* For a custom-track name of the form "myVariants_*", return the fully
 * qualified SQL table (db.tableName) holding the items.  Handles both
 * own tracks (resolved to current logged-in user) and shared tracks
 * (resolved via the cart-stored share record).  Returns NULL on failure. */
{
if (isEmpty(trackName))
    return NULL;
if (startsWith("myVariants_shared_", trackName))
    {
    char *token = trackName + strlen("myVariants_shared_");
    char cartVar[256];
    safef(cartVar, sizeof(cartVar), MYVAR_SHARED_CART_PREFIX "%s", token);
    char *cartVal = (cart != NULL) ? cartOptionalString(cart, cartVar) : NULL;
    if (isEmpty(cartVal))
        return NULL;
    char *owner = NULL;
    if (!myVariantsParseShareCartValue(cartVal, &owner, NULL, NULL, NULL, NULL))
        return NULL;
    char *dbTable = myVariantsGetDbTable(owner);
    freeMem(owner);
    return dbTable;
    }
if (startsWith("myVariants_", trackName))
    {
    char *userName = trackName + strlen("myVariants_");
    return myVariantsGetDbTable(userName);
    }
return NULL;
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
            ");", db, tableName);
    sqlUpdate(conn, dyStringCannibalize(&createTable));
    return myVariantsGetDbTable(userName);
    }
} 


char *myVariantsWriteCtFile(char *userName, char *targetDb, struct cart *cart)
/* Write a Custom Track file to trash for user's myVariants in targetDb and any shared
 * tracks found in cart. Return filename or NULL if nothing to write. */
{
if (isEmpty(targetDb))
    return NULL;

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

/* Collect shared track lines from cart */
struct dyString *sharedLines = dyStringNew(0);
if (cart != NULL)
    {
    struct hashEl *shareVars = cartFindPrefix(cart, MYVAR_SHARED_CART_PREFIX);
    struct hashEl *el;
    struct sqlConnection *centralConn = (shareVars != NULL) ? hConnectCentral() : NULL;
    for (el = shareVars; el != NULL; el = el->next)
        {
        char *owner = NULL, *project = NULL, *db = NULL, *label = NULL;
        int permission = 0;
        if (!myVariantsParseShareCartValue(el->val, &owner, &project, &db, &permission, &label))
            continue;
        if (!sameString(db, targetDb))
            {
            freeMem(owner);
            freeMem(project);
            freeMem(db);
            freeMem(label);
            continue;
            }
        /* Skip if the sharer is the current user — they already see their own track */
        if (isNotEmpty(userName) && sameString(owner, userName))
            {
            freeMem(owner);
            freeMem(project);
            freeMem(db);
            freeMem(label);
            continue;
            }
        /* Re-validate the share against hgcentral each render so that revoked
         * or user-targeted shares get filtered per the viewer's identity. */
        char *token = el->name + strlen(MYVAR_SHARED_CART_PREFIX);
        struct myVariantsShare *share = NULL;
        if (centralConn != NULL && sqlTableExists(centralConn, "myVariantsShares"))
            share = myVariantsGetShareByToken(centralConn, token);
        if (share == NULL)
            {
            freeMem(owner);
            freeMem(project);
            freeMem(db);
            freeMem(label);
            continue;
            }
        boolean allowed = FALSE;
        if (isEmpty(share->targetUser))
            allowed = TRUE;                                 /* anyone with link */
        else if (isNotEmpty(userName) && sameString(share->targetUser, userName))
            allowed = TRUE;                                 /* targeted at us */
        myVariantsShareFree(&share);
        if (!allowed)
            {
            freeMem(owner);
            freeMem(project);
            freeMem(db);
            freeMem(label);
            continue;
            }
        /* Verify the owner's table exists */
        char *ownerDbTable = myVariantsGetDbTable(owner);
        struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
        boolean tableOk = (isNotEmpty(ownerDbTable) && sqlTableExists(conn, ownerDbTable));
        hFreeConn(&conn);
        if (!tableOk)
            {
            freeMem(owner);
            freeMem(project);
            freeMem(db);
            freeMem(label);
            continue;
            }
        /* Strip double quotes from owner-controlled values to prevent injection
         * of additional trackDb settings via the CT file track line. */
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
            "track name=\"myVariants_shared_%s\" type=\"myVariants\" itemRgb=\"on\""
            " visibility=\"pack\""
            " shortLabel=\"%s\""
            " longLabel=\"Shared variants: %s (from %s)\"\n",
            token, shortLabel, projectLabel, owner);
        freeMem(owner);
        freeMem(project);
        freeMem(db);
        freeMem(label);
        }
    if (centralConn != NULL)
        hDisconnectCentral(&centralConn);
    hashElFreeList(&shareVars);
    }

if (!hasOwnItems && dyStringLen(sharedLines) == 0)
    {
    dyStringFree(&sharedLines);
    return NULL;
    }

/* Reusable, stable filename per user+db — always rewrite since shares are dynamic */
struct tempName tn;
char base[PATH_LEN];
char *hostPort = cgiServerNamePort();
safef(base, sizeof base, "myVariants_%s_%s_%s",
    hostPort ? hostPort : "localhost", targetDb,
    isNotEmpty(userName) ? userName : "shared");
for (char *p = base; *p; p++) if (*p == '/') *p = '_';
trashDirReusableFile(&tn, "ct", base, ".bed");
FILE *f = mustOpen(tn.forCgi, "w");
if (hasOwnItems)
    fprintf(f, "track name=\"myVariants_%s\" type=\"myVariants\" itemRgb=\"on\""
        " visibility=\"pack\" shortLabel=\"My Variants\""
        " longLabel=\"My Variants (%s)\"\n", userName, userName);
if (dyStringLen(sharedLines) > 0)
    fprintf(f, "%s", dyStringContents(sharedLines));
carefulClose(&f);
dyStringFree(&sharedLines);
return cloneString(tn.forCgi);
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
    "thickStart", "thickEnd", "itemRgb", "description", "db", "ref", "alt",
    "project", "mouseover", "id",
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
