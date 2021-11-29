/* tabJoin - Join together two tab-separated files based on a common field. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tabJoin - Join together two tab-separated files based on a common field\n"
  "usage:\n"
  "   tabJoin file1.tsv field1 file2.tsv field2 output.tsv\n"
  "The file1.tsv and file2.tsv should both be tab-separated files that have the\n"
  "same number of lines and the same items in the columns specified by field1\n"
  "and field2. The output is file in the same order as file1.tsv with the matching line from\n"
  "file2 appended to the line from file1.\n"
  "The field1 and field2 parameters can be numerical indexes (1-based) rather than\n"
  "field names\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void tableWithField(char *fileName, char *field, struct fieldedTable **retTable, int *retFieldIx)
/* Open up table and find index of field, which may be either a named field or a 1-based
 * field index into table */
{
struct fieldedTable *table = fieldedTableFromTabFile(fileName, fileName, NULL, 0);
int fieldIx = -1;
if (isAllDigits(field))
    {
    fieldIx = atoi(field) - 1;
    if (fieldIx < 0 || fieldIx >= table->fieldCount)
        errAbort("Numerical field %s for %s needs to be between 1 and %d"
	    , field, fileName, table->fieldCount);
    }
else
    {
    fieldIx = fieldedTableMustFindFieldIx(table, field);
    }
*retTable = table;
*retFieldIx = fieldIx;
}

void tabJoin(char *fileName1, char *fieldName1, char *fileName2, char *fieldName2, char *outFile)
/* tabJoin - Join together two tab-separated files based on a common field. */
{
/* Open up input and figure out where column is for joining */
int fieldIx1, fieldIx2;
struct fieldedTable *table1, *table2;
tableWithField(fileName1, fieldName1, &table1, &fieldIx1);
tableWithField(fileName2, fieldName2, &table2, &fieldIx2);

char *field1 = table1->fields[fieldIx1];
char *field2 = table2->fields[fieldIx2];

/* Do a little error checking, making sure the fields are unique and map 1-1 */
if (table1->rowCount != table2->rowCount)
    errAbort("%s has %d rows but %s has %d.  They must have same number of rows.", 
	fileName1, table1->rowCount, fileName2, table2->rowCount);
struct hash *hash1 = fieldedTableUniqueIndex(table1, field1);
struct hash *hash2 = fieldedTableUniqueIndex(table2, field2);

/* Open output and write out header */
FILE *f = mustOpen(outFile, "w");
if (table1->startsSharp)
    fputc('#', f);
fputs(table1->fields[0], f);
int i;
for (i=1; i<table1->fieldCount; ++i)
    {
    fputc('\t', f);
    fputs(table1->fields[i], f);
    }
for (i=0; i<table2->fieldCount; ++i)
    {
    fputc('\t', f);
    fputs(table2->fields[i], f);
    }
fputc('\n', f);

/* Write out rest of fields */
struct fieldedRow *row1;
for (row1 = table1->rowList; row1 != NULL; row1 = row1->next)
    {
    char *key = row1->row[fieldIx1];
    struct fieldedRow *row2 = hashFindVal(hash2, key);
    if (row2 == NULL)
        errAbort("%s is found in %s field of %s  but not %s field of %s", key, fieldName1, fileName1, 
	    fieldName2, fileName2);

    /* Write out data from table1 */
    for (i=0; i<table1->fieldCount; ++i)
	{
	fputs(row1->row[i], f);
	fputc('\t', f);
	}
    /* Write out data from table 2 */
    int lastField = table2->fieldCount-1;
    for (i=0; i<lastField;  ++i)
        {
	fputs(row2->row[i], f);
	fputc('\t', f);
	}
    fputs(row2->row[lastField], f);
    fputc('\n', f);
    }
carefulClose(&f);
hashFree(&hash1);
hashFree(&hash2);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
tabJoin(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
