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
  "   -facets=tableName What table to store faceting columns in\n"
  "   -fields=comma,separated,list,of,field,names to put in cdwFileFacets\n"
  "   -database=databaseName What database to store results in, default cdw\n"
  "   -types=fileName Dump list of types to file\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {"facets", OPTION_STRING},
   {"database", OPTION_STRING},
   {"fields", OPTION_STRING},
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
	     sqlDyStringPrintf(dy, ",");
	 else
	     deleteAny = TRUE;
	 sqlDyStringPrintf(dy, "'%s'", id);
	 }
     }
sqlDyStringPrintf(dy, ")");
sqlFreeResult(&sr);

if (deleteAny)
    sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

void cdwMakeFileTags(char *database, char *fullTable, char *facetTable, char *facetFieldsCsv)
/* cdwMakeFileTags - Create cdwFileTags table from tagStorm on same database.. */
{
/* Create facets table, a subset of the earlier table.  Note this may be mySQL specific */
int facetCount = chopByChar(facetFieldsCsv, ',', NULL, 0);
if (facetCount <= 0)
    errAbort("Comma separated facet field list is empty: %s", facetFieldsCsv);

// See if somebody is already running this utility. Should only happen rarely.
struct sqlConnection *conn = cdwConnect(database);
if (sqlIsLocked(conn, "makeFileTags"))
    errAbort("Another user is already running cdwMakeFileTags. Advisory lock found."); sqlGetLockWithTimeout(conn, "makeFileTags", 1);

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
// Functions in src/lib/tabToSql.c cannot use functions like sqlSafef
// since they are in src/hg/lib/ which is not available. 
// That is why we see NOSQLINJ exposed here.
dyStringAppend(query, NOSQLINJ);
tagStormToSqlCreate(tagStorm, fullTable, ttiList, ttiHash, 
    keyFields, ArraySize(keyFields), query);
verbose(2, "%s\n", query->string);
sqlRemakeTable(conn, fullTable, query->string);

/* Do insert statements for each accessioned file in the system */
struct slRef *stanzaList = tagStanzasMatchingQuery(tagStorm, "select * from files where accession");
struct slRef *stanzaRef;
for (stanzaRef = stanzaList; stanzaRef != NULL; stanzaRef = stanzaRef->next)
    {
    struct tagStanza *stanza = stanzaRef->val;
    dyStringClear(query);
    dyStringAppend(query, NOSQLINJ);
    tagStanzaToSqlInsert(stanza, fullTable, query);
    sqlUpdate(conn, query->string);
    }
slFreeList(&stanzaList);

/* Make facetTable as a subset of fullTable */
verbose(2, "making %s table for faceting\n", facetTable);
struct dyString *facetCreate = dyStringNew(0);
sqlDyStringPrintf(facetCreate, "create table %s as select %-s from %s", facetTable, sqlCkIl(facetFieldsCsv), fullTable);
sqlRemakeTable(conn, facetTable, facetCreate->string);
dyStringFree(&facetCreate);


/* Release lock, clean up, go home */
sqlReleaseLock(conn, "makeFileTags");
sqlDisconnect(&conn);
dyStringFree(&query);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
char *database = optionVal("database", "cdw");
char *table = optionVal("table", "cdwFileTags");
char *facets = optionVal("facets", "cdwFileFacets");
char *fields = optionVal("fields", "assay,data_set_id,lab,format,read_size,sample_label,species");

cdwMakeFileTags(database, table, facets, fields);
return 0;
}
