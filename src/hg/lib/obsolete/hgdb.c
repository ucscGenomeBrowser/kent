                 OBSOLETE
/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* hgdb - Stuff to access human genome database (non-relational version). */
#include "common.h"
#include "hgdb.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fof.h"
#include "keys.h"

char *hgdbRootDir()
/* Return root directory of human genome database. */
{
static char *rootDir = NULL;
if (rootDir != NULL)
    return rootDir;
dnaUtilOpen();
if ((rootDir = getenv("JKHGDB")) == NULL)
    errAbort("You must setenv JKHGDB to point to the root of the database.");
return rootDir;
}

static struct fof *shortSeqFof = NULL;

static struct fof *getShortFof()
/* Open up fof for short sequences. */
{
if (shortSeqFof != NULL)
    return shortSeqFof;
shortSeqFof = fofOpen("smallDna.fof", hgdbRootDir());
return shortSeqFof;
}

struct fof *hgdbRnaFof()
/* Return index for RNA sequences. */
{
return getShortFof();
}

struct fof *hgdbShortFof()
/* Return index for short sequences. */
{
return getShortFof();
}

static struct dnaSeq *fastFofFa(struct fof *fof, char *accession)
/* Read an indexed FA quickly. */
{
int memSize;
char *memFa;
struct dnaSeq *seq;

/* Fetch FA from file into memory buffer.  Cannibilize
 * buffer with sequence (filtering out white space, etc.) */
memFa = fofFetchString(fof, accession, &memSize);
seq = faFromMemText(memFa);
return seq;
}

struct dnaSeq *hgdbRnaSeq(char *accession)
/* Return mRNA or EST sequence from an accession number. */
{
return hgdbShortSeq(accession);
}

struct dnaSeq *hgdbShortSeq(char *accession)
/* Return mRNA, EST, BACend or STS sequence based on
 * accession number. */
{
return fastFofFa(getShortFof(), accession);
}

static char *pathName(char *subDir, char *fileName, char *suffix, char path[512])
/* Return full path name in path var. */
{
sprintf(path, "%s%s/%s%s", hgdbRootDir(), subDir, fileName, suffix);
return path;
}

struct dnaSeq *hgdbFinishedSeq(char *accession)
/* Return finished BAC sequence. */
{
char path[512];
pathName("finished", accession, ".fa", path);
return faReadDna(path);
}

struct dnaSeq *hgdbUnfinishedSeq(char *accession)
/* Return unfinished BAC sequence. May be in 
 * several contigs (dnaSeq returned may be a list.) */
{
char path[512];
pathName("unfinished", accession, ".fa", path);
return faReadAllDna(path);
}

static boolean faExists(char *subDir, char *fileName)
/* Return TRUE if file exists. */
{
char path[512];
FILE *f;
if ((f = fopen(pathName(subDir, fileName, ".fa", path), "rb")) == NULL)
    return FALSE;
fclose(f);
return TRUE;
}

struct dnaSeq *hgdbGetSeq(char *accession)
/* Return sequence from any source. */
{
if (faExists("finished", accession))
    return hgdbFinishedSeq(accession);
else if (faExists("unfinished", accession))
    return hgdbUnfinishedSeq(accession);
else
    return hgdbShortSeq(accession);
}

static struct fof *keyFof = NULL;

static struct fof *getKeyFof()
/* Open up fof for short sequences. */
{
if (keyFof != NULL)
    return keyFof;
keyFof = fofOpen("raAcc.fof", hgdbRootDir());
return keyFof;
}

struct fof *hgdbKeyFof()
/* Return index for key-values indexed by accession. */
{
return getKeyFof();
}

char *hgdbKeyText(char *accession)
/* Get key-value lines about accession number. */
{
int size;
return fofFetchString(getKeyFof(), accession, &size);
}

boolean hgdbSmallKey(char *accession, char *key, char *valBuf, int valBufSize)
/* Get value of small key.  Returns FALSE if key doesn't exist. */
{
char *text = hgdbKeyText(accession);
boolean ok = keyTextScan(text, key, valBuf, valBufSize);
freeMem(text);
return ok;
}
