/* cdwMakeFileTags - Create cdwFileTags table from tagStorm on same database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "obscure.h"
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

struct tagTypeInfo
/* Information on a tag type */
    {
    struct tagTypeInfo *next;
    char *name;	/* Name of tag */
    boolean isUnsigned;
    boolean isInt;
    boolean isNum;
    long long maxIntVal;
    long long minIntVal;
    int maxChars;
    };

struct tagTypeInfo *tagTypeInfoNew(char *name)
/* Return initialized new tagTypeInfo */
{
struct tagTypeInfo *tti;
AllocVar(tti);
tti->name = cloneString(name);
tti->isUnsigned = tti->isInt = tti->isNum = TRUE;
return tti;
}

void tagTypeInfoDump(struct tagTypeInfo *list, char *fileName)
/* Dump out types to file */
{
FILE *f = mustOpen(fileName, "w");
struct tagTypeInfo *tti;
for (tti = list; tti != NULL; tti = tti->next)
    {
    fprintf(f, "%s u=%d, i=%d, n=%d, max=%lld, min=%lld, chars=%d\n", 
	tti->name, tti->isUnsigned, tti->isInt, tti->isNum, tti->maxIntVal, tti->minIntVal, 
	tti->maxChars);
    }
carefulClose(&f);
}

void tagTypeInfoAdd(struct tagTypeInfo *tti, char *val)
/* Fold in information about val into tti. */
{
int len = strlen(val);
if (len > tti->maxChars)
     tti->maxChars = len;
if (tti->isNum)
    {
    if (tti->isUnsigned)
        {
	if (isAllDigits(val))
	    {
	    long long v = sqlLongLong(val);
	    if (v > tti->maxIntVal) tti->maxIntVal = v;
	    return;
	    }
	else
	    tti->isUnsigned = FALSE;
	}
    if (tti->isInt)
        {
	if (val[0] == '-')
	    {
	    if (isAllDigits(val+1))
	        {
		long long v = sqlLongLong(val);
		if (v < tti->minIntVal) tti->minIntVal = v;
		return;
		}
	    else
	        tti->isInt = FALSE;
	    }
	else
	    {
	    if (isAllDigits(val))
	        {
		long long v = sqlLongLong(val);
		if (v > tti->maxIntVal) tti->maxIntVal = v;
		return;
		}
	    else
	        tti->isInt = FALSE;
	    }
	}
    if (!isNumericString(val))
        tti->isNum = FALSE;
    }
}

struct tagTypeInfo *gTtiList = NULL;

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
	struct tagTypeInfo *tti = hashFindVal(hash, tag);
	if (tti == NULL)
	     {
	     tti = tagTypeInfoNew(tag);
	     hashAdd(hash, tag, tti);
	     slAddHead(&gTtiList, tti);
	     }
	tagTypeInfoAdd(tti, val);
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
removeUnusedMetaTags(conn);

struct tagStorm *tags = cdwTagStorm(conn);
struct slName *field, *fieldList = tagStormFieldList(tags);
ensureNamesCaseUnique(fieldList);

slSort(&fieldList, slNameCmpCase);
struct hash *fieldHash = tagStormFieldHash(tags);

/* Build up hash of column types */
struct hash *tagTypeHash = tagStanzaInferType(tags);

struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "CREATE TABLE %s (", table);
char *connector = "";
int totalFieldWidth = 0;
for (field = fieldList; field != NULL; field = field->next)
    {
    char *sqlType = NULL;
    char sqlTypeBuf[256];
    struct tagTypeInfo *tti = hashFindVal(tagTypeHash, field->name);
    assert(tti != NULL);
    if (!isSymbolString(field->name))
        errAbort("Error - database needs work. Somehow symbol %s got in field list\n", field->name);
    if (tti->isUnsigned)
	{
	if (tti->maxIntVal <= 255)
	    {
	    sqlType = "tinyint unsigned";
	    totalFieldWidth += 1;
	    }
	else if (tti->maxIntVal <= 65535)
	    {
	    sqlType = "smallint unsigned";
	    totalFieldWidth += 2;
	    }
	else if (tti->maxIntVal <= 16777215)
	    {
	    sqlType = "mediumint unsigned";
	    totalFieldWidth += 3;
	    }
	else if (tti->maxIntVal <= 4294967295L)
	    {
	    sqlType = "int unsigned";
	    totalFieldWidth += 4;
	    }
	else 
	    {
	    sqlType = "bigint unsigned";
	    totalFieldWidth += 8;
	    }
	}
    else if (tti->isInt)
        {
	if (tti->minIntVal >= -128  && tti->maxIntVal <= 127)
	    {
	    sqlType = "tinyint";
	    totalFieldWidth += 1;
	    }
	else if (tti->minIntVal >= -32768  && tti->maxIntVal <= 32767)
	    {
	    sqlType = "smallint";
	    totalFieldWidth += 2;
	    }
	else if (tti->minIntVal >= -8388608  && tti->maxIntVal <= 8388607)
	    {
	    sqlType = "mediumint";
	    totalFieldWidth += 3;
	    }
	else if (tti->minIntVal >= -2147483648  && tti->maxIntVal <= 2147483647)
	    {
	    sqlType = "int";
	    totalFieldWidth += 4;
	    }
	else
	    {
	    sqlType = "bigint";
	    totalFieldWidth += 8;
	    }
	}
    else if (tti->isNum)
        {
	sqlType = "double";
	totalFieldWidth += 8;
	}
    else
        {
	if (tti->maxChars <= 255)
	    {
	    safef(sqlTypeBuf, sizeof(sqlTypeBuf), "varchar(%d)", tti->maxChars);
	    sqlType = sqlTypeBuf;
	    totalFieldWidth += tti->maxChars + 1;
	    }
	else
	    {
	    sqlType = "longblob";
	    totalFieldWidth += 12;   // May be 9-12, not sure how to tell.
	    }
	}
    dyStringPrintf(query, "\n%s%s %s", connector, field->name, sqlType);
    connector = ", ";
    }

int myisamLimit = 65535;
verbose(2, "totalFieldWidth = %d, limit %d\n", totalFieldWidth, myisamLimit);
if (totalFieldWidth > myisamLimit)   // MYISAM limit
    {
    errAbort("Looks like we are going to have to switch to INNODB for the cdwFileTags table, rats!");
    }
char *dumpName = optionVal("types", NULL);
if (dumpName != NULL)
    tagTypeInfoDump(gTtiList, dumpName);

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

int i;
for (i=0; i<ArraySize(keyFields); ++i)
    {
    char *key = keyFields[i];
    if (hashLookup(fieldHash, key))
        {
	struct tagTypeInfo *tti = hashFindVal(tagTypeHash, key);
	assert(tti != NULL);
	if (tti->isNum)
	    {
	    dyStringPrintf(query, "%sINDEX(%s)", connector, key);
	    }
	else
	    {
	    int indexSize = tti->maxChars;
	    if (indexSize > 16)
	        indexSize = 16;
	    dyStringPrintf(query, "%sINDEX(%s(%d))", connector, key, indexSize);
	    }
	}
    }
dyStringPrintf(query, ")");
verbose(2, "%s\n", query->string);
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
	dyStringPrintf(query, "%s%s", connector, tag->name);
	connector = ",";
	}
    dyStringPrintf(query, ") values (");
    connector = "";
    for (tag = tagList; tag != NULL; tag = tag->next)
        {
	char *escaped = makeEscapedString(tag->val, '"');
	dyStringPrintf(query, "%s\"%s\"", connector, escaped);
	freeMem(escaped);
	connector = ",";
	}
    dyStringPrintf(query, ")");
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
