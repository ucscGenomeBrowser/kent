/* hgLoadAxt - Load an axt file index into the database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "hdb.h"
#include "hgRelate.h"
#include "scoredRef.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadAxt - Load an axt file index into the database\n"
  "usage:\n"
  "   hgLoadAxt database table\n"
  "The axt files need to already exist in chromosome coordinates\n"
  "in the directory /gbdb/database/table.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgLoadAxt(char *database, char *table)
/* hgLoadAxt - Load a axt file index into the database. */
{
char extFileDir[512];
struct fileInfo *fileList = NULL, *fileEl;
struct sqlConnection *conn;
long count = 0;
FILE *f = hgCreateTabFile(".", table);

safef(extFileDir, sizeof(extFileDir), "/gbdb/%s/%s", database, table);
fileList = listDirX(extFileDir, "*.axt", TRUE);
conn = hgStartUpdate(database);
scoredRefTableCreate(conn, table, hGetMinIndexLength(database));
if (fileList == NULL)
    errAbort("%s doesn't exist or doesn't have any axt files", extFileDir);
for (fileEl = fileList; fileEl != NULL; fileEl = fileEl->next)
    {
    char *fileName = fileEl->name;
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct axt *axt;
    struct scoredRef mr;
    off_t offset = 0;
    HGID extId = hgAddToExtFile(fileName, conn);

    verbose(1, "Indexing and tabulating %s\n", fileName);
    while ((axt = axtReadWithPos(lf, &offset)) != NULL)
        {
	double maxScore = 100, minScore = -100;
	++count;
	ZeroVar(&mr);
	mr.chrom = axt->tName;
	mr.chromStart = axt->tStart;
	mr.chromEnd = axt->tEnd;
	if (axt->tStrand == '-')
	    reverseIntRange(&mr.chromStart, &mr.chromEnd, hChromSize(database, mr.chrom));
	mr.extFile = extId;
	mr.offset = offset;
	mr.score = (double)axt->score/(axt->tEnd - axt->tStart);
	mr.score = (mr.score-minScore)/(maxScore-minScore);
	if (mr.score > 1.0) mr.score = 1.0;
	if (mr.score < 0.0) mr.score = 0.0;
	fprintf(f, "%u\t", hFindBin(mr.chromStart, mr.chromEnd));
	scoredRefTabOut(&mr, f);
	axtFree(&axt);
	}
    }
verbose(1, "Loading %s into database\n", table);
hgLoadTabFile(conn, ".", table, &f);
hgEndUpdate(&conn, "Add %ld axts in %d files from %s", count, slCount(fileList), extFileDir);
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hgLoadAxt(argv[1], argv[2]);
return 0;
}
