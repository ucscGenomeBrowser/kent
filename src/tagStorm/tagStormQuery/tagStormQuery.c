/* tagStormQuery - Find stanzas in tag storm based on SQL-like query.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "rql.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormQuery - Find stanzas in tag storm based on SQL-like query.\n"
  "usage:\n"
  "   tagStormQuery 'query'\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
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

void traverse(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children)
	traverse(tags, stanza->children, rql, lm);
    else    /* Just apply query to leaves */
	{
	if (statementMatch(rql, stanza, lm))
	    {
	    ++matchCount;
	    if (doSelect)
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
	}
    }
}

void tagStormQuery(char *query)
/* tagStormQuery - Find stanzas in tag storm based on SQL-like query.. */
{
/* Get parsed out query */
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(query));
struct rqlStatement *rql = rqlStatementParse(lf);
int stormCount = slCount(rql->tableList);
if (stormCount != 1)
    errAbort("Can only handle one tag storm file in query, got %d", stormCount);
char *tagsFileName = rql->tableList->name;

/* Read in tags */
struct tagStorm *tags = tagStormFromFile(tagsFileName);

/* Expand any field names with wildcards. */
struct slName *allFieldList = tagTreeFieldList(tags);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);

/* Traverse tree applying query */
struct lm *lm = lmInit(0);
doSelect = sameWord(rql->command, "select");
traverse(tags, tags->forest, rql, lm);
tagStormFree(&tags);
if (sameWord(rql->command, "count"))
    printf("%d\n", matchCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
tagStormQuery(argv[1]);
return 0;
}
