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
  "Only select statements are allowed.  No joins are allowed. Sql functions are also\n"
  "not allowed except for count(*).  The * is allowed in the field list in a more general way \n"
  "than SQL, but otherwise it is a subset of SQL\n"
  "usage:\n"
  "   tagStormQuery 'query'\n"
  "examples:\n"
  "   tagStormQuery 'select name,number from tagStorm.txt where number > 100'\n"
  "       This will print out the name and number fields where the number field is over 100\n"
  "       from stanzas in the file tagStorm.txt\n" 
  "   tagStormQuery 'select n*,m* from tagStorm.txt where number != 0'\n"
  "       This will print out all fields starting with 'n' or 'm' where number is non-zero\n"
  "   tagStormQuery 'select * from tagStorm.txt where name'\n"
  "       This prints out all fields in stanzas where name is defined\n"
  "   tagStormQuery 'select a,b,c from tagStorm.txt'\n"
  "       This prints out the a,b, and c fields from tagStorm.txt\n"
  "   tagStormQuery 'select count(*) from tagStorm.txt where name and not number'\n"
  "       Print out number of stanzas where name is defined but number is not\n"
  "   tagStormQuery 'select * from tagStorm.txt where name like \"%%Smith\"'\n"
  "       Print out everything from stanzas where name tag ends in \"Smith\"'\n"
  "   tagStormQuery 'select * from tagStorm.txt where number=1 or number=2'\n"
  "       Print out all fields where number is one or two\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

int matchCount = 0;
boolean doSelect = FALSE;

void traverse(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list. */
{
struct tagStanza *stanza;
int limit = rql->limit;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children)
	traverse(tags, stanza->children, rql, lm);
    else    /* Just apply query to leaves */
	{
	if (tagStanzaRqlMatch(rql, stanza, lm))
	    {
	    ++matchCount;
	    if (doSelect && (limit == 0 || matchCount <= limit))
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

void tagStormQueryMain(char *query)
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
struct slName *allFieldList = tagStormFieldList(tags);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);
uglyf("limit = %d\n", rql->limit);

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
tagStormQueryMain(argv[1]);
return 0;
}
