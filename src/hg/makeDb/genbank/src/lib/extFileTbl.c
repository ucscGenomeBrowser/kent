#include "extFileTbl.h"
#include "gbDefs.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "hash.h"
#include "jksql.h"
#include "gbFileOps.h"
#include "sqlDeleter.h"
#include "localmem.h"
#include "dystring.h"

static char const rcsid[] = "$Id: extFileTbl.c,v 1.8 2008/07/28 17:28:48 markd Exp $";

/*
 * Note: this use immediate inserts rather than batch, because the tables
 * don't tend to be that big, are generally incrementally updated, and it
 * simplified the code.
 *
 * Note: don't use autoincrement for id column, as it causes problems for
 * mysqlimport.
 */

/* Name of table */
char* EXT_FILE_TBL = "gbExtFile";
static char* createSql =
"create table gbExtFile ("
  "id int unsigned not null primary key,"  /* Unique ID. */
  "path varchar(255) not null,"   /* Full path. Ends in '/' if a dir. */
  "size bigint unsigned not null,"            /* Size of file (checked) */
                   /* Extra indices. */
  "index (path))";

static void checkEntry(struct extFile *ef)
/* check if an entry matches the actual file */
{
char lastChar = ef->path[strlen(ef->path)-1];
off_t gotSize = fileSize(ef->path);
if (gotSize < 0)
    errAbort("External file %s does not exist\n", ef->path);
if ((lastChar != '/') && (ef->size != gotSize))
    errAbort("External file %s out of sync. size in extTable: %lld, actual size %lld\n",
             ef->path, (long long)ef->size, (long long)gotSize);
}

static struct extFile *addEntry(struct extFileTbl *eft, HGID id, char* path,
                                off_t size)
/* create and add a new entry. */
{
struct hashEl* hel;
struct extFile *extFile;
char idBuf[64];
lmAllocVar(eft->pathHash->lm, extFile);
hel = hashAdd(eft->pathHash, path, extFile);
extFile->id = id;
extFile->path = hel->name;
extFile->size = size;

safef(idBuf, sizeof(idBuf), "%d", id);
hashAdd(eft->idHash, idBuf, extFile);
return extFile;
}

static void parseRow(struct extFileTbl *eft, char **row)
/* parse one row */
{
HGID id = gbParseUnsigned(NULL, row[0]);
if (id != 0)
    addEntry(eft, id, row[1], gbParseFileOff(NULL, row[2]));
}

struct extFileTbl *extFileTblLoad(struct sqlConnection *conn)
/* Load the file table from the database, creating table if it doesn't exist */
{
char **row;
struct extFileTbl *eft;
struct sqlResult *sr;

AllocVar(eft);
eft->pathHash = newHash(0);
eft->idHash = newHash(0);

if (!sqlTableExists(conn, EXT_FILE_TBL))
    sqlRemakeTable(conn, EXT_FILE_TBL, createSql);
else
    {
    /* table exists, read existing entries */
    sr = sqlGetResult(conn,"SELECT id,path,size FROM gbExtFile");
    while ((row = sqlNextRow(sr)) != NULL)
        parseRow(eft, row);
    sqlFreeResult(&sr);
    }
return eft;
}

HGID extFileTblGet(struct extFileTbl *eft, struct sqlConnection *conn,
                   char* path)
/* Lookup a file in the table.  If it doesn't exists, add it and update the
 * database. */
{
struct extFile *extFile = hashFindVal(eft->pathHash, path);
if (extFile != NULL)
    checkEntry(extFile);
else
    {
    /* insert and get auto-increment id */
    char query[PATH_LEN+64];
    off_t size = fileSize(path);
    HGID id;
    if (size < 0)
        errAbort("attempt to add non-existent file to gbExtFile table: %s",
                 path);
    id = hgGetMaxId(conn, EXT_FILE_TBL) + 1;
    safef(query, sizeof(query), "INSERT INTO gbExtFile VALUES(%d, '%s', %llu)",
          id, path, (long long)size);
    sqlUpdate(conn, query);
    extFile = addEntry(eft, id, path, size);
    }
return extFile->id;
}

struct extFile* extFileTblFindById(struct extFileTbl *eft, HGID id)
/* Get the entry for an id, or null if not found */
{
char idBuf[64];
safef(idBuf, sizeof(idBuf), "%d", id);
return hashFindVal(eft->idHash, idBuf);
}

static char *matchWildCard(struct gbSelect *select, char *filePrefix)
/* construct wildcard for match, free string when done */
{
struct dyString *wildCard = dyStringNew(0);
dyStringAppend(wildCard, "*/data/processed/");
dyStringAppend(wildCard, select->release->name);
dyStringPrintf(wildCard, "/%s/%s",
               ((select->update == NULL) ? "*" : select->update->name),
               filePrefix);
if (select->accPrefix != NULL)
    dyStringPrintf(wildCard, ".%s", select->accPrefix);
dyStringAppend(wildCard, ".fa");
return dyStringCannibalize(&wildCard);
}

static struct extFileRef *extFileRefNew(struct extFile *ef)
/* construct a reference to ef */
{
struct extFileRef *efr;
AllocVar(efr);
efr->extFile = ef;
return efr;
}

static void matchForFileType(struct extFileTbl *eft, struct gbSelect *select,
                             char *filePrefix, struct extFileRef **extFiles)
/* match based on file name prefix, used so we can get pep.fa files. */
{
char *wildCard = matchWildCard(select, filePrefix);
struct hashCookie cookie = hashFirst(eft->pathHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct extFile *ef = hel->val;
    if (wildMatch(wildCard, ef->path))
        slSafeAddHead(extFiles, extFileRefNew(ef));
    }
    freeMem(wildCard);
}

struct extFileRef* extFileTblMatch(struct extFileTbl *eft, struct gbSelect *select)
/* get list of files with paths matching the specified list.  For refseqs,
 * protein files are returned too.  Free results with slFreeList. */
{
struct extFileRef *extFiles = NULL;
if (select->type & GB_MRNA)
    matchForFileType(eft, select, "mrna", &extFiles);
if (select->type & GB_EST)
    matchForFileType(eft, select, "est", &extFiles);
if (select->release->srcDb & GB_REFSEQ)
    matchForFileType(eft, select, "pep", &extFiles);
return extFiles;
}

void extFileTblFree(struct extFileTbl** eftPtr)
/* Free a extFileTbl object */
{
struct extFileTbl* eft = *eftPtr;
if (eft != NULL)
    {
    hashFree(&eft->pathHash);
    hashFree(&eft->idHash);
    freez(eftPtr);
    }
}

static struct slName* getUnusedIds(struct sqlConnection *conn)
/* generate a list of extFile ids that are not referenced in the seq table. */
{
struct slName *idList = NULL;
if (sqlTableExists(conn, EXT_FILE_TBL))
    {
    char **row;
    struct sqlResult *sr
        = sqlGetResult(conn, "SELECT gbExtFile.id FROM gbExtFile "
                       "LEFT JOIN gbSeq on (gbSeq.gbExtFile = gbExtFile.id)"
                       "WHERE (gbSeq.gbExtFile IS NULL);");
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct slName *id = slNameNew(row[0]);
        slAddHead(&idList, id);
        }
    sqlFreeResult(&sr);
    }
return idList;
}

static void cleanUnusedEntries(struct sqlConnection *conn,
                               struct slName* idList,
                               boolean verbEnabled)
/* Remove unreferenced rows */
{
struct sqlDeleter* deleter = sqlDeleterNew(NULL, verbEnabled);
struct slName* id;
for (id = idList; id != NULL; id = id->next)
    sqlDeleterAddAcc(deleter, id->name);
sqlDeleterDel(deleter, conn, EXT_FILE_TBL, "id");
sqlDeleterFree(&deleter);
}

static boolean haveMrnaForPep(struct extFileTbl *extFileTbl,
                              struct extFile *pepExtFile)
/* see if there is a mrna.fa file for this pep.fa file */
{
char mrnaFaPath[PATH_LEN];
splitPath(pepExtFile->path, mrnaFaPath, NULL, NULL);
strcat(mrnaFaPath, "mrna.fa");
return (hashLookup(extFileTbl->pathHash, mrnaFaPath) != NULL);
}

static void cleanRefPep(struct sqlConnection *conn)
/* remove pep.fa file that are not associated with mrna.fa files */
{
/* slow way to do it, but this table is small */
struct extFileTbl *extFileTbl = extFileTblLoad(conn);
struct hashCookie pepScan = hashFirst(extFileTbl->idHash);
struct hashEl *pepEl;
char sql[256];

while ((pepEl = hashNext(&pepScan)) != NULL)
    {
    struct extFile *pepExtFile = pepEl->val;
    if (endsWith(pepExtFile->path, "/pep.fa")
        && !haveMrnaForPep(extFileTbl, pepExtFile))
        {
        safef(sql, sizeof(sql), "delete from gbExtFile where id = %d",
              pepExtFile->id);
        sqlUpdate(conn, sql);
        }
    }
}

void extFileTblClean(struct sqlConnection *conn, boolean verbEnabled)
/* Remove rows in the gbExtFile table that are not referenced in the gbSeq
 * table.  This is fast.  Also remove pep.fa file that are not associated with
 * any mrna.fa file.  This is a hack that gets around the refseq peptides not
 * being in gbSeq table. */
{
struct slName* idList = getUnusedIds(conn);
if (idList != NULL)
    cleanUnusedEntries(conn, idList, verbEnabled);
slFreeList(&idList);

cleanRefPep(conn);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

