/* metaTblUpdate - Adds, updates or removes metadata obkjects and variables from the metaTbl. */
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

static char const rcsid[] = "$Id: metaTblUpdate.c,v 1.1 2010/03/18 01:56:26 tdreszer Exp $";

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
  "   metaTblUpdate [-db=] [-table=] [-obj= [-type=] [-delete] [-var=] [-binary] [-val=]]\n"
  "                       [-replace] [fileName]\n\n"
  "Options:\n"
  "    -db database to load metadata to.  Default is 'hg19'.\n"
  "    -table table to load metadata to.  Default is 'metaTbl'.\n"
  "  if file not provided, then -obj must be provided\n"
  "    -obj={objName}     means Load from command line:\n"
  "       -type={objType} needed if adding new obj, otherwise ignored\n"
  "       -delete         a specific var or entire obj (if -var not provided) otherwise add or update\n"
  "       -var={varName}  provide variable name (if no -var then must be -remove to remove obj)\n"
  "       -binary         NOT YET IMPLEMENTED.  This var has a binary val and -val={file}\n"
  "       -val={value}    (enclosed in quotes if necessary. If -var and no -val then must be -remove\n"
  "    [file] File containing formatted metadata lines.  Ignored if -obj param provided:\n"
  "      -replace means remove all old variables for an object before adding new variables\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"db",      OPTION_STRING}, // default "hg19"
    {"table",   OPTION_STRING}, // default "metaTbl"
    {"obj",     OPTION_STRING}, // objName or objId
    {"type",    OPTION_STRING}, // object type: table or file
    {"var",     OPTION_STRING}, // variable
    {"val",     OPTION_STRING}, // value
    {"delete",  OPTION_BOOLEAN},// delete one obj or obj/var
    {"binary",  OPTION_BOOLEAN},// val is binary (NOT YET IMPLEMENTED) implies -val={file}
    {"replace", OPTION_BOOLEAN},// replace entire obj when loading from file
    {NULL,      0}
};

#define DB_DEFAULT      "hg19"
#define TBL_DEFAULT     "metaTbl"
#define OBJTYPE_DEFAULT "table"

int main(int argc, char *argv[])
// Process command line.
{
struct metaObj * metaObjs = NULL;

optionInit(&argc, argv, optionSpecs);
char *db    = optionVal("db",   DB_DEFAULT);
char *table = optionVal("table",TBL_DEFAULT);
boolean replace = FALSE;

if(optionExists("obj"))
    {
    if(argc > 1)
        usage(); // Must not have submitted formatted file also

    AllocVar(metaObjs);

    metaObjs->objName = cloneString(optionVal("obj",  NULL));
    metaObjs->objType = metaObjTypeStringToEnum(optionVal("type", OBJTYPE_DEFAULT));

    metaObjs->deleteThis = optionExists("delete");

    if(optionExists("var"))
        {
        struct metaVar * metaVar;
        AllocVar(metaVar);

        metaVar->var     = cloneString(optionVal("var", NULL));
        metaVar->varType = (optionExists("binary") ? vtBinary : vtTxt);
        metaVar->val     = cloneString(optionVal("val", NULL));
        metaObjs->vars = metaVar; // Only one

        if (!metaObjs->deleteThis && metaVar->val == NULL)
            usage();
        }
    else if (!metaObjs->deleteThis)
            usage();

    verbose(2, "metadata %s %s %s%s%s%s\n",
        metaObjs->objName,metaObjTypeEnumToString(metaObjs->objType),
        (metaObjs->deleteThis ? "delete ":""),
        (metaObjs->vars && metaObjs->vars->var!=NULL?metaObjs->vars->var:""),
        (metaObjs->vars && metaObjs->vars->val!=NULL?"=":""),
        (metaObjs->vars && metaObjs->vars->val!=NULL?metaObjs->vars->val:""));
    }
else // Must be submitting formatted file
    {
    if(argc != 2)
        usage();

    replace = optionExists("replace");

    metaObjs = metaObjsLoadFromFormattedFile(argv[1]);
    if(metaObjs != NULL)
        verbose(1, "Read %d metadata lines from %s\n", slCount(metaObjs),argv[1]);
    }

if(metaObjs == NULL)
    usage();

struct sqlConnection *conn = sqlConnect(db);

int count = metaObjsSetToDb(conn,table,metaObjs,replace);

verbose(1, "Affected %d row(s) in %s.%s\n", count,db,table);

metaObjsFree(&metaObjs);
sqlDisconnect(&conn);
return 0;
}
