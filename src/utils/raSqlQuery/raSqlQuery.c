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

static char const rcsid[] = "$Id: raSqlQuery.c,v 1.10 2009/11/20 07:41:56 kent Exp $";

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
  "   -merge=keyField - If there are multiple raFiles, records with the same keyField will be\n"
  "          merged together with fields in later files overriding fields in earlier files\n"
  "   -skipMissing - If merging, skip records without keyfield rather than abort\n"
  "The output will be to stdout, in the form of a .ra file if the select command is used\n"
  "and just a simple number if the count command is used\n"
  );
}

static char *clQueryFile = NULL;
static char *clQuery = NULL;
static char *clMerge = NULL;
static boolean clSkipMissing = FALSE;

static struct optionSpec options[] = {
   {"queryFile", OPTION_STRING},
   {"query", OPTION_STRING},
   {"merge", OPTION_STRING},
   {"skipMissing", OPTION_BOOLEAN},
   {NULL, 0},
};

struct rqlEval rqlEvalOnRecord(struct rqlParse *p, struct raRecord *ra);
/* Evaluate self on ra. */

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
}

static struct raRecord *readRaRecords(int inCount, char *inNames[], char *mergeField, struct lm *lm)
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

void rqlStatementOutput(struct rqlStatement *rql, struct raRecord *ra, FILE *out)
/* Output fields  from ra to file. */
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
    }
fprintf(out, "\n");
}

void raSqlQuery(int inCount, char *inNames[], struct lineFile *query, char *mergeField, struct lm *lm,
	FILE *out)
/* raSqlQuery - Do a SQL-like query on a RA file.. */
{
struct raRecord *raList = readRaRecords(inCount, inNames, mergeField, lm);
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
	    rqlStatementOutput(rql, ra, out);
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
clMerge = optionVal("merge", NULL);
clSkipMissing = optionExists("skipMissing");
clQueryFile = optionVal("queryFile", NULL);
clQuery = optionVal("query", NULL);
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
raSqlQuery(argc-1, argv+1, query, clMerge, lm, stdout);
return 0;
}
