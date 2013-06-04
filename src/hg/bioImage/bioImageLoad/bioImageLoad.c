/* bioImageLoad - Load data into bioImage database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"
#include "jksql.h"
#include "dystring.h"

/* Variables you can override from command line. */
char *database = "bioImage";
boolean replace = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bioImageLoad - Load data into bioImage database\n"
  "usage:\n"
  "   bioImageLoad setInfo.ra itemInfo.tab\n"
  "Please see bioImageLoad.doc for description of the .ra and .tab files\n"
  "Options:\n"
  "   -database=%s - Specifically set database\n"
  "   -replace - Replace image rather than complaining if it exists\n"
  , database
  );
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING,},
   {"replace", OPTION_BOOLEAN,},
   {NULL, 0},
};

struct hash *hashRowOffsets(char *line)
/* Given a space-delimited line, create a hash keyed by the words in 
 * line with values the position of the word (0 based) in line */
{
struct hash *hash = hashNew(0);
char *word;
int wordIx = 0;
while ((word = nextWord(&line)) != 0)
    {
    hashAdd(hash, word, intToPt(wordIx));
    wordIx += 1;
    }
return hash;
}

char *getVal(char *fieldName, struct hash *raHash, struct hash *rowHash, char **row, char *defaultVal)
/* Return value in row if possible, else in ra, else in default.  If no value and no default
 * return an error. */
{
char *val = NULL;
struct hashEl *hel = hashLookup(rowHash, fieldName);
if (hel != NULL)
    {
    int rowIx = ptToInt(hel->val);
    val = row[rowIx];
    }
else
    {
    val = hashFindVal(raHash, fieldName);
    if (val == NULL)
	{
	if (defaultVal != NULL)
	    val = defaultVal;
	else
	    errAbort("Can't find value for field %s", fieldName);
	}
    }
return val;
}

static char *requiredItemFields[] = {"fileName", "submitId"};
static char *requiredSetFields[] = {"contributor"};
static char *requiredFields[] = {"fullDir", "screenDir", "thumbDir", "taxon", "isEmbryo", "age", "bodyPart", 
	"sliceType", "imageType", };
static char *optionalFields[] = {"sectionSet", "sectionIx", "gene", "locusLink", "refSeq", "genbank", };

char *hashValOrDefault(struct hash *hash, char *key, char *defaultVal)
/* Lookup key in hash and return value, or return default if it doesn't exist. */
{
char *val = hashFindVal(hash, key);
if (val == NULL)
    val = defaultVal;
return val;
}

int findExactSubmissionId(struct sqlConnection *conn,
	char *contributors, char *publication, 
	char *pubUrl, char *setUrl, char *itemUrl)
/* Find ID of submissionSet that matches all parameters.  Return 0 if none found. */
{
char query[1024];
sqlSafef(query, sizeof(query),
      "select id from submissionSet "
      "where contributors = \"%s\" "
      "and publication = \"%s\" "
      "and pubUrl = '%s' and setUrl = '%s' and itemUrl = '%s'"
      , contributors, publication, pubUrl, setUrl, itemUrl);
return sqlQuickNum(conn, query);
}

int findOrAddIdTable(struct sqlConnection *conn, char *table, char *field, 
	char *value)
/* Get ID associated with field.value in table.  */
{
char query[256];
int id;
sqlSafef(query, sizeof(query), "select id from %s where %s = \"%s\"",
	table, field, value);
id = sqlQuickNum(conn, query);
if (id == 0)
    {
    sqlSafef(query, sizeof(query), "insert into %s values(default, \"%s\")",
    	table, value);
    sqlUpdate(conn, query);
    id = sqlLastAutoId(conn);
    }
return id;
}

int createSubmissionId(struct sqlConnection *conn,
	char *contributors, char *publication, 
	char *pubUrl, char *setUrl, char *itemUrl)
/* Add submission and contributors to database and return submission ID */
{
struct slName *slNameListFromString(char *s, char delimiter);
struct slName *contribList = NULL, *contrib;
int submissionSetId;
char query[1024];

sqlSafef(query, sizeof(query),
    "insert into submissionSet "
    "values(default, \"%s\", \"%s\", '%s', '%s', '%s')",
    contributors, publication, pubUrl, setUrl, itemUrl);
sqlUpdate(conn, query);
submissionSetId = sqlLastAutoId(conn);

contribList = slNameListFromComma(contributors);
for (contrib = contribList; contrib != NULL; contrib = contrib->next)
    {
    int contribId = findOrAddIdTable(conn, "contributor", "name", 
    	skipLeadingSpaces(contrib->name));
    sqlSafef(query, sizeof(query),
          "insert into submissionContributor values(%d, %d)",
	  submissionSetId, contribId);
    sqlUpdate(conn, query);
    }
slFreeList(&contribList);
return submissionSetId;
}

int saveSubmissionSet(struct sqlConnection *conn, struct hash *raHash)
/* Create submissionSet, submissionContributor, and contributor records. */
{
char *contributor = hashMustFindVal(raHash, "contributor");
char *publication = hashValOrDefault(raHash, "publication", "");
char *pubUrl = hashValOrDefault(raHash, "pubUrl", "");
char *setUrl = hashValOrDefault(raHash, "setUrl", "");
char *itemUrl = hashValOrDefault(raHash, "itemUrl", "");
int submissionId = findExactSubmissionId(conn, contributor, 
	publication, pubUrl, setUrl, itemUrl);
if (submissionId != 0)
     return submissionId;
else
     return createSubmissionId(conn, contributor, 
     	publication, pubUrl, setUrl, itemUrl);
}

int cachedId(struct sqlConnection *conn, char *tableName, char *fieldName,
	struct hash *cache, char *raFieldName, struct hash *raHash, 
	struct hash *rowHash, char **row)
/* Get value for named field, and see if it exists in table.  If so
 * return associated id, otherwise create new table entry and return 
 * that id. */
{
char *value = getVal(raFieldName, raHash, rowHash, row, "");
if (value[0] == 0)
    return 0;
return findOrAddIdTable(conn, tableName, fieldName, value);
}


void bioImageLoad(char *setRaFile, char *itemTabFile)
/* bioImageLoad - Load data into bioImage database. */
{
struct hash *raHash = raReadSingle(setRaFile);
struct hash *rowHash;
struct lineFile *lf = lineFileOpen(itemTabFile, TRUE);
char *line, *words[256];
struct sqlConnection *conn = sqlConnect(database);
int rowSize;
int submissionSetId;
struct hash *fullDirHash = newHash(0);
struct hash *screenDirHash = newHash(0);
struct hash *thumbDirHash = newHash(0);
struct hash *treatmentHash = newHash(0);
struct hash *bodyPartHash = newHash(0);
struct hash *sliceTypeHash = newHash(0);
struct hash *imageTypeHash = newHash(0);
struct hash *sectionSetHash = newHash(0);
struct dyString *dy = dyStringNew(0);

/* Read first line of tab file, and from it get all the field names. */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s appears to be empty", lf->fileName);
if (line[0] != '#')
    errAbort("First line of %s needs to start with #, and then contain field names",
    	lf->fileName);
rowHash = hashRowOffsets(line+1);
rowSize = rowHash->elCount;
if (rowSize >= ArraySize(words))
    errAbort("Too many fields in %s", lf->fileName);

/* Check that have all required fields */
    {
    char *fieldName;
    int i;

    for (i=0; i<ArraySize(requiredSetFields); ++i)
        {
	fieldName = requiredSetFields[i];
	if (!hashLookup(raHash, fieldName))
	    errAbort("Field %s is not in %s", fieldName, setRaFile);
	}

    for (i=0; i<ArraySize(requiredItemFields); ++i)
        {
	fieldName = requiredItemFields[i];
	if (!hashLookup(rowHash, fieldName))
	    errAbort("Field %s is not in %s", fieldName, itemTabFile);
	}

    for (i=0; i<ArraySize(requiredFields); ++i)
        {
	fieldName = requiredFields[i];
	if (!hashLookup(rowHash, fieldName) && !hashLookup(raHash, fieldName))
	    errAbort("Field %s is not in %s or %s", fieldName, setRaFile, itemTabFile);
	}
    }

/* Create/find submission record. */
submissionSetId = saveSubmissionSet(conn, raHash);

/* Process rest of tab file. */
while (lineFileNextRowTab(lf, words, rowSize))
    {
    int fullDir = cachedId(conn, "location", "name", 
    	fullDirHash, "fullDir", raHash, rowHash, words);
    int screenDir = cachedId(conn, "location", "name", 
    	screenDirHash, "screenDir", raHash, rowHash, words);
    int thumbDir = cachedId(conn, "location", 
    	"name", thumbDirHash, "thumbDir", raHash, rowHash, words);
    int bodyPart = cachedId(conn, "bodyPart", 
    	"name", bodyPartHash, "bodyPart", raHash, rowHash, words);
    int sliceType = cachedId(conn, "sliceType", 
    	"name", sliceTypeHash, "sliceType", raHash, rowHash, words);
    int imageType = cachedId(conn, "imageType", 
    	"name", imageTypeHash, "imageType", raHash, rowHash, words);
    int treatment = cachedId(conn, "treatment", 
    	"conditions", treatmentHash, "treatment", raHash, rowHash, words);
    char *fileName = getVal("fileName", raHash, rowHash, words, NULL);
    char *submitId = getVal("submitId", raHash, rowHash, words, NULL);
    char *taxon = getVal("taxon", raHash, rowHash, words, NULL);
    char *isEmbryo = getVal("isEmbryo", raHash, rowHash, words, NULL);
    char *age = getVal("age", raHash, rowHash, words, NULL);
    char *sectionSet = getVal("sectionSet", raHash, rowHash, words, "");
    char *sectionIx = getVal("sectionIx", raHash, rowHash, words, "0");
    char *gene = getVal("gene", raHash, rowHash, words, "");
    char *locusLink = getVal("locusLink", raHash, rowHash, words, "");
    char *refSeq = getVal("refSeq", raHash, rowHash, words, "");
    char *genbank = getVal("genbank", raHash, rowHash, words, "");
    char *priority = getVal("priority", raHash, rowHash, words, "200");
    int sectionId = 0;
    int oldId;
    // char *xzy = getVal("xzy", raHash, rowHash, words, xzy);

    if (sectionSet[0] != 0 && !sameString(sectionSet, "0"))
        {
	struct hashEl *hel = hashLookup(sectionSetHash, sectionSet);
	if (hel != NULL)
	    sectionId = ptToInt(hel->val);
	else
	    {
	    sqlUpdate(conn, "NOSQLINJ insert into sectionSet values(default)");
	    sectionId = sqlLastAutoId(conn);
	    hashAdd(sectionSetHash, sectionSet, intToPt(sectionId));
	    }
	}

    dyStringClear(dy);
    sqlDyStringAppend(dy, "select id from image ");
    dyStringPrintf(dy, "where fileName = '%s' ", fileName);
    dyStringPrintf(dy, "and fullLocation = %d",  fullDir);
    oldId = sqlQuickNum(conn, dy->string);
    if (oldId != 0)
        {
	if (replace)
	    {
	    dyStringClear(dy);
	    sqlDyStringPrintf(dy, "delete from image where id = %d", oldId);
	    sqlUpdate(conn, dy->string);
	    }
	else
	    errAbort("%s is already in database line %d of %s", 
	    	fileName, lf->lineIx, lf->fileName);
	}

    dyStringClear(dy);
    sqlDyStringAppend(dy, "insert into image set\n");
    dyStringPrintf(dy, " id = default,\n");
    dyStringPrintf(dy, " fileName = '%s',\n", fileName);
    dyStringPrintf(dy, " fullLocation = %d,\n", fullDir);
    dyStringPrintf(dy, " screenLocation = %d,\n", screenDir);
    dyStringPrintf(dy, " thumbLocation = %d,\n", thumbDir);
    dyStringPrintf(dy, " submissionSet = %d,\n", submissionSetId);
    dyStringPrintf(dy, " sectionSet = %d,\n", sectionId);
    dyStringPrintf(dy, " sectionIx = %s,\n", sectionIx);
    dyStringPrintf(dy, " submitId = '%s',\n", submitId);
    dyStringPrintf(dy, " gene = '%s',\n", gene);
    dyStringPrintf(dy, " locusLink = '%s',\n", locusLink);
    dyStringPrintf(dy, " refSeq = '%s',\n", refSeq);
    dyStringPrintf(dy, " genbank = '%s',\n", genbank);
    dyStringPrintf(dy, " priority = %s,\n", priority);
    dyStringPrintf(dy, " taxon = %s,\n", taxon);
    dyStringPrintf(dy, " isEmbryo = %s,\n", isEmbryo);
    dyStringPrintf(dy, " age = %s,\n", age);
    dyStringPrintf(dy, " bodyPart = %d,\n", bodyPart);
    dyStringPrintf(dy, " sliceType = %d,\n", sliceType);
    dyStringPrintf(dy, " imageType = %d,\n", imageType);
    dyStringPrintf(dy, " treatment = %d\n", treatment);

    sqlUpdate(conn, dy->string);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
database = optionVal("database", database);
replace = optionExists("replace");
bioImageLoad(argv[1], argv[2]);
return 0;
}
