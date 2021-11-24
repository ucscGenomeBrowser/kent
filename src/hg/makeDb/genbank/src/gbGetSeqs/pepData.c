/* Get RefSeq peptides using ra and pep.fa files */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "hash.h"
#include "pepData.h"
#include "dystring.h"
#include "gbIndex.h"
#include "gbDefs.h"
#include "gbFa.h"
#include "gbGenome.h"
#include "gbUpdate.h"
#include "gbProcessed.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include "gbGetSeqs.h"
#include <stdio.h>

// FIXME: currently doesn't complain if peptide not found in table

/* open output file */
static struct gbFa* gOutFa = NULL;

/* global options from command line */
static boolean gInclVersion;

static boolean readRaInfo(struct lineFile* inLf, char acc[GB_ACC_BUFSZ], 
                          short* version, char pepAcc[GB_ACC_BUFSZ], short *pepVersion)
/* read the accession/version info from next ra record */
{
int startLineIx = inLf->lineIx;
acc[0] = '\0';
*version = -1;
pepAcc[0] = '\0';
*pepVersion = -1;
char* line;
boolean gotOne = FALSE;

while (lineFileNext(inLf, &line, NULL) && (strlen(line) > 0))
    {
    if (startsWith("acc ", line))
        safef(acc, GB_ACC_BUFSZ, "%s", line+4);
    else if (startsWith("ver ", line))
        *version = gbParseInt(inLf, line+4);
    else if (startsWith("prt ", line))
        *pepVersion = gbSplitAccVer(line+4, pepAcc);
    gotOne = TRUE;
    }
if (!gotOne)
    return FALSE;
if (acc[0] == '\0')
    errAbort("%s:%d: no acc entry in ra record", inLf->fileName,  startLineIx);
if (version <= 0)
    errAbort("%s:%d: no ver entry in ra record", inLf->fileName,  startLineIx);
if (gbIsProteinCodingRefSeq(acc) && (pepAcc[0] == '\0'))
    errAbort("%s:%d: no prt entry in ra record", inLf->fileName,  startLineIx);
return TRUE;
}

static boolean shouldRetrieve(struct gbEntry* entry, struct hash *idHash,
                            char acc[GB_ACC_BUFSZ], short version,
                            char pepAcc[GB_ACC_BUFSZ], short pepVersion)
/* determine if the peptide for an entry should be retrieived */
{
struct seqIdSelect *seqIdSelect;
if (idHash == NULL)
    return (version == entry->selectVer); // get latest
else if ((seqIdSelect = hashFindVal(idHash, acc)) != NULL)
    {
    // mRNA acc
    if (seqIdSelect->version > 0)
        return (seqIdSelect->version == version);  // get specified version
    else
        return (version == entry->selectVer); // get latest
    }
else if ((seqIdSelect = hashFindVal(idHash, pepAcc)) != NULL)
    {
    // pep acc
    if (seqIdSelect->version > 0)
        return (seqIdSelect->version == pepVersion);  // get specified version
    else
        return (version == entry->selectVer); // get latest
    }
else
    return FALSE;
}

static void processRaRecord(struct gbSelect* select,  struct hash *idHash,
                            char acc[GB_ACC_BUFSZ], short version,
                            char pepAcc[GB_ACC_BUFSZ], short pepVersion,
                            struct hash *pepIdTbl)
/* process a ra entry, recording peptide accVersion if it should be retrieved */
{

struct gbEntry* entry = gbReleaseFindEntry(select->release, acc);
if ((entry != NULL) && shouldRetrieve(entry, idHash, acc, version, pepAcc, pepVersion)
    && !entry->clientFlags)
    {
    char pepAccVer[GB_ACC_BUFSZ];
    safef(pepAccVer, sizeof(pepAccVer), "%s.%d", pepAcc, pepVersion);
    hashAddInt(pepIdTbl, pepAccVer, TRUE);
    entry->clientFlags = TRUE; /* flag so only gotten once */
    }

/* trace if enabled */
if (gbVerbose >= 3)
    {
    if (entry == NULL)
        gbVerbPr(3, "no entry: %s.%d %s.%d", acc, version, pepAcc, pepVersion);
    else if (entry->selectVer <= 0)
        gbVerbPr(3, "not selected: %s.%d  %s.%d", acc, version, pepAcc, pepVersion);
    else if (version != entry->selectVer)
        gbVerbPr(3, "not version: %s.%d or %s.%d != %d", acc, version, pepAcc, pepVersion, entry->selectVer);
    else
        gbVerbPr(3, "save: %s.%d", acc, version);
    }
}

static void processRa(struct gbSelect* select, struct hash *idHash, struct hash *pepIdTbl)
/* read a ra file for a partation and get table of peptides to extract */
{
char inRa[PATH_LEN];
struct lineFile* inLf;
char acc[GB_ACC_BUFSZ], pepAcc[GB_ACC_BUFSZ];
short version, pepVersion;

gbProcessedGetPath(select, "ra.gz", inRa);
inLf = gzLineFileOpen(inRa); 
while (readRaInfo(inLf, acc, &version, pepAcc, &pepVersion))
    {
    if (gbIsProteinCodingRefSeq(acc))
        processRaRecord(select, idHash, acc, version, pepAcc, pepVersion, pepIdTbl);
    }
gzLineFileClose(&inLf);
}

static void processSeq(struct gbFa* inFa, struct hash *pepIdTbl)
/* process the next sequence from an update fasta file, possibly outputing
 * the sequence */
{
if (hashLookup(pepIdTbl, inFa->id) != NULL)
    {
    char *hdr = NULL;
    if (!gInclVersion)
        {
        /* put version in comment */
        char acc[GB_ACC_BUFSZ], hdrBuf[GB_ACC_BUFSZ];
        short version = gbSplitAccVer(inFa->id, acc);
        safef(hdrBuf, sizeof(hdrBuf), "%s %d", acc, version);
        hdr = hdrBuf;
        }
    gbFaWriteFromFa(gOutFa, inFa, hdr);
    gbVerbPr(3, "save: %s", inFa->id);
    }
else
    gbVerbPr(3, "skip: %s", inFa->id);
}

static void processFa(struct gbSelect* select, struct hash *pepIdTbl)
/* read and output select peptides in specified file */
{
char inFasta[PATH_LEN];
struct gbFa* inFa;
gbProcessedGetDir(select, inFasta);
safecat(inFasta, sizeof(inFasta), "/pep.fa");
inFa = gbFaOpen(inFasta, "r"); 
while (gbFaReadNext(inFa))
    processSeq(inFa, pepIdTbl);
gbFaClose(&inFa);
}

void pepDataOpen(boolean inclVersion, char *outFile)
/* open output file and set options */
{
gInclVersion = inclVersion;
gOutFa = gbFaOpen(outFile, "w");
}

void pepDataProcessUpdate(struct gbSelect* select, struct hash *idHash)
/* Get sequences for a partition and update.  Partition process and
 * aligned index should be loaded and newest versions flaged.  These
 * will be extracted if idHash NULL. Otherwise idHash has hash of
 * ids and optional versions, for either RefSeq mRNA or peptides. */
{
struct hash *pepIdTbl = hashNew(22);
processRa(select, idHash, pepIdTbl);
processFa(select, pepIdTbl);
hashFree(&pepIdTbl);
}

void pepDataClose()
/* close the output file */
{
gbFaClose(&gOutFa);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

