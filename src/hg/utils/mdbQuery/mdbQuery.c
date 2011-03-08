/* mdbQuery - Select objects from metaDb table.  Print all or selected fields in a variety 
 * of formats. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "rql.h"
#include "mdb.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

/* Variables that can be set from command line. */
char *clOut = "line";
char *clTable = "metaDb";


void usage()
/* Explain usage and exit. */
{
errAbort(
"mdbQuery - Select objects from metaDb table. Print all or selected fields in a variety\n"
"of formats\n"
"usage:\n"
"   mdbQuery sqlStatement\n"
"Where the SQL statement is enclosed in quotations to avoid the shell interpreting it.\n"
"Only a very restricted subset of a single SQL statement (select) is supported.   Examples:\n"
"    mdbQuery \"select count(*) from hg18\"\n"
"counts all of the metaDb objects in the hg18 database and prints the results to stdout\n"
"   mdbQuery \"select  obj,cell,antibody from hg18 where antibody like 'pol%%'\"\n"
"print the obj, cell, and antibody fields from all objects where there is an antibody fields\n"
"and that field starts with pol.  Note obj is treated differently from other fields a bit\n"
"since it comes from the obj column rather than the var/val columns in the database table.\n"
"You can use metaObject as a synonym for obj.\n"
"OPTIONS:\n"
"   -out=type output one of the following types.  Default is %s\n"
"        line - a line full of this=that; pairs\n"
"        3col - 3 column obj/var/val format like in metaDb table\n"
"        tab - tab-separated format.\n"
"        ra - two column ra format with objs as stanzas\n"
"   -table=name - use the given table.  Default is metaDb\n"
, clOut
  );
}

static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {"out", OPTION_STRING},
   {NULL, 0},
};

static char *lookupField(void *record, char *key)
/* Lookup a field in a mdbObj. */
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

static boolean stringMatch(char *a, char *b, boolean wild)
/* Return true if strings a and b are the same.  If wild is true, do comparision 
 * expanding wildcards in a. */
{
if (wild)
    return wildMatch(a,b);
else
    return strcmp(a,b) == 0;
}

static void raOutput(struct rqlStatement *rql, struct mdbObj *mdb, FILE *out)
/* Output fields  from mdb to file in ra format.  */
{
struct slName *fieldList = rql->fieldList, *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    struct mdbVar *var;
    boolean doWild = anyWild(field->name);
    if (stringMatch(field->name, "obj", doWild) || stringMatch(field->name, "metaObject", doWild))
        fprintf(out, "%s %s\n", "metaObject", mdb->obj);
    for (var = mdb->vars; var != NULL; var = var->next)
        {
	if (stringMatch(field->name, var->var, doWild))
	    fprintf(out, "%s %s\n", var->var, var->val);
	}
    }
fprintf(out, "\n");
}

static void fourColOutput(struct rqlStatement *rql, struct mdbObj *mdb, FILE *out)
/* Output fields  from mdb to file in four column obj/var/val format. */
{
struct slName *fieldList = rql->fieldList, *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    struct mdbVar *var;
    boolean doWild = anyWild(field->name);
    for (var = mdb->vars; var != NULL; var = var->next)
        {
        if (stringMatch(field->name, var->var, doWild))
            fprintf(out, "%s\t%s\t%s\n", mdb->obj, var->var, var->val);
        }
    }
}

static void lineOutput(struct rqlStatement *rql, struct mdbObj *mdb, FILE *out)
/* Output fields  from mdb to file in this=that;  line format.  */
{
struct slName *fieldList = rql->fieldList, *field;
fprintf(out, "%s", mdb->obj);
for (field = fieldList; field != NULL; field = field->next)
    {
    struct mdbVar *var;
    boolean doWild = anyWild(field->name);
    for (var = mdb->vars; var != NULL; var = var->next)
        {
	if (stringMatch(field->name, var->var, doWild))
	    fprintf(out, " %s=%s;", var->var, var->val);
	}
    }
fprintf(out, "\n");
}

static void tabOutput(struct rqlStatement *rql, struct mdbObj *mdb, FILE *out)
/* Output fields  from mdb to file in tab-separated format.  */
{
/* First make sure no wildcards in field name */
struct slName *fieldList = rql->fieldList, *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    if (anyWild(field->name))
        errAbort("No wildcards in field names with out=tab option");
    }
for (field = fieldList; field != NULL; field = field->next)
    {
    char *val = NULL;
    if (sameString(field->name, "obj") || sameString(field->name, "metaObject"))
        val = mdb->obj;
    else
	{
	struct mdbVar *var;
	for (var = mdb->vars; var != NULL; var = var->next)
	    {
	    if (sameString(field->name, var->var))
		{
		val = var->val;
		break;
		}
	    }
	}
    fprintf(out, "%s", naForNull(val));
    if (field->next == NULL)
	fprintf(out, "\n");
    else
	fprintf(out, "\t");
    }
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

/* Make sure we actually only have one valid database, and point database variable
 * to it's name. */
if (slCount(dbOrderList) != 1)
    {
    if (dbOrderList == NULL)
	{
	if (rql->tableList == NULL)
	    errAbort("Missing from clause in sql statement");
	else
	    errAbort("No database %s", rql->tableList->name);
	}
    else
        errAbort("Too many matching databases");
    }

db = dbOrderList->val;
char *database = db->name;

/* Grab list of all metaDb obj. */
struct sqlConnection *conn = sqlConnect(database);
struct mdbObj *mdb, *mdbList = mdbObjsQueryAll(conn, clTable);
verbose(2, "%d objects in %s.%s\n", slCount(mdbList), database, clTable);
sqlDisconnect(&conn);

/* Loop through all objects, evaluating the RQL on them. */
struct lm *lm = lmInit(0);
int matchCount = 0;
boolean doSelect = sameString(rql->command, "select");
for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
    {
    if (rqlStatementMatch(rql, mdb, lm))
	{
	matchCount += 1;
	if (doSelect)
	    {
	    if (sameString(clOut, "ra"))
	       raOutput(rql, mdb, stdout);
	    else if (sameString(clOut, "line"))
	       lineOutput(rql, mdb, stdout);
	    else if (sameString(clOut, "3col"))
	       fourColOutput(rql, mdb, stdout);
	    else if (sameString(clOut, "tab"))
	       tabOutput(rql, mdb, stdout);
	    else
	       errAbort("Unknown out type %s", clOut);
	    }
	if (rql->limit >= 0 && matchCount >= rql->limit)
	    break;
	}
    }
if (sameString(rql->command, "count"))
    printf("%d\n", matchCount);
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
