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

void makeCdwGroupFileTemp(char *database)
/* Make temporary table converting multiple groupIds per file into a comma-separated list */
{
struct sqlConnection *conn = cdwConnect(database);
struct sqlConnection *conn2 = cdwConnect(database);
char query2[256];
sqlSafef(query2, sizeof(query2), 
    "CREATE TABLE `cdwGroupFileTemp` ("
    "  `fileId` int(10) unsigned DEFAULT '0',"
    "  `groupIds` varchar(255) DEFAULT '',"
    "  KEY `fileId` (`fileId`)"
    ") ENGINE=MyISAM DEFAULT CHARSET=latin1"
    );
sqlRemakeTable(conn, "cdwGroupFileTemp", query2);

sqlSafef(query2, sizeof(query2), "select fileId, groupId from cdwGroupFile order by fileId, groupId");
struct sqlResult *sr = sqlGetResult(conn, query2);
char **row;
int lastFileId = -1;
struct dyString *query = dyStringNew(0);
struct dyString *groupList = dyStringNew(0);
int fileId = 0;
int groupId = 0;
do
    {
    row = sqlNextRow(sr);
    if (row)
	{	
	fileId = sqlUnsigned(row[0]);
	groupId = sqlUnsigned(row[1]);
	}
    else
	{
	fileId = -2;  // eof
	groupId = 0;
	}
    if (fileId == lastFileId)
	{
	sqlDyStringPrintf(groupList, ",");
	}
    else
	{
	if (!isEmpty(groupList->string))
	    {
	    dyStringClear(query);
	    sqlDyStringPrintf(query, "insert into cdwGroupFileTemp values (%u, '%s')", lastFileId, groupList->string);
	    sqlUpdate(conn2, query->string);
	    }
	dyStringClear(groupList);
	lastFileId = fileId;
	}
    dyStringPrintf(groupList, "%u", groupId);
    }
while (fileId != -2);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
sqlDisconnect(&conn2);
dyStringFree(&query);
dyStringFree(&groupList);
}

void addGroupAndAllAcessFields(char *database, char *table)
/* add groupIds and allAccess columns to table */
{
verbose(2, "adding group and allAccess fields to %s\n", table);
struct sqlConnection *conn = cdwConnect(database);
struct dyString *query = dyStringNew(0);
dyStringClear(query);
sqlDyStringPrintf(query,
    "ALTER TABLE %s " 
    "ADD `allAccess` tinyint(4) DEFAULT '0',"
    "ADD  `groupIds` varchar(255) DEFAULT ''"  // total rowsize cannot exceed 65k
    , table);
sqlUpdate(conn, query->string);

// allAccess
verbose(2, "setting allAccess field values\n");
dyStringClear(query);
sqlDyStringPrintf(query,
    "update %s t1 "
    "inner join cdwFile t2 on t2.id = t1.file_id "
    "set t1.allAccess = t2.allAccess"
    , table);
sqlUpdate(conn, query->string);
dyStringFree(&query);
sqlDisconnect(&conn);
}

void setGroupIdsFromTemp(char *database, char *table)
/* set groupIds from temp table */
{
verbose(2, "setting %s.groupIds from cdwFileGroupTemp\n", table);
struct sqlConnection *conn = cdwConnect(database);
struct dyString *query = dyStringNew(0);
dyStringClear(query);
sqlDyStringPrintf(query,
    "update %s t1 "
    "inner join cdwGroupFileTemp t2 on t2.fileId = t1.file_id "
    "set t1.groupIds = t2.groupIds"
    , table);
sqlUpdate(conn, query->string);

// set any unset default groupIds value to 0
dyStringClear(query);
sqlDyStringPrintf(query,
    "update %s "
    "set groupIds = '0' "
    "where groupIds = ''"
    , table);
sqlUpdate(conn, query->string);
dyStringFree(&query);
sqlDisconnect(&conn);
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


/* add columns to facilitate group and allAccess filtering */
addGroupAndAllAcessFields(database, fullTable);
addGroupAndAllAcessFields(database, facetTable);

// groupIds
verbose(2, "making table cdwFileGroupTemp\n");
makeCdwGroupFileTemp(database);

setGroupIdsFromTemp(database, fullTable);
setGroupIdsFromTemp(database, facetTable);

sqlDropTable(conn, "cdwGroupFileTemp");

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
char *fields = optionVal("fields", "file_id,file_name,file_size,ucsc_db,output,assay,data_set_id,lab,format,read_size,sample_label,species,biosample_cell_type");

cdwMakeFileTags(database, table, facets, fields);
return 0;
}
