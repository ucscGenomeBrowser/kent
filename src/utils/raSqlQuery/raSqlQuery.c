/* raSqlQuery - Do a SQL-like query on a RA file.. */
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

static char const rcsid[] = "$Id: raSqlQuery.c,v 1.12 2009/11/20 20:00:23 kent Exp $";

static char *clQueryFile = NULL;
static char *clQuery = NULL;
static char *clKey = "track";
static char *clParentField = "subTrack";
static char *clNoInheritField = "noInherit";
static boolean clMerge = FALSE;
static boolean clParent = FALSE;
static boolean clAddFile = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raSqlQuery - Do a SQL-like query on a RA file.\n"
  "usage:\n"
  "   raSqlQuery raFile(s) query-options\n"
  "One of the following query-options must be specified\n"
  "   -queryFile=fileName\n"
  "   \"-query=select list,of,fields where field='this'\"\n"
  "The queryFile just has a query in it in the same form as the query option, but\n"
  "without needing the quotes necessarily.\n"
  "  The syntax of a query statement is very SQL-like.  It must begin with either\n"
  "'select' or 'count'.  Select is followed by a field list, or '*' for all fields\n"
  "Count is not followed by anything.  The 'where' clause is optional, and if it\n"
  "exists it can contain expressions involving fields, numbers, strings, arithmetic, 'and'\n"
  "'or' and so forth.  Unlike SQL there is no 'from' claus.\n"
  "Other options:\n"
  "   -key=keyField - Use the as the key field for merges and parenting. Default %s\n"
  "   -parent - Merge together inheriting on parentField\n"
  "   -parentField=field - Use field as the one that tells us who is our parent. Default %s\n"
  "   -noInheritField=field - If field is present don't inherit fields from parent\n"
  "   -merge - If there are multiple raFiles, records with the same keyField will be\n"
  "          merged together with fields in later files overriding fields in earlier files\n"
  "   -addFile - Add 'file' field to say where record is defined\n"
  "The output will be to stdout, in the form of a .ra file if the select command is used\n"
  "and just a simple number if the count command is used\n"
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
   {NULL, 0},
};

static void mergeRecords(struct raRecord *old, struct raRecord *record, struct lm *lm)
/* Merge record into old,  updating any old fields with new record values. */
{
struct raField *field;
for (field = record->fieldList; field != NULL; field = field->next)
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
old->posList = slCat(old->posList, record->posList);
}

static void mergeParentRecord(struct raRecord *record, struct raRecord *parent, struct lm *lm)
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

static struct raRecord *readRaRecords(int inCount, char *inNames[], 
	char *mergeField, boolean addFile, struct lm *lm)
/* Scan through files, merging records on mergeField if it is non-NULL. */
{
if (inCount <= 0)
    return NULL;
if (mergeField)
    {
    struct raRecord *recordList = NULL, *record;
    struct hash *recordHash = hashNew(0);
    slReverse(&recordList);
    int i;
    for (i=0; i<inCount; ++i)
        {
	char *fileName = inNames[i];
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	while ((record = raRecordReadOne(lf, lm)) != NULL)
	    {
	    if (addFile)
	        record->posList = raFilePosNew(lm, fileName, lf->lineIx);
	    struct raField *keyField = raRecordField(record, mergeField);
	    if (keyField != NULL)
		{
		struct raRecord *oldRecord = hashFindVal(recordHash, keyField->val);
		if (oldRecord != NULL)
		    {
		    mergeRecords(oldRecord, record, lm);
		    }
		else
		    {
		    record->key = keyField;
		    slAddHead(&recordList, record);
		    hashAdd(recordHash, keyField->val, record);
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
	while ((record = raRecordReadOne(lf, lm)) != NULL)
	    {
	    if (addFile)
	        record->posList = raFilePosNew(lm, fileName, lf->lineIx);
	    slAddHead(&recordList, record);
	    }
	lineFileClose(&lf);
	}
    slReverse(&recordList);
    return recordList;
    }
}

boolean rqlStatementMatch(struct rqlStatement *rql, struct raRecord *ra)
/* Return TRUE if where clause in statement evaluates true for ra. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, ra);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

void rqlStatementOutput(struct rqlStatement *rql, struct raRecord *ra, 
	char *addFileField, FILE *out)
/* Output fields  from ra to file.  If addFileField is non-null add a new
 * field with this name at end of output. */
{
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
    }
fprintf(out, "\n");
}

static void addMissingKeys(struct raRecord *list, char *keyField)
/* Add key to all raRecords that don't already have it. */
{
struct raRecord *rec;
for (rec = list; rec != NULL; rec = rec->next)
    {
    if (rec->key == NULL)
        rec->key = raRecordField(rec, keyField);
    }
}

static struct raRecord *findParent(struct raRecord *rec, 
	char *parentFieldName, char *noInheritFieldName, struct hash *hash)
/* Find parent field if possible. */
{
struct raField *noInheritField = raRecordField(rec, noInheritFieldName);
if (noInheritField != NULL)
    return NULL;
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
     warn("%s is a subTrack of %s, but %s doesn't exist", rec->key->val,
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
	hashAdd(hash, rec->key->val, rec);
    }

/* Scan through doing inheritance. */
for (rec = list; rec != NULL; rec = rec->next)
    {
    struct raRecord *parent;
    for (parent = findParent(rec, parentField, noInheritField, hash); parent != NULL;
    	parent = findParent(parent, parentField, noInheritField, hash) )
	{
	mergeParentRecord(rec, parent, lm);
	}
    }
}

void raSqlQuery(int inCount, char *inNames[], struct lineFile *query, char *mergeField, 
	char *parentField, char *noInheritField, struct lm *lm, FILE *out)
/* raSqlQuery - Do a SQL-like query on a RA file.. */
{
struct raRecord *raList = readRaRecords(inCount, inNames, mergeField, clAddFile, lm);
if (parentField != NULL)
    {
    addMissingKeys(raList, clKey);
    inheritFromParents(raList, parentField, noInheritField, lm);
    }
struct rqlStatement *rql = rqlStatementParse(query);
verbose(2, "Got %d records in raFiles\n", slCount(raList));
if (verboseLevel() > 1)
    rqlStatementDump(rql, stderr);
struct raRecord *ra;
int matchCount = 0;
boolean doSelect = sameString(rql->command, "select");
for (ra = raList; ra != NULL; ra = ra->next)
    {
    if (rqlStatementMatch(rql, ra))
        {
	matchCount += 1;
	if (doSelect)
	    {
	    rqlStatementOutput(rql, ra, (clAddFile ? "file" : NULL), out);
	    }
	}
    }
if (!doSelect)
    printf("%d\n", matchCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
clMerge = optionExists("merge");
clParent = optionExists("parent");
clParentField = optionVal("parentField", clParentField);
clKey = optionVal("key", clKey);
clQueryFile = optionVal("queryFile", NULL);
clQuery = optionVal("query", NULL);
clNoInheritField = optionVal("noInheritField", clNoInheritField);
clAddFile = optionExists("addFile");
if (clQueryFile == NULL && clQuery == NULL)
    errAbort("Please specify either the query or queryFile option.");
if (clQueryFile != NULL && clQuery != NULL)
    errAbort("Please specify just one of the query or queryFile options.");
struct lineFile *query = NULL;
if (clQuery)
    query = lineFileOnString("query", TRUE, cloneString(clQuery));
else
    query = lineFileOpen(clQueryFile, TRUE);
struct lm *lm = lmInit(0);
char *mergeField = (clMerge ? clKey : NULL);
char *parentField = (clParent ? clParentField : NULL);
raSqlQuery(argc-1, argv+1, query, mergeField, parentField, clNoInheritField, lm, stdout);
return 0;
}
