/* cdwMakeRepeatQa - Figure out what proportion of things align to repeats.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "cdw.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwMakeRepeatQa - Figure out what proportion of things align to repeats.\n"
  "usage:\n"
  "   cdwMakeRepeatQa startFileId endFileId\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct cdwFastqFile *cdwFastqFileForFileId(struct sqlConnection *conn, long long fileId)
/* Return cdwFastqFile associated with given file ID or NULL if none exist. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from cdwFastqFile where fileId=%lld", fileId);
return cdwFastqFileLoadByQuery(conn, query);
}

void raIntoCdwRepeatQa(char *fileName, struct sqlConnection *conn, long long fileId)
/* Read in two column file and put it into cdwQaRepeat table. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *repeatClass = row[0];
    double mapRatio = lineFileNeedDouble(lf, row, 1);
    char query[512];
    sqlSafef(query, sizeof(query), 
	"insert into cdwQaRepeat (fileId,repeatClass,mapRatio) values (%lld, \"%s\", %g)",
	fileId, repeatClass, mapRatio);
    sqlUpdate(conn, query);
    }
lineFileClose(&lf);
}

void fastqRepeatQa(struct sqlConnection *conn, struct cdwFile *ef, struct cdwValidFile *vf)
/* Do repeat QA if possible on fastq file. */
{
/* First see if total repeat content is already in our table, in which case we are done. */
long long fileId = ef->id;
char query[512];
sqlSafef(query, sizeof(query), 
    "select count(*) from cdwQaRepeat where fileId=%lld and repeatClass='total'" , fileId);
if (sqlQuickNum(conn, query) != 0)
    return;	/* We've done this already */

/* Get sample file name from fastq table. */
struct cdwFastqFile *fqf = cdwFastqFileForFileId(conn, fileId);
if (fqf == NULL)
    errAbort("No edqFastqRecord for %s",  vf->licensePlate);
char *fastqPath = fqf->sampleFileName;

char bwaIndex[PATH_LEN];
safef(bwaIndex, sizeof(bwaIndex), "/dev/shm/btData/repeatMasker/%s/repeatMasker.fa", 
   vf->ucscDb);

char cmd[3*PATH_LEN];
char *samName = cloneString(rTempName(cdwTempDir(), "cdwQaRepeat", ".sam"));
//we used to use BWA here, see the discussion about BWA/bowtie in cdwLib.c
safef(cmd, sizeof(cmd), "bowtie --mm --threads 4 %s %s -S > %s", bwaIndex, fastqPath, samName);
mustSystem(cmd);


char *raName = cloneString(rTempName(cdwTempDir(), "cdwQaRepeat", ".ra"));
safef(cmd, sizeof(cmd), "edwSamRepeatAnalysis %s %s", samName, raName);
mustSystem(cmd);
verbose(2, "mustSystem(%s)\n", cmd);
remove(samName);

raIntoCdwRepeatQa(raName, conn, fileId);
remove(raName);

freez(&samName);
freez(&raName);
cdwFastqFileFree(&fqf);
}

void cdwMakeRepeatQa(int startFileId, int endFileId)
/* cdwMakeRepeatQa - Figure out what proportion of things align to repeats.. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwFile *ef, *efList = cdwFileAllIntactBetween(conn, startFileId, endFileId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct cdwValidFile *vf = cdwValidFileFromFileId(conn, ef->id);
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
cdwMakeRepeatQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
