/* edwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distrubution of 
 * insert size. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distrubution of \n"
  "insert size.\n"
  "usage:\n"
  "   edwMakePairedEndQa startId endId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *oppositeEnd(char *end)
/* Return "1" for "2" and vice versa */
{
if (sameString(end, "1"))
    return "2";
else if (sameString(end, "2"))
    return "1";
else
    {
    errAbort("Expecting 1 or 2, got %s in oppositeEnd", end);
    return NULL;
    }
}

void pairedEndQa(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf)
/* Look for other end,  do a pairwise alignment, and save results in database. */
{
/* Get other end, return if not found. */
char *otherEnd = oppositeEnd(vf->pairedEnd);
char query[512];
sqlSafef(query, sizeof(query), 
    "select * from edwValidFile where experiment='%s' and outputType='%s' and replicate='%s' "
    "and technicalReplicate='%s' and pairedEnd='%s'"
    , vf->experiment, vf->outputType, vf->replicate, vf->technicalReplicate, otherEnd);
struct edwValidFile *otherVf = edwValidFileLoadByQuery(conn, query);
if (otherVf == NULL)
    return;
if (otherVf->next != NULL)
    errAbort("Multiple results from pairedEnd query %s", query);

/* Sort the two ends. */
struct edwValidFile *vf1, *vf2;
if (sameString(vf->pairedEnd, "1"))
    {
    vf1 = vf;
    vf2 = otherVf;
    }
else
    {
    vf1 = otherVf;
    vf2 = vf;
    }
uglyf("Got pair %s %s\n", vf1->licensePlate, vf2->licensePlate);

/* See if we already have a record for these two. */
sqlSafef(query, sizeof(query), 
    "select * from edwQaPairedEndFastq where fileId1=%u and fileId2=%u",
    vf1->fileId, vf2->fileId);
struct edwQaPairedEndFastq *pair = edwQaPairedEndFastqLoadByQuery(conn, query);
if (pair == NULL)
    {
    sqlSafef(query, sizeof(query),
        "insert into edwQaPairedEndFastq (fileId1,fileId2) values (%u,%u)"
	, vf1->fileId, vf2->fileId);
    sqlUpdate(conn, query);
    unsigned id = sqlLastAutoId(conn);
    uglyf("Making new dummy record %u for %u %u\n", id, vf1->fileId, vf2->fileId);
    }
else
    uglyf("Skipping existing record for %u %u\n", vf1->fileId, vf2->fileId);

/* Clean up and go home. */
edwValidFileFree(&otherVf);
}

void edwMakePairedEndQa(unsigned startId, unsigned endId)
/* edwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distrubution of 
 * insert size. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *ef, *efList = edwFileAllIntactBetween(conn, startId, endId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    if (vf != NULL)
	{
	if (sameString(vf->format, "fastq") && !isEmpty(vf->pairedEnd))
	    pairedEndQa(conn, ef, vf);
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
edwMakePairedEndQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
