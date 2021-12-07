/* Get mRNA/EST meta data in ra format */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "raData.h"
#include "dystring.h"
#include "gbIndex.h"
#include "gbDefs.h"
#include "gbGenome.h"
#include "gbUpdate.h"
#include "gbProcessed.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include <stdio.h>

/* open output file */
static FILE* gOutFh = NULL;

/* buffer for current ra record.  We don't parse into a hash table for
 * speed. */
static struct dyString* gRaRecBuf = NULL;

static boolean readRaRecord(struct lineFile* inLf, char acc[GB_ACC_BUFSZ], 
                            short* versionPtr, char** recStr)
/* read the next ra record */
{
int startLineIx = inLf->lineIx;
short version = -1;
char* line;
acc[0] = '\0';
dyStringClear(gRaRecBuf);

while (lineFileNext(inLf, &line, NULL) && (strlen(line) > 0))
    {
    if (startsWith("acc ", line))
        safef(acc, GB_ACC_BUFSZ, "%s", line+4);
    else if (startsWith("ver ", line))
        version = gbParseInt(inLf, line+4);
    dyStringAppend(gRaRecBuf, line);
    dyStringAppendC(gRaRecBuf, '\n');
    }
if (gRaRecBuf->stringSize == 0)
    return FALSE;
dyStringAppendC(gRaRecBuf, '\n');

if (acc[0] == '\0')
    errAbort("%s:%d: no acc entry in ra record", inLf->fileName,  startLineIx);
if (version <= 0)
    errAbort("%s:%d: no ver entry in ra record", inLf->fileName,  startLineIx);
*recStr = gRaRecBuf->string;
*versionPtr = version;
return TRUE;
}

static void processRecord(struct gbSelect* select,  char acc[GB_ACC_BUFSZ], 
                          short version, char* rec)
/* process a ra from outputing the record */
{

struct gbEntry* entry = gbReleaseFindEntry(select->release, acc);
if ((entry != NULL) && (version == entry->selectVer)
    && !entry->clientFlags)
    {
    fputs(rec, gOutFh);
    entry->clientFlags = TRUE; /* flag so only gotten once */
    }

/* trace if enabled */
if (gbVerbose >= 3)
    {
    if (entry == NULL)
        gbVerbPr(3, "no entry: %s.%d", acc, version);
    else if (entry->selectVer <= 0)
        gbVerbPr(3, "not selected: %s.%d", acc, version);
    else if (version != entry->selectVer)
        gbVerbPr(3, "not version: %s.%d != %d", acc, version, entry->selectVer);
    else
        gbVerbPr(3, "save: %s.%d", acc, version);
    }
}

void raDataOpen(char *outFile)
/* open output file and set options */
{
gOutFh = gzMustOpen(outFile, "w");
gRaRecBuf = dyStringNew(4096);
}

void raDataProcessUpdate(struct gbSelect* select)
/* Get sequences for a partition and update.  Partition process and
 * aligned index should be loaded and selected versions flaged. */
{
char inRa[PATH_LEN];
struct lineFile* inLf;
char acc[GB_ACC_BUFSZ];
short version;
char* rec;

gbProcessedGetPath(select, "ra.gz", inRa);
inLf = gzLineFileOpen(inRa); 
while (readRaRecord(inLf, acc, &version, &rec))
    processRecord(select, acc, version, rec);
gzLineFileClose(&inLf);
}

void raDataClose()
/* close the output file */
{
gzClose(&gOutFh);
dyStringFree(&gRaRecBuf);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

