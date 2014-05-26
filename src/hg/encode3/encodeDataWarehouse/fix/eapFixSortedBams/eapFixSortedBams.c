/* eapFixSortedBams - Help fix early eap run that left bams sorted by read rather than by chromosome.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "bamFile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapFixSortedBams - Help fix early eap run that left bams sorted by read rather than by chromosome.\n"
  "usage:\n"
  "   eapFixSortedBams outSql\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

int badSubmit = 98, goodSubmit=100;

boolean bamIsSortedByTarget(char *fileName, int maxToCheck)
/* Return TRUE if bam is sorted by target for at least the first bits. */
{
int leftToCheck = maxToCheck;
struct hash *targetHash = hashNew(0);
boolean result = TRUE;

/* Open bam/sam file and set up basic I/O vars on it. */
samfile_t *sf = samopen(fileName, "rb", NULL);
bam_header_t *bamHeader = sf->header;
bam1_t one;
ZeroVar(&one);
int err;

char lastTarget[PATH_LEN] = "";
int lastPos = 0;

/* Loop through while still haven't hit our max and file still has data */
while ((err = bam_read1(sf->x.bam, &one)) >= 0)
    {
    if (--leftToCheck < 0)
        {
	break;
	}
    /* Get target,  skipping read if it's not aligned well enough to have a target. */
    int32_t tid = one.core.tid;
    if (tid < 0)
        continue;
    char *target = bamHeader->target_name[tid];

    int pos = one.core.pos;

    /* If we are on same target then make sure we are in ascending order. */
    if (sameString(target, lastTarget))
        {
	if (pos < lastPos)
	    {
	    result = FALSE;
	    break;
	    }
	}
    else
	{
	/* If sorted should not go back to a new chromosome. Use hash to check this */
	if (hashLookup(targetHash, target))
	    {
	    result = FALSE;
	    break;
	    }
	hashAdd(targetHash, target, NULL);
	safef(lastTarget, sizeof(lastTarget), "%s", target);
	}
    lastPos = pos;
    }
hashFree(&targetHash);
return result;
}


struct edwFile *getFilesFromIds(struct sqlConnection *conn, uint ids[], int idCount)
/* Get a list of files specified in an array of ids. */
{
struct edwFile *list = NULL, *el;
int i;
for (i=0; i<idCount; ++i)
    {
    el = edwFileFromId(conn, ids[i]);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

unsigned findNewId(struct sqlConnection *conn, struct edwAnalysisRun *oldRun, unsigned oldId)
/* Given old ID, find corresponding new one. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from edwAnalysisRun where firstInputId = %u and analysisStep = '%s'"
    " and id != %u "
    "order by id desc"
    , oldRun->firstInputId, oldRun->analysisStep, oldRun->id);
struct edwAnalysisRun *newRun = edwAnalysisRunLoadByQuery(conn, query);
if (newRun == NULL)
    errAbort("NULL result from %s\n", query);
if (newRun->createStatus <= 0)
     return 0;
if (isEmpty(newRun->createFileIds))
    errAbort("Strange no createFileIds result from %s", query);
return newRun->createFileIds[0];
}

void hookupFix(struct sqlConnection *conn, struct edwAnalysisRun *oldRun,
    struct edwFile *oldFile, FILE *f)
/* Write out sql to f to show oldFile is replaced with a better new file. */
{
unsigned newId = findNewId(conn, oldRun, oldFile->id);
if (newId == 0)
    {
    warn("Couldn't find replacement for %u", oldFile->id);
    return;
    }
fprintf(f, 
    "update edwFile set deprecated = 'BAM file not sorted by target', replacedBy=%u where id=%u;\n",
	newId, oldFile->id);
}

void eapFixSortedBams(char *outSql)
/* eapFixSortedBams - Help fix early eap run that left bams sorted by read rather than by chromosome.. */
{
FILE *f = mustOpen(outSql, "w");
struct sqlConnection *conn = edwConnect();
char query[512];
sqlSafef(query, sizeof(query), "select * from edwAnalysisRun");
struct edwAnalysisRun *run, *runList = edwAnalysisRunLoadByQuery(conn, query);
for (run = runList; run != NULL; run = run->next)
    {
    if (run->createStatus > 0 && run->createCount == 1)
	{
	struct edwFile *create = getFilesFromIds(conn, run->createFileIds, run->createCount);
	if (create->submitId == badSubmit)
	    {
	    char *fileName = edwPathForFileId(conn, create->id);
	    if (!bamIsSortedByTarget(fileName, 1000))
		{
		hookupFix(conn, run, create, f);
		}
	    else
		errAbort("Looks like bad %s is already sorted\n", fileName);
	    }
	else if (create->submitId == goodSubmit)
	    {
	    char *fileName = edwPathForFileId(conn, create->id);
	    if (!bamIsSortedByTarget(fileName, 1000))
		{
		errAbort("Looks like good %s needs sorting\n", fileName);
		}
	    }
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
eapFixSortedBams(argv[1]);
return 0;
}
