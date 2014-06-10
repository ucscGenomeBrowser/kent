/* edwFixTargetSeq - Fill in new fields about target seq to edwBamFile and edwAssembly.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "twoBit.h"
#include "bamFile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixTargetSeq - Fill in new fields about target seq to edwBamFile and edwAssembly.\n"
  "usage:\n"
  "   edwFixTargetSeq now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFixTargetSeq(char *when)
/* edwFixTargetSeq - Fill in new fields about target seq to edwBamFile and edwAssembly.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwAssembly *as, *asList = edwAssemblyLoadByQuery(conn, "select * from edwAssembly");
char query[512];
for (as = asList; as != NULL; as = as->next)
    {
    char *twoBitFileName = edwPathForFileId(conn, as->twoBitId);
    struct twoBitFile *tbf = twoBitOpen(twoBitFileName);
    safef(query, sizeof(query), "update edwAssembly set seqCount=%u where id=%u",
	tbf->seqCount, as->id);
    sqlUpdate(conn, query);
    freez(&twoBitFileName);
    twoBitClose(&tbf);
    }
edwAssemblyFreeList(&asList);

struct edwBamFile *bam, *bamList = edwBamFileLoadByQuery(conn, "select * from edwBamFile");
for (bam = bamList; bam != NULL; bam = bam->next)
    {
    char *fileName = edwPathForFileId(conn, bam->fileId);
    samfile_t *sf = samopen(fileName, "rb", NULL);
    if (sf == NULL)
	errnoAbort("Couldn't open %s.\n", fileName);
    bam_header_t *head = sf->header;
    if (head == NULL)
	errAbort("Aborting ... Bad BAM header in file: %s", fileName);

    /* Sum up some target sizes. */
    long long targetBaseCount = 0;   /* Total size of all bases in target seq */
    int i;
    for (i=0; i<head->n_targets; ++i)
	targetBaseCount  += head->target_len[i];

    safef(query, sizeof(query), 
	"update edwBamFile set targetBaseCount=%lld,targetSeqCount=%u where id=%u",
	targetBaseCount, (unsigned)head->n_targets, bam->id);
    sqlUpdate(conn, query);

    samclose(sf);
    freez(&fileName);
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFixTargetSeq(argv[1]);
return 0;
}
