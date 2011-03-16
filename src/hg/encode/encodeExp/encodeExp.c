/* encodeExp - manage ENCODE Experiment table (hgFixed:encodeExp) */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "mdb.h"
#include "ra.h"
#include "portable.h"
#include "../../inc/encode/encodeExp.h"

static char *encodeExpTableNew = ENCODE_EXP_TABLE "New";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeExp - manage ENCODE Experiments table (hgFixed:encodeExp)\n"
  "usage:\n"
  "   encodeExp <action> [arg]\n"
  "actions:\n"
  "   create 			create table (default \'%s\')\n"
  "   fileAdd <exp.ra>		add experiments to table from file\n"
  "   fileDump <exp.ra>		output experiment table to file\n"
  "   metaFind <db> <exp.ra>	find unassigned experiments in metaDb and create .ra to file\n"
  "   metaCheck <db>		find objects in metaDb incorrect or missing expId\n"
  "   id human|mouse <lab> <dataType> <cellType> <vars>\n"
  "				return ID for experiment (vars as 'var1:val1 var2:val2' or 'none')\n"
  "options:\n"
  "   -composite	limit to specified composite track (affects metaFind and metaCheck)"
  "   -mdb	specify metaDb table name (default \'%s\') - for test use \n"
  "   -table	specify experiment table name (default \'%s\')\n",
  encodeExpTableNew, MDB_DEFAULT_NAME, ENCODE_EXP_TABLE
  );
}

static struct optionSpec options[] = {
   {"composite", OPTION_STRING},
   {"mdb", OPTION_STRING},
   {"table", OPTION_STRING},
   {NULL, 0},
};

char *composite = NULL, *mdb = NULL, *table = NULL;
char *organism = NULL, *lab = NULL, *dataType = NULL, *cellType = NULL, *vars = NULL;
struct sqlConnection *connExp = NULL;

void expCreate()
/* Create table */
{
verbose(2, "Creating table \'%s\'\n", table);
encodeExpTableCreate(connExp, table);
}

void expFileAdd(char *file)
/* Add rows from .ra file. Exit on err -- bad format or not unique */
{
struct hash *ra = NULL;
struct lineFile *lf = lineFileOpen(file, TRUE);
struct encodeExp *exp;

/* TODO: Read in table to check for uniqueness */
verbose(2, "Adding experiments from file \'%s\' to table \'%s\'\n", file, table);
while ((ra = raNextRecord(lf)) != NULL)
    {
    exp = encodeExpFromRa(ra);
    encodeExpSave(connExp, exp, table);
    }
}

void expFileDump(char *file)
/* Output rows to .ra file */
{
struct encodeExp *exp = NULL, *exps = NULL;

verbose(2, "Dumping table %s to %s\n", table, file);
FILE *f  = mustOpen(file, "w");
exps = encodeExpLoadAllFromTable(connExp, table);
while ((exp = slPopHead(&exps)) != NULL)
    {
    encodeExpToRaFile(exp, f);
    }
carefulClose(&f);
}

void expMetaFind(char *assembly, char *file)
/* Find experiments in metaDb and output .ra file */
/* TODO: Experiment-defining variable support -> lib */
{
verbose(2, "Finding experiments in %s:%s\n", assembly, mdb);

struct sqlConnection *connMeta;
struct mdbObj *meta = NULL, *metas = NULL;
struct encodeExp *exp = NULL, *exps = NULL;
struct hash *newExps = hashNew(0), *oldExps = hashNew(0);
char *key;
int expNum = 0;

FILE *f  = mustOpen(file, "w");

/* create hash of keys for existing experiments so we can distinguish new ones */
exps = encodeExpLoadAllFromTable(connExp, table);
while ((exp = slPopHead(&exps)) != NULL)
    {
    hashAdd(oldExps, encodeExpKey(exp), NULL);
    freez(&exp);
    }

/* read mdb objects from database */
connMeta = sqlConnect(assembly);
metas = mdbObjsQueryAll(connMeta, mdb);
verbose(2, "Found %d objects\n", slCount(metas));

/* order so that oldest have lowest ids */
mdbObjsSortOnVars(&metas, "dateSubmitted lab dataType cell");

/* create new experiments */
while ((meta = slPopHead(&metas)) != NULL)
    {
    if (!mdbObjIsEncode(meta))
        continue;
    if (composite != NULL && !mdbObjInComposite(meta, composite))
        continue;

    exp = encodeExpFromMdb(meta);
    key = encodeExpKey(exp);

    if (hashLookup(newExps, key) == NULL)
        {
        verbose(2, "Date: %s	Experiment %d: %s\n", 
                mdbObjFindValue(meta, "dateSubmitted"), ++expNum, key);
        /* save new experiment */
        hashAdd(newExps, key, NULL);
        slAddHead(&exps, exp);
        }
    }
/* write out experiments in .ra format */
organism = hOrganism(assembly);
strLower(organism);
slReverse(&exps);
while ((exp = slPopHead(&exps)) != NULL)
    {
    exp->organism = organism;
    encodeExpToRaFile(exp, f);
    }
carefulClose(&f);
sqlDisconnect(&connMeta);
}

void expMetaCheck(char *assembly)
/* Check metaDb for objs with missing or inconsistent experimentId */
{
verbose(2, "Checking experiments %s:%s\n", assembly, mdb);
errAbort("metaCheck not implemented");
}

void expId()
/* Return ID */
{
struct dyString *queryDs = newDyString(0);
//TODO: -> lib
/* transform var:val to var=val. Can't use var=val on command-line as it conflicts with standard options processing */
memSwapChar(vars, strlen(vars), ':', '=');
if (sameString(vars, "none"))
    vars = "";
dyStringPrintf(queryDs, "select * from %s where organism=\'%s\' and lab=\'%s\' and dataType=\'%s\' and cellType=\'%s\' and vars=\'%s\'", 
                        table, organism, lab, dataType, cellType, vars); 
verbose(2, "QUERY: %s\n", queryDs->string);
struct encodeExp *exps = encodeExpLoadByQuery(connExp, dyStringCannibalize(&queryDs));
int count = slCount(exps);
verbose(2, "Results: %d\n", count);
if (count == 0)
    errAbort("Not found");
if (count > 1)
    errAbort("Found more than 1 match");
printf("%d\n", exps->ix);
}

int encodeExp(char *command, char *file, char *assembly)
/* manage ENCODE experiments table */
{
connExp = sqlConnect(ENCODE_EXP_DATABASE);
if (sameString(command, "create"))
    expCreate();
else if (sameString(command, "fileAdd"))
    expFileAdd(file);
else if (sameString(command, "fileDump"))
    expFileDump(file);
else if (sameString(command, "metaFind"))
    expMetaFind(assembly, file);
else if (sameString(command, "metaCheck"))
    expMetaCheck(assembly);
else if (sameString(command, "id"))
    expId();
else
    {
    fprintf(stderr, "ERROR: Unknown command %s\n\n", command);
    usage();
    }
sqlDisconnect(&connExp);
return(0);
}

int main(int argc, char *argv[])
/* Process command line. */
/* TODO: Clean up arg handling */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
char *command = argv[1];
char *assembly = NULL;
char *file = NULL;

table = optionVal("table", sameString(command, "create") ?
                        encodeExpTableNew: 
                        ENCODE_EXP_TABLE);
mdb = optionVal("mdb", MDB_DEFAULT_NAME);
composite = optionVal("composite", NULL);

verboseSetLevel(2);
verbose(3, "Experiment table name: %s\n", table);
if (startsWith("meta", command))
    {
    if (argc < 3)
        {
        fprintf(stderr, "ERROR: Missing assembly\n\n");
        usage();
        }
    else
        assembly = argv[2];
    if (sameString("metaFind", command))
        {
        if (argc < 4)
            {
            fprintf(stderr, "ERROR: Missing file\n\n");
            usage();
            }
        else
            file = argv[3];
        }
    }
if (startsWith("file", command))
    {
    if (argc < 3)
        {
        fprintf(stderr, "ERROR: Missing .ra file\n\n");
        usage();
        }
    else
        file = argv[2];
    }
else if (sameString("id", command))
    {
    if (argc < 7)
        usage();
    else
        {
        organism = argv[2];
        lab = argv[3];
        dataType = argv[4];
        cellType = argv[5];
        vars = argv[6];
        }
    }
encodeExp(command, file, assembly);
return 0;
}
