/* demergeSingle - Help propagate terminology changes used in merged tracks to what makes them up.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"
#include "fieldedTable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "demergeSingle - Help propagate terminology changes used in merged tracks to what makes them up.\n"
  "usage:\n"
  "   demergeSingle merged fileList backupSuffix\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void demergeOne(struct fieldedTable *mergedTable, struct hash *mergedHash, 
    char *oldFile, char *newFile)
{
char *requiredSingle[] = {"cluster", "total"};
struct fieldedTable *oldTable = fieldedTableFromTabFile(oldFile, oldFile,
    requiredSingle, ArraySize(requiredSingle));
uglyf("%s has %d rows and %d fields\n", oldFile, oldTable->rowCount, oldTable->fieldCount);

/* Open output file and write header */
FILE *f = mustOpen(newFile, "w");
fprintf(f, "#label\told_label");
int i;
for (i=1; i<mergedTable->fieldCount; ++i)
    fprintf(f, "\t%s", mergedTable->fields[i]);
fprintf(f, "\n");

int joinIx = fieldedTableMustFindFieldIx(oldTable, "total");
struct fieldedRow *fr;
for (fr = oldTable->rowList; fr != NULL; fr = fr->next)
    {
    char *key = fr->row[joinIx];
    struct fieldedRow *mergedRow = hashMustFindVal(mergedHash, key);
    char *organCellType = mergedRow->row[0];
    nextWord(&organCellType);	// skip organ
    fprintf(f, "%s\t%s", organCellType, fr->row[0]);
    int i;
    for (i=1; i<mergedTable->fieldCount; ++i)
	fprintf(f, "\t%s", mergedRow->row[i]);
    fprintf(f, "\n");
    }
carefulClose(&f);
}


void demergeSingle(char *merged, char *oldFiles, char *backupSuffix)
/* demergeSingle - Help propagate terminology changes used in merged tracks to what makes them up.. */
{
char *required[] = {"count", "total", "cell_class",};
struct fieldedTable *mergedTable = fieldedTableFromTabFile(merged, merged, 
    required, ArraySize(required));
uglyf("%s has %d rows and %d fields\n", merged, mergedTable->rowCount, mergedTable->fieldCount);
struct hash *mergedHash = fieldedTableUniqueIndex(mergedTable, "total");

struct slName *fileList = readAllLines(oldFiles);
uglyf("Processing %d files\n", slCount(fileList));
struct slName *file;
for (file = fileList; file != NULL; file = file->next)
    {
    char newPath[PATH_LEN];
    safef(newPath, sizeof(newPath), "%s%s", file->name, backupSuffix);
    demergeOne(mergedTable, mergedHash, file->name, newPath);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
demergeSingle(argv[1], argv[2], argv[3]);
return 0;
}
