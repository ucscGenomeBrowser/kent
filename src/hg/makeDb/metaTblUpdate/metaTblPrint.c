/* metaTblPrint - Prints metadata objects and variables from the metaTbl. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlList.h"
#include "jksql.h"
#include "trackDb.h"
#include "hui.h"
#include "hdb.h"
#include "hVarSubst.h"
#include "obscure.h"
#include "portable.h"
#include "dystring.h"
#include "metaTbl.h"

static char const rcsid[] = "$Id: metaTblPrint.c,v 1.1 2010/03/18 23:37:51 tdreszer Exp $";

#define DB_DEFAULT      "hg19"
#define OBJTYPE_DEFAULT "table"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaTblPrint - Prints metadata objects and variables from the metaTbl.\n\n"
  "There are two basic views of the data: by objects and by variables.  Unless a variable but "
  "no object is requested the default view is by object.  Each line of output will contain "
  "object and all it's var=val pairs as 'formatted metadata' line.  In var view, a single line "
  "per var=val and then a space seprated list of each obj(type) follows.  Long view will place "
  "each object and var=val on a separate line with tab indentation.  Alternatively, count will "
  "return a count of unique obj var=val combinations. It is also possible to select a "
  "combination of vars by entering a string of var=val pairs.\n\n"
  "usage:\n"
  "   metaTblPrint [-db=] [-table=] [-long/-count]\n"
  "                [-all [-byVar]]\n"
  "                [-obj=] [-var= [-val=]]]\n"
  "                [-var= [-val=]]\n"
  "                [-vars=\"var1=val1 var2=val2...\"]\n\n"
  "Options:\n"
  "    -db      Database to query.  Default is '" DB_DEFAULT "'.\n"
  "    -table   Table to query.  Default is '" METATBL_DEFAULT_NAME "'.\n"
  "    -long    Print each obj, var=val as separate line.\n"
  "    -count   Just print count of unique obj var=val combinations.\n"
  "  Four alternate ways to select metadata:\n"
  "    -all       Will print entire table (this could be huge).\n"
  "      -byVar   Print which objects belong to which var=val pairs.\n"
  "    -obj={objName}  Request a single object.  The request can be further narrowed by var and val.\n"
  "    -var={varName}  Request a single variable.  The request can be further narrowed by val.\n"
  "    -vars={var=val...}  Request a combination of var=val pairs.  Example:\n"
  "       -vars=\"grant=Snyder cell=GM12878 antibody=CTCF\" will return all objs\n"
  "             that satify those constraints.\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"db",      OPTION_STRING}, // default "hg19"
    {"table",   OPTION_STRING}, // default "metaTbl"
    {"long",    OPTION_BOOLEAN},// long format
    {"count",   OPTION_BOOLEAN},// returns only count of unique obj var=val combinations
    {"all",     OPTION_BOOLEAN},// query entire table
    {"byVar",   OPTION_BOOLEAN},// With -all prints from var perspective
    {"obj",     OPTION_STRING}, // objName or objId
    {"var",     OPTION_STRING}, // variable
    {"val",     OPTION_STRING}, // value
    {"vals",     OPTION_STRING},// var1=val1 var2=val2...
    {NULL,      0}
};

int main(int argc, char *argv[])
// Process command line.
{
struct metaObj   * metaObjs   = NULL;
struct metaByVar * metaByVars = NULL;

optionInit(&argc, argv, optionSpecs);
char *db    = optionVal("db",   DB_DEFAULT);
char *table = optionVal("table",METATBL_DEFAULT_NAME);
boolean printLong = optionExists("long");
boolean printCount = optionExists("count");
boolean byVar = FALSE;

if(printLong && printCount)
    usage();

boolean all = optionExists("all");
if(all)
    {
    if(optionExists("obj")
    || optionExists("var") || optionExists("val") || optionExists("vars"))
        usage();

    byVar = optionExists("byVar");
    }
else if(optionExists("obj"))
    {
    byVar = FALSE;
    metaObjs = metaObjCreate(optionVal("obj",  NULL),NULL,optionVal("var", NULL), NULL,optionVal("val", NULL));
    }
else if(optionExists("var"))
    {
    byVar = TRUE;
    metaByVars =  metaByVarCreate(optionVal("var", NULL),NULL,optionVal("val", NULL));
    }
else if(optionExists("vars"))
    {
    byVar = TRUE;
    metaByVars = metaByVarsLineParse(optionVal("vars", NULL));
    }
else
    usage();

struct sqlConnection *conn = sqlConnect(db);
if(byVar)
    {
    struct metaByVar * queryResults = metaByVarsQuery(conn,table,metaByVars);
    if(queryResults == NULL)
        verbose(1, "No metadata met your selection criteria\n");
    else
        {
        if(printCount)
            printf("%d\n",metaByVarCount(queryResults));
        else
            metaByVarPrint(queryResults,printLong); // FIXME: write
        metaByVarsFree(&queryResults);
        }
    }
else
    {
    struct metaObj * queryResults = metaObjQuery(conn,table,metaObjs);
    if(queryResults == NULL)
        verbose(1, "No metadata met your selection criteria\n");
    else
        {
        if(printCount)
            printf("%d\n",metaObjCount(queryResults));
        else
            metaObjPrint(queryResults,printLong); // FIXME: write
        metaObjsFree(&queryResults);
        }
    }
sqlDisconnect(&conn);

if(metaObjs)
    metaObjsFree(&metaObjs);
if(metaByVars)
    metaByVarsFree(&metaByVars);

return 0;
}
