/* mdbPrint - Prints metadata objects and variables from the mdb metadata table. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "mdb.h"

static char const rcsid[] = "$Id: mdbPrint.c,v 1.6 2010/06/11 17:33:57 tdreszer Exp $";

#define OBJTYPE_DEFAULT "table"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mdbPrint - Prints metadata objects and variables from the '" MDB_DEFAULT_NAME "' metadata table.\n"
  "usage:\n"
  "   mdbPrint {db} [-table=] [-byVar] [-ra/-line/-countObjs/-countVars/-countVals]\n"
  "                 [-all]\n"
  "                 [-obj= [-var= [-val=]]]\n"
  "                 [-var= [-val=]]\n"
  "                 [-vars=\"var1=val1 var2=val2...\"]\n"
  "                 [-specialHelp]\n"
  "Options:\n"
  "    {db}     Database to query metadata from.  This argument is required.\n"
  "    -table   Table to query metadata from.  Default is the sandbox version of\n"
  "             '" MDB_DEFAULT_NAME "'.\n"
  "    -byVar   Print each var and val, then all objects that match,\n"
  "             as opposed to printing objects and all the var=val pairs that match.\n"
  "    -ra      Default. Print each obj with a set of indented var val pairs on separate\n"
  "             lines. With -byVar prints pseudo-RA style with multiple objects per stanza.\n"
  "    -line    Print each obj and all var=val pairs on a single line.\n"
  "    -countObjs   Just print count of objects returned in the query.\n"
  "    -countVars   Just print count of variables returned in the query.\n"
  "    -countVals   Just print count of values returned in the query.\n"
  "    -specialHelp Prints help for some special case features.\n"
  "  Four alternate ways to select metadata:\n"
  "    -all       Will print entire table (this could be huge).\n"
  "    -obj={objName}  Request a single object.  Can be narrowed by var and val.\n"
  "    -var={varName}  Request a single variable.  Can be narrowed by val.\n"
  "    -vars={var=val...}  Request a combination of var=val pairs.\n\n"
  "                    Use of 'var!=val', 'var=v%%' and 'var=?' are supported.\n"
  "There are two basic views of the data: by objects and by variables.  The default view "
  "is by object.  Each object will print out in an RA style stanza (by default) or as "
  "a single line of output containing all var=val pairs. In 'byVar' view, each RA style "
  "stanza holds a var val pair and all objects belonging to that pair on separate lines.  "
  "Linear 'byVar' view puts the entire var=val pair on one line.  Alternatively, request "
  "only counting of objects, variables or values.\n"
  "HINT: Use '%%' in any obj, var or val as a wildcard for selection.\n\n"
  "Examples:\n"
  "  mdbPrint hg19 -vars=\"grant=Snyder cell=GM12878 antibody=CTCF\"\n"
  "           Return all objs that satify ALL of the constraints.\n"
  "  mdbPrint mm9 -vars=\"grant=Snyder cell=GM12878 antibody=?\" -byVar\n"
  "           Return each all vars for all objects with the constraint.\n"
  "  mdbPrint hg18 -obj=wgEncodeUncFAIREseqPeaksPanislets -line\n"
  "           Return a single formatted metadata line for one object.\n"
  "  mdbPrint hg18 -countObjs -var=cell -val=GM%%\n"
  "           Return the count of objects which have a declared cell begining with 'GM'.\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"table",    OPTION_STRING}, // default "metaDb"
    {"ra",       OPTION_BOOLEAN},// ra format
    {"line",     OPTION_BOOLEAN},// linear format
    {"countObjs",OPTION_BOOLEAN},// returns only count of objects
    {"countVars",OPTION_BOOLEAN},// returns only count of variables
    {"countVals",OPTION_BOOLEAN},// returns only count of values
    {"all",      OPTION_BOOLEAN},// query entire table
    {"byVar",    OPTION_BOOLEAN},// With -all prints from var perspective
    {"specialHelp",OPTION_BOOLEAN},// Certain very specialized features are described
    {"obj",      OPTION_STRING}, // objName or objId
    {"var",      OPTION_STRING}, // variable
    {"val",      OPTION_STRING}, // value
    {"vars",     OPTION_STRING},// var1=val1 var2=val2...
    {"updDb",    OPTION_STRING},// DB to update
    {"updMdb",   OPTION_STRING},// MDB table to update
    {"updSelect",OPTION_STRING},// Experiment defining vars: "var1,var2"
    {"updVars",  OPTION_STRING},// Vars to update: "var1,var2"
    {NULL,       0}
};

void specialHelp()
/* Explain usage and exit. */
{
errAbort(
  "mdbPrint - Prints specially output from metadata objects and variables.\n"
  "usage:\n"
  "   mdbPrint {db} [-table=] -vars=\"var1=val1 var2=val2...\"\n"
  "            -updDB={db} -updMdb={metaDb} -updSelect=var1,var2,... -updVars=varA,varB,...\n"
  "Options:\n"
  "    {db}     Database to query metadata from.  This argument is required.\n"
  "    -table   Table to query metadata from.  Default is the sandbox version of\n"
  "             '" MDB_DEFAULT_NAME "'.\n"
  "    -vars={var=val...}  Request a combination of var=val pairs.\n"
  "                    Use of 'var!=val', 'var=v%%' and 'var=?' are supported.\n"
  "  Special functions:\n"
  "    Print mdbUpdate lines to assist importing metadata from one db.table to another.\n"
  "    -updDb      Database to aim mdbUpdate lines at.\n"
  "    -updMdb     The metaDb table to aim mdbUpdate lines at.\n"
  "    -updSelect  A comma separated list of variables that will be selected with\n"
  "                the mdbUpdate (via '-vars').\n"
  "    -updVars    A comma separated list of variables that will be set in the\n"
  "                mdbUpdate lines (via '-setVars').\n"
  "The purpose of this special option is to generate mdbUpdate commands from existing metadata.\n"
  "Examples:\n"
  "  mdbPrint hg18 -vars=\"composite=wgEncodeYaleChIPseq\" -upd=hg19 -updMdb=metaDb_braney\n"
  "    (cont.)     -updSelect=grant,cell,antibody,treatment -updVars=dateSubmitted,dateUnrestricted\n"
  "           This command assists importing dateSubmitted from hg18 to hg19 for all\n"
  "           objects in hg19 that match the grant, cell, antibody, and treatment of\n"
  "           objects in hg18.  It would result in output that looks something like:\n"
  "     mdbUpdate hg19 -table=metaDb_cricket -vars=\"grant=Snyder cell=GM12878 antibody=c-Fos treatment=None\"\n"
  "       (cont.) -setVars=\"dateSubmitted=2009-02-13 dateUnrestricted=2009-11-12\" -test\n"
  "     mdbUpdate hg19 -table=metaDb_braney -vars=\"grant=Snyder cell=GM12878 antibody=c-Jun treatment=None\"\n"
  "       (cont.) -setVars=\"dateSubmitted=2009-01-08 dateUnrestricted=2009-08-07\" -test\n"
  "     mdbUpdate hg19 ...\n"
  "           Note the -test flag in the output that will allow confirmation of effects before actual update.\n"
  "  mdbPrint hg18 -vars=\"composite=wgEncodeYaleChIPseq view=RawSignal\" -upd=hg18 -updMdb=metaDb_vsmalladi\n"
  "    (cont.)     -updSelect=obj -updVars=fileName\n"
  "           You can select by object too (but not in combination with other vars).  This example shows\n"
  "           how to assist updating vals to the same mdb, where an editor or awk is also needed.\n"
  );
}

int main(int argc, char *argv[])
// Process command line.
{
struct mdbObj   * mdbObjs   = NULL;
struct mdbByVar * mdbByVars = NULL;
int objsCnt=0, varsCnt=0,valsCnt=0;

if(argc == 1)
    usage();

optionInit(&argc, argv, optionSpecs);

if(optionExists("specialHelp"))
    specialHelp();

if(argc < 2)
    {
    verbose(1, "REQUIRED 'DB' argument not found:\n");
    usage();
    }

char *db        = argv[1];
char *table     = optionVal("table",NULL);
boolean raStyle = TRUE;
if(optionExists("line") && !optionExists("ra"))
    raStyle = FALSE;
boolean cntObjs = optionExists("countObjs");
boolean cntVars = optionExists("countVars");
boolean cntVals = optionExists("countVals");
boolean byVar   = optionExists("byVar");

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
            {
            if(optionExists("updSelect")) // Special print of mdbUpdate lines
                {
                if(!optionExists("updDb") || !optionExists("updMdb") || !optionExists("updVars"))
                    errAbort("To print mdbUpdate lines, all the following values are needed: '-updDb=' '-updMdb=' '-updSelect=' '-updVars='.\n");

                mdbObjPrintUpdateLines(&queryResults,optionVal("updDb",NULL),optionVal("updMdb",NULL),
                                                    optionVal("updSelect",NULL),optionVal("updVars",NULL));
                }
            else
                mdbObjPrint(queryResults,raStyle);
            }
        mdbObjsFree(&queryResults);
        }
    }
sqlDisconnect(&conn);

if(cntObjs || cntVars || cntVals)
    {
    if(cntObjs)
        printf("%d object%s\n",objsCnt,(objsCnt==1?"":"s"));
    if(cntVars)
        printf("%d variable%s\n",varsCnt,(varsCnt==1?"":"s"));
    if(cntVals)
        printf("%d value%s\n",valsCnt,(valsCnt==1?"":"s"));
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
