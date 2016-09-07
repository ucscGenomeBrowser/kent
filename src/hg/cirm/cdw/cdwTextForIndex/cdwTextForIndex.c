/* cdwTextForIndex - Make text file used for building ixIxx indexes. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdw.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwTextForIndex - Make text file used for building ixIxx indexes\n"
  "usage:\n"
  "   cdwTextForIndex out.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

/* Fields that should go first, in the order they should go */
char *priorityFields[] = { "accession", "keywords", "sample_label", "data_set_id", "lab", "age", 
    "age_unit", "donor", "file_name", "meta", "output", "pipeline", };

struct hash *hashTextFields(struct sqlConnection *conn, char *table)
/* Return hash with just text (char, varchar, blob, text) fields in it, and no values */
{
struct hash *hash = hashNew(0);
struct sqlResult *sr = sqlDescribe(conn, table);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *field = row[0];
    char *type = row[1];
    if (stringIn("char", type) || stringIn("blob", type) || stringIn("text", type))
        hashAdd(hash, field, NULL);
    }
sqlFreeResult(&sr);
return hash;
}

struct sqlResult *sqlDescribe(struct sqlConnection *conn, char *table);
void cdwTextForIndex(char *outFile)
/* cdwTextForIndex - Make text file used for building ixIxx indexes. */
{
struct sqlConnection *conn = cdwConnect();
struct hash *textHash = hashTextFields(conn, "cdwFileTags");

/* Start up query of all fields of fileTags table and get array of all fields from result */
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwFileTags");
struct sqlResult *sr = sqlGetResult(conn, query);
char **allFields = NULL;
int fieldCount = sqlResultFieldArray(sr, &allFields);

/* Accession is special, make sure it's there */
int idIx = stringArrayIx("file_id", allFields, fieldCount);
if (idIx < 0)
    errAbort("Can't find file_id in cdwFileTags");

/* Make up an array that tells us the order of fields we'll output, starting with priority fields */
/* Get all priority fields first */
int order[fieldCount];
int fieldsUsed = 0;
int i;
struct hash *usedHash = hashNew(0);
for (i=0; i<ArraySize(priorityFields); ++i)
    {
    char *field = priorityFields[i];
    int pos = stringArrayIx(field, allFields, fieldCount);
    if (pos >= 0)
	{
	order[fieldsUsed++] = pos;
	hashAdd(usedHash, field, NULL);
	}
    }

/* Get other fields now */
for (i=0; i<fieldCount; ++i)
    {
    char *field = allFields[i];
    if (!hashLookup(usedHash, field) && hashLookup(textHash, field))
	order[fieldsUsed++] = i;
    }

/* Now loop through sql result and write output */
FILE *f = mustOpen(outFile, "w");
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *id = row[idIx];
    if (id != NULL)
	{
	fprintf(f, "%s", id);
	for (i=0; i<fieldsUsed; ++i)
	    {
	    char *val = row[order[i]];
	    if (val != NULL)
		fprintf(f, " %s", row[order[i]]);
	    }
	fprintf(f, "\n");
	}
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwTextForIndex(argv[1]);
return 0;
}
