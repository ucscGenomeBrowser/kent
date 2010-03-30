/* metaTblPrint - Prints metadata objects and variables from the metaTbl. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "metaTbl.h"

static char const rcsid[] = "$Id: metaTblPrint.c,v 1.5 2010/03/30 23:45:15 tdreszer Exp $";

#define OBJTYPE_DEFAULT "table"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaTblPrint - Prints metadata objects and variables from the metaTbl.\n\n"
  "There are two basic views of the data: by objects and by variables.  Unless a single variable "
  "and no object is requested the default view is by object.  Each line of output will contain "
  "object and all it's var=val pairs as 'formatted metadata' line.  In 'byVar' view, a single line "
  "per var=val and then a space seperated list of each obj(type) follows.  RA style will place "
  "each object and var val on a separate line with indentation.  Alternatively, count will "
  "return a count of unique obj var=val combinations. It is also possible to select a "
  "combination of vars by entering a string of var=val pairs.\n\n"
  "usage:\n"
  "   metaTblPrint -db= [-table=]  [-byVar] [-ra/-countObjs/-countVarss/-countVals]\n"
  "                [-all]\n"
  "                [-obj= [-var= [-val=]]]\n"
  "                [-var= [-val=]]\n"
  "                [-vars=\"var1=val1 var2=val2...]\n\n"
  "Options:\n"
  "    -db      Database to query.  This argument is required.\n"
  "    -table   Table to query.  Default is '" METATBL_DEFAULT_NAME "'.\n"
  "    -byVar   Print each var and val, then all objects that match,\n"
  "             as opposed to printing objects and all the var=val pairs that match.\n"
  "    -ra      Print each obj with a set of indented var val pairs on separate lines.\n"
  "             With -byVar prints pseudo-RA style with 2 levels of indentation (not val obj pairs).\n"
  "    -countObjs   Just print count of objects returned in the query.\n"
  "    -countVars   Just print count of variables returned in the query.\n"
  "    -countVals   Just print count of values returned in the query.\n"
  "  Four alternate ways to select metadata:\n"
  "    -all       Will print entire table (this could be huge).\n"
  "    -obj={objName}  Request a single object.  The request can be further narrowed by var and val.\n"
  "    -var={varName}  Request a single variable.  The request can be further narrowed by val.\n"
  "    -vars={var=val...}  Request a combination of var=val pairs.\n"
  "Examples:\n"
  "  metaTblPrint -vars=\"grant=Snyder cell=GM12878 antibody=CTCF\"\n"
  "               Return all objs that satify ALL of the constraints.\n"
  "  metaTblPrint -byVar -vars=\"grant=Snyder cell=GM12878 antibody=CTCF\"\n"
  "               Return each var=val pair and ANY objects that have any constraint.\n"
  "  metaTblPrint -db=hg18 -obj=wgEncodeUncFAIREseqPeaksPanislets\n"
  "               Return the formatted metadata line for the one object.\n"
  "  metaTblPrint -countObjs -var=treatment\n"
  "               Return the count of objects which have a declared treatment.\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"db",       OPTION_STRING}, // default "hg19"
    {"table",    OPTION_STRING}, // default "metaTbl"
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
struct metaObj   * metaObjs   = NULL;
struct metaByVar * metaByVars = NULL;
int objsCnt=0, varsCnt=0,valsCnt=0;

optionInit(&argc, argv, optionSpecs);
if(!optionExists("db"))
    {
    verbose(1, "REQUIRED -db argument is missing.\n");
    usage();
    }

char *db    = optionVal("db",NULL);
char *table = optionVal("table",METATBL_DEFAULT_NAME);
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
    metaObjs = metaObjCreate(optionVal("obj",  NULL),NULL,optionVal("var", NULL), NULL,optionVal("val", NULL));
    }
else if(optionExists("var"))
    {
    metaByVars =  metaByVarCreate(optionVal("var", NULL),NULL,optionVal("val", NULL));
    }
else if(optionExists("vars"))
    {
    metaByVars = metaByVarsLineParse(optionVal("vars", NULL));
    }
else
    usage();

struct sqlConnection *conn = sqlConnect(db);
if(byVar)
    {
    if(!all && metaByVars == NULL) // assertable
        usage();

    // Requested a single var
    struct metaByVar * queryResults = metaByVarsQuery(conn,table,metaByVars);
    if(queryResults == NULL)
        verbose(1, "No metadata met your selection criteria\n");
    else
        {
        objsCnt=metaByVarCount(queryResults,FALSE,FALSE);
        varsCnt=metaByVarCount(queryResults,TRUE ,FALSE);
        valsCnt=metaByVarCount(queryResults,FALSE,TRUE );
        if(!cntObjs && !cntVars && !cntVals)
            metaByVarPrint(queryResults,raStyle);
        metaByVarsFree(&queryResults);
        }
    }
else
    {
    struct metaObj * queryResults = NULL;
    if(metaByVars != NULL)
        {
        // Requested a set of var=val pairs and looking for the unique list of objects that have all of them!
        queryResults = metaObjsQueryByVars(conn,table,metaByVars);
        }
    else
        {
        // Requested a single obj
        queryResults = metaObjQuery(conn,table,metaObjs);
        }

    if(queryResults == NULL)
        verbose(1, "No metadata met your selection criteria\n");
    else
        {
        objsCnt=metaObjCount(queryResults,TRUE);
        varsCnt=metaObjCount(queryResults,FALSE);
        valsCnt=varsCnt;
        if(!cntObjs && !cntVars && !cntVals)
            metaObjPrint(queryResults,raStyle);
        metaObjsFree(&queryResults);
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

if(metaObjs)
    metaObjsFree(&metaObjs);
if(metaByVars)
    metaByVarsFree(&metaByVars);

return 0;
}
