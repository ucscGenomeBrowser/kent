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

void tagStormQuery(char *query)
/* tagStormQuery - Find stanzas in tag storm based on SQL-like query.. */
{
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(query));
struct rqlStatement *rql = rqlStatementParse(lf);
struct lm *lm = lmInit(0);
struct slName *file;
for (file = rql->tableList; file != NULL; file = file->next)
    {
    struct tagStorm *tags = tagStormFromFile(file->name);
    traverse(tags, tags->forest, rql, lm);
    tagStormFree(&tags);
    }
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
