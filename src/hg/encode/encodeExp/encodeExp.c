/* encodeExp - manage ENCODE Experiment table (hgFixed:encodeExp) */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "mdb.h"
#include "ra.h"
#include "portable.h"
#include "../../inc/encode/encodeExp.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeExp - manage ENCODE Experiments table (hgFixed:encodeExp)\n"
  "usage:\n"
  "   encodeExp <action> [arg]\n"
  "actions:\n"
  "   create 	create table (default %sNew)\n"
  "   fileAdd expFile.ra        add experiments to table from file\n"
  "   fileDump expFile.ra	output experiment table to file\n"
  "   metaFind db	find unassigned experiments in metaDb and create .ra to stdout\n"
  "   metaCheck db	find objects in metaDb incorrect or missing expId\n"
  "options:\n"
  "   -table	specify table name (default %s)"
  "   -composite	limit to specified composite track (affects metaFind and metaCheck)",
  ENCODE_EXP_TABLE, ENCODE_EXP_TABLE
  );
}

char *table = NULL;
char *composite = NULL;
struct sqlConnection *conn = NULL;

static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {"composite", OPTION_STRING},
   {NULL, 0},
};

void expCreate()
/* Create table */
{
verbose(2, "Creating table %s\n", table);
encodeExpTableCreate(conn, table);
}

void expFileAdd(char *file)
/* Add rows from .ra file. Exit on err -- bad format or not unique */
{
struct hash *ra = NULL;
struct lineFile *lf = lineFileOpen(file, TRUE);

/* TODO: Read in table to check for uniqueness */
verbose(2, "Adding experiments from file: %s\n", file);
while ((ra = raNextRecord(lf)) != NULL)
    {
    struct encodeExp *exp = encodeExpFromRa(ra);
    encodeExpSave(conn, exp, table);
    }
}

void expFileDump(char *file)
/* Output rows to .ra file */
{
struct encodeExp *exp, *exps;
char query[256];

verbose(2, "Dumping table to %s\n", file);
FILE *f  = mustOpen(file, "w");
safef(query, 256, "select * from %s order by ix", table);
exps = encodeExpLoadByQuery(conn, query);
while ((exp = slPopHead(&exps)) != NULL)
    {
    /* TODO -> lib */
    fprintf(f, "accession %s\n", exp->accession);
    fprintf(f, "organism %s\n", exp->organism);
    fprintf(f, "lab %s\n", exp->lab);
    fprintf(f, "dataType %s\n", exp->dataType);
    fprintf(f, "vars %s\n", exp->vars);
    fprintf(f, "\n");
    }
}

void expFind(char *assembly)
/* Find experiments in metaDb and output .ra file */
{
verbose(2, "Finding experiments in %s metaDb\n", assembly);
}

void expCheck(char *assembly)
/* Check metaDb for objs with missing or inconsistent experimentId */
{
verbose(2, "Checking experiments %s metaDb\n", assembly);
}

#ifdef TODO
void makeAccession(int ix)
{
/* Make accession for an id */
printf("%s%c%06d", ENCODE_EXP_ACC_PREFIX,[H|M], ix);
}
#endif

void encodeExp(char *command, char *file, char *assembly)
/* manage ENCODE experiments table */
{
conn = sqlConnect(ENCODE_EXP_DATABASE);
if (sameString(command, "create"))
    expCreate();
else if (sameString(command, "fileAdd"))
    expFileAdd(file);
else if (sameString(command, "fileDump"))
    expFileDump(file);
else if (sameString(command, "metaFind"))
    expFind(assembly);
else if (sameString(command, "metaCheck"))
    expCheck(assembly);
else
    {
    fprintf(stderr, "ERROR: Unknown command %s\n\n", command);
    usage();
    }
sqlDisconnect(&conn);
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
                        ENCODE_EXP_TABLE "New": 
                        ENCODE_EXP_TABLE);
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
encodeExp(command, file, assembly);
return 0;
}
