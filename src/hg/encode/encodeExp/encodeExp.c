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
  "   add <exp.ra>		add experiments to table from file\n"
  "   dump <exp.ra>		output experiment table to file\n"
  "   find <db> <exp.ra>	find unassigned experiments in metaDb and create .ra to file\n"
  "   check <db>		find objects in metaDb incorrect or missing accession\n"
  "   acc human|mouse <lab> <dataType> <cellType> [<factors>]\n"
  "				return accession for experiment (factors as 'factor1:val1 factor2:val2')\n"
  "options:\n"
  "   -composite	limit to specified composite track (affects find and check)"
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

struct sqlConnection *connExp = NULL;
char *composite = NULL, *mdb = NULL, *table = NULL;
char *organism = NULL, *lab = NULL, *dataType = NULL, *cellType = NULL, *factors = NULL;

static struct hash *expKeyHashFromTable(struct sqlConnection *conn, char *table)
/* create hash of keys for existing experiments so we can distinguish new ones */
{
struct hash *hash = hashNew(0);
struct encodeExp *exp = NULL, *exps = NULL;

exps = encodeExpLoadAllFromTable(conn, table);
while ((exp = slPopHead(&exps)) != NULL)
    {
    hashAdd(hash, encodeExpKey(exp), NULL);
    freez(&exp);
    }
return hash;
}

void expCreate()
/* Create table */
{
verbose(2, "Creating table \'%s\'\n", table);
encodeExpTableCreate(connExp, table);
}

void expAdd(char *file)
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
    encodeExpSave(connExp, table, exp);
    }
}

void expDump(char *file)
/* Output rows to .ra file */
{
struct encodeExp *exp = NULL, *exps = NULL;

verbose(2, "Dumping table %s to %s\n", table, file);
FILE *f  = mustOpen(file, "w");
exps = encodeExpLoadAllFromTable(connExp, table);
while ((exp = slPopHead(&exps)) != NULL)
    encodeExpToRaFile(exp, f);
carefulClose(&f);
}

void expFind(char *assembly, char *file)
/* Find experiments in metaDb and output .ra file */
{
verbose(2, "Finding experiments in %s:%s\n", assembly, mdb);

struct sqlConnection *connMeta;
struct mdbObj *meta = NULL, *metas = NULL;
struct encodeExp *exp = NULL, *exps = NULL;
struct hash *oldExps, *newExps;
char *key;
int expNum = 0;

FILE *f  = mustOpen(file, "w");

/* create hash of keys for existing experiments so we can distinguish new ones */
oldExps = expKeyHashFromTable(connExp, table);
newExps = hashNew(0);

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
slReverse(&exps);
while ((exp = slPopHead(&exps)) != NULL)
    {
    exp->organism = organism;
    encodeExpToRaFile(exp, f);
    }
carefulClose(&f);
sqlDisconnect(&connMeta);
}

void expCheck(char *assembly)
/* Check metaDb for objs with missing or inconsistent experimentId */
{
verbose(2, "Checking experiments in %s:%s\n", assembly, mdb);
errAbort("check not implemented");
}

void expAccession()
/* Return accession */
{
struct encodeExp *exps = NULL;
int count;
struct slPair *factorPairs = NULL;

/* transform var:val to var=val. Can't use var=val on command-line as it conflicts with standard options processing */
memSwapChar(factors, strlen(factors), ':', '=');
factorPairs = slPairFromString(factors);
exps = encodeExpGetFromTable(organism, lab, dataType, cellType, factorPairs, table);
count = slCount(exps);
verbose(2, "Results: %d\n", count);
if (count == 0)
    errAbort("Experiment not found");
if (count > 1)
    errAbort("Found more than 1 match for experiment");
printf("%s\n", exps->accession);
}

int encodeExp(char *command, char *file, char *assembly)
/* manage ENCODE experiments table */
{
connExp = sqlConnect(ENCODE_EXP_DATABASE);
organism = hOrganism(assembly);
strLower(organism);

if (sameString(command, "create"))
    expCreate();
else if (sameString(command, "add"))
    expAdd(file);
else if (sameString(command, "dump"))
    expDump(file);
else if (sameString(command, "find"))
    expFind(assembly, file);
else if (sameString(command, "check"))
    expCheck(assembly);
else if (sameString(command, "acc"))
    expAccession();
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
if (sameString("find", command) || sameString("check", command))
    {
    if (argc < 3)
        {
        fprintf(stderr, "ERROR: Missing assembly\n\n");
        usage();
        }
    else
        assembly = argv[2];
    if (sameString("find", command))
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
if (sameString("add", command) || sameString("dump", command))
    {
    if (argc < 3)
        {
        fprintf(stderr, "ERROR: Missing .ra file\n\n");
        usage();
        }
    else
        file = argv[2];
    }
else if (sameString("acc", command))
    {
    if (argc < 6)
        usage();
    else
        {
        organism = argv[2];
        lab = argv[3];
        dataType = argv[4];
        cellType = argv[5];
        if (argc > 6)
            factors = argv[6];
        }
    }
encodeExp(command, file, assembly);
return 0;
}
