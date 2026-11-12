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
#include "htmshell.h"
#include "hui.h"
#include "hVarSubst.h"

char *myVariantsItemTypes[] = { "transcript", "snv", "cnv", "other" };
int myVariantsNumItemTypes = ArraySize(myVariantsItemTypes);

char *myVariantsCnvTypes[] = {
    "deletion", "duplication", "insertion", "inversion",
    "translocation", "complex", "breakend"
};
int myVariantsNumCnvTypes = ArraySize(myVariantsCnvTypes);

char *myVariantsCanonicalItemType(char *s)
/* Return the matching canonical entry from myVariantsItemTypes (case-insensitive),
 * or NULL if s is empty or not in the allow-list. */
{
if (isEmpty(s))
    return NULL;
int ix = stringArrayIx(s, myVariantsItemTypes, myVariantsNumItemTypes);
return ix < 0 ? NULL : myVariantsItemTypes[ix];
}

char *myVariantsCanonicalCnvType(char *s)
/* Return the matching canonical entry from myVariantsCnvTypes (case-insensitive),
 * or NULL if s is empty or not in the allow-list. */
{
if (isEmpty(s))
    return NULL;
int ix = stringArrayIx(s, myVariantsCnvTypes, myVariantsNumCnvTypes);
return ix < 0 ? NULL : myVariantsCnvTypes[ix];
}

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
ret->mouseover = row[17];
ret->itemType = row[18];
ret->cnvType = row[19];
ret->id = sqlUnsigned(row[20]);
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
 * If el->name is NULL or empty, fills it in post-INSERT as "Annotation N" using
 * the row's auto-increment id; sqlLastAutoId wraps MariaDB's mysql_insert_id,
 * which is per-connection and unaffected by concurrent INSERTs on other
 * connections. */
{
struct dyString *update = dyStringNew(updateSize);
sqlDyStringPrintf(update, "insert into %s (bin,chrom,chromStart,chromEnd,name,score,strand,thickStart,thickEnd,itemRgb,blockCount,blockSizes,chromStarts,description,db,ref,alt,mouseover,itemType,cnvType", tableName);

/* Append custom field column names, quoted because one can be named order. */
struct slPair *cf;
for (cf = el->customFields; cf != NULL; cf = cf->next)
    sqlDyStringPrintf(update, ",`%s`", cf->name);

char *insertName = isEmpty(el->name) ? "" : el->name;
char *blockSizesStr = commaIntList(el->blockSizes, el->blockCount);
char *chromStartsStr = commaIntList(el->chromStarts, el->blockCount);
sqlDyStringPrintf(update, ") values (%u,'%s',%u,%u,'%s',%u,'%s',%u,%u,%u,%u,'%s','%s','%s','%s','%s','%s','%s','%s','%s'",
    el->bin, el->chrom, el->chromStart, el->chromEnd, insertName, el->score, el->strand,
    el->thickStart, el->thickEnd, el->itemRgb, el->blockCount, blockSizesStr, chromStartsStr,
    el->description, el->db, el->ref, el->alt, el->mouseover,
    isEmpty(el->itemType) ? "snv" : el->itemType,
    isEmpty(el->cnvType) ? "" : el->cnvType);
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
        "update %s set name = 'Annotation %u' where id = %u",
        tableName, newId, newId);
    sqlUpdate(conn, dyStringCannibalize(&nameUpdate));
    el->id = newId;
    freez(&el->name);
    struct dyString *dy = dyStringNew(0);
    dyStringPrintf(dy, "Annotation %u", newId);
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
ret->mouseover = cloneString(row[17]);
ret->itemType = cloneString(row[18]);
ret->cnvType = cloneString(row[19]);
ret->id = sqlUnsigned(row[20]);
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
ret->mouseover = sqlStringComma(&s);
ret->itemType = sqlStringComma(&s);
ret->cnvType = sqlStringComma(&s);
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
freeMem(el->mouseover);
freeMem(el->itemType);
freeMem(el->cnvType);
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
fprintf(f, "%s", el->mouseover);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->itemType ? el->itemType : "");
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->cnvType ? el->cnvType : "");
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->id);
fputc(lastSep,f);
}

/* Keep in step with myVariants.as - this is the copy the schema page shows. */
static char *myVariantsAutoSqlString =
"table myVariants\n"
"\"One annotation in a My Annotations track.\"\n"
"    (\n"
"    uint bin;         \"Range index used internally; not part of the annotation\"\n"
"    string chrom;     \"Reference sequence chromosome or scaffold\"\n"
"    uint   chromStart;\"Start position in chromosome\"\n"
"    uint   chromEnd;  \"End position in chromosome\"\n"
"    string name;      \"Label shown on the annotation\"\n"
"    uint  score;      \"Unused. Annotations are colored by itemRgb\"\n"
"    char[1] strand;   \"+ or - for strand, . when not known\"\n"
"    uint   thickStart;\"Start of the thick part; the coding start for a transcript\"\n"
"    uint   thickEnd;  \"End of the thick part; the coding end for a transcript\"\n"
"    uint itemRgb;     \"Display color, RGB 8 bits each as in bed\"\n"
"    uint blockCount;  \"Number of blocks\"\n"
"    int[blockCount] blockSizes; \"Comma separated list of block sizes\"\n"
"    int[blockCount] chromStarts; \"Start positions relative to chromStart\"\n"
"    lstring description; \"Longer notes about the annotation\"\n"
"    string db;        \"Assembly this annotation is on (hg38, mm39, etc.)\"\n"
"    string ref;       \"Reference allele on the forward genomic strand\"\n"
"    string alt;       \"Alternate allele on the forward genomic strand\"\n"
"    string mouseover;  \"Short text shown when hovering over the annotation\"\n"
"    string itemType;   \"Annotation type: transcript, snv, cnv or other\"\n"
"    string cnvType;    \"gnomAD CNV term; empty unless itemType is cnv\"\n"
"    uint id;          \"Row identifier used internally; not part of the annotation\"\n"
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

/* Longest encoded login allowed in a table name.  MySQL identifiers stop at 64
 * characters, and the prefix, separator and random suffix take 20. */
#define MYVARIANTS_ENCODED_USER_MAXLEN 30

static char *myVariantsNewTableName(char *userName)
/* Build an unused SQL table name for a new project of this user's:
 * myVariants_<encoded login>_<8 random characters>.  The suffix is random rather
 * than the registry id because the table is created before the registry row, so
 * the id is not known yet, and because a name that never changes lets a project
 * be renamed without renaming its track.  Caller frees. */
{
char *encoded = encodeUserNameForIdentifier(userName);
/* The login is in the name to make a stray table recognizable, not to make it
 * unique; the random suffix does that.  So truncating it is safe. */
if (strlen(encoded) > MYVARIANTS_ENCODED_USER_MAXLEN)
    encoded[MYVARIANTS_ENCODED_USER_MAXLEN] = 0;
char *key = makeRandomKey(32);
struct dyString *dy = dyStringCreate(MYVARIANTS_TRACK_PREFIX "%s_%s", encoded, key);
freeMem(encoded);
freeMem(key);
return dyStringCannibalize(&dy);
}

static void myVariantsCreateItemTable(struct sqlConnection *conn, char *dbTable)
/* Create one project's annotation table.  Keep in step with myVariants.sql and
 * with myVariantsAutoSqlString above. */
{
struct dyString *createTable = sqlDyStringCreate(
        "CREATE TABLE %s (\n"
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
        "    mouseover varchar(255) not null,\n"
        "    itemType varchar(16) not null default 'snv',\n"
        "    cnvType varchar(32) not null default '',\n"
        "    id int auto_increment,\n"
        "    PRIMARY KEY(id),\n"
        "    INDEX(chrom(16),bin),\n"
        "    INDEX(db)\n"
        ") ENGINE=InnoDB;", dbTable);
sqlUpdate(conn, dyStringCannibalize(&createTable));
}

struct myVariantsProject *myVariantsNewProject(char *userName, char *label,
    boolean *retCreated)
/* Create a project for userName: a new annotation table plus the registry row
 * that names it.  The table is created first, so a request that dies part way
 * leaves a table no row points at, never a row pointing at a table that is not
 * there.  Returns the existing project when the label is already taken, so a
 * user cannot end up with two tracks they cannot tell apart.  Returns NULL if
 * userName is empty, label is empty or only whitespace, or the label is too
 * long.  retCreated, when not NULL, is set TRUE only for a project this call
 * made, so a caller that means to alter the new project's table does not alter
 * one the user already had.  Caller frees with myVariantsProjectFreeList. */
{
if (retCreated != NULL)
    *retCreated = FALSE;
if (isEmpty(userName) || isEmpty(skipLeadingSpaces(label))
    || strlen(label) > MYVARIANTS_LABEL_MAXLEN)
    return NULL;

struct sqlConnection *central = hConnectCentral();
if (!sqlTableExists(central, MYVARIANTS_PROJECT_TABLE))
    {
    hDisconnectCentral(&central);
    return NULL;
    }
/* Two projects with the same label would draw two tracks a user cannot tell
 * apart, so reuse the existing one instead of making a second. */
struct myVariantsProject *found = myVariantsProjectByLabelOnConn(central, userName, label);
if (found != NULL)
    {
    hDisconnectCentral(&central);
    return found;
    }

char *shardDb = myVariantsGetDatabaseForUser(userName);
char *tableName = myVariantsNewTableName(userName);
struct dyString *dy = dyStringCreate("%s.%s", shardDb, tableName);
char *dbTable = dyStringCannibalize(&dy);

struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
myVariantsCreateItemTable(conn, dbTable);
hFreeConn(&conn);

struct myVariantsProject *created = myVariantsProjectInsert(central, userName, dbTable, label);
myVariantsForgetProjects();
if (created == NULL)
    {
    /* Another request claimed this label between the lookup and the insert.
     * Take theirs and drop the table just made, so the loser of the race does
     * not leave one behind on every attempt. */
    created = myVariantsProjectByLabelOnConn(central, userName, label);
    conn = hAllocConn(CUSTOM_TRASH);
    struct dyString *drop = sqlDyStringCreate("DROP TABLE IF EXISTS %s", dbTable);
    sqlUpdate(conn, dyStringCannibalize(&drop));
    hFreeConn(&conn);
    }
else if (retCreated != NULL)
    *retCreated = TRUE;
hDisconnectCentral(&central);
freeMem(shardDb);
freeMem(tableName);
freeMem(dbTable);
return created;
}

/* One request asks for the project list several times: the pending-edit sweep,
 * the track build, and the block that hands the list to the page's JavaScript.
 * The answer cannot change between them, so it is read once.  A CGI serves one
 * request and exits, so there is nothing to invalidate. */
static struct myVariantsProject *cachedProjects = NULL;
static char *cachedProjectsUser = NULL;

static struct myVariantsProject *cloneProjectList(struct myVariantsProject *list)
/* Copy a project list so the caller can free what it is given without
 * disturbing the cache.  Order is kept. */
{
struct myVariantsProject *copy = NULL, *proj;
for (proj = list; proj != NULL; proj = proj->next)
    {
    struct myVariantsProject *one;
    AllocVar(one);
    one->id = proj->id;
    one->userName = cloneString(proj->userName);
    one->dbTable = cloneString(proj->dbTable);
    one->label = cloneString(proj->label);
    one->createdAt = cloneString(proj->createdAt);
    slAddHead(&copy, one);
    }
slReverse(&copy);
return copy;
}

void myVariantsForgetProjects()
/* Drop the cached project list, so the next call reads it again.  Anything that
 * creates, renames or removes a project calls this. */
{
myVariantsProjectFreeList(&cachedProjects);
freez(&cachedProjectsUser);
}

struct myVariantsProject *myVariantsListProjects(char *userName)
/* Return every project this user owns, oldest first.  NULL if none, or if the
 * registry table is not installed.  Caller frees with
 * myVariantsProjectFreeList. */
{
if (isEmpty(userName))
    return NULL;
if (cachedProjectsUser == NULL || !sameString(cachedProjectsUser, userName))
    {
    myVariantsForgetProjects();
    struct sqlConnection *central = hConnectCentral();
    if (sqlTableExists(central, MYVARIANTS_PROJECT_TABLE))
        cachedProjects = myVariantsProjectList(central, userName);
    hDisconnectCentral(&central);
    cachedProjectsUser = cloneString(userName);
    }
return cloneProjectList(cachedProjects);
}

struct myVariantsProject *myVariantsDefaultProject(char *userName)
/* Return the project a save with no project chosen goes into: the user's oldest
 * one, or a new project named MYVARIANTS_DEFAULT_PROJECT when they have none.
 * Every annotation belongs to a project, so this is what the quick-add paths
 * and a first-time user get.  Caller frees. */
{
if (isEmpty(userName))
    return NULL;
struct myVariantsProject *list = myVariantsListProjects(userName);
if (list != NULL)
    {
    struct myVariantsProject *oldest = list;
    list = list->next;
    oldest->next = NULL;
    myVariantsProjectFreeList(&list);
    return oldest;
    }
return myVariantsNewProject(userName, MYVARIANTS_DEFAULT_PROJECT, NULL);
}

struct myVariantsProject *myVariantsProjectForUser(char *userName, unsigned id)
/* Return the project with this id if userName owns it, otherwise NULL.  This is
 * the ownership check for a project id that came in from a form or a URL.
 * Caller frees with myVariantsProjectFreeList. */
{
if (isEmpty(userName) || id == 0)
    return NULL;
struct sqlConnection *central = hConnectCentral();
struct myVariantsProject *proj = NULL;
if (sqlTableExists(central, MYVARIANTS_PROJECT_TABLE))
    proj = myVariantsProjectById(central, id);
hDisconnectCentral(&central);
if (proj != NULL && !sameOk(proj->userName, userName))
    myVariantsProjectFreeList(&proj);
return proj;
}

struct myVariantsProject *myVariantsFindProject(char *userName, char *trackName)
/* Return the project userName owns that draws as trackName, or NULL when
 * trackName is not one of theirs.  This is the ownership check for an own
 * track: a track name that does not come back is one the caller may not read.
 * Caller frees. */
{
if (isEmpty(userName) || isEmpty(trackName))
    return NULL;
struct myVariantsProject *list = myVariantsListProjects(userName), *proj;
struct myVariantsProject *found = NULL;
for (proj = list; proj != NULL; proj = proj->next)
    {
    if (sameOk(myVariantsProjectTrackName(proj), trackName))
        {
        found = proj;
        break;
        }
    }
if (found != NULL)
    {
    slRemoveEl(&list, found);
    found->next = NULL;
    }
myVariantsProjectFreeList(&list);
return found;
}

boolean myVariantsRenameProject(unsigned id, char *userName, char *label)
/* Set a project's label.  userName must own it, and the label must not be empty
 * or only whitespace.  Returns TRUE if a row was updated.  The table and the
 * track name are untouched, so visibility and the other per-track cart
 * variables survive a rename. */
{
if (isEmpty(userName) || isEmpty(skipLeadingSpaces(label)))
    return FALSE;
struct sqlConnection *central = hConnectCentral();
boolean ok = FALSE;
if (sqlTableExists(central, MYVARIANTS_PROJECT_TABLE))
    ok = myVariantsProjectRename(central, id, userName, label);
myVariantsForgetProjects();
hDisconnectCentral(&central);
return ok;
}

boolean myVariantsDropProject(unsigned id, char *userName)
/* Delete a project: its share, its registry row, then its table, in that
 * order, so a request that dies part way leaves a table no row points at.
 * userName must own the project.  Returns TRUE if it was deleted. */
{
if (isEmpty(userName))
    return FALSE;
struct sqlConnection *central = hConnectCentral();
if (!sqlTableExists(central, MYVARIANTS_PROJECT_TABLE))
    {
    hDisconnectCentral(&central);
    return FALSE;
    }
struct myVariantsProject *proj = myVariantsProjectById(central, id);
if (proj == NULL || !sameString(proj->userName, userName))
    {
    myVariantsProjectFreeList(&proj);
    hDisconnectCentral(&central);
    return FALSE;
    }
char *dbTable = cloneString(proj->dbTable);
myVariantsProjectFreeList(&proj);
if (sqlTableExists(central, MYVARIANTS_SHARE_TABLE))
    myVariantsRevokeSharesForProject(central, id, userName);
boolean ok = myVariantsProjectDeleteRow(central, id, userName);
myVariantsForgetProjects();
hDisconnectCentral(&central);

if (ok)
    {
    /* The table can go away between asking and dropping, and a concurrent
     * delete should not abort. */
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct dyString *drop = sqlDyStringCreate("DROP TABLE IF EXISTS %s", dbTable);
    sqlUpdate(conn, dyStringCannibalize(&drop));
    hFreeConn(&conn);
    }
freeMem(dbTable);
return ok;
}

int myVariantsCountForDb(char *dbTable, char *targetDb)
/* Return how many annotations dbTable holds on targetDb.  0 if the table is
 * gone, which is how a project whose table was swept shows up. */
{
if (isEmpty(dbTable) || isEmpty(targetDb))
    return 0;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
int count = 0;
if (sqlTableExists(conn, dbTable))
    {
    char query[512];
    sqlSafef(query, sizeof(query), "SELECT count(*) FROM %s WHERE db='%s'", dbTable, targetDb);
    count = sqlQuickNum(conn, query);
    }
hFreeConn(&conn);
return count;
}

struct myVariantsShare *myVariantsResolveSharedTrack(char *trackName, struct cart *cart)
/* For a "myVariants_shared_*" custom-track name, look up and revalidate the
 * share record from hgcentral.  Returns NULL if the track is not a shared
 * track, the cart does not hold the token, or the share has been revoked.  The
 * returned share carries the validated projectId and permission; callers
 * should use these, not any cart-supplied value, for authorization and
 * scoping.  Caller frees with myVariantsShareFree.
 *
 * The token is the credential: anyone holding it may view the project, and may
 * edit it when the share says so.  The cart variable is the record that this
 * session was given the token, which is why cartLoadUserSession drops these
 * variables when a user loads somebody else's saved session - a session must
 * not pass the credential on. */
{
if (isEmpty(trackName) || !isMyVariantsSharedTrack(trackName))
    return NULL;
char *token = trackName + strlen(MYVARIANTS_SHARED_TRACK_PREFIX);
if (!myVariantsIsShareToken(token))
    return NULL;
char cartVar[256];
safef(cartVar, sizeof(cartVar), MYVAR_SHARED_CART_PREFIX "%s", token);
if (cart == NULL || !cartVarExists(cart, cartVar))
    return NULL;
struct sqlConnection *conn = hConnectCentral();
if (!sqlTableExists(conn, MYVARIANTS_SHARE_TABLE))
    {
    hDisconnectCentral(&conn);
    return NULL;
    }
struct myVariantsShare *share = myVariantsGetShareByToken(conn, token);
hDisconnectCentral(&conn);
return share;
}

char *myVariantsShareDbTable(struct myVariantsShare *share)
/* Return the fully qualified table the share's project is stored in, or NULL
 * if the project is gone.  Caller frees. */
{
if (share == NULL)
    return NULL;
struct sqlConnection *central = hConnectCentral();
struct myVariantsProject *proj = NULL;
if (sqlTableExists(central, MYVARIANTS_PROJECT_TABLE))
    proj = myVariantsProjectById(central, share->projectId);
hDisconnectCentral(&central);
if (proj == NULL)
    return NULL;
/* A project id is only meaningful together with the owner the share names, so
 * confirm the two still agree before handing back a table. */
if (!sameOk(proj->userName, share->ownerUser))
    {
    myVariantsProjectFreeList(&proj);
    return NULL;
    }
char *dbTable = cloneString(proj->dbTable);
myVariantsProjectFreeList(&proj);
return dbTable;
}

char *myVariantsScopeWhere(char *database, char *trackName)
/* Return a SQL WHERE-clause fragment limiting a myVariants query to the rows a
 * page on database should see.  A project's table holds its annotations on
 * every assembly, so this is the assembly clause; the project itself is the
 * table, and needs no clause.  Returns NULL when there is nothing to add.
 * Caller frees. */
{
if (isEmpty(trackName) || !isMyVariantsTrack(trackName) || isEmpty(database))
    return NULL;
struct dyString *dy = sqlDyStringCreate("db='%s'", database);
return dyStringCannibalize(&dy);
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
    if (startsWithNoCase("_hidden_", bare))
        freeMem(fld);
    else
        slAddHead(&kept, fld);
    }
slReverse(&kept);
*pFieldList = kept;
}

boolean myVariantsIsHiddenField(char *table, char *field)
/* TRUE if table is a myVariants table, bare or as db.table, and field is one
 * of its "_hidden_" columns.  NULL-safe. */
{
if (table == NULL || field == NULL)
    return FALSE;
char *bare = strrchr(table, '.');
bare = bare ? bare + 1 : table;
return isMyVariantsTrack(bare) && startsWithNoCase("_hidden_", field);
}

static char *myVariantsDescriptionHtml(char *trackName, char *ctDb)
/* Return the shared description page for a My Annotations track, with $db and
 * $track filled in so its Data Access links point at this track.  NULL if the
 * file is not installed.  Caller frees. */
{
char descPath[PATH_LEN];
safef(descPath, sizeof descPath, "%s/inc/myVariantsDesc.html", hDocumentRoot());
if (!fileExists(descPath))
    return NULL;
char *html = NULL;
readInGulp(descPath, &html, NULL);
if (isEmpty(html))
    {
    freeMem(html);
    return NULL;
    }
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = trackName;
tdb->table = trackName;
hVarSubstInVar(trackName, tdb, ctDb, &html);
freeMem(tdb);
return html;
}

static struct customTrack *myVariantsTrackForProject(char *trackName, char *dbTable,
    char *shortLabel, char *longLabel, char *ctDb, int itemCount)
/* Build one My Annotations custom track.  There is no ct file: the annotations
 * are in SQL and every field here comes out of the registry, so the track is
 * assembled in memory rather than written to a file and parsed back.  That
 * also means no custom-track text a user can write ever names one of these
 * tables. */
{
struct customTrack *ct;
AllocVar(ct);
struct trackDb *tdb = customTrackTdbDefault();
tdb->track = cloneString(trackName);
tdb->table = cloneString(trackName);
tdb->shortLabel = cloneString(shortLabel);
tdb->longLabel = cloneString(longLabel);
tdb->type = cloneString(MYVARIANTS_TYPE);
tdb->grp = cloneString(MYVARIANTS_GROUP_NAME);
tdb->visibility = tvPack;
tdb->html = myVariantsDescriptionHtml(tdb->track, ctDb);
ct->tdb = tdb;
ct->genomeDb = cloneString(ctDb);
ct->dbTableName = cloneString(dbTable);
ct->fieldCount = MYVARIANTS_NUM_COLS;
/* dbTrackType is the loader type, and the annotations really are in SQL, so the
 * type checks in hgTables read it here as they always have.  dbTrack and
 * dbDataLoad stay FALSE on purpose: setting dbDataLoad sends every parse
 * through ctDbTableExists, which touches customTrash.metaInfo, and dbTrash
 * drops what it finds there after a few days.  These tables hold the user's
 * own data and must not be swept. */
ct->dbTrackType = cloneString(MYVARIANTS_TYPE);
ctAddToSettings(ct, "tdbType", MYVARIANTS_TYPE);
ctAddToSettings(ct, "itemRgb", "on");
ctAddToSettings(ct, "group", MYVARIANTS_GROUP_NAME);
/* ctAddToSettings hashes the pointer it is given rather than a copy, so the
 * value has to outlive this function. */
char countBuf[32];
safef(countBuf, sizeof countBuf, "%d", itemCount);
ctAddToSettings(ct, "itemCount", cloneString(countBuf));
return ct;
}

void myVariantsAddTracks(struct customTrack **pCtList, struct cart *cart, char *genomeDb)
/* Append the signed-in user's My Annotations tracks for genomeDb, one per
 * project that has annotations there, plus one per share accepted into this
 * cart.  Replaces the ct file the feature used to write and parse back on
 * every request: the registry is the only copy of a project's name and table,
 * so there is nothing a file would add. */
{
if (isEmpty(genomeDb))
    return;
struct customTrack *added = NULL;
char *userName = getUserName();

/* The user's own projects. */
struct myVariantsProject *projects = myVariantsListProjects(userName), *proj;
for (proj = projects; proj != NULL; proj = proj->next)
    {
    int count = myVariantsCountForDb(proj->dbTable, genomeDb);
    if (count == 0)
        continue;
    struct dyString *longLabel = dyStringCreate("My Annotations: %s", proj->label);
    struct customTrack *ct = myVariantsTrackForProject(myVariantsProjectTrackName(proj),
        proj->dbTable, proj->label, dyStringContents(longLabel), genomeDb, count);
    dyStringFree(&longLabel);
    slAddHead(&added, ct);
    }
myVariantsProjectFreeList(&projects);

/* Shares accepted into this cart.  All of the metadata comes from the live
 * share and its project, never from the cart value. */
if (cart != NULL)
    {
    struct hashEl *shareVars = cartFindPrefix(cart, MYVAR_SHARED_CART_PREFIX), *el;
    for (el = shareVars; el != NULL; el = el->next)
        {
        /* The suffix of a cart variable name is whatever the request said it
         * was, so check it before it goes into a buffer. */
        char *token = el->name + strlen(MYVAR_SHARED_CART_PREFIX);
        if (!myVariantsIsShareToken(token))
            continue;
        char trackName[512];
        safef(trackName, sizeof(trackName), MYVARIANTS_SHARED_TRACK_PREFIX "%s", token);
        struct myVariantsShare *share = myVariantsResolveSharedTrack(trackName, cart);
        if (share == NULL)
            continue;
        /* The owner already sees the project as their own track. */
        if (isNotEmpty(userName) && sameString(share->ownerUser, userName))
            {
            myVariantsShareFree(&share);
            continue;
            }
        struct sqlConnection *central = hConnectCentral();
        struct myVariantsProject *shared = NULL;
        if (sqlTableExists(central, MYVARIANTS_PROJECT_TABLE))
            shared = myVariantsProjectById(central, share->projectId);
        hDisconnectCentral(&central);
        if (shared == NULL)
            {
            myVariantsShareFree(&share);
            continue;
            }
        int count = myVariantsCountForDb(shared->dbTable, genomeDb);
        if (count > 0)
            {
            struct dyString *shortLabel = dyStringCreate("%s's %s",
                share->ownerUser, shared->label);
            struct dyString *longLabel = dyStringCreate("Shared annotations: %s (from %s)",
                shared->label, share->ownerUser);
            struct customTrack *ct = myVariantsTrackForProject(trackName, shared->dbTable,
                dyStringContents(shortLabel), dyStringContents(longLabel), genomeDb, count);
            dyStringFree(&shortLabel);
            dyStringFree(&longLabel);
            slAddHead(&added, ct);
            }
        myVariantsProjectFreeList(&shared);
        myVariantsShareFree(&share);
        }
    hashElFreeList(&shareVars);
    }

slReverse(&added);
*pCtList = slCat(*pCtList, added);
}

boolean myVariantsHandleCtRemoval(struct customTrack *ct, struct cart *cart,
                                  char *database)
/* Handle "Remove custom track" on a My Annotations track.  For an own track
 * this deletes the project: its share, its registry row and its table.  For a
 * shared track it drops the acceptance so the recipient stops seeing it, and
 * leaves the owner's data alone.  Returns TRUE when ct was a My Annotations
 * track and this function handled the cart cleanup, so the caller must skip
 * its own.  The confirmation is the caller's job; by the time we are here the
 * user has agreed. */
{
if (ct == NULL || ct->tdb == NULL || !isMyVariantsType(ct->tdb->type))
    return FALSE;
char *trackName = ct->tdb->track;
if (isEmpty(trackName))
    return FALSE;

if (isMyVariantsSharedTrack(trackName))
    {
    char *token = trackName + strlen(MYVARIANTS_SHARED_TRACK_PREFIX);
    char cartVar[256];
    safef(cartVar, sizeof(cartVar), MYVAR_SHARED_CART_PREFIX "%s", token);
    cartRemove(cart, cartVar);
    cartRemove(cart, trackName);
    return TRUE;
    }

char *userName = getUserName();
struct myVariantsProject *proj = myVariantsFindProject(userName, trackName);
if (proj == NULL)
    return FALSE;
myVariantsDropProject(proj->id, userName);
myVariantsProjectFreeList(&proj);
cartRemove(cart, trackName);
/* The track name is gone for good, so its own settings go with it.  Other
 * projects keep theirs because nothing here touches them. */
char prefix[256];
safef(prefix, sizeof prefix, "%s.", trackName);
cartRemovePrefix(cart, prefix);
return TRUE;
}

struct slPair *myVariantsGetDbCounts(char *userName)
/* Return a list of assemblies this user has annotations on, each val an item
 * count made with intToPt, summed over all of their projects.  Caller must
 * slPairFreeList the result.  Returns NULL if the user has no annotations. */
{
if (isEmpty(userName))
    return NULL;
struct myVariantsProject *projects = myVariantsListProjects(userName), *proj;
if (projects == NULL)
    return NULL;

struct hash *totals = hashNew(0);
struct slPair *counts = NULL;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
for (proj = projects; proj != NULL; proj = proj->next)
    {
    if (!sqlTableExists(conn, proj->dbTable))
        continue;
    char query[512];
    sqlSafef(query, sizeof(query),
        "SELECT db, count(*) FROM %s WHERE db IS NOT NULL AND db != ''"
        " GROUP BY db", proj->dbTable);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct hashEl *hel = hashLookup(totals, row[0]);
        if (hel == NULL)
            {
            struct slPair *pair = slPairNew(row[0], intToPt(sqlSigned(row[1])));
            slAddHead(&counts, pair);
            hashAdd(totals, row[0], pair);
            }
        else
            {
            struct slPair *pair = hel->val;
            pair->val = intToPt(ptToInt(pair->val) + sqlSigned(row[1]));
            }
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
hashFree(&totals);
myVariantsProjectFreeList(&projects);
slSort(&counts, slPairCmp);
return counts;
}

/* Built-in column names from myVariants.as - any column NOT in this list is a
 * user-added custom column.  Filtering by name (rather than index) is robust
 * against column reordering or future schema changes. */
static const char *builtInColumns[] = {
    "bin", "chrom", "chromStart", "chromEnd", "name", "score", "strand",
    "thickStart", "thickEnd", "itemRgb", "blockCount", "blockSizes",
    "chromStarts", "description", "db", "ref", "alt",
    "mouseover", "itemType", "cnvType", "id",
};

static boolean isBuiltInColumn(char *name)
{
int i;
for (i = 0;  i < ArraySize(builtInColumns);  i++)
    if (sameString(name, builtInColumns[i]))
        return TRUE;
return FALSE;
}

void myVariantsGetFieldLists(char *dbTable, struct slName **retCustom,
    struct slName **retHidden)
/* Split one project's user-added columns into the visible ones and the hidden
 * ones, with the _hidden_ prefix stripped from the latter.  Both lists come off
 * a single description of the table.  Either return may be NULL if the caller
 * does not want that list.  Caller slFreeLists what it asked for. */
{
if (retCustom != NULL)
    *retCustom = NULL;
if (retHidden != NULL)
    *retHidden = NULL;
if (isEmpty(dbTable))
    return;

/* The table can be gone when the project was deleted in another window. */
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
struct slName *allFields = NULL;
if (sqlTableExists(conn, dbTable))
    allFields = sqlListFields(conn, dbTable);
hFreeConn(&conn);
if (allFields == NULL)
    return;

struct slName *customFields = NULL, *hiddenFields = NULL, *field;
for (field = allFields; field != NULL; field = field->next)
    {
    if (isBuiltInColumn(field->name))
        continue;
    if (startsWithNoCase("_hidden_", field->name))
        slNameAddHead(&hiddenFields, field->name + strlen("_hidden_"));
    else
        slNameAddHead(&customFields, field->name);
    }
slFreeList(&allFields);
slReverse(&customFields);
slReverse(&hiddenFields);
if (retCustom != NULL)
    *retCustom = customFields;
else
    slFreeList(&customFields);
if (retHidden != NULL)
    *retHidden = hiddenFields;
else
    slFreeList(&hiddenFields);
}

struct slName *myVariantsGetCustomFields(char *dbTable)
/* Return list of user-added custom column names on one project's table.
 * Excludes built-in columns and _hidden_ prefixed columns.  Custom fields are
 * per project, so each project has its own list.
 * Caller must slFreeList the result. Returns NULL if no custom fields or table doesn't exist. */
{
struct slName *customFields = NULL;
myVariantsGetFieldLists(dbTable, &customFields, NULL);
return customFields;
}
