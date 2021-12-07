/* Get mRNA/EST orientation information */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "oiData.h"
#include "gbIndex.h"
#include "gbDefs.h"
#include "gbGenome.h"
#include "gbUpdate.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include "estOrientInfo.h"

/* open output file */
static FILE* gOutOi = NULL;

/* global options from command line */
static boolean gInclVersion;

static void flagUnaligned(struct gbSelect* select, unsigned orgCat)
/* mark unaligned entries so that they are not reported as missing */
{
struct gbAligned* aln;
for (aln = select->update->aligned; aln != NULL; aln = aln->updateLink)
    {
    if ((aln->entry->orgCat & orgCat) && (aln->numAligns == 0))
        aln->entry->clientFlags = TRUE;
    }
}

static void processOi(struct gbSelect* select, struct estOrientInfo* oi)
/* process the next OI from an update OI file, possibly outputing
 * the alignment record */
{
char acc[GB_ACC_BUFSZ];
short version = gbSplitAccVer(oi->name, acc);

/* will return NULL on ignored sequences */
struct gbEntry* entry = gbReleaseFindEntry(select->release, acc);
if ((entry != NULL) && (version == entry->selectVer))
    {
    /* selected */
    if (!gInclVersion)
        strcpy(oi->name, acc);  /* remove version */
    estOrientInfoTabOut(oi, gOutOi);
    entry->clientFlags = TRUE; /* flag so we know we got it */
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

static void processOrgCatOi(struct gbSelect* select, unsigned orgCat)
/* process files in an update an organism category.  OIs are only available
 * for native, however this follow the structure of the PSL code */
{
char inOi[PATH_LEN], *row[EST_ORIENT_INFO_NUM_COLS];
struct lineFile* inOiLf;
unsigned orgCatsHold = select->orgCats;
select->orgCats = orgCat;

gbAlignedGetPath(select, "oi.gz", NULL, inOi);

inOiLf = gzLineFileOpen(inOi);
while (lineFileNextRowTab(inOiLf, row, EST_ORIENT_INFO_NUM_COLS))
    {
    struct estOrientInfo* oi = estOrientInfoLoad(row);
    processOi(select, oi);
    estOrientInfoFree(&oi);
    }
gzLineFileClose(&inOiLf);
select->orgCats = orgCatsHold;
}

void oiDataOpen(boolean inclVersion, char *outFile)
/* open output file and set options. */
{
gInclVersion = inclVersion;
gOutOi = gzMustOpen(outFile, "w");
}

void oiDataProcessUpdate(struct gbSelect* select)
/* Get orientationInfo records for a partition and update.  Partition
 * processed and aligned indexes should be loaded and selected versions
 * flaged. */
{
assert(select->orgCats & GB_NATIVE);
if (select->update->numNativeAligns[gbTypeIdx(select->type)] > 0)
    processOrgCatOi(select, GB_NATIVE);
flagUnaligned(select, GB_NATIVE);
}

void oiDataClose()
/* close the output file */
{
gzClose(&gOutOi);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
