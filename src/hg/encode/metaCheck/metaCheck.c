/* metaCheck - a program to validate that tables, files, and trackDb entries exist. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "mdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaCheck - a program to validate that tables, files, and trackDb entries exist\n"
  "usage:\n"
  "   metaCheck database composite metaDb.ra trackDb.ra downloadDir\n"
  "options:\n"
  "   -outMdb=file.ra     output cruft-free metaDb ra file\n"
  );
}

char *outMdb = NULL;

static struct optionSpec options[] = {
   {"outMdb", OPTION_STRING},
   {NULL, 0},
};

void checkMetaTables(struct mdbObj *mdbObj, char *database)
{
struct sqlConnection *conn = sqlConnect(database);

for(; mdbObj != NULL; mdbObj=mdbObj->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObj->varHash, "objType");
    if (mdbVar == NULL)
        {
        warn("objType not found in object %s\n", mdbObj->obj);
        continue;
        }
    if (differentString(mdbVar->val, "table"))
        continue;

    mdbObj->deleteThis = FALSE;
    mdbVar = hashFindVal(mdbObj->varHash, "tableName");

    if (mdbVar == NULL)
        {
        mdbObj->deleteThis = TRUE;
        warn("tableName not found in object %s\n", mdbObj->obj);
        continue;
        }

    char *tableName = mdbVar->val;

    if (!sqlTableExists(conn, tableName))
        {
        mdbObj->deleteThis = TRUE;
        warn("tableName %s: not found in object %s\n",tableName, mdbObj->obj);
        }
    }
sqlDisconnect(&conn);
}

void checkMetaFiles(struct mdbObj *mdbObj, char *downDir)
{
for(; mdbObj != NULL; mdbObj=mdbObj->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObj->varHash, "objType");
    if (mdbVar == NULL)
        {
        warn("objType not found in object %s\n", mdbObj->obj);
        continue;
        }
    if (differentString(mdbVar->val, "file"))
        continue;

    mdbObj->deleteThis = FALSE;
    mdbVar = hashFindVal(mdbObj->varHash, "composite");

    if (mdbVar == NULL)
        {
        warn("composite not found in object %s\n", mdbObj->obj);
        continue;
        }
    char *composite = mdbVar->val;

    mdbVar = hashFindVal(mdbObj->varHash, "fileName");

    if (mdbVar == NULL)
        {
        mdbObj->deleteThis = TRUE;
        warn("fileName not found in object %s\n", mdbObj->obj);
        continue;
        }

    char *fileName = mdbVar->val;
    char buffer[10 * 1024];

    safef(buffer, sizeof buffer, "%s/%s/%s", downDir, composite, fileName);

    if (!fileExists(buffer))
        {
        mdbObj->deleteThis = TRUE;
        warn("fileName %s: not found in object %s\n",buffer, mdbObj->obj);
        }
    }
}

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

struct hash *getTrackDbHash(char *trackDb, struct trackDb **head)
{
struct trackDb *trackObjs = trackDbFromRa(trackDb), *trackObj;
struct hash *hash = newHash(10);

for(trackObj = trackObjs; trackObj; trackObj = trackObj->next)
    {
    char *table = trackObj->table;
    if (table == NULL)
        table = trackObj->track;
    hashAdd(hash, table, trackObj);
    }

*head = trackObjs;
return hash;
}

void checkMetaTrackDb(struct mdbObj *mdbObj, struct hash *trackHash)
{
for(; mdbObj != NULL; mdbObj=mdbObj->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObj->varHash, "objType");
    if (mdbVar == NULL)
        {
        warn("objType not found in object %s\n", mdbObj->obj);
        continue;
        }
    if (differentString(mdbVar->val, "table"))
        continue;

    if (mdbObj->deleteThis)
        continue;

    mdbVar = hashFindVal(mdbObj->varHash, "tableName");

    char *tableName = mdbVar->val;

    if (hashLookup(trackHash, tableName) == NULL)
        {
        warn("table %s: not found in trackDb\n",tableName);
        }
    }
}

struct mdbObj *dropDeleted(struct mdbObj *head)
{
struct mdbObj *next, *mdbObj, *prev;
for(mdbObj = head; mdbObj != NULL; mdbObj = next)
    {
    next = mdbObj->next;
    if (mdbObj->deleteThis)
        {
        if (prev == NULL)
            head = next;
        else 
            prev->next = next;
        }
    else
        prev = mdbObj;
    }

return head;
}

void checkDbTables(char *database, char *composite, struct hash *mdbHash)
// search the database for tables that begin with composite and check
// to see if they are represented in the metaDb.ra
{
}

void checkTrackDb(struct trackDb *trackDbObjs, char *composite,
    struct hash *mdbHash)
// check through trackDb for tables that begin with composite
// and check to see if they are represented in the metaDb.ra
{
}

void metaCheck(char *database, char *composite, char *metaDb, char *trackDb, 
    char *downDir)
/* metaCheck - a program to validate that tables, files, and trackDb entries exist. */
{
struct mdbObj *mdbObjs = NULL;
struct hash *mdbHash = getMetaDbHash(metaDb, &mdbObjs);
struct trackDb *trackDbObjs = NULL;
struct hash *trackHash = getTrackDbHash(trackDb, &trackDbObjs);

checkMetaTables(mdbObjs, database);
checkMetaFiles(mdbObjs, downDir);
checkMetaTrackDb(mdbObjs, trackHash);

checkDbTables(database, composite, mdbHash);
checkTrackDb(trackDbObjs, composite, mdbHash);

if (outMdb)
    {
    mdbObjs = dropDeleted(mdbObjs);
    mdbObjPrintToFile(mdbObjs, TRUE, outMdb);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
outMdb = optionVal("outMdb", outMdb);

metaCheck(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
