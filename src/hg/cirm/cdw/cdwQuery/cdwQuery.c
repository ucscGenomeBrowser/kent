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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwQuery - Get list of tagged files.\n"
  "usage:\n"
  "   cdwQuery 'sql-like query'\n"
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
    if (rql->limit < 0 || rql->limit > matchCount)
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
}

void cdwQuery(char *rqlQuery)
/* cdwQuery - Get list of tagged files.. */
{
/* Turn rqlQuery string into a parsed out rqlStatement. */
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(rqlQuery));
struct rqlStatement *rql = rqlStatementParse(lf);

/* Load tags from database */
struct sqlConnection *conn = cdwConnect();
struct tagStorm *tags = cdwTagStorm(conn);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagTreeFieldList(tags);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);
slSort(&rql->fieldList, slNameCmp);

/* Traverse tag tree outputting when rql statement matches in select case, just
 * updateing count in count case. */
doSelect = sameWord(rql->command, "select");
struct lm *lm = lmInit(0);
traverse(tags, tags->forest, rql, lm);
if (sameWord(rql->command, "count"))
    printf("%d\n", matchCount);

/* Clean up and go home. */
tagStormFree(&tags);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwQuery(argv[1]);
return 0;
}
