/* raSqlQuery - Do a SQL-like query on a RA file.. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"
#include "localmem.h"
#include "tokenizer.h"
#include "sqlNum.h"
#include "raRecord.h"
#include "rql.h"
#include "portable.h"
#include "../../hg/inc/hdb.h"  /* Just for strict option. */


static char *clQueryFile = NULL;
static char *clQuery = NULL;
static char *clKey = "name";
static char *clParentField = "subTrack";
static char *clNoInheritField = "noInherit";
static boolean clMerge = FALSE;
static boolean clParent = FALSE;
static boolean clAddFile = FALSE;
static boolean clAddDb = FALSE;
static char *clRestrict = NULL;
static boolean clStrict = FALSE;
static char *clDb = NULL;
static boolean clOverrideNeeded = FALSE;

static char *clTrackDbRootDir = "~/kent/src/hg/makeDb/trackDb";
static char *clTrackDbRelPath = "../../trackDb*.ra ../trackDb*.ra trackDb*.ra"; 

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raSqlQuery - Do a SQL-like query on a RA file.\n"
  "   raSqlQuery raFile(s) query-options\n"
  "or\n"
  "   raSqlQuery -db=dbName query-options\n"
  "Where dbName is a UCSC Genome database like hg18, sacCer1, etc.\n"
  "One of the following query-options must be specified\n"
  "   -queryFile=fileName\n"
  "   \"-query=select list,of,fields from file where field='this'\"\n"
  "The queryFile just has a query in it in the same form as the query option.\n"
  "The syntax of a query statement is very SQL-like. The most common commands are:\n"
  "    select tag1,tag2,tag3 where tag1 like 'prefix%%'\n"
  "where the %% is a SQL wildcard.  Sorry to mix wildcards. Another command query is\n"
  "    select count(*) from * where tag = 'val\n"
  "The from list is optional.  If it exists it is a list of raFile names\n"
  "    select track,type from *Encode* where type like 'bigWig%%'\n"
  "Other command line options:\n"
  "   -addFile - Add 'file' field to say where record is defined\n"
  "   -addDb - Add 'db' field to say where record is defined\n"
  "   -strict - Used only with db option.  Only report tracks that exist in db\n"
  "   -key=keyField - Use the as the key field for merges and parenting. Default %s\n"
  "   -parent - Merge together inheriting on parentField\n"
  "   -parentField=field - Use field as the one that tells us who is our parent. Default %s\n"
  "   -overrideNeeded - If set records are only overridden field-by-field by later records\n"
  "               if 'override' follows the track name. Otherwiser later record replaces\n"
  "               earlier record completely.  If not set all records overridden field by field\n"
  "   -noInheritField=field - If field is present don't inherit fields from parent\n"
  "   -merge - If there are multiple raFiles, records with the same keyField will be\n"
  "          merged together with fields in later files overriding fields in earlier files\n"
  "   -restrict=keyListFile - restrict output to only ones with keys in file.\n"
  "   -db=hg19 - Acts on trackDb files for the given database.  Sets up list of files\n"
  "              appropriately and sets parent, merge, and override all.\n"
  "              Use db=all for all databases\n"
  , clKey, clParentField
  );
}


static struct optionSpec options[] = {
   {"queryFile", OPTION_STRING},
   {"query", OPTION_STRING},
   {"merge", OPTION_BOOLEAN},
   {"key", OPTION_STRING},
   {"parent", OPTION_BOOLEAN},
   {"parentField", OPTION_STRING},
   {"noInheritField", OPTION_STRING},
   {"addFile", OPTION_BOOLEAN},
   {"addDb", OPTION_BOOLEAN},
   {"restrict", OPTION_STRING},
   {"strict", OPTION_BOOLEAN},
   {"db", OPTION_STRING},
   {"overrideNeeded", OPTION_BOOLEAN},
   {NULL, 0},
};


struct dbPath
/* A database directory and path. */
    {
    struct dbPath *next;
    char *db;
    char *dir;
    };

static struct dbPath *getDbPathList(char *rootDir)
/* Get list of all "database" directories with any trackDb.ra files two under us. */
{
char *root = simplifyPathToDir(rootDir);
struct dbPath *pathList = NULL, *path;
struct fileInfo *org, *orgList = listDirX(root, "*", TRUE);
for (org = orgList; org != NULL; org = org->next)
    {
    if (org->isDir)
        {
	struct fileInfo *db, *dbList = listDirX(org->name, "*", TRUE);
	for (db = dbList; db != NULL; db = db->next)
	    {
	    if (db->isDir)
	        {
		char trackDbPath[PATH_LEN];
		safef(trackDbPath, sizeof(trackDbPath), "%s/trackDb.ra", db->name);
		if (fileExists(trackDbPath))
		    {
		    AllocVar(path);
		    path->dir = cloneString(db->name);
		    char *s = strrchr(db->name, '/');
		    assert(s != NULL);
		    path->db = cloneString(s+1);
		    slAddHead(&pathList, path);
		    }
		}
	    }
	slFreeList(&dbList);
	}
    }
slFreeList(&orgList);
slReverse(&pathList);
freez(&root);
return pathList;
}

static struct slName *dbPathToFiles(struct dbPath *p)
/* Convert dbPath to a list of files. */
{
struct slName *pathList = NULL;
char *dbDir = p->dir;
char *buf = cloneString(clTrackDbRelPath);
char *line = buf, *word;
while ((word = nextWord(&line)) != NULL)
    {
    char relDir[PATH_LEN], relFile[PATH_LEN], relSuffix[PATH_LEN];
    splitPath(word, relDir, relFile, relSuffix);
    char dir[PATH_LEN];
    safef(dir, sizeof(dir), "%s/%s", dbDir, relDir);
    char *path = simplifyPathToDir(dir);
    char pattern[PATH_LEN];
    safef(pattern, sizeof(pattern), "%s%s", relFile, relSuffix);
    struct fileInfo *fi, *fiList = listDirX(path, pattern, TRUE);
    for (fi = fiList; fi != NULL; fi = fi->next)
	slNameAddHead(&pathList, fi->name);
    freeMem(path);
    slFreeList(&fiList);
    }
freeMem(buf);
slReverse(&pathList);
return pathList;
}



static void mergeRecords(struct raRecord *old, struct raRecord *record, char *key, struct lm *lm)
/* Merge record into old,  updating any old fields with new record values. */
{
struct raField *field;
for (field = record->fieldList; field != NULL; field = field->next)
    {
    if (!sameString(field->name, key))
	{
	struct raField *oldField = raRecordField(old, field->name);
	if (oldField != NULL)
	    oldField->val = field->val;
	else
	    {
	    lmAllocVar(lm, oldField);
	    oldField->name = field->name;
	    oldField->val = field->val;
	    slAddTail(&old->fieldList, oldField);
	    }
	}
    }
old->posList = slCat(old->posList, record->posList);
}

static void mergeParentRecord(struct raRecord *record, struct raRecord *parent, 
	struct lm *lm)
/* Merge in parent record.  This only updates fields that are in parent but not record. */
{
struct raField *parentField;
for (parentField= parent->fieldList; parentField!= NULL; parentField= parentField->next)
    {
    struct raField *oldField = raRecordField(record, parentField->name);
    if (oldField == NULL)
        {
	struct raField *newField;
	lmAllocVar(lm, newField);
	newField->name = parentField->name;
	newField->val = parentField->val;
	slAddTail(&record->fieldList, newField);
	}
    }
}

static struct raRecord *readRaRecords(int inCount, char *inNames[], char *keyField,
	boolean doMerge, char *db, boolean addDb,
	boolean overrideNeeded, struct lm *lm)
/* Scan through files, merging records on key if doMerge. */
{
if (inCount <= 0)
    return NULL;
if (doMerge)
    {
    struct raRecord *recordList = NULL, *record;
    struct hash *recordHash = hashNew(0);
    int i;
    for (i=0; i<inCount; ++i)
        {
	char *fileName = inNames[i];
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	while ((record = raRecordReadOne(lf, keyField, lm)) != NULL)
	    {
	    record->posList = raFilePosNew(lm, fileName, lf->lineIx);
	    if (addDb)
		record->db = db;
	    char *key = record->key;
	    if (key != NULL)
		{
		struct raRecord *oldRecord = hashFindVal(recordHash, key);
		if (oldRecord != NULL)
		    {
		    if (overrideNeeded && !record->override)
		        {
			oldRecord->fieldList = record->fieldList;
			oldRecord->posList = record->posList;
			oldRecord->settingsByView = record->settingsByView;
			oldRecord->subGroups = record->subGroups;
			oldRecord->view = record->view;
			oldRecord->viewHash = record->viewHash;
			}
		    else
			mergeRecords(oldRecord, record, keyField, lm);
		    }
		else
		    {
		    slAddHead(&recordList, record);
		    hashAdd(recordHash, key, record);
		    }
		}
	    }
	lineFileClose(&lf);
	}
    slReverse(&recordList);
    return recordList;
    }
else
    {
    struct raRecord *recordList = NULL;
    int i;
    for (i=0; i<inCount; ++i)
        {
	char *fileName = inNames[i];
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	struct raRecord *record;
	while ((record = raRecordReadOne(lf, keyField, lm)) != NULL)
	    {
	    record->posList = raFilePosNew(lm, fileName, lf->lineIx);
	    slAddHead(&recordList, record);
	    }
	lineFileClose(&lf);
	}
    slReverse(&recordList);
    return recordList;
    }
}

static char *lookupField(void *record, char *key)
/* Lookup a field in a raRecord. */
{
struct raRecord *ra = record;
struct raField *field = raRecordField(ra, key);
if (field == NULL)
    return NULL;
else
    return field->val;
}

boolean rqlStatementMatch(struct rqlStatement *rql, struct raRecord *ra, struct lm *lm)
/* Return TRUE if where clause and tableList in statement evaluates true for ra. */
{
if (rql->tableList != NULL)
    {
    boolean gotMatch = FALSE;
    struct slName *table;
    for (table = rql->tableList; table != NULL; table = table->next)
        {
	struct raFilePos *fp;
	for (fp = ra->posList; fp != NULL; fp = fp->next)
	    {
	    if (wildMatch(table->name, fp->fileName))
	         {
		 gotMatch = TRUE;
		 break;
		 }
	    }
	if (gotMatch)
	    break;
	}
    if (!gotMatch)
        return FALSE;
    }
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, ra, lookupField, lm);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

void rqlStatementOutput(struct rqlStatement *rql, struct raRecord *ra, 
	char *addFileField, char *addDbField, FILE *out)
/* Output fields  from ra to file.  If addFileField is non-null add a new
 * field with this name at end of output. */
{
if (addDbField)
    fprintf(out, "%s %s\n", addDbField, ra->db);
struct slName *fieldList = rql->fieldList, *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    struct raField *r;
    boolean doWild = anyWild(field->name);
    for (r = ra->fieldList; r != NULL; r = r->next)
        {
	boolean match;
	if (doWild)
	    match = wildMatch(field->name, r->name);
	else
	    match = (strcmp(field->name, r->name) == 0);
	if (match)
	    fprintf(out, "%s %s\n", r->name, r->val);
	}
    }
if (addFileField != NULL)
    {
    fprintf(out, "%s", addFileField);
    struct raFilePos *fp;
    for (fp = ra->posList; fp != NULL; fp = fp->next)
	{
	fprintf(out, " %s %d", fp->fileName, fp->lineIx);
	}
    fprintf(out, "\n");
    }
fprintf(out, "\n");
}

struct hash *hashAllWordsInFile(char *fileName)
/* Make a hash of all space or line delimited words in file. */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
while (lineFileNext(lf, &line, NULL))
    {
    while ((word = nextWord(&line)) != NULL)
        hashAdd(hash, word, NULL);
    }
lineFileClose(&lf);
return hash;
}


static struct raRecord *findParent(struct raRecord *rec, 
	char *parentFieldName, struct hash *hash)
/* Find parent field if possible. */
{
struct raField *parentField = raRecordField(rec, parentFieldName);
if (parentField == NULL)
    return NULL;
char *parentLine = parentField->val;
int len = strlen(parentLine);
char buf[len+1];
strcpy(buf, parentLine);
char *parentName = firstWordInLine(buf);
struct raRecord *parent = hashFindVal(hash, parentName);
if (parent == NULL)
     warn("%s is a subTrack of %s, but %s doesn't exist", rec->key,
     	parentField->val, parentField->val);
return parent;
}

static void inheritFromParents(struct raRecord *list, char *parentField, char *noInheritField,
	struct lm *lm)
/* Go through list.  If an element has a parent field, then fill in non-existent fields from
 * parent. */
{
/* Build up hash of records indexed by key field. */
struct hash *hash = hashNew(0);
struct raRecord *rec;
for (rec = list; rec != NULL; rec = rec->next)
    {
    if (rec->key != NULL)
	hashAdd(hash, rec->key, rec);
    }

/* Scan through linking up parents. */
for (rec = list; rec != NULL; rec = rec->next)
    {
    struct raRecord *parent = findParent(rec, parentField, hash);
    if (parent != NULL)
	{
	rec->parent = parent;
	rec->olderSibling = parent->children;
	parent->children = rec;
	}
    }

/* Scan through doing inheritance. */
for (rec = list; rec != NULL; rec = rec->next)
    {
    /* First inherit from view. */
    char *viewName = rec->view;
    if (viewName != NULL)
        {
	struct slPair *view;
	if (rec->parent == NULL)
	     {
	     verbose(2, "%s has view %s but no parent\n", rec->key, viewName);
	     continue;
	     }
	for (view = rec->parent->settingsByView; view != NULL; view = view->next)
	    {
	    if (sameString(view->name, viewName))
		break;
	    else 
	        {
		if (rec->parent->viewHash != NULL)
		    {
		    char *alias = hashFindVal(rec->parent->viewHash, viewName);
		    if (alias != NULL)
			if (sameString(view->name, alias))
			    break;
		    }
		}
	    }
	if (view != NULL)
	    {
	    struct slPair *setting;
	    for (setting = view->val; setting != NULL; setting = setting->next)
	        {
		struct raField *oldField = raRecordField(rec, setting->name);
		if (oldField == NULL)
		    {
		    struct raField *newField;
		    lmAllocVar(lm, newField);
		    newField->name = lmCloneString(lm, setting->name);
		    newField->val = lmCloneString(lm, setting->val);
		    slAddTail(&rec->fieldList, newField);
		    }
		}
	    }
	else 
	    {
	    verbose(3, "view %s not in parent settingsByView of %s\n", viewName, rec->key);
	    }
	}

    /* Then inherit from parents. */
    struct raRecord *parent;
    for (parent = rec->parent; parent != NULL; parent = parent->parent)
	{
	mergeParentRecord(rec, parent, lm);
	}
    }
}

int raSqlQuery(struct rqlStatement *rql, int inCount, char *inNames[], 
	char *db, char *parentField, struct lm *lm, FILE *out)
/* raSqlQuery - Do a SQL-like query on a RA file.. */
{
struct raRecord *raList = readRaRecords(inCount, inNames, clKey, 
	clMerge, db, clAddDb, clOverrideNeeded, lm);
if (parentField != NULL)
    {
    inheritFromParents(raList, parentField, clNoInheritField, lm);
    }
if (clRestrict)
    {
    struct hash *restrictHash = hashAllWordsInFile(clRestrict);
    restrictHash = hashAllWordsInFile(clRestrict);
    struct raRecord *newList = NULL, *next, *rec;
    for (rec = raList; rec != NULL; rec = next)
        {
	next = rec->next;
	if (rec->key && hashLookup(restrictHash, rec->key))
	    {
	    slAddHead(&newList, rec);
	    }
	}
    slReverse(&newList);
    raList = newList;
    hashFree(&restrictHash);
    }
verbose(2, "Got %d records in raFiles\n", slCount(raList));
if (verboseLevel() > 1)
    rqlStatementDump(rql, stderr);
struct raRecord *ra;
int matchCount = 0;
boolean doSelect = sameString(rql->command, "select");
for (ra = raList; ra != NULL; ra = ra->next)
    {
    if (rqlStatementMatch(rql, ra, lm))
        {
	if (!clStrict || (ra->key && hTableOrSplitExists(db, ra->key)))
	    {
	    matchCount += 1;
	    if (doSelect)
		{
		rqlStatementOutput(rql, ra, (clAddFile ? "file" : NULL), 
			(clAddDb ? "db" : NULL), out);
		}
	    }
	}
    }
return matchCount;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clMerge = optionExists("merge");
clParent = optionExists("parent");
clParentField = optionVal("parentField", clParentField);
clKey = optionVal("key", clKey);
clQueryFile = optionVal("queryFile", NULL);
clQuery = optionVal("query", NULL);
clNoInheritField = optionVal("noInheritField", clNoInheritField);
clAddFile = optionExists("addFile");
clAddDb = optionExists("addDb");
clRestrict = optionVal("restrict", NULL);
clStrict = optionExists("strict");
clOverrideNeeded = optionExists("overrideNeeded");
clDb = optionVal("db", NULL);
if (argc < 2 && !clDb)
    usage();
if (clQueryFile == NULL && clQuery == NULL)
    errAbort("Please specify either the query or queryFile option.");
if (clQueryFile != NULL && clQuery != NULL)
    errAbort("Please specify just one of the query or queryFile options.");
struct lm *lm = lmInit(0);
if (clStrict && clDb == NULL)
    errAbort("Only can use -strict with -db.");

if (clDb != NULL)
    {
    clMerge = TRUE;
    clParent = TRUE;
    clOverrideNeeded = TRUE;
    clKey = "track";
    if (sameString(clDb, "all"))
        clAddDb = TRUE;
    }
/* Parse query */
struct lineFile *query;
if (clQuery)
    query = lineFileOnString("query", TRUE, cloneString(clQuery));
else
    query = lineFileOpen(clQueryFile, TRUE);
struct rqlStatement *rql = rqlStatementParse(query);

char *parentField = (clParent ? clParentField : NULL);
char **fileNames;
int fileCount;
int matchCount = 0;
if (clDb)
    {
    if (argc != 1)
	 errAbort("You can't specify any input files with the -db option.");
    struct dbPath *db, *dbList = getDbPathList(clTrackDbRootDir);
    boolean gotAny = FALSE;
    for (db = dbList; db != NULL; db = db->next)
	{
	if (sameString(clDb, "all") || sameString(clDb, db->db))
	    {
	    struct slName *path, *pathList = dbPathToFiles(db);
	    fileCount = slCount(pathList);
	    if (fileCount == 0)
		errAbort("No paths returned by dbToTrackDbFiles(%s)", clDb);
	    AllocArray(fileNames, fileCount);
	    int i;
	    for (i=0, path = pathList; path != NULL; path = path->next, ++i)
		{
		fileNames[i] = path->name;
		}
	    matchCount += raSqlQuery(rql, fileCount, fileNames, db->db, parentField, lm, stdout);
	    gotAny = TRUE;
	    }
	}
    if (!gotAny)
        errAbort("No database named %s found off %s\n", clDb, clTrackDbRootDir);
    }
else
    {
    fileNames = argv+1;
    fileCount = argc-1;
    matchCount += raSqlQuery(rql, fileCount, fileNames, "n/a", parentField, lm, stdout);
    }
if (sameString(rql->command, "count"))
    printf("%d\n", matchCount);
return 0;
}
