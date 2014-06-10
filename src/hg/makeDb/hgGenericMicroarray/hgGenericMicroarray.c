/* Load a generic microarray file into database. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "linefile.h"
#include "jksql.h"
#include "bed.h"
#include "expData.h"
#include "expRecord.h"
#include "hgRelate.h"

#define CSNA cloneString("n/a")

char *database = NULL;
char *table = NULL;

void usage() 
{
errAbort("hgGenericMicroarray - Load generic microarray file into database.\n"
	 "A generic microarray file has the following format: \n\n"
	 "\tarrayName1\tarrayName2\t...\tarrayNameM\n"
	 "probe1\tmeasure1,1\tmeasure1,2\t...\tmeasure1,M\n"
	 "probe2\tmeasure2,1\tmeasure2,2\t...\tmeasure2,M\n"
	 "...\n"
	 "probeN\tmeasureN,1\tmeasureN,2\t...\tmeasureN,M\n\n"
	 "usage:\n"
	 "   hgGenericMicroarray database table file.txt");
}

void saveDataTable(struct expData *data)
/* Create the expression table the cheesey way by loading a temp tab file. */
{
FILE *f = hgCreateTabFile(".", table);
struct expData *cur;
struct sqlConnection *conn = sqlConnect(database);
expDataCreateTable(conn, table);
for (cur = data; cur != NULL; cur = cur->next)
    expDataTabOut(cur, f);
hgLoadTabFile(conn, ".", table, &f);
hgRemoveTabFile(".", table);
sqlDisconnect(&conn);
}

void saveExpsTable(struct expRecord *records)
/* Create the Exps table. */
{
struct expRecord *cur;
struct sqlConnection *conn = sqlConnect(database);
char expTableName[100];
safef(expTableName, sizeof(expTableName), "%sExps", table);
expRecordCreateTable(conn, expTableName);
for (cur = records; cur != NULL; cur = cur->next)
    expRecordSaveToDb(conn, cur, expTableName, 2048);
sqlDisconnect(&conn);
}

void hgGenericMicroarray(char *file)
/* Load the simple file into two tables.*/
{
struct expRecord *exps = NULL;
struct expData *data = NULL;
struct lineFile *lf = lineFileOpen(file,TRUE);
char *strings[1000];
int ncols, i, n;
/* First line has experiment info */
ncols = lineFileChopTab(lf,strings);
for (i = 1; i < ncols; i++)
    {
    struct expRecord *oneRec = NULL;
    AllocVar(oneRec);
    oneRec->id = i - 1;
    oneRec->name = cloneString(strings[i]);
    oneRec->description = cloneString(strings[i]);
    oneRec->url = CSNA;
    oneRec->ref = CSNA;
    oneRec->credit = CSNA;
    oneRec->numExtras = 3;
    AllocArray(oneRec->extras, oneRec->numExtras);
    oneRec->extras[0] = cloneString("n/a");
    oneRec->extras[1] = cloneString("n/a");
    oneRec->extras[2] = cloneString(strings[i]);
    slAddHead(&exps, oneRec);
    }
slReverse(&exps);
/* Other lines have the data. */
while ((n = lineFileChopTab(lf,strings)) > 0)
    {
    struct expData *oneGene = NULL;
    AllocVar(oneGene);
    oneGene->name = cloneString(strings[0]);
    oneGene->expCount = ncols - 1;
    AllocArray(oneGene->expScores, oneGene->expCount);
    for (i = 1; i < ncols; i++)
	oneGene->expScores[i-1] = sqlFloat(strings[i]);
    slAddHead(&data, oneGene);
    }
if ((n != ncols) && (n > 0))
    errAbort("Wrong number of columns in line %d of %s. Got %d, want %d", lf->lineIx, file, n, ncols);
slReverse(&data);
saveExpsTable(exps);
saveDataTable(data);
expRecordFreeList(&exps);
expDataFreeList(&data);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* The program */
{
if (argc != 4) 
    usage();
database = argv[1];
table = argv[2];
hgGenericMicroarray(argv[3]);
return 0;
}
