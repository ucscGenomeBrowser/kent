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
  "   cdwMakeFileTags now\n"
  "options:\n"
  "   -table=tableName What table to store results in, default cdwFileTags\n"
  "   -database=databaseName What database to store results in, default cdw\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {"database", OPTION_STRING},
   {NULL, 0},
};

static void rInfer(struct tagStanza *list, struct hash *hash)
/* Traverse recursively updating hash with type of each field. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	char *tag = pair->name;
	char *val = pair->val;
	char *oldType = NULL;
	struct hashEl *oldHel = hashLookup(hash, tag);
	if (oldHel != NULL)
	     oldType = oldHel->val;
	if (isNumericString(val))
	    {
	    if (oldType == NULL)
	        {
		hashAdd(hash, tag, "float");
		}
	    }
	else
	    {
	    int len = strlen(val);
	    char *stringType = (len < 255 ? "string" : "lstring");
	    if (oldType == NULL)
	        {
		hashAdd(hash, tag, stringType);
		}
	    else 
	        {
		if (!sameString(oldHel->val, "lstring"))
		    oldHel->val = stringType;
		}
	    }
	}
    if (stanza->children)
        rInfer(stanza->children, hash);
    }
}

struct hash *tagStanzaInferType(struct tagStorm *tags)
/* Go through every tag/value in storm and return a hash that is
 * keyed by tag and has one of three string values:
 *     "int", "float", "string"
 * depending on the values seen at a field. */
{
struct hash *hash = hashNew(0);
rInfer(tags->forest, hash);
return hash;
}

void cdwMakeFileTags(char *database, char *table)
/* cdwMakeFileTags - Create cdwFileTags table from tagStorm on same database.. */
{
struct sqlConnection *conn = cdwConnect(database);
struct tagStorm *tags = cdwTagStorm(conn);
struct slName *field, *fieldList = tagStormFieldList(tags);
slSort(&fieldList, slNameCmpCase);
struct hash *fieldHash = tagStormFieldHash(tags);

/* Build up hash of column types */
struct hash *tagType = tagStanzaInferType(tags);
uglyf("Got %d els in tagType\n", tagType->elCount);


struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "CREATE TABLE %s (", table);
char *connector = "";
for (field = fieldList; field != NULL; field = field->next)
    {
    char *sqlType = NULL;
    char *type = hashFindVal(tagType, field->name);
    assert(type != NULL);
    if (sameString(type, "string"))
        {
	sqlType = "varchar(255)";
	}
    else if (sameString(type, "lstring"))
        {
	sqlType = "longblob";
	}
    else if (sameString(type, "float"))
        {
	sqlType = "double";
	}
    else
        {
	errAbort("Unrecoginzed tag %s type in cdwMakeFileTags", type);
	}

    sqlDyStringPrintf(query, "%s%s %s", connector, field->name, sqlType);
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
	char *type = hashFindVal(tagType, key);
	assert(type != NULL);
	if (sameString(type, "string"))
	    {
	    sqlDyStringPrintf(query, "%sINDEX(%s(16))", connector, key);
	    }
	else if (sameString(type, "float"))
	    {
	    sqlDyStringPrintf(query, "%sINDEX(%s)", connector, key);
	    }
	else
	    {
	    errAbort("Unrecoginzed tag %s type in cdwMakeFileTags2", type);
	    }
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
if (argc != 2)
    usage();
char *database = optionVal("database", "cdw");
char *table = optionVal("table", "cdwFileTags");
cdwMakeFileTags(database, table);
return 0;
}
