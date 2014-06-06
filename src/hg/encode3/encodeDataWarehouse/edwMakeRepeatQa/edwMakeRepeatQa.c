/* edwMakeRepeatQa - Figure out what proportion of things align to repeats.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeRepeatQa - Figure out what proportion of things align to repeats.\n"
  "usage:\n"
  "   edwMakeRepeatQa startFileId endFileId\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct edwFastqFile *edwFastqFileForFileId(struct sqlConnection *conn, long long fileId)
/* Return edwFastqFile associated with given file ID or NULL if none exist. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from edwFastqFile where fileId=%lld", fileId);
return edwFastqFileLoadByQuery(conn, query);
}

void raIntoEdwRepeatQa(char *fileName, struct sqlConnection *conn, long long fileId)
/* Read in two column file and put it into edwQaRepeat table. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *repeatClass = row[0];
    double mapRatio = lineFileNeedDouble(lf, row, 1);
    char query[512];
    sqlSafef(query, sizeof(query), 
	"insert into edwQaRepeat (fileId,repeatClass,mapRatio) values (%lld, \"%s\", %g)",
	fileId, repeatClass, mapRatio);
    sqlUpdate(conn, query);
    }
lineFileClose(&lf);
}

void fastqRepeatQa(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf)
/* Do repeat QA if possible on fastq file. */
{
/* First see if total repeat content is already in our table, in which case we are done. */
long long fileId = ef->id;
char query[512];
sqlSafef(query, sizeof(query), 
    "select count(*) from edwQaRepeat where fileId=%lld and repeatClass='total'" , fileId);
if (sqlQuickNum(conn, query) != 0)
    return;	/* We've done this already */

/* Get sample file name from fastq table. */
struct edwFastqFile *fqf = edwFastqFileForFileId(conn, fileId);
if (fqf == NULL)
    errAbort("No edqFastqRecord for %s",  vf->licensePlate);
char *fastqPath = fqf->sampleFileName;

char bwaIndex[PATH_LEN];
safef(bwaIndex, sizeof(bwaIndex), "%s%s/repeatMasker/repeatMasker.fa", 
    edwValDataDir, vf->ucscDb);

char cmd[3*PATH_LEN];
char *saiName = cloneString(rTempName(edwTempDir(), "edwQaRepeat", ".sai"));
safef(cmd, sizeof(cmd), "bwa aln %s %s > %s", bwaIndex, fastqPath, saiName);
mustSystem(cmd);

char *samName = cloneString(rTempName(edwTempDir(), "edwQaRepeat", ".sam"));
safef(cmd, sizeof(cmd), "bwa samse %s %s %s > %s", bwaIndex, saiName, fastqPath, samName);
mustSystem(cmd);
remove(saiName);

char *raName = cloneString(rTempName(edwTempDir(), "edwQaRepeat", ".ra"));
safef(cmd, sizeof(cmd), "edwSamRepeatAnalysis %s %s", samName, raName);
mustSystem(cmd);
verbose(2, "mustSystem(%s)\n", cmd);
remove(samName);

raIntoEdwRepeatQa(raName, conn, fileId);
remove(raName);
#ifdef SOON
#endif /* SOON */

freez(&saiName);
freez(&samName);
freez(&raName);
edwFastqFileFree(&fqf);
}

void edwMakeRepeatQa(int startFileId, int endFileId)
/* edwMakeRepeatQa - Figure out what proportion of things align to repeats.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *ef, *efList = edwFileAllIntactBetween(conn, startFileId, endFileId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    if (vf != NULL)
	{
	if (sameString(vf->format, "fastq"))
	    fastqRepeatQa(conn, ef, vf);
	}
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwMakeRepeatQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
