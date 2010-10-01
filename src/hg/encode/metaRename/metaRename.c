/* metaRename - rename a list of metaData objs based on metaData. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "mdb.h"
#include "dystring.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaRename - rename a list of metaData objs based on metaData\n"
  "usage:\n"
  "   metaRename database object.lst metaDb.ra trackDb.ra term.txt outDir\n"
  "Without options, checks to see if rules can be followed to rename\n"
  "metaDb objNames\n\n"
  "arguments\n"
  "   database      name of database with tables\n"
  "   object.lst    list of metaDb objName's\n"
  "   metaDb.ra     metaDb ra file\n"
  "   trackDb.ra    trackDb ra file\n"
  "   term.txt      list of metadata items in name\n"
  "   outDir        if not -test, then put metaDb, trackDb, and rename.sql\n"
  "                 in outDir\n"
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

void outRenameTables(char *outDir, struct hash *mapping)
{
struct hashCookie cook = hashFirst(mapping);
char buffer[10 * 1024];
safef(buffer, sizeof buffer, "%s/rename.sql", outDir);
//FILE *f = mustOpen(buffer, "w");
struct hashEl *hel;

while((hel = hashNext(&cook)) != NULL)
    {
    }
}

void outMetaDb(char *outDir, struct hash *mdbHash, struct hash *mapping)
{
}

void outTrackDbSed(char *outDir, struct hash *mapping)
{
}

void metaRename(char *database, char *objNames, char *metaDbName,
    char *trackDbName, char *termFile, char *outDir)
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

outMetaDb(outDir, mdbHash, mapping);
outTrackDbSed(outDir, mapping);
outRenameTables(outDir, mapping);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
doTest = optionExists("test");
metaRename(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
