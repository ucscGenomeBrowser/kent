/* cdwMakeFileTags - Create cdwFileTags table from tagStorm on same database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "cdw.h"
#include "cdwLib.h"
#include "rql.h"
#include "intValTree.h"
#include "tagStorm.h"
#include "tagToSql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwMakeFileTags - Create cdwFileTags table from tagStorm on same database.\n"
  "usage:\n"
  "   cdwMakeFileTags now\n"
  "options:\n"
  "   -table=tableName What table to store results in, default cdwFileTags\n"
  "   -database=databaseName What database to store results in, default cdw\n"
  "   -types=fileName Dump list of types to file\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {"database", OPTION_STRING},
   {"types", OPTION_STRING},
   {NULL, 0},
};

void removeUnusedMetaTags(struct sqlConnection *conn)
/* Remove rows from cdwMetaTags table that aren't referenced by cdwFile.metaTagsId */
{
/* Build up hash of used metaTagsIds */
struct hash *hash = hashNew(0);
char query[512];
sqlSafef(query, sizeof(query), "select metaTagsId from cdwFile");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(hash, row[0], NULL);
sqlFreeResult(&sr);

struct dyString *dy = dyStringNew(0);
sqlDyStringPrintf(dy, "delete from cdwMetaTags where id in (");
boolean deleteAny = FALSE;
sqlSafef(query, sizeof(query), "select id from cdwMetaTags");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
     {
     char *id = row[0];
     if (!hashLookup(hash, id))
         {
	 if (deleteAny)
	     dyStringAppendC(dy, ',');
	 else
	     deleteAny = TRUE;
	 dyStringAppend(dy, id);
	 }
     }
dyStringPrintf(dy, ")");
sqlFreeResult(&sr);

if (deleteAny)
    sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

void cdwMakeFileTags(char *database, char *table)
/* cdwMakeFileTags - Create cdwFileTags table from tagStorm on same database.. */
{
struct sqlConnection *conn = cdwConnect(database);

// See if somebody is already running this utility. Should only happen rarely.
if (sqlIsLocked(conn, "makeFileTags"))
    errAbort("Another user is already running cdwMakeFileTags. Advisory lock found.");
sqlGetLockWithTimeout(conn, "makeFileTags", 1);

removeUnusedMetaTags(conn);

/* Get tagStorm and make sure that all tags are unique in case insensitive way */
struct tagStorm *tagStorm = cdwTagStorm(conn);

/* Build up list and hash of column types */
struct tagTypeInfo *ttiList = NULL;
struct hash *ttiHash = NULL;
tagStormInferTypeInfo(tagStorm, &ttiList, &ttiHash);

/* Make optionally a dump of the tag type info. */
char *dumpName = optionVal("types", NULL);
if (dumpName != NULL)
    tagTypeInfoDump(ttiList, dumpName);

/* Create SQL table with key fields indexed */
static char *keyFields[] =  {
    "accession",
    "data_set_id",
    "submit_file_name",
    "paired_end_mate",
    "file_name",
    "format",
    "lab",
    "read_size",
    "item_count",
    "sample_label",
    "submit_dir",
    "species",
};

struct dyString *query = dyStringNew(0);
dyStringAppend(query, NOSQLINJ);
tagStormToSqlCreate(tagStorm, table, ttiList, ttiHash, 
    keyFields, ArraySize(keyFields), query);
verbose(2, "%s\n", query->string);
sqlRemakeTable(conn, table, query->string);

/* Do insert statements for each accessioned file in the system */
struct slRef *stanzaList = tagStanzasMatchingQuery(tagStorm, "select * from files where accession");
struct slRef *stanzaRef;
for (stanzaRef = stanzaList; stanzaRef != NULL; stanzaRef = stanzaRef->next)
    {
    struct tagStanza *stanza = stanzaRef->val;
    dyStringClear(query);
    dyStringAppend(query, NOSQLINJ);
    tagStanzaToSqlInsert(stanza, table, query);
    sqlUpdate(conn, query->string);
    }
dyStringFree(&query);
slFreeList(&stanzaList);
sqlReleaseLock(conn, "makeFileTags");
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
char *database = optionVal("database", "cdw");
char *table = optionVal("table", "cdwFileTags");
cdwMakeFileTags(database, table);
return 0;
}
