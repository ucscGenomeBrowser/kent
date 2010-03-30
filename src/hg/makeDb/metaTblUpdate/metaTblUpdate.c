/* metaTblUpdate - Adds, updates or removes metadata objects and variables from the metaTbl. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "metaTbl.h"
#include "hash.h"

static char const rcsid[] = "$Id: metaTblUpdate.c,v 1.6 2010/03/30 23:45:27 tdreszer Exp $";

#define OBJTYPE_DEFAULT "table"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaTblUpdate - Adds, updates or removes metadata objects and variables from the metaTbl.\n\n"
  "There are two ways to call this.  A single object/variable can be declared on the command "
  "line, or a file of formatted metadata lines can be provided.  The lines in the file can "
  "have the following formats:\n"
  "  metadata objName objType var1=val1 var2=\"val2 with spaces\" var3=...\n"
  "    Adds or updates the specific object and variables\n"
  "  metadata objName delete\n"
  "    delete all metadata for objName\n"
  "  metadata objName delete var=val var2=val2\n"
  "     deletes specifically named variables for objName. Must provide val but value is ignored.\n"
  "Special ENCODE format:"
  "  metadata var=val var2=\"val2 with spaces\" tableName=someTable fileName=someTable.narrowPeak.gz\n"
  "    if tableName and fileName and tableName=fileName.* then objName=someTable and objType=table.\n"
  "    else if tableName or fileName load as table or file\n\n"
  "usage:\n"
  "   metaTblUpdate -db= [-table=] [-test [-verbose=2]]\n"
  "                      [-obj= [-type=] [-delete] [-var=] [-binary] [-val=]]\n"
  "                      [-vars=\"var1=val1 var2=val2... [-delete] [-var=] [-binary] [-val=]]\n"
  "                      [fileName] [-replace]\n\n"
  "Options:\n"
  "    -db      Database to load metadata to.  This argument is required.\n"
  "    -table   Table to load metadata to.  Default is '" METATBL_DEFAULT_NAME "'.\n"
  "    -test    Does not update but only reports results.  Use with -verbose=2 to see each SQL statement.\n"
  "  if file not provided, then -obj or -vars must be provided\n"
  "    -obj={objName}     Means Load from command line:\n"
  "       -type={objType} Used if adding new obj, otherwise ignored.  Default is '" OBJTYPE_DEFAULT "'.\n"
  "    -vars={var=val...} An alternate way of requesting a group of objects to apply an update to.\n"
  "                       It is recommended you use the selection criteria in metaTblPrint to verify\n"
  "                       which objects meat your selection criteria first.\n"
  "    These options work with -obj or -vars:\n"
  "       -delete         Remove a specific var or entire obj (if -var not provided) otherwise add or update\n"
  "       -var={varName}  Provide variable name (if no -var then must be -remove to remove objects)\n"
  "       -binary         NOT YET IMPLEMENTED.  This var has a binary val and -val={file}\n"
  "       -val={value}    (Enclosed in \"quotes if necessary\".) If -var and no -val then must be -remove\n"
  "    [file]       File containing formatted metadata lines.  Ignored if -obj param provided:\n"
  "      -replace   Means remove all old variables for each object before adding new variables\n"
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
    {"delete",  OPTION_BOOLEAN},// delete one obj or obj/var
    {"binary",  OPTION_BOOLEAN},// val is binary (NOT YET IMPLEMENTED) implies -val={file}
    {"replace", OPTION_BOOLEAN},// replace entire obj when loading from file
    {"test",    OPTION_BOOLEAN},// give it a test, will ya?
    {NULL,      0}
};


int main(int argc, char *argv[])
// Process command line.
{
struct metaObj * metaObjs = NULL;

optionInit(&argc, argv, optionSpecs);
if(!optionExists("db"))
    {
    verbose(1, "REQUIRED: -db argument is missing.\n");
    usage();
    }

char *db         = optionVal("db",NULL);
char *table      = optionVal("table",METATBL_DEFAULT_NAME);
boolean deleteIt = optionExists("delete");
boolean testIt   = optionExists("test");
boolean replace  = FALSE;
char *var        = optionVal("var",NULL);
char *val        = optionVal("val",NULL);

if(argc > 1 && (deleteIt || var != NULL || val != NULL))
    {
    verbose(1, "INCONSISTENT REQUEST: can't combine supplied file with -delete -var or -val arguments.\n");
    usage();
    }
if(deleteIt && var != NULL && val != NULL)
    {
    verbose(1, "INCONSISTENT REQUEST: cant combine -delete with -var and -val.\n");
    usage();
    }
if (argc != 2 && !deleteIt && (var == NULL || val == NULL))
    {
    verbose(1, "INCONSISTENT REQUEST: need both -var and -val.\n");
    usage();
    }

struct sqlConnection *conn = sqlConnect(db);

if(optionExists("obj"))
    {
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

    verbose(2, "metadata %s %s %s%s%s%s\n",
        metaObjs->objName,metaObjTypeEnumToString(metaObjs->objType),
        (metaObjs->deleteThis ? "delete ":""),
        (metaObjs->vars && metaObjs->vars->var!=NULL?metaObjs->vars->var:""),
        (metaObjs->vars && metaObjs->vars->val!=NULL?"=":""),
        (metaObjs->vars && metaObjs->vars->val!=NULL?metaObjs->vars->val:""));
    }
else if(optionExists("vars"))
    {
    if(argc > 1)
        {
        sqlDisconnect(&conn);
        verbose(1, "INCONSISTENT REQUEST: can't combine -vars with a supplied file.\n");
        usage(); // Must not have submitted formatted file also
        }
    struct metaByVar * metaByVars = metaByVarsLineParse(optionVal("vars", NULL));
    metaObjs = metaObjsQueryByVars(conn,table,metaByVars);
    metaObjTransformToUpdate(metaObjs,var,
                            (optionExists("binary") ? "binary" : "txt"), // FIXME: don't know how to deal with binary yet
                             val,deleteIt);
    }
else // Must be submitting formatted file
    {
    if(argc != 2)
        {
        sqlDisconnect(&conn);
        verbose(1, "REQUIRED: must declare -obj, -vars or supply a file.\n");
        usage(); // Must not have submitted formatted file also
        }

    replace = optionExists("replace");
    metaObjs = metaObjsLoadFromFormattedFile(argv[1]);
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
// 1) make hgc use metaTbl
// 2) support for metaTbl_tdreszer in sandbox.  Auto-create table when requsting.
// 3) Restrict updates to metaTbl to ONLY support checked in files.  No command line, no single line
// 4) Diagram flow: (a) update/add "metaTbl_tdreszer" (b) Test in sandbox (c) print to RA file. (d) check in RA file (e) load RA file to "metaTbl"
// 5) expId table?
}
