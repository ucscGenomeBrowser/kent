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


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwMakeFileTags - Create cdwFileTags table from tagStorm on same database.\n"
  "usage:\n"
  "   cdwMakeFileTags database table\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwMakeFileTags(char *database, char *table)
/* cdwMakeFileTags - Create cdwFileTags table from tagStorm on same database.. */
{
struct sqlConnection *conn = cdwConnect(database);
struct tagStorm *tags = cdwTagStorm(conn);
struct slName *field, *fieldList = tagStormFieldList(tags);
slSort(&fieldList, slNameCmp);
struct hash *fieldHash = tagStormFieldHash(tags);

struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "CREATE TABLE %s (", table);
char *connector = "";
for (field = fieldList; field != NULL; field = field->next)
    {
    sqlDyStringPrintf(query, "%s%s varchar(255)", connector, field->name);
    connector = ", ";
    }

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
    "body_part",
    "submit_dir",
    "species",
};

int i;
for (i=0; i<ArraySize(keyFields); ++i)
    {
    char *key = keyFields[i];
    if (hashLookup(fieldHash, key))
        {
	sqlDyStringPrintf(query, "%sINDEX(%s(16))", connector, key);
	}
    }
sqlDyStringPrintf(query, ")");
sqlRemakeTable(conn, table, query->string);

struct slRef *stanzaList = tagStanzasMatchingQuery(tags, "select * from files where accession");
struct slRef *stanzaRef;
for (stanzaRef = stanzaList; stanzaRef != NULL; stanzaRef = stanzaRef->next)
    {
    dyStringClear(query);
    sqlDyStringPrintf(query, "insert %s (", table);
    struct tagStanza *stanza = stanzaRef->val;
    struct slPair *tag, *tagList = tagListIncludingParents(stanza);
    connector = "";
    for (tag = tagList; tag != NULL; tag = tag->next)
        {
	sqlDyStringPrintf(query, "%s%s", connector, tag->name);
	connector = ",";
	}
    sqlDyStringPrintf(query, ") values (");
    connector = "";
    for (tag = tagList; tag != NULL; tag = tag->next)
        {
	sqlDyStringPrintf(query, "%s\"%s\"", connector, (char*)tag->val);
	connector = ",";
	}
    sqlDyStringPrintf(query, ")");
    sqlUpdate(conn, query->string);
    }
dyStringFree(&query);
slFreeList(&stanzaList);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwMakeFileTags(argv[1], argv[2]);
return 0;
}
