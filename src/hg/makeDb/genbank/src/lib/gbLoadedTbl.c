#include "gbLoadedTbl.h"
#include "gbDefs.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "localmem.h"
#include "hash.h"
#include "jksql.h"

static char const rcsid[] = "$Id: gbLoadedTbl.c,v 1.3 2005/11/06 22:56:26 markd Exp $";

static char* GB_LOADED_TBL = "gbLoaded";
static char* createSql =
"create table gbLoaded ("
  "srcDb enum('GenBank','RefSeq') not null,"   /* source database */
  "type enum('EST','mRNA') not null,"          /* mRNA or EST */
  "loadRelease char(8) not null,"              /* release version */
  "loadUpdate char(10) not null,"              /* update date or full */
  "accPrefix char(2) not null,"                /* acc prefix for ESTs */
  "time timestamp not null,"                   /* time entry was added */
  "extFileUpdated tinyint(1) not null,"        /* has the extFile entries been
                                                * updated for this partation
                                                * of the release (full
                                                * only) */
  "index(srcDb,loadRelease))";

#define KEY_BUF_SIZE 128

/* assert that a release has been loaded */
#define assertHaveRelease(loadedTbl, release) \
    assert(hashLookup((loadedTbl)->releaseHash, (release)->name) != NULL)

static void makeKey(char *key, struct gbRelease *release, char *updateShort,
                    unsigned type, char *accPrefix)
/* generate a hash key for an update/type/accPrefix (which make be null)  */
{
safef(key, KEY_BUF_SIZE, "%s %s %s %s", release->name, updateShort,
      gbFmtSelect(type), ((accPrefix == NULL) ? "" : accPrefix));
}

static struct hashEl *getRelease(struct gbLoadedTbl* loadedTbl,
                                 struct gbRelease* release)
/* get the hash entry for a release */
{
struct hashEl *relHashEl = hashLookup(loadedTbl->releaseHash, release->name);
assert(relHashEl != NULL);
return relHashEl;
}

static struct gbLoaded *addEntry(struct gbLoadedTbl* loadedTbl,
                                 struct gbRelease* release,
                                 char *updateShort, unsigned type,
                                 char *accPrefix, boolean extFileUpdated)
/* add an entry to the table. */
{
struct gbLoaded *loaded, *relHead;
struct hashEl *relHashEl = getRelease(loadedTbl, release);
char key[KEY_BUF_SIZE];

lmAllocVar(loadedTbl->entryHash->lm, loaded);
loaded->srcDb = release->srcDb;
loaded->loadRelease = release->version;
loaded->loadUpdate = lmCloneString(loadedTbl->entryHash->lm, updateShort);
loaded->type = type;
if (accPrefix != NULL)
    loaded->accPrefix = lmCloneString(loadedTbl->entryHash->lm, accPrefix);
loaded->extFileUpdated = extFileUpdated;

/* add to entry hash */
makeKey(key, release, updateShort, type, accPrefix);
hashAdd(loadedTbl->entryHash, key, loaded);

/* add to list for this release */
relHead = relHashEl->val;
loaded->relNext = relHead;
relHead = loaded;
relHashEl->val = relHead;

return loaded;
}

static struct gbLoaded *getEntry(struct gbLoadedTbl* loadedTbl,
                                 struct gbSelect *select,
                                 char *updateOverride)
/* get lookup an entry in the table, or return null if not found.  if
 * updateOverride is not NULL, use that name instead of the update in
 * select. */
{
char key[KEY_BUF_SIZE];
struct gbLoaded *loaded;
assertHaveRelease(loadedTbl, select->release);
makeKey(key, select->release,
        ((updateOverride != NULL) ? updateOverride : select->update->shortName),
        select->type, select->accPrefix);
loaded = hashFindVal(loadedTbl->entryHash, key);
return loaded;
}
static struct gbLoaded *mustGetEntry(struct gbLoadedTbl* loadedTbl,
                                     struct gbSelect *select,
                                     char *updateOverride)
/* get lookup an entry in the table, or abort if not found.  if updateOverride
 * is not NULL, use that name instead of the update in select. */
{
struct gbLoaded *loaded = getEntry(loadedTbl, select, updateOverride);
if (loaded == NULL)
    errAbort("can't find entry that must be there");
return loaded;
}

struct gbLoaded *gbLoadedTblGetEntry(struct gbLoadedTbl* loadedTbl,
                                     struct gbSelect *select)
/* get the specified entry, or NULL.  It may not have been commited yet */
{
return getEntry(loadedTbl, select, NULL);
}

boolean gbLoadedTblHasEntry(struct gbLoadedTbl* loadedTbl,
                            struct gbSelect *select)
/* Check if the the specified entry exists. It may not have been commited
 * yet */
{
return (getEntry(loadedTbl, select, NULL) != NULL);
}

static void addedExtFileUpdCol(struct sqlConnection *conn)
/* add the new extFileUpdated column to a table that doesn't have it */
{
char sql[128];
safef(sql, sizeof(sql), "ALTER TABLE %s ADD COLUMN boolean extFileUpdated not null",
      GB_LOADED_TBL);
sqlUpdate(conn, sql);
}

struct gbLoadedTbl* gbLoadedTblNew(struct sqlConnection *conn)
/* create a new object for the loaded tables.  Create the table if it doesn't
 * exist. */
{
struct gbLoadedTbl* loadedTbl;

AllocVar(loadedTbl);
loadedTbl->releaseHash = hashNew(10);
loadedTbl->entryHash = hashNew(19);

if (!sqlTableExists(conn, GB_LOADED_TBL))
    sqlRemakeTable(conn, GB_LOADED_TBL, createSql);
else if (sqlFieldIndex(conn, GB_LOADED_TBL, "extFileUpdated") < 0)
    addedExtFileUpdCol(conn);

return loadedTbl;
}

static void loadRelease(struct gbLoadedTbl* loadedTbl, 
                        struct sqlConnection *conn, struct gbRelease *release)
/* load table rows for a release */
{
char query[256];
struct sqlResult *result;
char **row;
safef(query, sizeof(query),
      "SELECT loadUpdate, type, accPrefix, extFileUpdated FROM gbLoaded"
      " WHERE (srcDb = '%s') AND (loadRelease = '%s')",
      gbSrcDbName(release->srcDb), release->version);
result = sqlGetResult(conn, query);
while ((row = sqlNextRow(result)) != NULL)
    addEntry(loadedTbl, release, row[0], gbParseType(row[1]),
             ((strlen(row[2]) > 0) ? row[2] : NULL),
             sqlUnsigned(row[3]));
}

void gbLoadedTblUseRelease(struct gbLoadedTbl* loadedTbl,
                           struct sqlConnection *conn,
                           struct gbRelease *release)
/* If the specified release has not been loaded from the database, load it.
 * Must be called before using a release. */
{
if (hashLookup(loadedTbl->releaseHash, release->name) == NULL)
    {
    hashAdd(loadedTbl->releaseHash, release->name, NULL);
    loadRelease(loadedTbl, conn, release);
    }
}

boolean gbLoadedTblIsLoaded(struct gbLoadedTbl* loadedTbl,
                            struct gbSelect *select)
/* Check if the type and accPrefix has been loaded for this update */
{
struct gbLoaded *loaded = getEntry(loadedTbl, select, NULL);
/* entries might be added during load process for bookkeeping, so 
 * new doesn't count as loaded.  It is cleared during commit. */
return (loaded != NULL) && !loaded->isNew;
}

void gbLoadedTblAdd(struct gbLoadedTbl* loadedTbl,
                    struct gbSelect *select)
/* add an entry to the table */
{
struct gbLoaded *loaded
    = addEntry(loadedTbl, select->release, select->update->shortName,
               select->type, select->accPrefix, FALSE);
/* flag as uncommited */
loaded->isNew = TRUE;
loaded->isDirty = TRUE;
slAddHead(&loadedTbl->uncommitted, loaded);
}

boolean gbLoadedTblExtFileUpdated(struct gbLoadedTbl* loadedTbl,
                                  struct gbSelect *select)
/* Check if the type and accPrefix has had their extFile entries update
 * for this release. */
{
struct gbLoaded *loaded = mustGetEntry(loadedTbl, select, GB_FULL_UPDATE);
return loaded->extFileUpdated;
}

static boolean samePartition(struct gbSelect *select,
                             struct gbLoaded *loaded)
/* check if an entry is in the selected partition; assumes releases are 
 * the same*/
{
/* partation doesn't include update */
return (select->type == loaded->type)
    && (((select->accPrefix == NULL) && (loaded->accPrefix == NULL))
        || ((select->accPrefix != NULL) && sameString(select->accPrefix, loaded->accPrefix)));
}

static void setExtFileUpdated(struct gbLoadedTbl* loadedTbl,
                              struct gbLoaded *loaded)
/* flag an entry as having it's extFiles updated */
{
loaded->extFileUpdated = TRUE;
if (!loaded->isDirty)
    {
    loaded->isDirty = TRUE;
    slAddHead(&loadedTbl->uncommitted, loaded);
    }
}

void gbLoadedTblSetExtFileUpdated(struct gbLoadedTbl* loadedTbl,
                                  struct gbSelect *select)
/* Flag that type and accPrefix has had their extFile entries update
 * for this release. */
{
/* flag all entries for this partation of the release */
struct hashEl *relHashEl = getRelease(loadedTbl, select->release);
struct gbLoaded *loaded;

for (loaded = relHashEl->val; loaded != NULL; loaded = loaded->relNext)
    {
    if (samePartition(select, loaded))
        setExtFileUpdated(loadedTbl, loaded);
    }
}

static void insertRow(struct sqlConnection *conn, struct gbLoaded *loaded)
/* insert a row into the table */
{
char query[512];
/* null inserts current time */
safef(query, sizeof(query), 
      "INSERT INTO %s VALUES('%s', '%s', '%s', '%s', '%s', NULL, '%d')",
      GB_LOADED_TBL, gbSrcDbName(loaded->srcDb), gbTypeName(loaded->type), 
      loaded->loadRelease, loaded->loadUpdate,
      ((loaded->accPrefix == NULL) ? "" : loaded->accPrefix),
      loaded->extFileUpdated);
sqlUpdate(conn, query);
}

static void updateRow(struct sqlConnection *conn, struct gbLoaded *loaded)
/* update a row in the table.  Only the extFileUpdated field can be updated */
{
char query[512];
safef(query, sizeof(query), "UPDATE %s SET extFileUpdated=%d",
      GB_LOADED_TBL,  loaded->extFileUpdated);
sqlUpdate(conn, query);
}

void gbLoadedTblCommit(struct gbLoadedTbl* loadedTbl,
                       struct sqlConnection *conn)
/* commit pending changes */
{
struct gbLoaded *loaded;
for (loaded = loadedTbl->uncommitted;  loaded != NULL; loaded = loaded->next)
    {
    assert(loaded->isDirty);
    if (loaded->isNew)
        insertRow(conn, loaded);
    else
        updateRow(conn, loaded);
    loaded->isNew = loaded->isDirty = FALSE;
    }
loadedTbl->uncommitted = NULL;
}

void gbLoadedTblFree(struct gbLoadedTbl** loadedTblPtr)
/* Free the object */
{
struct gbLoadedTbl *loadedTbl = *loadedTblPtr;
if (loadedTbl != NULL)
    {
    assert(loadedTbl->uncommitted == NULL);
    hashFree(&loadedTbl->releaseHash);
    hashFree(&loadedTbl->entryHash);
    free(loadedTbl);
    *loadedTblPtr = NULL;
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

