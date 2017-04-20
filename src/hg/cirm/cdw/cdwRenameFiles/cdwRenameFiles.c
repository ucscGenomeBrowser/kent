/* cdwRenameFiles - Rename files submitted name.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "portable.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwRenameFiles - Rename files' submitted name both in database and manifest.\n"
  "usage:\n"
  "   cdwRenameFiles rename.tab oldManifest.txt newManifest.txt rename.sql\n"
  "where:\n"
  "   rename.tab is a two column tab or space separated files with old file name in\n"
  "              one column and current file name in the other\n"
  "   oldManifest.txt is the manifest file with the old name in it\n"
  "   newManifest.txt is the manifest file with the new name\n"
  "   rename.sql is the sql to execute to make the change in the database\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct oldToNew
/* Store old name and new name */
    {
    struct oldToNew *next;
    char *oldName;
    char *newName;
    };

struct oldToNew *oldToNewFromFile(char *fileName)
/* Read in a two column file into an oldToNew list */
{
char *row[2];
struct oldToNew *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    AllocVar(el);
    el->oldName = cloneString(row[0]);
    el->newName = cloneString(row[1]);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void fieldedTableToTabFile(struct fieldedTable *table, char *fileName)
/* Write out a fielded table back to file */
{
FILE *f = mustOpen(fileName, "w");

/* Write out header row with optional leading # */
if (table->startsSharp)
    fputc('#', f);
int i;
fputs(table->fields[0], f);
for (i=1; i<table->fieldCount; ++i)
    {
    fputc('\t', f);
    fputs(table->fields[i], f);
    }
fputc('\n', f);

/* Write out rest. */
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    fputs(fr->row[0], f);
    for (i=1; i<table->fieldCount; ++i)
	{
	fputc('\t', f);
	fputs(fr->row[i], f);
	}
    fputc('\n', f);
    }

carefulClose(&f);
}

void cdwRenameFiles(char *renameFile, char *oldMani, char *newMani, char *renameSql)
/* cdwRenameFiles - Rename files submitted name.. */
{
/* Figure out submission directory */
struct sqlConnection *conn = cdwConnect();
char *submitDir = getCurrentDir();
char query[512];
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId == 0)
    errAbort("%s not in cdwSubmit, run program from submission directory please\n", submitDir);

char *requiredFields[] = {"file",} ;
struct fieldedTable *maniTable = fieldedTableFromTabFile(oldMani, oldMani, requiredFields, 
    ArraySize(requiredFields));
int fileIx = stringArrayIx("file", maniTable->fields, maniTable->fieldCount);
struct hash *maniHash = fieldedTableUniqueIndex(maniTable, "file");

FILE *sqlF = mustOpen(renameSql, "w");

struct oldToNew *mv, *mvList = oldToNewFromFile(renameFile);
verbose(1, "Got %d files to rename in %s\n", slCount(mvList), renameFile);
for (mv = mvList; mv != NULL; mv = mv->next)
    {
    verbose(2, "%s->%s\n", mv->oldName, mv->newName);
    struct fieldedRow *fr = hashFindVal(maniHash, mv->oldName);
    if (fr == NULL)
        errAbort("%s not found in %s\n", mv->oldName, oldMani);
    fr->row[fileIx] = mv->newName;
    fprintf(sqlF, 
	"update cdwFile set submitFileName='%s' where submitFileName='%s' and submitDirId=%d;\n",
	mv->newName, mv->oldName, submitDirId);
    }
carefulClose(&sqlF);
fieldedTableToTabFile(maniTable, newMani);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
cdwRenameFiles(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
