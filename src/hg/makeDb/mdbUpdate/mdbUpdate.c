/* mdbUpdate - Adds, updates or removes metadata objects and variables from the 'mdb' metadata table. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "mdb.h"
#include "hash.h"

static char const rcsid[] = "$Id: mdbUpdate.c,v 1.3 2010/04/27 22:40:12 tdreszer Exp $";

#define OBJTYPE_DEFAULT "table"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mdbUpdate - Adds, updates or removes metadata objects and variables from the\n"
  "            '" MDB_DEFAULT_NAME "' metadata table.\n"
  "usage:\n"
  "   mdbUpdate {db} [-table= [-force]] [-recreate] [-test [-verbose=2]]\n"
  "                  [{fileName}] [-replace] or\n"
  "                  [[-vars=\"var1=val1 var2=val2...\"] or [-obj=]]\n"
  "                  [[-setVars=\"var1=val1 ...\"]] [-delete]\n"
  "                  [-encodeExp [-accession]\n"
  "Options:\n"
  "    {db}     Database to update metadata in.  This argument is required.\n"
  "    -table   Table to update metadata to.  Default is the sandbox version of\n"
  "             '" MDB_DEFAULT_NAME "'.\n"
  "       -recreate   Creates or empties the table.  No further arguements needed.\n"
  "       -force      Overrides restrictions placed on shared  '" MDB_DEFAULT_NAME "'.\n"
  "    -test    Does not actually update and does full cv validation.\n"
  "             Use with -verbose=2 to see the SQL.\n"
  "             RECOMMENDED: Test complex selection criteria with mdbPrint first.\n"
  "    [{fileName}] File of formatted metadata from mdbPrint (RA or lines).\n"
  "      -replace   Remove all old variables for each object before adding new.\n"
  "    if {fileName}, then -obj, -composite or -vars must be provided:\n"
  "  SELECT Objects to work on:\n"
  "     -obj={objName}  Select this single object to update\n"
  "     -composite={}   Select all objects belonging to this composite.\n"
  "     -vars=\"{var=val ...}\" Apply update to a group of objects matching vars.\n"
  "        Use: 'var=val'  'var=v%%'  'var='  'var=val1,val2' (val1 or val2).\n"
  "             'var!=val' 'var!=v%%' 'var!=' 'var!=val1,val2' are all supported.\n"
  "  TARGET vars to set or delete:\").\n"
  "     -setVars={var=val...}  Allows setting multiple var=val pairs.\n"
  "        NOTE: val cannot have blank spaces (yet).  TO BE DONE.\n"
  "  ACTION (update to values defined in -setVars is the default):\n"
  "     -delete         Remove specific var (or entire object if -setVar is not used).\n"
  "     -encodeExp={table}  Update groups of objs as experiments defined in hgFixed.{table}.\n"
  "       -accession        If encodeExp, then make/update accession too.\n"
  "There are two ways to call mdbUpdate: with a file or by command line selection of objects. "
  "The file can be the formatted output from mdbPrint or the following special formats:\n"
  "  metadata objName var1=val1 var2=\"val2 with spaces\" var3=...\n"
  "    Adds or updates the specific object and variables\n"
  "  metadata objName delete\n"
  "    delete all metadata for objName\n"
  "  metadata objName delete var=val var2=val2\n"
  "     deletes specifically named variables for objName (vals are ignored).\n"
  "  Special ENCODE format as produced by the doEncodeValidate.pl\n"
  "NOTE: Updates to the shared '" MDB_DEFAULT_NAME "' can only be done by a file written\n"
  "      directly from mdbPrint.  Update sandbox first, then move to shared table.\n"
  "HINT: Use '%%' in any command line obj, var or val as a wildcard for selection.\n\n"
  "Examples:\n"
  "  mdbUpdate hg19 -vars=\"grant=Snyder cell=K562 antibody=CTCF\" -setVars=\"expId=1427\"\n"
  "            Update all objects matcing Snyder/K562/CTCF and set the expId=1472.\n"
  "  mdbUpdate hg19 -composite=%%BroadHist%% -vars=\"dccAccession=\" -setVars=\"dccAccession=\" -delete\n"
  "            Deletes all accessions from Broad Histone objects that have them.\n"
  "  mdbUpdate mm9 -table=mdb_braney -recreate\n"
  "            Creates or empties the named metadata table.\n"
  "  mdbUpdate hg18 -vars=\"composite=wgEncodeDukeDNase\" -delete -test\n"
  "            Tests the delete of all objects in the wgEncodeDukeDNase composite.\n"
  "  mdbUpdate hg18 -table=" MDB_DEFAULT_NAME " mdbBroadHistone.ra -replace\n"
  "            Replaces all metadata in the shared table for objects found in the\n"
  "            provided file.  File must have been printed with mdbPrint.\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"table",   OPTION_STRING}, // default "metaDb"
    {"obj",     OPTION_STRING}, // objName or objId
    {"vars",    OPTION_STRING}, // Select set of object by vars
    {"composite",OPTION_STRING}, // Special case of a commn var (replaces vars="composite=wgEncodeBroadHistone")
    {"var",     OPTION_STRING}, // variable
    {"val",     OPTION_STRING}, // value
    {"encodeExp",OPTION_STRING},// Update Experiments as defined in the hgFixed.encodeExp table
    {"accession",OPTION_BOOLEAN},// Adds/updates accession when experimentifying
    {"setVars", OPTION_STRING}, // Allows setting multiple var=val pairs
    {"delete",  OPTION_BOOLEAN},// delete one obj or obj/var
    {"replace", OPTION_BOOLEAN},// replace entire obj when loading from file
    {"recreate",OPTION_BOOLEAN},// creates or recreates the table
    {"force",   OPTION_BOOLEAN},// override restrictions on shared table
    {"test",    OPTION_BOOLEAN},// give it a test, will ya?
    {NULL,      0}
};

int main(int argc, char *argv[])
// Process command line.
{
if(argc == 1)
    usage();

struct mdbObj * mdbObjs = NULL;

optionInit(&argc, argv, optionSpecs);

if(argc < 2)
    {
    errAbort("REQUIRED 'DB' argument not found.\n");
    }

char *db         = argv[1];
char *table      = optionVal("table",NULL);
boolean deleteIt = optionExists("delete");
boolean testIt   = optionExists("test");
boolean recreate = optionExists("recreate");
boolean force    = optionExists("force");
boolean replace  = FALSE;
char *var        = optionVal("var",NULL);
char *val        = optionVal("val",NULL);
char *setVars    = optionVal("setVars",NULL);
char *encodeExp  = optionVal("encodeExp",NULL);
if (encodeExp != NULL)
    {
    if (strlen(encodeExp) == 0 || sameWord("std",encodeExp))
        encodeExp = "encodeExp";
    }

if (recreate && encodeExp != NULL)
    errAbort("Incompatible options 'recreate' and 'encodeExp':\n");
if (recreate && deleteIt)
    errAbort("Incompatible options 'recreate' and 'delete':\n");
if (encodeExp != NULL && deleteIt)
    errAbort("Incompatible options 'encodeExp' and 'delete':\n");
if (testIt && force)
    errAbort("Incompatible options 'test' and 'force':\n");

struct sqlConnection *conn = sqlConnect(db);

// Find the table if necessary
if(table == NULL)
    {
    table = mdbTableName((recreate?NULL:conn),TRUE); // Look for sandBox name first
    if(table == NULL)
        {
        table = mdbTableName((recreate?NULL:conn),FALSE); // Okay, default then
        if(table == NULL)  // Now you are just getting me angry!
            {
            sqlDisconnect(&conn);
            if(!recreate) // assertable
                errAbort("No '%s.%s' found.  Consider using -recreate flag.\n",db,MDB_DEFAULT_NAME);
            else
                errAbort("No '%s.%s' found.\n",db,MDB_DEFAULT_NAME);
            }
        }
    }
if (encodeExp != NULL)
    verbose(1, "Using tables named '%s.%s' and 'hgFixed.%s'.\n",db,table,encodeExp);
else
    verbose(1, "Using table named '%s.%s'.\n",db,table);

boolean sharedTbl = sameWord(table,MDB_DEFAULT_NAME);  // Special restrictions apply

// Recreate the table
if(recreate)
    {
    if(sharedTbl && ! force)
        {
        sqlDisconnect(&conn);
        errAbort("NOT SUPPORTED for shared table '%s'.\n",MDB_DEFAULT_NAME);
        }
    boolean recreated = sqlTableExists(conn,table);
    mdbReCreate(conn,table,testIt);
    if(testIt)
        {
        verbose(1, "Would %screate table named '%s'.\n",
                (recreated?"re":""),table);
        if(!recreated)
            {
        sqlDisconnect(&conn);
        if(optionExists("obj") || optionExists("vars") || argc > 2)
                verbose(1, "Can't test further commands.  Consider '-db= [-table=] -recreate' as the only arguments.\n");
            return 0;  // Don't test any update if we haven't actually created the table!
            }
        }
    else
        verbose(1, "%s table named '%s'.\n",(recreated?"Recreated":"Created"),table);
    }

if(argc > 2 && (deleteIt || encodeExp != NULL || var != NULL || val != NULL || setVars != NULL))
    errAbort("INCONSISTENT REQUEST: can't combine supplied file with -delete, -encodeExp, -var, -val or -setVars.\n");
if((deleteIt || encodeExp != NULL) && (var != NULL || val != NULL))
    errAbort("INCONSISTENT REQUEST: can't combine -%s with -var and -val.\n",deleteIt?"delete":"encodeExp");
if (argc != 3 && !deleteIt && encodeExp == NULL)
    {
    if(setVars == NULL && (var == NULL || val == NULL))
        {
        if(recreate) // no problem
            return 0;
        errAbort("INCONSISTENT REQUEST: need both -var and -val.\n");
        }
    else if (setVars != NULL && (var != NULL || val != NULL))
        {
        if(recreate) // no problem
            return 0;
        errAbort("INCONSISTENT REQUEST: can't combine -var or -val with -setVars.\n");
        }
    }

// Now get the object list
if(optionExists("obj"))
    {
    if(sharedTbl && !force)
        {
        sqlDisconnect(&conn);
        errAbort("NOT SUPPORTED for shared table '%s'.\n",MDB_DEFAULT_NAME);
        }
    if(argc > 2 || optionExists("vars"))
        {
        sqlDisconnect(&conn);
        errAbort("INCONSISTENT REQUEST: can't combine -obj with -vars or a supplied file.\n");
        }

    mdbObjs = mdbObjCreate(optionVal("obj",  NULL),var,val);
    mdbObjs->deleteThis = deleteIt;

    if(setVars != NULL)
        mdbObjSwapVars(mdbObjs,setVars,deleteIt);

    verbose(2, "metadata %s %s%s%s%s\n",
         mdbObjs->obj,(mdbObjs->deleteThis ? "delete ":""),
        (mdbObjs->vars && mdbObjs->vars->var!=NULL?mdbObjs->vars->var:""),
        (mdbObjs->vars && mdbObjs->vars->val!=NULL?"=":""),
        (mdbObjs->vars && mdbObjs->vars->val!=NULL?mdbObjs->vars->val:""));
    }
else if(optionExists("vars") || optionExists("composite"))
    {
    if(sharedTbl && !force)
        {
        sqlDisconnect(&conn);
        errAbort("NOT SUPPORTED for shared table '%s'.\n",MDB_DEFAULT_NAME);
        }
    if((argc > 2 && (!optionExists("vars") || !optionExists("composite")))
    || (argc > 3 &&   optionExists("vars") &&  optionExists("composite")) )
        {
        sqlDisconnect(&conn);
        errAbort("INCONSISTENT REQUEST: can't combine -vars or -composite with a supplied file.\n");
        }
    struct mdbByVar * mdbByVars = NULL;
    if (optionExists("vars"))
        {
        mdbByVars = mdbByVarsLineParse(optionVal("vars", NULL));
        if (optionExists("composite"))
            mdbByVarAppend(mdbByVars,"composite", optionVal("composite", NULL),FALSE);
        // Would be nice to do this as mdbPrint.  However -var and -val are values to be set
        //if (optionExists("var"))
        //    mdbByVarAppend(mdbByVars,optionVal("var", NULL), optionVal("val", NULL),FALSE);
        }
    else //if (optionExists("composite"))
        {
        mdbByVars = mdbByVarCreate("composite", optionVal("composite", NULL));
        // Would be nice to do this as mdbPrint.  However -var and -val are values to be set
        //if (optionExists("var"))
        //    mdbByVarAppend(mdbByVars,optionVal("var", NULL), optionVal("val", NULL),FALSE);
        }
    mdbObjs = mdbObjsQueryByVars(conn,table,mdbByVars);

    // replace all found vars but update request
    if(setVars != NULL)
        mdbObjSwapVars(mdbObjs,setVars,deleteIt);
    else if (encodeExp == NULL)
        mdbObjTransformToUpdate(mdbObjs,var,val,deleteIt);
    }
else // Must be submitting formatted file
    {
    if(argc != 3)
        {
        sqlDisconnect(&conn);
        if(recreate) // no problem
            return 0;
        errAbort("REQUIRED: must declare -obj, -vars or supply a file.\n");
        }

    replace = optionExists("replace");
    boolean validated = FALSE;
    mdbObjs = mdbObjsLoadFromFormattedFile(argv[2],(sharedTbl ? &validated : NULL)); // Only validate magic for shared table.
    if(sharedTbl && !force && !validated)
        { // Can't test magic if RAs are catted together!  Requires reading in 1 stanza at a time, slAddTail, and testing magic when intercepted.
        sqlDisconnect(&conn);
        errAbort("Update to shared table '%s' requires file directly written by mdbPrint from sandbox file.\n", table);
        }
    if(mdbObjs != NULL)
        verbose(1, "Read %d metadata objects from %s\n", slCount(mdbObjs),argv[1]);
    }

int count = 0;

if(mdbObjs != NULL)
    {
    if(testIt && verboseLevel() > 2)
        mdbObjPrint(mdbObjs,FALSE);

    if(recreate) // recreate then do the fast load
        count = mdbObjsLoadToDb(conn,table,mdbObjs,testIt);
    else
        {
        if (encodeExp != NULL)
            {
            boolean createExpIfNecessary = testIt ? FALSE : TRUE; // FIXME: When we are ready, this should allow creating an experiment in the hgFixed.encodeExp table

            boolean updateAccession = (optionExists("accession"));
            struct mdbObj *updatable = mdbObjsEncodeExperimentify(conn,db,table,encodeExp,&mdbObjs,(verboseLevel() > 1? 1:0),createExpIfNecessary,updateAccession); // 1=warnings
            if (updatable == NULL)
                verbose(1, "No Experiment ID updates were discovered in %d object(s).\n", slCount(mdbObjs));
            else
                {
                if (testIt)
                    verbose(1, "Found %d of %d object(s) would have their experiment ID updated.\n", slCount(updatable), slCount(mdbObjs));
                mdbObjsFree(&mdbObjs);
                mdbObjs = updatable;
                }
            }
        count = mdbObjsSetToDb(conn,table,mdbObjs,replace,testIt);
        }

    if (testIt && encodeExp == NULL)
        {
        int invalids = mdbObjsValidate(mdbObjs,TRUE);
        int varsCnt=mdbObjCount(mdbObjs,FALSE);
        verbose(1, "%d invalid%s of %d variable%s according to the cv.ra.\n",invalids,(invalids==1?"":"s"),varsCnt,(varsCnt==1?"":"s"));
        if (invalids > 0)
            verbose(1, "Variables invalid to cv.ra ARE PERMITTED in the mdb.\n");
        }
    }
if(testIt)
    verbose(1, "Command would affected %d row(s) in %s.%s\n", count,db,table);
else
    verbose(1, "Affected %d row(s) in %s.%s\n", count,db,table);

sqlDisconnect(&conn);
mdbObjsFree(&mdbObjs);
return 0;

// TODO:
// 1) Case insensitive hashs?
// 2) -ra by default (-1 line)?
// 3) expId table?  -exp=start requests generating unique ids for selected vars, then updating them. -expTbl generates expTable as id,"var=val var=val var=val"
}
