#include "gbLoadedTbl.h"
#include "gbDefs.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "localmem.h"
#include "hash.h"
#include "jksql.h"

static char const rcsid[] = "$Id: gbLoadedTbl.c,v 1.1 2003/06/03 01:27:46 markd Exp $";

static char* GB_LOADED_TBL = "gbLoaded";
static char* createSql =
"create table gbLoaded ("
  "srcDb enum('GenBank','RefSeq') not null,"   /* source database */
  "type enum('EST','mRNA') not null,"          /* mRNA or EST */
  "loadRelease char(8) not null,"              /* release version */
  "loadUpdate char(10) not null,"              /* update date or full */
  "accPrefix char(2) not null,"                /* acc prefix for ESTs */
  "time timestamp not null,"                   /* time entry was added */
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

static struct gbLoaded *addEntry(struct gbLoadedTbl* loadedTbl,
                                 struct gbRelease* release,
                                 char *updateShort, unsigned type,
                                 char *accPrefix)
/* add an entry to the table. */
{
struct gbLoaded *loaded;
char key[KEY_BUF_SIZE];

lmAllocVar(loadedTbl->entryHash->lm, loaded);
loaded->srcDb = release->srcDb;
loaded->loadRelease = release->version;
loaded->loadUpdate = lmCloneString(loadedTbl->entryHash->lm, updateShort);
loaded->type = type;
if (accPrefix != NULL)
    loaded->accPrefix = lmCloneString(loadedTbl->entryHash->lm, accPrefix);

makeKey(key, release, updateShort, type, accPrefix);
hashAdd(loadedTbl->entryHash, key, loaded);
return loaded;
}

struct gbLoadedTbl* gbLoadedTblNew(struct sqlConnection *conn)
/* create a new object for the loaded tables.  Create the table if it doesn't
 * exist. */
{
struct gbLoadedTbl* loadedTbl;

AllocVar(loadedTbl);
loadedTbl->releaseHash = hashNew(8);
loadedTbl->entryHash = hashNew(14);

if (!sqlTableExists(conn, GB_LOADED_TBL))
    sqlRemakeTable(conn, GB_LOADED_TBL, createSql);

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
      "SELECT loadUpdate, type, accPrefix FROM gbLoaded"
      " WHERE (srcDb = '%s') AND (loadRelease = '%s')",
      gbSrcDbName(release->srcDb), release->version);
result = sqlGetResult(conn, query);
while ((row = sqlNextRow(result)) != NULL)
    addEntry(loadedTbl, release, row[0], gbParseType(row[1]),
             ((strlen(row[2]) > 0) ? row[2] : NULL));
}

void gbLoadedTblUseRelease(struct gbLoadedTbl* loadedTbl,
                           struct sqlConnection *conn,
                           struct gbRelease *release)
/* If the specified release has not been loaded from the database, load it.
 * Must be called before using a release. */
{
if (hashLookup(loadedTbl->releaseHash, release->name) == NULL)
    {
    loadRelease(loadedTbl, conn, release);
    hashAdd(loadedTbl->releaseHash, release->name, release);
    }
}

boolean gbLoadedTblIsLoaded(struct gbLoadedTbl* loadedTbl,
                            struct gbSelect *select)
/* Check if the type and accession has been loaded for this update */
{
char key[KEY_BUF_SIZE];
assertHaveRelease(loadedTbl, select->release);

makeKey(key, select->release, select->update->shortName, select->type,
        select->accPrefix);
return (hashLookup(loadedTbl->entryHash, key) != NULL);
}

void gbLoadedTblAdd(struct gbLoadedTbl* loadedTbl,
                    struct gbSelect *select)
/* add an entry to the table */
{
struct gbLoaded *loaded;
assertHaveRelease(loadedTbl, select->release);

loaded = addEntry(loadedTbl, select->release, select->update->shortName,
                  select->type, select->accPrefix);
loaded->next = loadedTbl->uncommitted;
loadedTbl->uncommitted = loaded;
}

static void insertRow(struct sqlConnection *conn, struct gbLoaded *loaded)
/* insert a row into the table */
{
char query[512];
safef(query, sizeof(query), 
      "INSERT INTO %s VALUES('%s', '%s', '%s', '%s', '%s', NULL)",
      GB_LOADED_TBL, gbSrcDbName(loaded->srcDb), gbTypeName(loaded->type), 
      loaded->loadRelease, loaded->loadUpdate,
      ((loaded->accPrefix == NULL) ? "" : loaded->accPrefix));
sqlUpdate(conn, query);
}

void gbLoadedTblCommit(struct gbLoadedTbl* loadedTbl,
                       struct sqlConnection *conn)
/* commit pending changes */
{
for (; loadedTbl->uncommitted != NULL;
     loadedTbl->uncommitted = loadedTbl->uncommitted->next)
    insertRow(conn, loadedTbl->uncommitted);
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

