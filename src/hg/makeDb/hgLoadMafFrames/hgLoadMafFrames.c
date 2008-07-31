/* hgLoadMafFrames - load an mafFrames table  */
#include "common.h"
#include "options.h"
#include "mafFrames.h"
#include "linefile.h"
#include "binRange.h"
#include "pipeline.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgLoadMafFrames.c,v 1.2.100.1 2008/07/31 02:24:37 markd Exp $";

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void processFrameFile(FILE *sortFh, char *framesFile)
/* read records from one frame file, adding bin and write to pipe to sort */
{
struct lineFile *inLf = lineFileOpen(framesFile, TRUE);
struct mafFrames mf;
char *row[MAFFRAMES_NUM_COLS];

while (lineFileNextRowTab(inLf, row, MAFFRAMES_NUM_COLS))
    {
    mafFramesStaticLoad(row, &mf);
    fprintf(sortFh, "%d\t",  binFromRange(mf.chromStart, mf.chromEnd));
    mafFramesTabOut(&mf, sortFh);
    }
lineFileClose(&inLf);
}

void processFrameFiles(char *tabFile, int numFramesFiles, char **framesFiles)
/* combine and sort input files, adding bin column and write top tabFile */
{
/* sort by chrom location, accounting for bin column */
static char *cmd[] = {"sort", "-k", "2,2", "-k", "3,3n", NULL};
int i;
struct pipeline *pl = pipelineOpen1(cmd, pipelineWrite, tabFile, NULL);
FILE *sortFh = pipelineFile(pl);

for (i = 0; i < numFramesFiles; i++)
    processFrameFile(sortFh, framesFiles[i]);
pipelineWait(pl);
}

void hgLoadMafFrames(char *db, char *table, int numFramesFiles, char **framesFiles)
/* load an mafFrames table  */
{
char tabFile[PATH_LEN], *createSql;
struct sqlConnection *conn;
safef(tabFile, sizeof(tabFile), "%s.tab", table);

processFrameFiles(tabFile, numFramesFiles, framesFiles);

/* create table */
conn = hAllocConn(db);
createSql = mafFramesGetSql(table, 0, hGetMinIndexLength(db));
sqlRemakeTable(conn, table, createSql);
freez(&createSql);

sqlLoadTabFile(conn, tabFile, table, SQL_TAB_FILE_ON_SERVER);

unlink(tabFile);
hFreeConn(&conn);
}

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n"
    "\n"
    "hgLoadMafFrames - load an mafFrames table\n"
    "\n"
    "hgLoadMafFrames db table mafFrames1.tab [mafFrames2.tab...]\n"
    "\n"
    "Load table from the mafFrames files.  The input files will be sorted\n"
    "and loaded with a bin column\n"
    "\n", msg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage("wrong # args");
hgLoadMafFrames(argv[1], argv[2], argc-3, argv+3);

return 0;
}
