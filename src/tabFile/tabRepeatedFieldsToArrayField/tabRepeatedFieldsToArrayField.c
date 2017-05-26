/* tabRepeatedFieldsToArrayField - Convert columns that are repeated in a tab-separated file to a 
 * single column with comma separated values.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tabRepeatedFieldsToArrayField - Convert columns that are repeated in a tab-separated file to a\n"
  "single column with comma separated values.\n"
  "usage:\n"
  "   tabRepeatedFieldsToArrayField in.tsv out.tsv\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct fieldInfo 
/* Information about a field */
    {
    struct fieldInfo *next;
    char *name;
    struct slInt *offsetList;
    };

void csvItemOutput(char *val, FILE *f)
/* Rewrite val, which may have some quotes or commas in it, in a way to be more compatable with
 * csv list representation */
{
/* If there are no commas just output it */
if (strchr(val, ',') == NULL)
    {
    fputs(val, f);
    return;
    }

/* Strip surrounding quotes if any */
val = trimSpaces(val);
int valLen = strlen(val);
if (valLen > 2 && val[0] == '"' && lastChar(val) == '"')
     {
     val[valLen-1] = 0;
     val += 1;
     }

/* Put quotes around it and output, escaping internal quotes with double quotes */
fputc('"', f);
char c;
while ((c = *val++) != 0)
    {
    if (c == '"')
	fputc('"', f);
    fputc(c, f);
    }
fputc('"', f);
}

void tabRepeatedFieldsToArrayField(char *inFile, char *outFile)
/* tabRepeatedFieldsToArrayField - Convert columns that are repeated in a tab-separated file to a 
 * single column with comma separated values. */
{
/* Read in tab-sep file */
struct fieldedTable *table = fieldedTableFromTabFile(inFile, inFile, NULL, 0);

/* Build up list and hash of fieldInfo from table's field list */
struct hash *hash = hashNew(0);
struct fieldInfo *list = NULL, *field;
int i;
for (i=0; i<table->fieldCount; ++i)
    {
    char *name = table->fields[i];
    field = hashFindVal(hash, name);
    if (field == NULL)
        {
	AllocVar(field);
	field->name = name;
	slAddHead(&list, field);
	hashAdd(hash, name, field);
	}
    struct slInt *si = slIntNew(i);
    slAddTail(&field->offsetList, si);
    }
slReverse(&list);


/* Open output file and write out header row with optional leading # */
FILE *f = mustOpen(outFile, "w");
if (table->startsSharp)
    fputc('#', f);
char *sep = "";
for (field = list; field != NULL; field = field->next)
    {
    fprintf(f, "%s%s", sep, field->name);
    sep = "\t";
    }
fputc('\n', f);

/* Write out main rows */
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    char *sep = "";
    for (field = list; field != NULL; field = field->next)
	{
	fputs(sep, f);
	struct slInt *offset;
	for (offset = field->offsetList; offset != NULL; offset = offset->next)
	    {
	    char *val = row[offset->val];
	    csvItemOutput(val, f);
	    if (offset->next != NULL)
	        fputc(',', f);
	    }
	sep = "\t";
	}
    fputc('\n', f);
    }
fputc('\n', f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tabRepeatedFieldsToArrayField(argv[1], argv[2]);
return 0;
}
