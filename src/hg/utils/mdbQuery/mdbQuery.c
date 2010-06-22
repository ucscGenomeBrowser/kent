/* mdbQuery - Select things from metaDb table.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "rql.h"
#include "mdb.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

char *clOut = "line";

void usage()
/* Explain usage and exit. */
{
errAbort(
"mdbQuery - Select things from metaDb table.\n"
"usage:\n"
"   mdbQuery sqlStatement\n"
"Where the SQL statement is enclosed in quotations to avoid the shell interpreting it.\n"
"Only a very restricted subset of a single SQL statement (select) is supported.   Examples:\n"
"    mdbQuery \"select count(*) from hg18\"\n"
"counts all of the objects in hg18 and prints the results to stdout\n"
"   mdbQuery \"select count(*) from *\"\n"
"counts all objects in all databases.\n"
"   mdbQuery \"select  obj,cell,antibody from hg18 where antibody like 'pol%%'\"\n"
"print the obj, cell, and antibody fields from all objects where there is an antibody fields\n"
"and that field starts with pol\n"
"OPTIONS:\n"
"   -out=type output one of the following types.  Default is %s\n"
"        line - a line full of this=that; pairs\n"
"        table - three column obj/var/val format\n"
"        ra - two column ra format with objs as stanzas\n"
"   -table=name - use the given table.  Default is metaDb\n"
, clOut
  );
}

char *clTable = "metaDb";

static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {"out", OPTION_STRING},
   {NULL, 0},
};

static char *lookupField(void *record, char *key)
/* Lookup a field in a tdbRecord. */
{
struct mdbObj *mdb = record;
if (sameString(key, "obj"))
    {
    return mdb->obj;
    }
else
    {
    struct mdbVar *var =  hashFindVal(mdb->varHash, key);
    if (var == NULL)
	return NULL;
    else
	return var->val;
    }
}

static boolean rqlStatementMatch(struct rqlStatement *rql, struct mdbObj *mdb,
	struct lm *lm)
/* Return TRUE if where clause and tableList in statement evaluates true for tdb. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, mdb, lookupField, lm);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

static boolean match(char *a, char *b, boolean wild)
/* Return true if strings a and b are the same.  If wild is true, do comparision 
 * expanding wildcards in a. */
{
if (wild)
    return wildMatch(a,b);
else
    return strcmp(a,b) == 0;
}

static void raOutput(struct rqlStatement *rql, struct mdbObj *mdb, FILE *out)
/* Output fields  from tdb to file in ra format.  */
{
struct slName *fieldList = rql->fieldList, *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    struct mdbVar *var;
    boolean doWild = anyWild(field->name);
    if (match(field->name, "obj", doWild))
        fprintf(out, "%s %s\n", "obj", mdb->obj);
    for (var = mdb->vars; var != NULL; var = var->next)
        {
	if (match(field->name, var->var, doWild))
	    fprintf(out, "%s %s\n", var->var, var->val);
	}
    }
fprintf(out, "\n");
}

static void tableOutput(struct rqlStatement *rql, struct mdbObj *mdb, FILE *out)
/* Output fields  from tdb to file in three column obj/var/val format. */
{
struct slName *fieldList = rql->fieldList, *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    struct mdbVar *var;
    boolean doWild = anyWild(field->name);
    for (var = mdb->vars; var != NULL; var = var->next)
        {
	if (match(field->name, var->var, doWild))
	    fprintf(out, "%s\t%s\t%s\n", mdb->obj, var->var, var->val);
	}
    }
}

static void lineOutput(struct rqlStatement *rql, struct mdbObj *mdb, FILE *out)
/* Output fields  from tdb to file in this=that;  line format.  */
{
struct slName *fieldList = rql->fieldList, *field;
fprintf(out, "%s", mdb->obj);
for (field = fieldList; field != NULL; field = field->next)
    {
    struct mdbVar *var;
    boolean doWild = anyWild(field->name);
    for (var = mdb->vars; var != NULL; var = var->next)
        {
	if (match(field->name, var->var, doWild))
	    fprintf(out, " %s=%s;", var->var, var->val);
	}
    }
fprintf(out, "\n");
}

void mdbQuery(char *statement)
/* mdbQuery - Select things from metaDb table.. */
{
/* Parse out sql statement. */
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(statement));
struct rqlStatement *rql = rqlStatementParse(lf);
lineFileClose(&lf);

/* Figure out list of databases to work on. */
struct slName *db, *dbList = sqlListOfDatabases();
struct slRef *dbOrderList = NULL;
struct slName *t;
for (t = rql->tableList; t != NULL; t = t->next)
    {
    for (db = dbList; db!= NULL; db = db->next)
        {
	if (wildMatch(t->name, db->name))
	    refAdd(&dbOrderList, db);
	}
    }
verbose(2, "%d databases in from clause\n", slCount(dbOrderList));
if (slCount(dbOrderList) != 1)
    {
    if (dbOrderList == NULL)
        errAbort("No database %s", rql->tableList->name);
    else
        errAbort("Too many matching databases");
    }

db = dbOrderList->val;
char *database = db->name;
struct sqlConnection *conn = sqlConnect(database);

struct mdbObj *mdb, *mdbList = mdbObjsQueryAll(conn, clTable);
verbose(2, "%d objects in %s.%s\n", slCount(mdbList), database, clTable);

struct lm *lm = lmInit(0);
for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
    {
    if (rqlStatementMatch(rql, mdb, lm))
	{
	if (sameString(clOut, "ra"))
	   raOutput(rql, mdb, stdout);
	else if (sameString(clOut, "line"))
	   lineOutput(rql, mdb, stdout);
	else if (sameString(clOut, "table"))
	   tableOutput(rql, mdb, stdout);
	else
	   errAbort("Unknown out type %s", clOut);
	}
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clTable = optionVal("table", clTable);
clOut = optionVal("out", clOut);
mdbQuery(argv[1]);
return 0;
}
