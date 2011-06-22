/* encodeExp - manage ENCODE Experiment table (hgFixed.encodeExp) */

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
  "encodeExp - manage ENCODE Experiments table (hgFixed.encodeExp)\n"
  "usage:\n"
  "   encodeExp <action> [arg]\n"
  "\n"
  "actions:\n"
  "   add <exp.ra>		add experiments to table from file\n"
  "   acc <id>		add accession to experiment (approve it)\n"
  "   dump <exp.ra>	output experiment table to file\n"
  "   find <db> <exp.ra>	find unassigned experiments in metaDb and create .ra to file\n"
  "   history [<id>]	show changes to experiment entry\n"
  "   show <id>		print experiment to stdout\n"
  /* This command isn't very usable -- recommend using SQL queries instead
  "   id human|mouse <lab> <dataType> <cellType> [<vars>]\n"
  "			return id for experiment (vars as 'var1:val1 var2:val2')\n"
  */
  "\nmanagement actions: (default table \'%s\')\n"
  "   copy <newName>	copy table and add update triggers\n"
  "   create 		create table\n"
  "   drop 		drop table\n"
  "   rename <newName> 	rename table and update triggers\n"
  "   restore <exp.ra>	restore table from .ra file\n"
  "\n"
  "   deacc <id>			  deaccession experiment (remove accession, leave in table)\n"
  "   update <id> <var> <oldval> <newval>  change experiment var (must remove accession first)\n"
  "                                             e.g.  update 123 treatment ifnng ifn\n"
  "   remove <id> <why>		  remove experiment (delete from table)\n"
  "\n"
  "options:\n"
  "   -composite	limit to specified composite track (affects find)\n"
  "   -mdb		specify metaDb table name (default \'%s\') - for test use \n"
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
char *organism = NULL, *lab = NULL, *dataType = NULL, *cellType = NULL, *expVars = NULL;
char *var = NULL, *old = NULL, *new = NULL;
char *newTable = NULL;
char *why = NULL;

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

void expCreateTable()
/* Create table and history */
{
verbose(1, "Creating table \'%s\'\n", table);
encodeExpTableCreate(connExp, table);
}

void expDropTable()
/* Drop table and history*/
{
verbose(1, "Dropping table \'%s\'\n", table);
encodeExpTableDrop(connExp, table);
}

void expAdd(char *file)
/* Add rows from .ra file */
{
struct hash *ra = NULL;
struct lineFile *lf = lineFileOpen(file, TRUE);
struct encodeExp *exp;
struct hash *oldExps;
char *key;

/* create hash of keys for existing experiments so we can distinguish new ones */
oldExps = expKeyHashFromTable(connExp, table);

verbose(1, "Adding experiments from file \'%s\' to table \'%s\'\n", file, table);
while ((ra = raNextRecord(lf)) != NULL)
    {
    exp = encodeExpFromRa(ra);
    key = encodeExpKey(exp);
    if (hashLookup(oldExps, key) == NULL)
        {
        verbose(2, "Adding new experiment: %s\n", key);
        encodeExpAdd(connExp, table, exp);
        }
    else
        verbose(2, "Old experiment: %s\n", key);
    }
}

void expRestoreTable(char *file)
/* Fill empty table with experiments in .ra file with id's */
{
struct hash *ra = NULL;
struct lineFile *lf = lineFileOpen(file, TRUE);
struct encodeExp *exp;
int ix = 1;
int expId;
char *accession;
char *key;

verbose(1, "Restoring experiments from file \'%s\' to table \'%s\'\n", file, table);
if (sqlRowCount(connExp, table) != 0)
    errAbort("ERROR: table for restore must exist and be empty");

while ((ra = raNextRecord(lf)) != NULL)
    {
    exp = encodeExpFromRa(ra);

    /* save accession and id as we may stomp on these for to-delete experiments */
    accession = cloneString(exp->lab);
    expId = exp->ix;

    key = encodeExpKey(exp);
    while (ix < expId)
        {
        exp->accession = "DELETED";
        exp->ix = ix;
        verbose(3, "Adding row for deleted experiment %d\n", ix);
        encodeExpAdd(connExp, table, exp);
        ix++;
        }
    /* restore accession and id */
    exp->accession = accession;
    exp->ix = expId;
    encodeExpAdd(connExp, table, exp);
    verbose(3, "Adding row for experiment %d: %s\n", ix, key);
    ix++;
    }
verbose(1, "To complete restore, delete rows where accession=DELETED\n");
}

void expRenameTable()
/* Rename table and update history table triggers */
{
verbose(1, "Renaming table %s to %s\n", table, newTable);
encodeExpTableRename(connExp, table, newTable);
}

void expCopyTable()
/* Rename table and update history table triggers */
{
verbose(1, "Copying table %s to %s\n", table, newTable);
encodeExpTableCopy(connExp, table, newTable);
}

void expAcc(int id)
/* Add accession to an existing experiment (approve it)*/
{
char *acc = encodeExpAddAccession(connExp, table, id);
verbose(1, "Added accession: %s\n", acc);
}

void expDeacc(int id)
/* Decession an experiment (remove accession but leave in table)*/
{
verbose(1, "Removing accession from id: %d\n", id);
encodeExpRemoveAccession(connExp, table, id);
}

void expRemove(int id, char *why)
/* Remove an experiment (delete from table) */
{
struct encodeExp *exp = encodeExpGetByIdFromTable(connExp, table, id);
if (exp == NULL)
    errAbort("Id %d not found in experiment table %s", id, table);
verbose(1, "Deleting experiment id %d\n", id);
encodeExpRemove(connExp, table, exp, why);
}

void expShow(int id)
/* Print experiment in .ra format to stdout */
{
struct encodeExp *exp = encodeExpGetByIdFromTable(connExp, table, id);
if (exp == NULL)
    errAbort("Id %d not found in experiment table %s", id, table);
encodeExpToRaFile(exp, stdout);
}

void expHistory(int id)
/* Show changes to an existing experiment */
{
// TODO:  Libify

int i;
char **row;
struct dyString *dy = dyStringCreate("select * from %sHistory", table);
if (id != 0)
    dyStringPrintf(dy, " where %s=%d ", ENCODE_EXP_FIELD_IX, id);
dyStringPrintf(dy, " order by updateTime, %s", ENCODE_EXP_FIELD_IX);
struct sqlResult *sr = sqlGetResult(connExp, dyStringCannibalize(&dy));
while ((row = sqlNextRow(sr)) != NULL)
    {
    for (i = 0; i < ENCODEEXP_NUM_COLS+2; i++)
        // history has 2 additional fields:  action and user
        {
        printf("%s\t", row[i]);
        }
    puts("\n");
    }
sqlFreeResult(&sr);
}

void expDump(char *file)
/* Output rows to .ra file */
{
struct encodeExp *exp = NULL, *exps = NULL;

verbose(1, "Dumping table %s to %s\n", table, file);
FILE *f  = mustOpen(file, "w");
exps = encodeExpLoadAllFromTable(connExp, table);
while ((exp = slPopHead(&exps)) != NULL)
    encodeExpToRaFile(exp, f);
carefulClose(&f);
}

void expFind(char *assembly, char *file)
/* Find experiments in metaDb and output .ra file */
{
verbose(1, "Finding experiments in %s:%s\n", assembly, mdb);

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

    exp = encodeExpFromMdb(connMeta,assembly,meta);
    if (exp == NULL)
        continue;

    key = encodeExpKey(exp);
    verbose(3, "key: %s\n", key);

    if (hashLookup(newExps, key) == NULL &&
        hashLookup(oldExps, key) == NULL)
        {
        verbose(2, "Found new experiment - Date: %s	Experiment %d: %s\n",
                mdbObjFindValue(meta, "dateSubmitted"), ++expNum, key);
        /* save new experiment */
        hashAdd(newExps, key, NULL);
        slAddHead(&exps, exp);
        }
    /* Skip other metas belonging to the same exp by:
    struct mdbVar *edvs = mdbObjFindEncodeEdvs(connMeta,meta); // Can't use encodeExpVars(exp) because of "None" issues
    assert(edvs != NULL);
    char *expVars = slPairListToString(edvs,FALSE); // don't bother with quoting since edvs should not have spaces
    struct mdbObj *mdbExpObjs = mdbObjsFilterByVars(&metas,expVars,TRUE,TRUE);
    freeMem(expVars);
    mdbVarsFree(&edvs); // If you want to do this, then encodeExpFromMdb() above should be replaced with encodeExpFromMdbVars()
    mdbObjFree(&mdbExpObjs);
    // Filtering destroyed sort order // NOTE: Given the re-sort, this may not prove much more efficient
    mdbObjsSortOnVars(&metas, "dateSubmitted lab dataType cell");
    */
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

void expId()
/* Print id */
{
struct encodeExp *exps = NULL;
int count;
struct slPair *varPairs = NULL;

/* transform var:val to var=val. Can't use var=val on command-line as it conflicts with standard options processing */
if (expVars == NULL)
    varPairs = NULL;
else
    {
    memSwapChar(expVars, strlen(expVars), ':', '=');
    varPairs = slPairListFromString(expVars,FALSE); // don't expect quoted EDVs which should always be simple tokens.
    }
exps = encodeExpGetFromTable(organism, lab, dataType, cellType, varPairs, table);
count = slCount(exps);
verbose(2, "Results: %d\n", count);
if (count == 0)
    errAbort("Experiment not found in table %s", table);
if (count > 1)
    errAbort("Found more than 1 match for experiment");
printf("%d\n", exps->ix);
}

void expUpdate(int id)
/* Modify an experiment.  Changes value of one field or expVar for id specified. 
 * Aborts if experiment has an accession (must deaccession first) */
{
encodeExpUpdate(connExp, table, id, var, new, old);
verbose(1, "Modified id %d in table %s\n", id, table);
}

int encodeExp(char *command, char *file, char *assembly, int id)
/* manage ENCODE experiments table */
{
connExp = sqlConnect(ENCODE_EXP_DATABASE);
if (assembly != NULL)
    {
    organism = hOrganism(assembly);
    strLower(organism);
    }

if (sameString(command, "create"))
    expCreateTable();
else if (sameString(command, "drop"))
    expDropTable();
else if (sameString(command, "copy"))
    expCopyTable();
else if (sameString(command, "rename"))
    expRenameTable();
else if (sameString(command, "restore"))
    expRestoreTable(file);
else if (sameString(command, "add"))
    expAdd(file);
else if (sameString(command, "remove"))
    expRemove(id, why);
else if (sameString(command, "acc"))
    expAcc(id);
else if (sameString(command, "deacc"))
    expDeacc(id);
else if (sameString(command, "show"))
    expShow(id);
else if (sameString(command, "history"))
    expHistory(id);
else if (sameString(command, "dump"))
    expDump(file);
else if (sameString(command, "find"))
    expFind(assembly, file);
else if (sameString(command, "id"))
    expId();
else if (sameString(command, "update"))
    expUpdate(id);
else
    {
    errAbort("ERROR: Unknown command %s\n", command);
    usage();
    }
sqlDisconnect(&connExp);
return(0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *assembly = NULL;
char *file = NULL;
int id = 0;

optionInit(&argc, argv, options);
if (argc < 2)
    usage();

char *command = argv[1];
table = optionVal("table", (sameString(command, "copy") ||
                            sameString(command, "create") || 
                            sameString(command, "drop") ||
                            sameString(command, "rename") ||
                            sameString(command, "restore") ||
                            sameString(command, "deacc") ||
                            sameString(command, "update") ||
                            sameString(command, "remove")) ?
                        encodeExpTableNew: ENCODE_EXP_TABLE);
mdb = optionVal("mdb", MDB_DEFAULT_NAME);
composite = optionVal("composite", NULL);

verbose(2, "Experiment table name: %s\n", table);
if (sameString("find", command))
    {
    if (argc < 3)
        {
        errAbort("ERROR: Missing assembly\n");
        usage();
        }
    else
        assembly = argv[2];
    if (argc < 4)
        {
        errAbort("ERROR: Missing .ra file\n");
        usage();
        }
    else
        file = argv[3];
    }
else if (sameString("add", command) ||
         sameString("dump", command) || 
         sameString("restore", command))
    {
    if (argc < 3)
        {
        errAbort("ERROR: Missing .ra file\n");
        usage();
        }
    else
        file = argv[2];
    }
else if (sameString("id", command))
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
            expVars = argv[6];
        }
    }
else if ( sameString("acc", command) || 
        sameString("deacc", command) || 
        sameString("remove", command) || 
        sameString("update", command) ||
        sameString("show", command))
    {
    if (argc < 3)
        {
        errAbort("ERROR: Missing id\n");
        }
    else
        {
        id = atoi(argv[2]);
        if (id < 1)
            errAbort("ERROR: Bad id %d\n", id);
        if (sameString("remove", command))
            {
            if (argc < 4)
                errAbort("ERROR: remove must include why");
            else
                why = argv[3];
            }
        else if (sameString("update", command))
            {
            if (argc < 6)
                usage();
            var = argv[3];
            old = argv[4];
            new = argv[5];
            }
        }
    }
else if (sameString("history", command))
    {
    if (argc > 2)
        id = atoi(argv[2]);
    }
else if (sameString("rename", command) || sameString("copy", command))
    {
    if (argc < 3)
        errAbort("ERROR: Missing new tablename\n");
    if (argc != 3)
        usage();
    newTable = argv[2];
    }
encodeExp(command, file, assembly, id);
return 0;
}
