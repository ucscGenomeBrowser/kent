/* mdbPrint - Prints metadata objects and variables from the mdb metadata table. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "mdb.h"

static char const rcsid[] = "$Id: mdbPrint.c,v 1.1 2010/04/13 19:59:16 tdreszer Exp $";

#define OBJTYPE_DEFAULT "table"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mdbPrint - Prints metadata objects and variables from the mdb metadata table.\n"
  "usage:\n"
  "   mdbPrint {db} [-table=] [-byVar] [-ra/-countObjs/-countVars/-countVals]\n"
  "                 [-all]\n"
  "                 [-obj= [-var= [-val=]]]\n"
  "                 [-var= [-val=]]\n"
  "                 [-vars=\"var1=val1 var2=val2...]\n"
  "Options:\n"
  "    {db}     Database to query metadata from.  This argument is required.\n"
  "    -table   Table to query metadata from.  Default is the sandbox version of\n"
  "             '" MDB_DEFAULT_NAME "'.\n"
  "    -byVar   Print each var and val, then all objects that match,\n"
  "             as opposed to printing objects and all the var=val pairs that match.\n"
  "    -ra      Print each obj with a set of indented var val pairs on separate lines.\n"
  "             With -byVar prints pseudo-RA style with 2 levels of indentation.\n"
  "    -countObjs   Just print count of objects returned in the query.\n"
  "    -countVars   Just print count of variables returned in the query.\n"
  "    -countVals   Just print count of values returned in the query.\n"
  "  Four alternate ways to select metadata:\n"
  "    -all       Will print entire table (this could be huge).\n"
  "    -obj={objName}  Request a single object.  Can be narrowed by var and val.\n"
  "    -var={varName}  Request a single variable.  Can be narrowed by val.\n"
  "    -vars={var=val...}  Request a combination of var=val pairs.\n\n"
  "                    Use of 'var!=val', 'var=v%%' and 'var=?' are supported.\n"
  "There are two basic views of the data: by objects and by variables.  The default view "
  "is by object.  Each line of output will contain an object and all it's var=val pairs "
  "as a 'formatted metadata' line.  In 'byVar' view, a single line per var=val and then "
  "a space seperated list of each objects is printed.  RA style will place each object "
  "and var val on a separate line with indentation.  Alternatively, request only counting "
  "of objects, variables or values.\n"
  "HINT: Use '%%' in any obj, var or val as a wildcard for selection.\n\n"
  "Examples:\n"
  "  mdbPrint hg19 -vars=\"grant=Snyder cell=GM12878 antibody=CTCF\"\n"
  "           Return all objs that satify ALL of the constraints.\n"
  "  mdbPrint mm9 -byVar -vars=\"grant=Snyder cell=GM12878 antibody=?\"\n"
  "           Return each var=val pair and ANY objects that have the constraint.\n"
  "  mdbPrint hg18 -obj=wgEncodeUncFAIREseqPeaksPanislets\n"
  "           Return the formatted metadata line for the one object.\n"
  "  mdbPrint hg18 -countObjs -var=cell -val=GM%%\n"
  "           Return the count of objects which have a declared cell begining with 'GM'.\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"table",    OPTION_STRING}, // default "mdb"
    {"ra",       OPTION_BOOLEAN},// ra format
    {"countObjs",OPTION_BOOLEAN},// returns only count of objects
    {"countVars",OPTION_BOOLEAN},// returns only count of variables
    {"countVals",OPTION_BOOLEAN},// returns only count of values
    {"all",      OPTION_BOOLEAN},// query entire table
    {"byVar",    OPTION_BOOLEAN},// With -all prints from var perspective
    {"obj",      OPTION_STRING}, // objName or objId
    {"var",      OPTION_STRING}, // variable
    {"val",      OPTION_STRING}, // value
    {"vars",     OPTION_STRING},// var1=val1 var2=val2...
    {NULL,       0}
};

int main(int argc, char *argv[])
// Process command line.
{
struct mdbObj   * mdbObjs   = NULL;
struct mdbByVar * mdbByVars = NULL;
int objsCnt=0, varsCnt=0,valsCnt=0;

if(argc == 1)
    usage();

optionInit(&argc, argv, optionSpecs);
if(argc < 2)
    {
    verbose(1, "REQUIRED 'DB' argument not found:\n");
    usage();
    }

char *db        = argv[1];
char *table     = optionVal("table",NULL);
boolean raStyle = optionExists("ra");
boolean cntObjs = optionExists("countObjs");
boolean cntVars = optionExists("countVars");
boolean cntVals = optionExists("countVals");
boolean byVar   = optionExists("byVar");

if(raStyle && (cntObjs || cntVars || cntVals))
    usage();

boolean all = optionExists("all");
if(all)
    {
    if(optionExists("obj")
    || optionExists("var") || optionExists("val") || optionExists("vars"))
        usage();
    }
else if(optionExists("obj"))
    {
    mdbObjs = mdbObjCreate(optionVal("obj",  NULL),optionVal("var", NULL), NULL,optionVal("val", NULL));
    }
else if(optionExists("var"))
    {
    mdbByVars =  mdbByVarCreate(optionVal("var", NULL),NULL,optionVal("val", NULL));
    }
else if(optionExists("vars"))
    {
    mdbByVars = mdbByVarsLineParse(optionVal("vars", NULL));
    }
else
    usage();

struct sqlConnection *conn = sqlConnect(db);

// Find the table if necessary
if(table == NULL)
    {
    table = mdbTableName(conn,TRUE); // Look for sandBox name first
    if(table == NULL)
        errAbort("TABLE NOT FOUND: '%s.%s'.\n",db,MDB_DEFAULT_NAME);
    verbose(1, "Using table named '%s.%s'.\n",db,table);
    }

if(byVar)
    {
    if(!all && mdbByVars == NULL) // assertable
        usage();

    // Requested a single var
    struct mdbByVar * queryResults = mdbByVarsQuery(conn,table,mdbByVars);
    if(queryResults == NULL)
        verbose(1, "No metadata met your selection criteria\n");
    else
        {
        objsCnt=mdbByVarCount(queryResults,FALSE,FALSE);
        varsCnt=mdbByVarCount(queryResults,TRUE ,FALSE);
        valsCnt=mdbByVarCount(queryResults,FALSE,TRUE );
        if(!cntObjs && !cntVars && !cntVals)
            mdbByVarPrint(queryResults,raStyle);
        mdbByVarsFree(&queryResults);
        }
    }
else
    {
    struct mdbObj * queryResults = NULL;
    if(mdbByVars != NULL)
        {
        // Requested a set of var=val pairs and looking for the unique list of objects that have all of them!
        queryResults = mdbObjsQueryByVars(conn,table,mdbByVars);
        }
    else
        {
        // Requested a single obj
        queryResults = mdbObjQuery(conn,table,mdbObjs);
        }

    if(queryResults == NULL)
        verbose(1, "No metadata met your selection criteria\n");
    else
        {
        objsCnt=mdbObjCount(queryResults,TRUE);
        varsCnt=mdbObjCount(queryResults,FALSE);
        valsCnt=varsCnt;
        if(!cntObjs && !cntVars && !cntVals)
            mdbObjPrint(queryResults,raStyle);
        mdbObjsFree(&queryResults);
        }
    }
sqlDisconnect(&conn);

if(cntObjs || cntVars || cntVals)
    {
    if(cntObjs)
        printf("%d objects\n",objsCnt);
    if(cntVars)
        printf("%d variable\n",varsCnt);
    if(cntVals)
        printf("%d values\n",valsCnt);
    }
else if( varsCnt>0 || valsCnt>0 || objsCnt>0 )
    {
    if(byVar)
        verbose(1,"vars:%d  vals:%d  objects:%d\n",varsCnt,valsCnt,objsCnt);
    else
        verbose(1,"objects:%d  vars:%d\n",objsCnt,varsCnt);
    }

if(mdbObjs)
    mdbObjsFree(&mdbObjs);
if(mdbByVars)
    mdbByVarsFree(&mdbByVars);

return 0;
}
