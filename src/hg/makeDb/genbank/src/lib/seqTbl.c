#include "seqTbl.h"
#include "sqlUpdater.h"
#include "sqlDeleter.h"
#include "gbFileOps.h"
#include "gbDefs.h"
#include "gbVerb.h"
#include "gbRelease.h"

static char const rcsid[] = "$Id: seqTbl.c,v 1.6 2006/03/11 00:07:59 markd Exp $";

/*
 * Note: don't use autoincrement for id column, as it causes problems for
 * mysqlimport.
 */


/* name of gbSeqTbl */
char* SEQ_TBL = "gbSeq";
static char* createSql =
/* This keeps track of a sequence. */
"create table gbSeq ("
  "id int unsigned not null primary key," /* Unique ID across all tables. */
  "acc char(12) not null,"                /* Unique accession. */
  "version smallint unsigned not null,"   /* genbank version number */
  "size int unsigned not null,"           /* Size of sequence in bases. */
  "gbExtFile int unsigned not null,"      /* File it is in. */
  "file_offset bigint not null,"          /* Offset in file. */
  "file_size int unsigned not null,"      /* Size in file. */
  "type enum('EST','mRNA', 'PEP')"        /* Sequence type */
  "          not null,"
  "srcDb enum('GenBank','RefSeq',"        /* Source database */
  "           'Other') not null,"

  /* Extra indices. */
  "unique(acc),"
  /* Index for selecting ESTs based on first two letters of the accession */
  "index typeSrcDbAcc2(type, srcDb, acc(2)),"
  "index(gbExtFile))";  /* for finding seqs associated with a file */

/* Values for type field */
char *SEQ_MRNA = "mRNA";
char *SEQ_EST  = "EST";
char *SEQ_PEP  = "PEP";
char *SEQ_DNA  = "DNA";

/* Values for srcDb field */
char *SEQ_GENBANK = "GenBank";
char *SEQ_REFSEQ  = "RefSeq";

struct seqTbl* seqTblNew(struct sqlConnection* conn, char* tmpDir,
                         boolean verbose)
/* Create a seqTbl object.  If the sql table doesn't exist, create it.
 * If tmpDir is not null, this is the directory where the tab file
 * is created adding entries.
 */
{
struct seqTbl *seqTbl;
AllocVar(seqTbl);
seqTbl->verbose = verbose;
if (tmpDir != NULL)
    seqTbl->tmpDir = cloneString(tmpDir);
if (!sqlTableExists(conn, SEQ_TBL))
    {
    sqlRemakeTable(conn, SEQ_TBL, createSql);
    seqTbl->nextId = 1;
    }
else
    seqTbl->nextId = hgGetMaxId(conn, SEQ_TBL) + 1;
return seqTbl;
}

void seqTblFree(struct seqTbl** stPtr)
/* free a seqTbl object */
{
struct seqTbl* st = *stPtr;
if (st != NULL)
    {
    sqlUpdaterFree(&st->updater);
    sqlDeleterFree(&st->deleter);
    freeMem(st->tmpDir);
    freez(stPtr);
    }
}

static void ensureUpdater(struct seqTbl *st)
/* ensure updater object exists */
{
if (st->updater == NULL)
    {
    assert(st->tmpDir != NULL);
    st->updater = sqlUpdaterNew(SEQ_TBL, st->tmpDir, st->verbose, NULL);
    }
}

HGID seqTblAdd(struct seqTbl *st, char* acc, int version, char* type,
               char *srcDb, HGID extFileId, unsigned seqSize, off_t fileOff,
               unsigned recSize)
/* add a seq to the table, allocating an id */
{
ensureUpdater(st);
sqlUpdaterAddRow(st->updater, "%u\t%s\t%d\t%u\t%u\t%llu\t%u\t%s\t%s",
                 st->nextId, acc, version, seqSize,
                 extFileId, (long long)fileOff, recSize, type, srcDb);
return st->nextId++;
}

void seqTblMod(struct seqTbl *st, HGID id, int version, HGID extFileId,
               unsigned seqSize, off_t fileOff, unsigned recSize)
/* Modify attributes of an existing sequence.  If fileId is not zero, set
 * extFile, size, file_off and file_size.  If version >= 0, set version. */
{
char query[512];
int len = 0;
if (version >= 0)
    len += safef(query+len, sizeof(query)-len,
                 "version=%d", version);
if (extFileId != 0)
    len += safef(query+len, sizeof(query)-len,
                 "%ssize=%u, gbExtFile=%u, file_offset=%lld, file_size=%d",
                 ((len != 0) ? ", " : ""),
                 seqSize, extFileId, (long long)fileOff, recSize);
len += safef(query+len, sizeof(query)-len, " WHERE id=%u", id);

ensureUpdater(st);
sqlUpdaterModRow(st->updater, 1, "%s", query);
}

static void ensureDeleter(struct seqTbl *st)
/* ensure deleter object exists */
{
if (st->deleter == NULL)
    st->deleter = sqlDeleterNew(st->tmpDir, (st->verbose >= gbVerbose));
}

void seqTblDelete(struct seqTbl *st, char *acc)
/* delete a row from the seqTbl */
{
ensureDeleter(st);
sqlDeleterAddAcc(st->deleter, acc);
}

HGID seqTblGetId(struct seqTbl *st, struct sqlConnection *conn, char* acc)
/* Get the id for a sequence, or 0 of it's not it the table */
{
char query[256];

safef(query, sizeof(query), "SELECT id FROM gbSeq WHERE acc='%s'", acc);
return sqlQuickNum(conn, query);
}

void seqTblCommit(struct seqTbl *st, struct sqlConnection *conn)
/* commit pending changes */
{
if (st->deleter != NULL)
    sqlDeleterDel(st->deleter, conn, SEQ_TBL, "acc");
if (st->updater != NULL)
    sqlUpdaterCommit(st->updater, conn);
}

void seqTblCancel(struct seqTbl *st)
/* cancel pending changes */
{
if (st->updater != NULL)
    sqlUpdaterCancel(st->updater);
}

static void buildSelect(struct gbSelect* select, char* query,
                        int queryBufSize)
/* Build up a sql select for accessions based on the gbSelect */
{
int len = 0;
len = safef(query, queryBufSize,
            "SELECT acc from gbSeq WHERE (srcDb='%s') AND (type='%s')",
            ((select->release->srcDb == GB_REFSEQ) ? SEQ_REFSEQ
             : SEQ_GENBANK),
            ((select->type == GB_MRNA) ? SEQ_MRNA : SEQ_EST));
if (select->accPrefix != NULL)
    len += safef(query+len, queryBufSize-len,
                 " AND (acc LIKE '%s%%')", select->accPrefix);
}

struct hash* seqTblLoadAcc(struct sqlConnection *conn,
                           struct gbSelect* select)
/* build a hash table for the acc in the sequence table matching the 
 * select paramters.  No values are stored in the hash. */
{
char query[512];
char **row;
struct sqlResult *sr;
struct hash* seqHash = hashNew(20);

if (sqlTableExists(conn, SEQ_TBL))
    {
    buildSelect(select, query, sizeof(query));
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        hashAdd(seqHash, row[0], NULL);
    sqlFreeResult(&sr);
    }
return seqHash;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

