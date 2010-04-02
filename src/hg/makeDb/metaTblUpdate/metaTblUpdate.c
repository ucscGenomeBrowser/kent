/* metaTblUpdate - Adds, updates or removes metadata objects and variables from the metaTbl. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "metaTbl.h"
#include "hash.h"

static char const rcsid[] = "$Id: metaTblUpdate.c,v 1.9 2010/04/02 21:20:36 tdreszer Exp $";

#define OBJTYPE_DEFAULT "table"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaTblUpdate - Adds, updates or removes metadata objects and variables from the metaTbl.\n"
  "usage:\n"
  "   metaTblUpdate -db= [-table= [-force]] [-recreate] [-test [-verbose=2]]\n"
  "                      [-obj= [-type=] [-delete] [-var=] [-binary] [-val=]]\n"
  "                      [-vars=\"var1=val1 var2=val2... [-delete] [-var=] [-binary] [-val=]]\n"
  "                      [fileName] [-replace]\n"
  "Options:\n"
  "    -db      Database to load metadata to.  This argument is required.\n"
  "    -table   Table to load metadata to.  Default is the sandbox version of\n"
  "             '" METATBL_DEFAULT_NAME "'.\n"
  "       -recreate   Creates or empties the table.  No further arguemts are needed.\n"
  "       -force      Overrides restrictions placed on shared  '" METATBL_DEFAULT_NAME "'.\n"
  "    -test    Does not update but only reports results.  Use with -verbose=2 to see SQL.\n"
  "  if file not provided, then -obj or -vars must be provided\n"
  "    -obj={objName}     Means Load from command line:\n"
  "       -type={objType} Used if adding new obj, otherwise ignored.  Default is '" OBJTYPE_DEFAULT "'.\n"
  "    -vars={var=val...} Apply update to group of objects matching these restrictions.\n"
  "                       It is recommended you use the selection criteria in metaTblPrint\n"
  "                       to verify which objects meet your selection criteria first.\n"
  "                       Use of 'var!=val', 'var=v%%' and 'var=?' are supported.\n"
  "    These options work with -obj or -vars:\n"
  "       -delete         Remove a specific var or entire obj (if -var not provided).\n"
  "       -var={varName}  Provide variable name (if no -var then must be -delete)\n"
  "       -binary         NOT YET IMPLEMENTED.  This var has a binary val and -val={file}\n"
  "       -val={value}    (Enclosed in \"quotes if necessary\".) Not required to delete var\n"
  "       -setVas={var=val...}  Allows setting multiple var=val pairs.\n"
  "    [file]       File containing formatted metadata lines.  Ignored if -obj param provided:\n"
  "      -replace   Means remove all old variables for each object before adding new variables\n\n"
  "There are two ways to call this.  The object (or objects matching vars) and var to update "
  "can be declared on the command line, or a file of formatted metadata lines can be provided. "
  "The file can be the formatted output from metaTblPrint or the following special formats:\n"
  "  metadata objName objType var1=val1 var2=\"val2 with spaces\" var3=...\n"
  "    Adds or updates the specific object and variables\n"
  "  metadata objName delete\n"
  "    delete all metadata for objName\n"
  "  metadata objName delete var=val var2=val2\n"
  "     deletes specifically named variables for objName. Must provide val but value is ignored.\n"
  "  Special ENCODE format as produced by the doEncodeValidate/pl\n"
  "NOTE: Updates to the shared '" METATBL_DEFAULT_NAME "' can only be done by a file written\n"
  "      directly from metaTblPrint.  Update sandbox first, then move updates to shared table.\n"
  "HINT: Use '%%' in any command line obj, var or val as a wildcard for selection.\n\n"
  "Examples:\n"
  "  metaTblUpdate -db=hg19 -vars=\"grant=Snyder cell=GM12878 antibody=CTCF\" -var=expId -val=1427\n"
  "                Update all objects matcing Snyder/GM12878/CTCF and set the expId=1472.\n"
  "  metaTblUpdate -db=hg19 -obj=fredsTable -var=description val=\"Ethyl's husband's clutter\"\n"
  "                Updates fredsTable with a description.\n"
  "  metaTblUpdate -db=mm9 -table=metaTbl_braney -recreate\n"
  "                Creates or empties the name metaTbl.\n"
  "  metaTblUpdate -db=hg18 -test vars=\"composite=wgEncodeDukeDNase\" -delete\n"
  "                Tests the delete of all objects that have the named composite defined.\n"
  "  metaTblUpdate -db=hg18 -table=metaTbl encBroadHistone.metaTbl.ra -replace\n"
  "                Replaces all metadata for objects found in the provided file.\n"
  "                File must have been printed with metaTblPrint.\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"db",      OPTION_STRING}, // default "hg19"
    {"table",   OPTION_STRING}, // default "metaTbl"
    {"obj",     OPTION_STRING}, // objName or objId
    {"type",    OPTION_STRING}, // object type: table or file
    {"vars",    OPTION_STRING}, // Select set of object by vars
    {"var",     OPTION_STRING}, // variable
    {"val",     OPTION_STRING}, // value
    {"setVars", OPTION_STRING}, // Allows setting multiple var=val pairs
    {"delete",  OPTION_BOOLEAN},// delete one obj or obj/var
    {"binary",  OPTION_BOOLEAN},// val is binary (NOT YET IMPLEMENTED) implies -val={file}
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

struct metaObj * metaObjs = NULL;

optionInit(&argc, argv, optionSpecs);
if(!optionExists("db"))
    {
    verbose(1, "REQUIRED: -db argument is missing.\n");
    usage();
    }

char *db         = optionVal("db",NULL);
char *table      = optionVal("table",NULL);
boolean deleteIt = optionExists("delete");
boolean testIt   = optionExists("test");
boolean recreate = optionExists("recreate");
boolean force    = optionExists("force");
boolean replace  = FALSE;
char *var        = optionVal("var",NULL);
char *val        = optionVal("val",NULL);
char *setVars    = optionVal("setVars",NULL);

struct sqlConnection *conn = sqlConnect(db);

// Find the table if necessary
if(table == NULL)
    {
    table = metaTblName((recreate?NULL:conn),TRUE); // Look for sandBox name first
    if(table == NULL)
        {
        table = metaTblName((recreate?NULL:conn),FALSE); // Okay, default then
        if(table == NULL)  // Now you are just getting me angry!
            {
            sqlDisconnect(&conn);
            if(!recreate) // assertable
                errAbort("No '%s.%s' found.  Consider using -recreate flag.\n",db,METATBL_DEFAULT_NAME);
            else
                errAbort("No '%s.%s' found.\n",db,METATBL_DEFAULT_NAME);
            }
        }
    verbose(1, "Using table named '%s.%s'.\n",db,table);
    }

boolean sharedTbl = sameWord(table,METATBL_DEFAULT_NAME);  // Special restrictions apply

// Recreate the table
if(recreate)
    {
    if(sharedTbl && ! force)
        {
        sqlDisconnect(&conn);
        verbose(1, "NOT SUPPORTED for shared table '%s'.\n",METATBL_DEFAULT_NAME);
        }
    boolean recreated = sqlTableExists(conn,table);
    metaTblReCreate(conn,table,testIt);
    if(testIt)
        {
        verbose(1, "Would %screate table named '%s'.\n",
                (recreated?"re":""),table);
        if(!recreated)
            {
        sqlDisconnect(&conn);
        if(optionExists("obj") || optionExists("vars") || argc > 1)
                verbose(1, "Can't test further commands.  Consider '-db= [-table=] -recreate' as the only arguments.\n");
            return 0;  // Don't test any update if we haven't actually created the table!
            }
        }
    else
        verbose(1, "%s table named '%s'.\n",(recreated?"Recreated":"Created"),table);
    }

if(argc > 1 && (deleteIt || var != NULL || val != NULL || setVars != NULL))
    {
    verbose(1, "INCONSISTENT REQUEST: can't combine supplied file with -delete, -var, -val or -setVars.\n");
    usage();
    }
if(deleteIt && var != NULL && val != NULL)
    {
    verbose(1, "INCONSISTENT REQUEST: can't combine -delete with -var and -val.\n");
    usage();
    }
if (argc != 2 && !deleteIt)
    {
    if(setVars == NULL && (var == NULL || val == NULL))
        {
        if(recreate) // no problem
            return 0;
        verbose(1, "INCONSISTENT REQUEST: need both -var and -val.\n");
        usage();
        }
    else if (setVars != NULL && (var != NULL || val != NULL))
        {
        if(recreate) // no problem
            return 0;
        verbose(1, "INCONSISTENT REQUEST: can't combin -var or -val with -setVars.\n");
        usage();
        }
    }

// Now get the object list
if(optionExists("obj"))
    {
    if(sharedTbl && !force)
        {
        sqlDisconnect(&conn);
        verbose(1, "NOT SUPPORTED for shared table '%s'.\n",METATBL_DEFAULT_NAME);
        usage(); // Must not have submitted formatted file also
        }
    if(argc > 1 || optionExists("vars"))
        {
        sqlDisconnect(&conn);
        verbose(1, "INCONSISTENT REQUEST: can't combine -obj with -vars or a supplied file.\n");
        usage(); // Must not have submitted formatted file also
        }

    metaObjs = metaObjCreate(optionVal("obj",  NULL),
                            optionVal("type", OBJTYPE_DEFAULT),
                            var,
                            (optionExists("binary") ? "binary" : "txt"), // FIXME: don't know how to deal with binary yet
                            val);
    metaObjs->deleteThis = deleteIt;

    if(setVars != NULL)
        metaObjSwapVars(metaObjs,setVars,deleteIt);

    verbose(2, "metadata %s %s %s%s%s%s\n",
        metaObjs->obj,metaObjTypeEnumToString(metaObjs->objType),
        (metaObjs->deleteThis ? "delete ":""),
        (metaObjs->vars && metaObjs->vars->var!=NULL?metaObjs->vars->var:""),
        (metaObjs->vars && metaObjs->vars->val!=NULL?"=":""),
        (metaObjs->vars && metaObjs->vars->val!=NULL?metaObjs->vars->val:""));
    }
else if(optionExists("vars"))
    {
    if(sharedTbl && !force)
        {
        sqlDisconnect(&conn);
        verbose(1, "NOT SUPPORTED for shared table '%s'.\n",METATBL_DEFAULT_NAME);
        usage(); // Must not have submitted formatted file also
        }
    if(argc > 1)
        {
        sqlDisconnect(&conn);
        verbose(1, "INCONSISTENT REQUEST: can't combine -vars with a supplied file.\n");
        usage(); // Must not have submitted formatted file also
        }
    struct metaByVar * metaByVars = metaByVarsLineParse(optionVal("vars", NULL));
    metaObjs = metaObjsQueryByVars(conn,table,metaByVars);

    if(setVars != NULL)
        metaObjSwapVars(metaObjs,setVars,deleteIt);
    else

    metaObjTransformToUpdate(metaObjs,var,
                            (optionExists("binary") ? "binary" : "txt"), // FIXME: don't know how to deal with binary yet
                             val,deleteIt);
    }
else // Must be submitting formatted file
    {
    if(argc != 2)
        {
        sqlDisconnect(&conn);
        if(recreate) // no problem
            return 0;
        verbose(1, "REQUIRED: must declare -obj, -vars or supply a file.\n");
        usage(); // Must not have submitted formatted file also
        }

    replace = optionExists("replace");
    boolean validated = FALSE;
    metaObjs = metaObjsLoadFromFormattedFile(argv[1],&validated);
    if(sharedTbl && !force && !validated)
        {
        sqlDisconnect(&conn);
        errAbort("Update to shared table '%s' requires file directly written by metTblPrint from sandbox file.\n", table);
        }
    if(metaObjs != NULL)
        verbose(1, "Read %d metadata objects from %s\n", slCount(metaObjs),argv[1]);
    }

int count = 0;

if(metaObjs != NULL)
    {
    if(testIt && verboseLevel() > 2)
        metaObjPrint(metaObjs,FALSE);

    count = metaObjsSetToDb(conn,table,metaObjs,replace,testIt);
    }
if(testIt)
    verbose(1, "Command would affected %d row(s) in %s.%s\n", count,db,table);
else
    verbose(1, "Affected %d row(s) in %s.%s\n", count,db,table);

sqlDisconnect(&conn);
metaObjsFree(&metaObjs);
return 0;

// TODO:
// 1) Check in changes to hui.c hgc.c and remove metadataSettings in trackDbCustom.c
// 2) Diagram flow: (a) update/add "metaTbl_tdreszer" (b) Test in sandbox (c) print to RA file. (d) check in RA file (e) load RA file to "metaTbl"
// 3) Case insensitive hashs?
// 4) -ra by default (-1 line)?
// 5) expId table?  -exp=start requests generating unique ids for selected vars, then updating them. -expTbl generates expTable as id,"var=val var=val var=val"
}
