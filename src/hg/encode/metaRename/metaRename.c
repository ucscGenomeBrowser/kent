/* metaRename - rename a list of metaData objs based on metaData. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "mdb.h"
#include "dystring.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaRename - rename a list of metaData objs based on metaData\n"
  "usage:\n"
  "   metaRename database object.lst metaDb.ra trackDb.ra term.txt downloadDir outDir\n"
  "Without options, checks to see if rules can be followed to rename\n"
  "metaDb objNames\n\n"
  "arguments\n"
  "   database      name of database with tables\n"
  "   object.lst    list of metaDb objName's\n"
  "   metaDb.ra     metaDb ra file\n"
  "   trackDb.ra    trackDb ra file\n"
  "   term.txt      list of metadata items in name\n"
  "   downloadDir   directory with download files\n"
  "   outDir        if not -test, then put metaDb.ra and trackDb.ra in outDir\n"
  "options:\n"
  "   -test         test to make sure rename would be valid\n"
//  "   -outMetaDb=metaDb.ra       outputs metaDb with renamed list\n"
//  "   -outTrackDb=trackDb.ra\n"
//  "   -outSqlScript=script.txt\n"
  );
}

boolean doTest = FALSE;

static struct optionSpec options[] = {
   {"test", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *getMetaDbHash(char *metaDb, struct mdbObj **head)
{
boolean validated = FALSE;
struct mdbObj *mdbObjs = mdbObjsLoadFromFormattedFile(metaDb, &validated), *mdbObj;
struct hash *hash = newHash(10);

for(mdbObj = mdbObjs; mdbObj; mdbObj = mdbObj->next)
    hashAdd(hash, mdbObj->obj, mdbObj);

*head = mdbObjs;
return hash;
}

struct mdbObj *getObjsInMeta(struct slName *objList, struct hash *mdbHash)
{
struct mdbObj *newList = NULL;

for(; objList; objList = objList->next)
    {
    struct hashEl *hel;
    if ((hel = hashLookup(mdbHash, objList->name)) == NULL)
        errAbort("can't find %s in metaDb file", objList->name);

    struct mdbObj *obj = hel->val;

    slAddHead(&newList, obj);
    }
return newList;
}

void verifyTerms(struct mdbObj *mdbObjs, struct slName *terms)
{
for(; mdbObjs; mdbObjs=mdbObjs->next)
    {
    struct slName *t;

    for(t=terms; t; t = t->next)
        {
        struct mdbVar *mdbVar = hashFindVal(mdbObjs->varHash, t->name);

        if (mdbVar == NULL)
            errAbort("obj %s doesn't have var %s\n", mdbObjs->obj, t->name);
        }
    }
}

void validateName(char *name)
{
if (strlen(name) > 64)
    errAbort("name %s is more than 64 characters", name);
}

struct hash *checkNames(struct mdbObj *mdbObjs, struct slName *terms)
{
struct dyString *dy = dyStringNew(100);
struct hash *mapping = newHash(10);

for(; mdbObjs; mdbObjs=mdbObjs->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObjs->varHash, "docId");
    char *docId = "";
    if (mdbVar)
        docId = mdbVar->val;

    docId += strlen("wgEncode");

    dyStringClear(dy);
    //dyStringPrintf(dy, "wgEncode");
    struct slName *t;
    for(t=terms; t; t = t->next)
        {
        struct mdbVar *mdbVar = hashFindVal(mdbObjs->varHash, t->name);

        dyStringPrintf(dy, "%s", mdbVar->val);
        }
    dyStringPrintf(dy, "%s", docId);

    validateName(dy->string);
    char *newString = cloneString(dy->string);

    if (hashLookup(mapping, mdbObjs->obj) != NULL)
        errAbort("multiple mappings for %s\n", mdbObjs->obj);

    hashAdd(mapping, mdbObjs->obj, newString);
    //printf("changing %s to %s\n", mdbObjs->obj, dy->string);
    }

return mapping;
}

char *getSymLinkFile(char *path)
{
char buffer[10 * 1024];

int size;
if ((size = readlink(path, buffer, sizeof buffer)) > 0)
    {
    buffer[size] = 0;
    return cloneString(buffer);
    }

errAbort("couldn't get link from %s\n", path);

return NULL;
}

char *mayRenameFile(struct sqlConnection *conn, char *table, char *oldFileName, char *newFileName, char *downDir)
// this is a table with a fileName field, this may point to the same file
// that we renamed earlier, so we don't want to rename it again
{
char buffer[10 * 1024];
char fileName[10 * 1024];

sqlSafef(buffer, sizeof buffer, "select fileName from %s limit 1", table);
if (sqlQuickQuery(conn, buffer, fileName, sizeof fileName) == NULL)
    errAbort("couldn't get fileName from table %s\n", table);

char *link = getSymLinkFile(fileName);

#ifdef NOTNOW
verbose(2,"got fileName %s\n", fileName);

FILE *f = fopen(fileName, "r");
if (f == NULL)
    errAbort("fileName %s from table %s can't be opened", fileName, table);
else
    fclose(f);
#endif

safef(buffer, sizeof buffer, "%s/%s", downDir, oldFileName);
if (differentString(link, buffer))
    errAbort("symlink to '%s' in table '%s', is not the same as '%s'\n", link,
        table, buffer);


if (!doTest)
    {
    verbose(2, "unlinking file %s\n", fileName);
    }

char *ptr = strrchr(fileName, '/');
if (ptr == NULL)
    errAbort("can't find '/' in %s\n", fileName);
ptr++;

int bufferLen = sizeof(fileName) - (ptr - fileName);
safef(ptr, bufferLen, "%s", newFileName);

safef(buffer, sizeof buffer, "%s/%s", downDir, newFileName);

if (!doTest)
    {
    verbose(2, "linking %s to %s\n", buffer, fileName);
    }

return cloneString(fileName);
}

void renameTable(char *database, char *oldTableName, char *newTableName,
    char *oldFileName, char *newFileName, char *downDir)
{
struct sqlConnection *conn = sqlConnect(database);

if (sqlTableExists(conn, oldTableName))
    {
    // do we need to change the gbdb fileName ?
    int result = sqlFieldIndex(conn, oldTableName, "fileName");

    if (result >= 0)
        {
        // this may be the same file we just renamed
        char *newPath = mayRenameFile(conn, oldTableName, oldFileName, 
            newFileName, downDir);

        if (!doTest && (newPath != NULL))
            {
            // change fileName in table
            char query[10 * 1024];

            sqlSafef(query, sizeof query, "update %s set fileName='%s'", 
                oldTableName, newPath);

            verbose(2, "sending query '%s' to database\n", query);

            }
        }

    if (!doTest)
        {
        verbose(2, "renaming table %s to %s\n", oldTableName, newTableName);
        //sqlRenameTable(conn, oldTableName, newTableName);
        }
    }
else
    errAbort("can't find table %s in database %s\n", 
        oldTableName, database);
}

char *replaceFront(char *string, char *oldFront, char *newFront)
{
if (!startsWith(oldFront, string))
    errAbort("fileName %s doesn't start with %s", string, oldFront);

char *oldEnd = string + strlen(oldFront);
char buffer[10 * 1024];

safef(buffer, sizeof buffer, "%s%s",newFront, oldEnd);

return cloneString(buffer);
}

void renameFile(char *downDir, char *old, char *new)
{
char oldPath[10 * 1024];
safef(oldPath, sizeof oldPath, "%s/%s", downDir, old);
FILE *f = fopen(oldPath, "r");
if (f == NULL)
    errAbort("fileName %s can't be opened", oldPath);
else
    fclose(f);

char newPath[10 * 1024];
safef(newPath, sizeof newPath, "%s/%s", downDir, new);

if (!doTest)
    {
    verbose(2, "renaming path %s to %s\n", oldPath, newPath);
    //rename(oldPath, newPath);
    }
}


void editMdbObj(char *database, char *downDir, struct mdbObj *mdbObj, 
    char *old, char *new)
{
mdbObj->obj = new;

char *newFileName = NULL;
char *oldFileName = NULL;
struct mdbVar *mdbVar;

mdbVar = hashFindVal(mdbObj->varHash, "fileName");
if (mdbVar != NULL)
    {
    oldFileName = mdbVar->val;
    mdbVar->val = newFileName = replaceFront(oldFileName, old, new);
    renameFile(downDir, oldFileName, mdbVar->val);
    }

mdbVar = hashFindVal(mdbObj->varHash, "tableName");
if (mdbVar != NULL)
    {
    char *oldTableName = mdbVar->val;
    mdbVar->val = replaceFront(oldTableName, old, new);
    renameTable(database, oldTableName, mdbVar->val, oldFileName, newFileName,
        downDir);
    }
}

void outMetaDb(char *database, char *downDir, char *outDir, 
    struct hash *mdbHash, struct hash *mapping)
{
struct hashCookie cook = hashFirst(mdbHash);
struct hashEl *hel, *hel2;
struct mdbObj *mdbObjs = NULL;

while((hel = hashNext(&cook)) != NULL)
    {
    char *objName = hel->name;
    struct mdbObj *mdbObj = hel->val;

    slAddHead(&mdbObjs, mdbObj);

    if ((hel2 = hashLookup(mapping, objName)) != NULL)
        editMdbObj(database, downDir, mdbObj, objName, (char *)hel2->val);
    }

slReverse(&mdbObjs);

char buffer[10 * 1024];
safef(buffer, sizeof buffer, "%s/metaDb.ra", outDir);

mdbObjPrintToFile(mdbObjs, TRUE, buffer);
}

void outTrackDbSed(char *outDir, struct hash *mapping)
{
}

void metaRename(char *database, char *objNames, char *metaDbName,
    char *trackDbName, char *termFile, char *downloadDir, char *outDir)
/* metaRename - rename a list of metaData objs based on metaData. */
{
struct mdbObj *mdbObjs;
struct hash *mdbHash = getMetaDbHash(metaDbName, &mdbObjs);
struct slName *objList = slNameLoadReal(objNames);
struct mdbObj *renameMdbList = getObjsInMeta(objList, mdbHash);
struct slName *terms = slNameLoadReal(termFile);

verifyTerms(renameMdbList, terms);
struct hash *mapping = checkNames(renameMdbList, terms);
makeDirs(outDir);

outMetaDb(database, downloadDir, outDir, mdbHash, mapping);
outTrackDbSed(outDir, mapping);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 8)
    usage();
doTest = optionExists("test");
metaRename(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[6]);
return 0;
}
