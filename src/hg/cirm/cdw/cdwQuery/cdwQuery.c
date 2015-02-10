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

void tagStanzaAddLongLong(struct tagStorm *tagStorm, struct tagStanza *stanza, char *var, 
    long long val)
/* Add long long integer valued tag to stanza */
{
char buf[32];
safef(buf, sizeof(buf), "%lld", val);
tagStanzaAdd(tagStorm, stanza, var, buf);
}

void tagStanzaAddDouble(struct tagStorm *tagStorm, struct tagStanza *stanza, char *var, 
    double val)
/* Add double valued tag to stanza */
{
char buf[32];
safef(buf, sizeof(buf), "%g", val);
tagStanzaAdd(tagStorm, stanza, var, buf);
}

struct tagStorm *tagStormFromDatabase(struct sqlConnection *conn)
/* Load  cdwMetaTags.tags, cdwFile.tags, and select other fields into a tag
 * storm for searching */
{
/* First pass through the cdwMetaTags table.  Make up a high level stanza for each
 * row, and save a reference to it in metaTree. */
struct tagStorm *tagStorm = tagStormNew("constructed from cdwMetaTags and cdwFile");
struct rbTree *metaTree = intValTreeNew();
char query[512];
sqlSafef(query, sizeof(query), "select id,tags from cdwMetaTags");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned id = sqlUnsigned(row[0]);
    char *cgiVars = row[1];
    struct tagStanza *stanza = tagStanzaNew(tagStorm, NULL);
    char *var, *val;
    while (cgiParseNext(&cgiVars, &var, &val))
	 {
         tagStanzaAdd(tagStorm, stanza, var, val);
	 }
    slReverse(&stanza->tagList);
    intValTreeAdd(metaTree, id, stanza);
    }
sqlFreeResult(&sr);


/* Now go through the cdwFile table, adding it files as substanzas to 
 * meta cdwMetaTags stanzas. */
sqlSafef(query, sizeof(query), 
    "select cdwFile.*,cdwValidFile.* from cdwFile,cdwValidFile "
    "where cdwFile.id=cdwValidFile.fileId ");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwFile ef;
    struct cdwValidFile vf;
    cdwFileStaticLoad(row, &ef);
    cdwValidFileStaticLoad(row + CDWFILE_NUM_COLS, &vf);
    struct tagStanza *metaStanza = intValTreeFind(metaTree, ef.metaTagsId);
    if (metaStanza != NULL)
	{
	struct tagStanza *stanza = tagStanzaNew(tagStorm, metaStanza);

	/* Add stuff we want from non-tag fields */
	tagStanzaAdd(tagStorm, stanza, "accession", vf.licensePlate);
	if (!isEmpty(ef.deprecated))
	    tagStanzaAdd(tagStorm, stanza, "deprecated", ef.deprecated);
	tagStanzaAdd(tagStorm, stanza, "md5", ef.md5);
	tagStanzaAdd(tagStorm, stanza, "submit_file_name", ef.submitFileName);
	tagStanzaAdd(tagStorm, stanza, "cdw_file_name", ef.cdwFileName);
	tagStanzaAddLongLong(tagStorm, stanza, "size", ef.size);
	if (vf.itemCount != 0)
	    tagStanzaAddLongLong(tagStorm, stanza, "item_count", vf.itemCount);
	if (vf.mapRatio != 0)
	    tagStanzaAddDouble(tagStorm, stanza, "map_ratio", vf.mapRatio);

	/* Add tag field */
	char *cgiVars = ef.tags;
	char *var,*val;
	while (cgiParseNext(&cgiVars, &var, &val))
	    tagStanzaAdd(tagStorm, stanza, var, val);

	slReverse(&stanza->tagList);
	}
    }
sqlFreeResult(&sr);
rbTreeFree(&metaTree);
return tagStorm;
}

void cdwQuery(char *rqlQuery)
/* cdwQuery - Get list of tagged files.. */
{
/* Turn rqlQuery string into a parsed out rqlStatement. */
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(rqlQuery));
struct rqlStatement *rql = rqlStatementParse(lf);

/* Load tags from database */
struct sqlConnection *conn = cdwConnect();
struct tagStorm *tags = tagStormFromDatabase(conn);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagTreeFieldList(tags);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);

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
