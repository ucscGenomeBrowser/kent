/* fixHarbisonMotifs - Trim motifs that have beginning or ending columns that are degenerate.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"
#include "transRegCode.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixHarbisonMotifs - Trim motifs that have beginning or ending columns that are degenerate.\n"
  "usage:\n"
  "   fixHarbisonMotifs database\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void formatProb(struct dyString *dy, char *name, float *prob, int count)
/* Print part of sql update statement for probability row into dy. */
{
int i;
dyStringPrintf(dy, "%s = '", name);
for (i=0; i<count; ++i)
    dyStringPrintf(dy, "%4.3f,", prob[i]);
dyStringAppend(dy, "', ");
}

void fixMotif(struct dnaMotif *motif,  int targetSize, char *motifTable, struct sqlConnection *conn)
/* Try and fix motif by trimming degenerate columns. */
{
struct dyString *dy = dyStringNew(0);

printf("Fixing %s in %s from:\n", motif->name, motifTable);
dnaMotifPrintProb(motif, uglyOut);
while (targetSize < motif->columnCount)
    {
    double startInfo = dnaMotifBitsOfInfo(motif, 0);
    double endInfo = dnaMotifBitsOfInfo(motif, motif->columnCount-1);
    motif->columnCount -= 1;
    if (startInfo < endInfo)
	{
	memcpy(motif->aProb, motif->aProb+1, sizeof(motif->aProb[0]) * motif->columnCount);
	memcpy(motif->cProb, motif->cProb+1, sizeof(motif->cProb[0]) * motif->columnCount);
	memcpy(motif->gProb, motif->gProb+1, sizeof(motif->gProb[0]) * motif->columnCount);
	memcpy(motif->tProb, motif->tProb+1, sizeof(motif->tProb[0]) * motif->columnCount);
	}
    }
printf("to:\n");
dnaMotifPrintProb(motif, uglyOut);

sqlDyStringPrintf(dy, "update %s set ", motifTable);
formatProb(dy, "aProb", motif->aProb, motif->columnCount);
formatProb(dy, "cProb", motif->cProb, motif->columnCount);
formatProb(dy, "gProb", motif->gProb, motif->columnCount);
formatProb(dy, "tProb", motif->tProb, motif->columnCount);
dyStringPrintf(dy, "columnCount=%d ", motif->columnCount);
dyStringPrintf(dy, "where name = '%s'", motif->name);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}


struct motifSize
/* Keep track of motif size. */
    {
    struct motifSize *next;
    char *name;		/* Allocated in hash */
    int minSize;	/* Minimum motif size observed. */
    int maxSize;	/* Maximum motif size observed. */
    struct dnaMotif *motif;  /* Associated motif. */
    };

void fixHarbisonMotifs(char *database)
/* fixHarbisonMotifs - Trim motifs that have beginning or ending columns that 
 * are degenerate.. */
{
char *motifTable = "transRegCodeMotif";
char *siteTable = "transRegCode";
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char query[512], **row;
struct motifSize *msList = NULL, *ms;
struct hash *msHash = newHash(16);
boolean anyMinNotMax = FALSE;
boolean anyMissingMotif = FALSE;
boolean anyMotifNotFound = FALSE;
struct dnaMotif *motif;

/* Stream through site table collecting data about motif sizes. */
sqlSafef(query, sizeof(query), 
	"select name,chromEnd-chromStart from %s", siteTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    int size = atoi(row[1]);
    ms = hashFindVal(msHash, name);
    if (ms == NULL)
        {
	AllocVar(ms);
	hashAddSaveName(msHash, name, ms, &ms->name);
	ms->minSize = ms->maxSize = size;
	slAddHead(&msList, ms);
	}
    else
        {
	if (size < ms->minSize)
	    ms->minSize = size;
	if (size > ms->maxSize)
	    ms->maxSize = size;
	}
    }
sqlFreeResult(&sr);

/* Go through and report if minSize != maxSize. */
for (ms = msList; ms != NULL; ms = ms->next)
    {
    if (ms->minSize != ms->maxSize)
        {
	anyMinNotMax = TRUE;
	warn("%s size inconsistent:  min %d, max %d", 
		ms->name, ms->minSize, ms->maxSize);
	}
    }
if (!anyMinNotMax)
    warn("All sizes agree in %s", siteTable);

/* Stream through motifs and add to msList. */
sqlSafef(query, sizeof(query), "select * from %s", motifTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    motif = dnaMotifLoad(row);
    ms = hashFindVal(msHash, motif->name);
    if (ms == NULL)
        {
	anyMissingMotif = TRUE;
	warn("Motif %s is in %s but not %s", 
		motif->name, motifTable, siteTable);
	}
    else
        {
	ms->motif = motif;
	}
    }
sqlFreeResult(&sr);
if (!anyMissingMotif)
    warn("All motifs in %s are also in %s", motifTable, siteTable);

/* Make sure that all items in msList have a motif. */
for (ms = msList; ms != NULL; ms = ms->next)
    {
    if (ms->motif == NULL)
        {
	anyMotifNotFound = TRUE;
	warn("Motif %s is in %s but not %s",
		ms->name, siteTable, motifTable);
	}
    }
if (!anyMotifNotFound)
    warn("All motifs in %s are also in %s", siteTable, motifTable);
    
/* Loop through table and deal with motifs that have different number
 * of columns in motif and site tables. */
for (ms = msList; ms != NULL; ms = ms->next)
    {
    motif = ms->motif;
    if (motif != NULL && ms->minSize == ms->maxSize)
        {
	if (motif->columnCount != ms->minSize)
	    {
	    warn("Motif %s has %d columns in %s but %d columns in %s",
	    	ms->name, ms->minSize, siteTable, 
		motif->columnCount, motifTable);
	    fixMotif(motif, ms->minSize, motifTable, conn);
	    }
	}
    }

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
fixHarbisonMotifs(argv[1]);
return 0;
}
