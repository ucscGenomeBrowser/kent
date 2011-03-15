#include "gbStatusTbl.h"
#include "gbDefs.h"
#include "gbEntry.h"
#include "gbFileOps.h"
#include "sqlUpdater.h"
#include "sqlDeleter.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "errabort.h"
#include "jksql.h"

static char const rcsid[] = "$Id: gbStatusTbl.c,v 1.5 2006/08/30 05:06:41 markd Exp $";

// FIXME: the stateChg flags vs the list is a little confusing; maybe
// have simpler lists (new, deleted, change, orphaned)
// FIXME: use the select record in here


/* name of status table */
char* GB_STATUS_TBL = "gbStatus";

/* sql to create the table */
static char* createSql =
"create table gbStatus ("
  "acc char(12) not null primary key,"         /* Genbank accession */
  "version smallint unsigned not null,"        /* genbank version number */
  "modDate date not null,"                     /* last modified date */
  "type enum('EST','mRNA') not null,"	       /* full length or EST */
  "srcDb enum('GenBank','RefSeq') not null,"   /* source database */
  "orgCat enum('native','xeno') not null,"     /* Organism category */
  "gbSeq int unsigned not null,"               /* id in gbSeq table */
  "numAligns int unsigned not null,"           /* number of alignments */
  "seqRelease char(8) not null,"               /* release version where
                                                * sequence was obtained */
  "seqUpdate char(10) not null,"               /* update where sequence was
                                                * obtained (date or "full" */
  "metaRelease char(8) not null,"              /* release version where
                                                * metadata was obtained */
  "metaUpdate char(10) not null,"              /* update where metadata was
                                                * obtained (date or "full") */
  "extRelease char(8) not null,"               /* release version containing
                                                * external file */
  "extUpdate char(10) not null,"               /* update containing external
                                                * file (date or "full") */
  "time timestamp not null,"                   /* time entry was inserted,
                                                * auto-updated by mysql */
  "unique(gbSeq),"
  /* Index for selecting ESTs based on first two letters of the accession */
  "index typeSrcDbAcc2(type, srcDb, acc(2)))";

static char *GB_STAT_ALL_COLS = "acc,version,modDate,type,srcDb,orgCat,"
"gbSeq,numAligns,seqRelease,seqUpdate,metaRelease,metaUpdate,"
"extRelease,extUpdate,time";


char* gbStatusTblGetStr(struct gbStatusTbl *statusTbl, char *str)
/* allocate a unique string for the table */
{
struct hashEl* hel = hashLookup(statusTbl->strPool, str);
if (hel == NULL)
    hel = hashAdd(statusTbl->strPool, str, NULL);
return hel->name;
}

static struct sqlResult* loadQuery(struct sqlConnection *conn, 
                                   unsigned select, char* accPrefix, char *columns)
/* generate a query to load the table */
{
char query[1024];
int len;
boolean haveWhere = FALSE;

len = safef(query, sizeof(query),
            "SELECT acc,version,modDate,type,srcDb,orgCat,gbSeq,numAligns,"
            "seqRelease,seqUpdate,metaRelease,metaUpdate,"
            "extRelease,extUpdate,time FROM gbStatus");
if ((select & GB_TYPE_MASK) != (GB_MRNA|GB_EST))
    {
    /* type subset */
    len += safef(query+len, sizeof(query)-len,
                 " WHERE (type='%s')", gbTypeName(select & GB_TYPE_MASK));
    haveWhere = TRUE;
    }
if ((select & GB_SRC_DB_MASK) != (GB_GENBANK|GB_REFSEQ))
    {
    len += safef(query+len, sizeof(query)-len,
                 " %s (srcDb='%s')", (haveWhere ? " AND " : " WHERE "),
                 gbSrcDbName(select & GB_SRC_DB_MASK));
    haveWhere = TRUE;
    }
if (accPrefix != NULL)
    {
    len += safef(query+len, sizeof(query)-len,
                 " %s (acc LIKE '%s%%')", (haveWhere ? " AND " : " WHERE "),
                 accPrefix);
    haveWhere = TRUE;
    }

return sqlGetResult(conn, query);
}

static void processRow(struct gbStatusTbl *statusTbl, char** row,
                       gbStatusLoadSelect* selectFunc, void* clientData
 )
/* process a row of the table, calling the select function on a tmp
 * entry */
{
struct gbStatus tmpStatus;
ZeroVar(&tmpStatus);
tmpStatus.acc = row[0];
tmpStatus.version = gbParseUnsigned(NULL, row[1]);
tmpStatus.modDate = gbParseDate(NULL, row[2]);
tmpStatus.type = gbParseType(row[3]);
tmpStatus.srcDb = gbParseSrcDb(row[4]);
tmpStatus.orgCat = gbParseOrgCat(row[5]);
tmpStatus.gbSeqId = gbParseUnsigned(NULL, row[6]);
tmpStatus.numAligns = gbParseUnsigned(NULL, row[7]);
tmpStatus.seqRelease = row[8];
tmpStatus.seqUpdate = row[9];
tmpStatus.metaRelease = row[10];
tmpStatus.metaUpdate = row[11];
tmpStatus.extRelease = row[12];
tmpStatus.extUpdate = row[13];
tmpStatus.time = gbParseTimeStamp(row[14]);

selectFunc(statusTbl, &tmpStatus, clientData);
}

static void loadTable(struct gbStatusTbl *statusTbl,
                      struct sqlConnection *conn, 
                      unsigned select, char* accPrefix,
                      gbStatusLoadSelect* selectFunc,
                      void* clientData)
/* load an existing table */
{
/* table exists, read existing entries */
char **row;
struct sqlResult *sr =  loadQuery(conn, select, accPrefix, GB_STAT_ALL_COLS);
while ((row = sqlNextRow(sr)) != NULL)
    processRow(statusTbl, row, selectFunc, clientData);
sqlFreeResult(&sr);
}

struct gbStatusTbl *gbStatusTblSelectLoad(struct sqlConnection *conn,
                                          unsigned select, char* accPrefix,
                                          gbStatusLoadSelect* selectFunc,
                                          void* clientData, char* tmpDir,
                                          boolean extFileUpdate, boolean verbose)
/* Selectively load the status table file table from the database, creating
 * table if it doesn't exist.  This calls selectFunc on each entry that is
 * found.  See gbStatusLoadSelect for details. */
{
struct gbStatusTbl *statusTbl;

AllocVar(statusTbl);
statusTbl->accHash = newHash(20);
statusTbl->strPool = newHash(18);
strcpy(statusTbl->tmpDir, tmpDir);
statusTbl->extFileUpdate = extFileUpdate;
statusTbl->verbose = verbose;

if (!sqlTableExists(conn, GB_STATUS_TBL))
    sqlRemakeTable(conn, GB_STATUS_TBL, createSql);
else
    loadTable(statusTbl, conn, select, accPrefix, selectFunc, clientData);
return statusTbl;
}

struct gbStatus* gbStatusStore(struct gbStatusTbl* statusTbl,
                               struct gbStatus* tmpStatus)
/* Function called to store status on selective load.  Returns the
 * entry that was added to the table. */
{
struct hashEl* hel;
struct gbStatus* status;
assert(hashLookup(statusTbl->accHash, tmpStatus->acc) == NULL);

lmAllocVar(statusTbl->accHash->lm, status);
memcpy(status, tmpStatus, sizeof(struct gbStatus));
hel = hashAdd(statusTbl->accHash, tmpStatus->acc, status);
status->acc = hel->name;
status->seqRelease = gbStatusTblGetStr(statusTbl, tmpStatus->seqRelease);
status->seqUpdate = gbStatusTblGetStr(statusTbl, tmpStatus->seqUpdate);
status->metaRelease = gbStatusTblGetStr(statusTbl, tmpStatus->metaRelease);
status->metaUpdate = gbStatusTblGetStr(statusTbl, tmpStatus->metaUpdate);
status->extRelease = gbStatusTblGetStr(statusTbl, tmpStatus->extRelease);
status->extUpdate = gbStatusTblGetStr(statusTbl, tmpStatus->extUpdate);

statusTbl->numEntries++;
return status;
}

static void loadSelectAll(struct gbStatusTbl* statusTbl,
                          struct gbStatus* tmpStatus, void* clientData)
/* gbStatusLoadSelect function that selects all */
{
gbStatusStore(statusTbl, tmpStatus);
}

struct gbStatusTbl *gbStatusTblLoad(struct sqlConnection *conn,
                                    unsigned select, char* accPrefix,
                                    char* tmpDir, boolean extFileUpdate, 
                                    boolean verbose)
/* Load the file table from the database, creating table if it doesn't exist.
 * The select flags are some combination of GB_MRNA,GB_EST,GB_GENBANK,
 * GB_REFSEQ.  If accPrefix is not null, it is the first two characters of the
 * accessions to select. */
{
return gbStatusTblSelectLoad(conn, select, accPrefix, loadSelectAll, NULL,
                             tmpDir, extFileUpdate, verbose);
}

struct gbStatus *gbStatusTblAdd(struct gbStatusTbl *statusTbl, char* acc,
                                int version, time_t modDate,
                                unsigned type, unsigned srcDb, unsigned orgCat,
                                HGID gbSeqId, unsigned numAligns, 
                                char *relVersion, char *update, time_t time)
/* create and add a new entry. */
{
struct hashEl* hel;
struct gbStatus *status;
assert(hashLookup(statusTbl->accHash, acc) == NULL);

lmAllocVar(statusTbl->accHash->lm, status);
hel = hashAdd(statusTbl->accHash, acc, status);
status->acc = hel->name;
status->version = version;
status->modDate = modDate;
status->type = type;
status->srcDb = srcDb;
status->orgCat = orgCat;
status->gbSeqId = gbSeqId;
status->numAligns = numAligns;
status->time = time;
status->seqRelease = gbStatusTblGetStr(statusTbl, relVersion);
status->seqUpdate = gbStatusTblGetStr(statusTbl, update);
status->metaRelease = status->seqRelease;
status->metaUpdate = status->seqUpdate;
status->extRelease = status->seqRelease;
status->extUpdate = status->seqUpdate;
statusTbl->numEntries++;
return status;
}

void gbStatusTblRemoveDeleted(struct gbStatusTbl *statusTbl,
                              struct sqlConnection *conn)
/* Remove sequences marked as deleted in status table. They are not removed
 * from the in memory table, but will have their gbSeqId zeroed. */
{
struct sqlDeleter* deleter = sqlDeleterNew(statusTbl->tmpDir,
                                           statusTbl->verbose);
struct gbStatus* status;

for (status = statusTbl->deleteList; status != NULL; status = status->next)
    {
    assert(status->stateChg & GB_DELETED);
    sqlDeleterAddAcc(deleter, status->acc);
    status->gbSeqId = 0;
    }
sqlDeleterDel(deleter, conn, "gbStatus", "acc");
sqlDeleterFree(&deleter);
}

struct gbStatus *gbStatusTblFind(struct gbStatusTbl *statusTbl, char *acc)
/* Lookup a entry in the table, return NULL if it doesn't exist */
{
return hashFindVal(statusTbl->accHash, acc);
}

static void queueUpdate(struct sqlUpdater *updater, struct gbStatus* status)
/* generate a update statement for status changing */
{
char query[2048];
int len = 0;

if (status->stateChg & GB_SEQ_CHG)
    len += safef(query+len, sizeof(query)-len,
                 "version=%d, numAligns=%d, seqRelease='%s', "
                 "seqUpdate='%s', ", status->version, status->numAligns,
                 status->seqRelease, status->seqUpdate);

if (status->stateChg & GB_META_CHG)
    len += safef(query+len, sizeof(query)-len,
                 "modDate='%s', metaRelease='%s', metaUpdate='%s', ",
                 gbFormatDate(status->modDate),
                 status->metaRelease, status->metaUpdate);

if (status->stateChg & GB_EXT_CHG)
    len += safef(query+len, sizeof(query)-len,
                 "extRelease='%s', extUpdate='%s', ",
                 status->extRelease, status->extUpdate);

len += safef(query+len, sizeof(query)-len,
             "time=NULL WHERE gbSeq=%u", status->gbSeqId);

sqlUpdaterModRow(updater, 1, "%s", query);
}

static void queueLoad(struct sqlUpdater *updater, struct gbStatus* status)
/* write a status object to a tab file for new or seqChg  */
{
assert(status->gbSeqId != 0);

/* set timestamp to null causes it to auto-update */
sqlUpdaterAddRow(updater, "%s\t%d\t%s\t%s\t%s\t%s\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t\\N",
                 status->acc, status->version, gbFormatDate(status->modDate),
                 gbTypeName(status->type),
                 gbSrcDbName(status->srcDb),
                 gbOrgCatName(status->orgCat),
                 status->gbSeqId, status->numAligns,
                 status->seqRelease, status->seqUpdate,
                 status->metaRelease, status->metaUpdate,
                 status->extRelease, status->extUpdate);
}

struct sqlUpdater* gbStatusTblUpdate(struct gbStatusTbl *statusTbl,
                                     struct sqlConnection *conn,
                                     boolean commit)
/* Update the database with to reflect the changes in the status table entries
 * flagged as changed.  If commit is true, commit the changes and return null.
 * If it is false, return the sqlUpdater with the changes ready to for
 * commit */
{
struct sqlUpdater* updater = sqlUpdaterNew(GB_STATUS_TBL, statusTbl->tmpDir,
                                           statusTbl->verbose, NULL);
struct hashCookie cookie = hashFirst(statusTbl->accHash);
struct hashEl* hel;

/* visit all entries in the hash table, write the new ones or changed ones */
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct gbStatus* status = hel->val;
    if (status->stateChg & GB_NEW)
        queueLoad(updater, status);
    else if (status->stateChg & (GB_SEQ_CHG|GB_META_CHG|GB_EXT_CHG))
        queueUpdate(updater, status);
    }
if (commit)
    {
    sqlUpdaterCommit(updater, conn);
    sqlUpdaterFree(&updater);
    }
return updater;
}

void gbStatusTblFree(struct gbStatusTbl** statusTbl)
/* Free a gbStatusTbl object */
{
#ifdef DUMP_HASH_STATS
hashPrintStats((*statusTbl)->accHash, "statusAcc", stderr);
hashPrintStats((*statusTbl)->strPool, "statusStr", stderr);
#endif
hashFree(&(*statusTbl)->accHash);
hashFree(&(*statusTbl)->strPool);
freez(statusTbl);
}

struct slName* gbStatusTblLoadAcc(struct sqlConnection *conn, 
                                  unsigned select, char* accPrefix)
/* load a list of accession from the table matching the selection
 * criteria */
{
struct slName* accList = NULL;
if (sqlTableExists(conn, GB_STATUS_TBL))
    {
    char **row;
    struct sqlResult *sr =  loadQuery(conn, select, accPrefix, "acc");
    while ((row = sqlNextRow(sr)) != NULL)
        slSafeAddHead(&accList, slNameNew(row[0]));
    sqlFreeResult(&sr);
    }
return accList;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

