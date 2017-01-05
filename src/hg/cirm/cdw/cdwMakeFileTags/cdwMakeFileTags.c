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
    char *name;		    /* Name of tag */
    boolean isUnsigned;	    /* True if an unsigned integer */
    boolean isInt;	    /* True if an integer */
    boolean isNum;	    /* True if a number (real or integer) */
    long long maxIntVal;    /* Maximum value for integer or unsigned */
    long long minIntVal;    /* Minimum value for integer */
    int maxChars;	    /* Maximum width of string representation */
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

int tagTypeInfoCmpName(const void *va, const void *vb)
/* Compare two tagTypeInfo's by name field */
{
const struct tagTypeInfo *a = *((struct tagTypeInfo **)va);
const struct tagTypeInfo *b = *((struct tagTypeInfo **)vb);
return strcasecmp(a->name, b->name);
}

static char *tfForInt(int i)
/* Return "T" or "F" depending on whether i is nonzero */
{
if (i)
    return "T";
else
    return "F";
}

void tagTypeInfoDump(struct tagTypeInfo *list, char *fileName)
/* Dump out types to file */
{
FILE *f = mustOpen(fileName, "w");
struct tagTypeInfo *tti;
for (tti = list; tti != NULL; tti = tti->next)
    {
    fprintf(f, "%s\tu=%s, i=%s, n=%s, min=%lld, max=%lld, chars=%d\n", 
	tti->name, tfForInt(tti->isUnsigned), tfForInt(tti->isInt), tfForInt(tti->isNum), 
	tti->minIntVal, tti->maxIntVal, tti->maxChars);
    }
carefulClose(&f);
}

void tagTypeInfoAdd(struct tagTypeInfo *tti, char *val)
/* Fold in information about val into tti. */
{
int len = strlen(val);
if (len > tti->maxChars)
     tti->maxChars = len;
if (!tti->isNum)
    return;
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

static void rInfer(struct tagStanza *list, struct hash *hash, struct tagTypeInfo **pList)
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
	     slAddHead(pList, tti);
	     }
	tagTypeInfoAdd(tti, val);
	}
    if (stanza->children)
        rInfer(stanza->children, hash, pList);
    }
}

void tagStormInferTypeInfo(struct tagStorm *tags, 
    struct tagTypeInfo **retList, struct hash **retHash)
/* Go through every tag/value in storm and return a hash that is
 * keyed by tag and a tagTypeInfo as a value */
{
struct hash *hash = hashNew(0);
struct tagTypeInfo *list = NULL;
rInfer(tags->forest, hash, &list);
slSort(&list, tagTypeInfoCmpName);
*retList = list;
*retHash = hash;
}

char *tagMysqlCreateString(char *tableName, struct tagTypeInfo *ttiList, struct hash *ttiHash,
    char **keyFields, int keyFieldCount)
/* Make a mysql create string out of ttiList/ttiHash, which is gotten from
 * tagStormInferTypeInfo */
{
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "CREATE TABLE %s (", tableName);
char *connector = "";
int totalFieldWidth = 0;
struct tagTypeInfo *tti;
for (tti = ttiList; tti != NULL; tti = tti->next)
    {
    int fieldWidth = 0;
    char *sqlType = NULL;
    char sqlTypeBuf[256];
    if (!isSymbolString(tti->name))
        errAbort("Error - database needs work. Somehow symbol %s got in field list\n", tti->name);
    if (tti->isUnsigned)
	{
	long long maxTinyUnsigned = (1<<8)-1;    // Fits in one byte
	long long maxSmallUnsigned = (1<<16)-1;  // Fits in two bytes
	long long maxMediumUnsigned = (1<<24)-1; // Fits in three bytes
	long long maxIntUnsigned = (1LL<<32)-1;  // Fits in four bytes

	if (tti->maxIntVal <= maxTinyUnsigned)
	    {
	    sqlType = "tinyint unsigned";
	    fieldWidth = 1;
	    }
	else if (tti->maxIntVal <= maxSmallUnsigned)
	    {
	    sqlType = "smallint unsigned";
	    fieldWidth = 2;
	    }
	else if (tti->maxIntVal <= maxMediumUnsigned)
	    {
	    sqlType = "mediumint unsigned";
	    fieldWidth = 3;
	    }
	else if (tti->maxIntVal <= maxIntUnsigned)
	    {
	    sqlType = "int unsigned";
	    fieldWidth = 4;
	    }
	else 
	    {
	    sqlType = "bigint unsigned";
	    fieldWidth = 8;
	    }
	}
    else if (tti->isInt)
        {
	long long minTinyInt = -128, maxTinyInt = 127;  // Fits in one byte
	long long minSmallInt = -32768, maxSmallInt = 32767; // Fits in two bytes
	long long minMediumInt = -8388608, maxMediumInt = 8388607;  // Fits in three bytes
	long long minInt = -2147483648LL, maxInt = 2147483647LL; // Fits in three bytes
	if (tti->minIntVal >= minTinyInt  && tti->maxIntVal <= maxTinyInt)
	    {
	    sqlType = "tinyint";
	    fieldWidth = 1;
	    }
	else if (tti->minIntVal >= minSmallInt  && tti->maxIntVal <= maxSmallInt)
	    {
	    sqlType = "smallint";
	    fieldWidth = 2;
	    }
	else if (tti->minIntVal >= minMediumInt  && tti->maxIntVal <= maxMediumInt)
	    {
	    sqlType = "mediumint";
	    fieldWidth = 3;
	    }
	else if (tti->minIntVal >= minInt  && tti->maxIntVal <= maxInt)
	    {
	    sqlType = "int";
	    fieldWidth = 4;
	    }
	else
	    {
	    sqlType = "bigint";
	    fieldWidth = 8;
	    }
	}
    else if (tti->isNum)
        {
	sqlType = "double";
	fieldWidth = 8;
	}
    else
        {
	if (tti->maxChars <= 255)
	    {
	    safef(sqlTypeBuf, sizeof(sqlTypeBuf), "varchar(%d)", tti->maxChars);
	    sqlType = sqlTypeBuf;
	    fieldWidth = tti->maxChars + 1;
	    }
	else
	    {
	    sqlType = "longblob";
	    fieldWidth = 12;   // May be 9-12, not sure how to tell.
	    }
	}
    totalFieldWidth += fieldWidth;
    dyStringPrintf(query, "\n%s%s %s", connector, tti->name, sqlType);
    connector = ", ";
    }

/* Check that we don't overflow a MySQL MYISAM buffer */
int myisamLimit = 65535;
verbose(2, "totalFieldWidth = %d, limit %d\n", totalFieldWidth, myisamLimit);
if (totalFieldWidth > myisamLimit)   // MYISAM limit
    {
    errAbort("Looks like we are going to have to switch to INNODB for the %s table, rats!",
	tableName);
    }


int i;
for (i=0; i<keyFieldCount; ++i)
    {
    char *key = keyFields[i];
    struct tagTypeInfo *tti = hashFindVal(ttiHash, key);
    if (tti != NULL)
        {
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
return dyStringCannibalize(&query);
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

/* Get tagStorm and make sure that all tags are unique in case insensitive way */
struct tagStorm *tags = cdwTagStorm(conn);
struct slName *fieldList = tagStormFieldList(tags);
ensureNamesCaseUnique(fieldList);

/* Build up list and hash of column types */
struct tagTypeInfo *ttiList = NULL;
struct hash *ttiHash = NULL;
tagStormInferTypeInfo(tags, &ttiList, &ttiHash);
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
char *createString = tagMysqlCreateString(table, ttiList, ttiHash, keyFields, ArraySize(keyFields));
verbose(2, "%s\n", createString);
sqlRemakeTable(conn, table, createString);

/* Do insert statements for each accessioned file in the system */
struct slRef *stanzaList = tagStanzasMatchingQuery(tags, "select * from files where accession");
struct slRef *stanzaRef;
struct dyString *query = dyStringNew(0);
for (stanzaRef = stanzaList; stanzaRef != NULL; stanzaRef = stanzaRef->next)
    {
    dyStringClear(query);
    sqlDyStringPrintf(query, "insert %s (", table);
    struct tagStanza *stanza = stanzaRef->val;
    struct slPair *tag, *tagList = tagListIncludingParents(stanza);
    char *connector = "";
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
