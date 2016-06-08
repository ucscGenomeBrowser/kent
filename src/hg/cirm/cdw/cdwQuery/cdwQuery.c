/* cdwQuery - Get list of tagged files.. */
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

char *clOut = "ra";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwQuery - Get list of tagged files.\n"
  "usage:\n"
  "   cdwQuery 'sql-like query'\n"
  "options:\n"
  "   -out=output format where format is ra, tab, or tags\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"out", OPTION_STRING},
   {NULL, 0},
};

static char *lookupField(void *record, char *key)
/* Lookup a field in a tagStanza. */
{
struct tagStanza *stanza = record;
return tagFindVal(stanza, key);
}

boolean statementMatch(struct rqlStatement *rql, struct tagStanza *stanza,
	struct lm *lm)
/* Return TRUE if where clause and tableList in statement evaluates true for tdb. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, stanza, lookupField, lm);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

int matchCount = 0;
boolean doSelect = FALSE;

void output(int depth, struct rqlStatement *rql, struct tagStorm *tags, struct tagStanza *stanza)
/* Output stanza according to clOut */
{
char *format = clOut;
if (sameString(format, "ra"))
    {
    if (stanza->children == NULL)
	{
	struct slName *field;
	for (field = rql->fieldList; field != NULL; field = field->next)
	    {
	    char *val = tagFindVal(stanza, field->name);
	    if (val != NULL)
		printf("%s\t%s\n", field->name, val);
	    }
	printf("\n");
	}
    }
else if (sameString(format, "tab"))
    {
    if (stanza->children == NULL)
	{
	struct slName *field;
	char *connector = "";
	for (field = rql->fieldList; field != NULL; field = field->next)
	    {
	    char *val = emptyForNull(tagFindVal(stanza, field->name));
	    printf("%s%s", connector, val);
	    connector = "\t";
	    }
	printf("\n");
	}
    }
else if (sameString(format, "tags"))
    {
    struct slName *field;
    for (field = rql->fieldList; field != NULL; field = field->next)
	{
	char *val = tagFindLocalVal(stanza, field->name);
	if (val != NULL)
	    {
	    repeatCharOut(stdout, '\t', depth);
	    printf("%s\t%s\n", field->name, val);
	    }
	}
    printf("\n");
    }
else
    errAbort("Unrecognized format %s", format);
}

void traverse(int depth, struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > matchCount)
	{
	if (statementMatch(rql, stanza, lm))
	    {
	    ++matchCount;
	    if (doSelect)
		{
		output(depth, rql, tags, stanza);
		}
	    }
	if (stanza->children)
	    traverse(depth+1, tags, stanza->children, rql, lm);
	}
    }
}

void cdwQuery(char *rqlQuery)
/* cdwQuery - Get list of tagged files.. */
{
/* Turn rqlQuery string into a parsed out rqlStatement. */
struct rqlStatement *rql = rqlStatementParseString(rqlQuery);

/* Load tags from database */
struct sqlConnection *conn = cdwConnect();
struct tagStorm *tags = cdwTagStorm(conn);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagStormFieldList(tags);
cdwCheckRqlFields(rql, allFieldList);
slSort(&allFieldList, slNameCmp);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);

/* Output header row in tab case */
if (sameString(clOut, "tab"))
    {
    char before = '#';
    struct slName *field;
    for (field = rql->fieldList; field != NULL; field = field->next)
        {
	printf("%c%s", before, field->name);
	before = '\t';
	}
    printf("\n");
    }

/* Traverse tag tree outputting when rql statement matches in select case, just
 * updateing count in count case. */
doSelect = sameWord(rql->command, "select");
struct lm *lm = lmInit(0);
traverse(0, tags, tags->forest, rql, lm);
if (sameWord(rql->command, "count"))
    printf("%d\n", matchCount);

/* Clean up and go home. */
tagStormFree(&tags);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clOut = optionVal("out", clOut);
if (argc != 2)
    usage();
cdwQuery(argv[1]);
return 0;
}
