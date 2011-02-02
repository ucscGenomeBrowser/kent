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
  "   fileAdd expFile.ra    add experiments to table from file\n"
  "   fileDump expFile.ra	output experiment table to file\n"
  "   metaFind db	find unassigned experiments in metaDb and create .ra to stdout\n"
  "   metaCheck db	find objects in metaDb incorrect or missing expId\n"
  "   id ix	generate id from ix (wgEncodeE<H|M>00000<ix>)\n"
  "options:\n"
  "   -table	specify table name (default %s)",
  DEFAULT_ENCODE_EXP_TABLE, DEFAULT_ENCODE_EXP_TABLE
  );
}

#define EXP_DATABASE    "hgFixed"

char *table = NULL;
struct sqlConnection *conn = NULL;

static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {NULL, 0},
};

void expCreate()
/* Create table */
{
verbose(2, "Creating table %s\n", table);
encodeExpTableCreate(conn, table);
}

void expAdd(char *file)
/* Add rows from .ra file */
{
verbose(2, "Adding rows from %s\n", file);
}

void expDump(char *file)
/* Output rows to .ra file */
{
verbose(2, "Dumping table to %s\n", file);
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

void expId(int id)
/* Output full experiment identifier for id */
{
verbose(2, "Experiment ID for %d\n", id);
}

void encodeExp(char *command, char *file, char *assembly, int id)
/* manage ENCODE experiments table */
{
conn = sqlConnect(EXP_DATABASE);
if (sameString(command, "create"))
    expCreate();
else if (sameString(command, "fileAdd"))
    expAdd(file);
else if (sameString(command, "fileDump"))
    expDump(file);
else if (sameString(command, "metaFind"))
    expFind(assembly);
else if (sameString(command, "metaCheck"))
    expCheck(assembly);
else if sameString(command, "id")
    expId(id);
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
int id = 0;

table = optionVal("table", sameString(command, "create") ?
                        DEFAULT_ENCODE_EXP_TABLE "New": 
                        DEFAULT_ENCODE_EXP_TABLE);
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
if (startsWith("id", command))
    {
    if (argc < 3)
        {
        fprintf(stderr, "ERROR: Missing id\n\n");
        usage();
        }
    else
        id = atoi(argv[2]);
    }
encodeExp(command, file, assembly, id);
return 0;
}
