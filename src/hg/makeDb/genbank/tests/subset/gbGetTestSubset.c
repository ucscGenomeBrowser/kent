/* gbGetTestSubset - generate a test subset of the genbank and refseq data */

#include "gbIndex.h"
#include "gbGenome.h"
#include "gbEntry.h"
#include "gbProcessed.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "common.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "errabort.h"
#include <stdio.h>


/* FIXME: need a way to get both native and xenos that are know to align */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"numMrnas", OPTION_INT},
    {"numEsts", OPTION_INT},
    {"accList", OPTION_STRING},
    {"selectAcc", OPTION_STRING},
    {NULL, 0}
};

static void usage()
/* Explain usage and exit */
{
errAbort(
    "gbGetTestSubset - Program to extract a subset of the download\n"
    "area for test purposes.  Requires running gbProcessStep for the\n"
    "full download area first. Creates an interesting subset of the files\n"
    "given the total number of entries to extract.\n"
    "\n"
    "   gbGetTestSubset [options] relName db organism outdir\n"
    "     relname - release name, in the form genbank.130.0 or refseq.130.0.\n"
    "     db - database, used for determining native vs genome\n"
    "     organism - name of organism\n"
    "     outdir - Directory where the download area is created."
    "Options:\n"
    "  -verbose=n - verbose output, values greater than 1 increase verbosity\n"
    "               (max is 2)\n"
    "  -numMrnas=10 - number of mRNAs to extract\n"
    "  -numEsts=10 - number of ESTs to extract\n"
    "  -accList=fname - Write selected accs to this file\n"
    "  -selectAcc=fname - Force these accessions to be gotten.  The number\n"
    "   specified number of mRNAs/ESTs will be in addition to these.\n"
    "  -organism='Homo sapiens' - native organism name\n"
    "  \n");
}

/* Flags to find entries */
#define FE_FULL  0x1   /* get from full */
#define FE_DAILY 0x2   /* get from daily */

struct numRange
/* structure used to specify a min/max range */
{
    int minNum;
    int maxNum;
};

struct slName* loadAccList(char *fname)
{
struct slName* accList = NULL;
struct lineFile* accFh;
char *line;
accFh = lineFileOpen(fname, TRUE);

while (lineFileNextReal(accFh, &line))
    {
    line = trimSpaces(line);
    slSafeAddHead(&accList, newSlName(line));
    }

lineFileClose(&accFh);
return accList;
}

boolean isInFull(struct gbEntry* entry)
/* determine if an entry in if the full update */
{
struct gbProcessed* processed;
for (processed = entry->processed; processed != NULL;
     processed = processed->next)
    {
    if (processed->update->isFull)
        return TRUE;
    }
return FALSE;
}

int getNumVersions(struct gbEntry* entry)
/* get the number of versions for an entry */
{
int cnt = 0;
struct gbProcessed* processed;
int prevVer = NULL_VERSION;
for (processed = entry->processed; processed != NULL;
     processed = processed->next)
    {
    if (processed->version != prevVer)
        cnt++;
    prevVer = processed->version;
    }
return cnt;
}

int getNumModDates(struct gbEntry* entry)
/* get the number of change moddates without version changes */
{
int cnt = 0;
struct gbProcessed* processed;
time_t prevModDate = NULL_DATE;
int prevVer = NULL_VERSION;
for (processed = entry->processed; processed != NULL;
     processed = processed->next)
    {
    if ((processed->modDate != prevModDate) && (processed->version == prevVer))
        cnt++;
    prevModDate = processed->modDate;
    prevVer = processed->version;
    }
return cnt;
}

boolean shouldSelect(unsigned type, struct numRange* versions, struct
                     numRange* modDates, unsigned flags, unsigned orgCats,
                     struct gbEntry* entry)
/* should the entry be selected */
{
if (entry->type != type)
    return FALSE;  /* wrong type */
if (entry->selectVer > 0)
    return FALSE;  /* already selected */
if (isInFull(entry))
    {
    if (!(flags & FE_FULL))
        return FALSE; /* not allowed in full */
    }
else
    {
    if (!(flags & FE_DAILY))
        return FALSE; /* not allowed in daily */
    }

if ((orgCats & GB_NATIVE) && (entry->orgCat != GB_NATIVE))
    return FALSE; /* not native */
if ((orgCats & GB_XENO) && (entry->orgCat != GB_XENO))
    return FALSE; /* not xeno */
if (versions != NULL)
    {
    int numVersions = getNumVersions(entry);
    if ((numVersions < versions->minNum) || (numVersions > versions->maxNum))
        return FALSE;
    }
if (modDates != NULL)
    {
    int numModDates = getNumModDates(entry);
    if ((numModDates < modDates->minNum) || (numModDates > modDates->maxNum))
        return FALSE;
    }
return TRUE;
}

void selectAcc(struct gbProcessed* processed, struct hash* accTbl,
               int* accCount)
/* select an access to extract from all updates it is contained in. */
{
struct gbEntry* entry = processed->entry;
hashAdd(accTbl, entry->acc, NULL);
if (accCount != NULL)
    (*accCount)++;

/* flag acc, and flag all updates that contain acc */
entry->selectVer = processed->version;
verbose(1, "  select:");
for (processed = entry->processed; processed != NULL;
     processed = processed->next)
    {
    processed->update->selectProc = TRUE;
    verbose(1, "  %s.%d (%s)", entry->acc, processed->version,
                processed->update->name);
    }
verbose(1, "\n");
}

void findInUpdate(int numAccs, unsigned type, struct gbRelease* release,
                  struct numRange* versions, struct numRange* modDates,
                  unsigned flags, unsigned orgCats,
                  struct gbUpdate* update, struct hash* accTbl, int* accCount)
/* find entries in an update to copy based on number of versions. */
{
struct gbProcessed* processed;
/* scan entries in an update */
for (processed = update->processed;
     (processed != NULL) && (*accCount < numAccs);
     processed = processed->updateLink)
    {
    if (shouldSelect(type, versions, modDates, flags, orgCats,
                     processed->entry))
        selectAcc(processed, accTbl, accCount);
    }
}

void findEntries(int numAccs, unsigned type, struct gbRelease* release,
                 struct numRange* versions, struct numRange* modDates,
                 unsigned flags, unsigned orgCats,
                 struct hash* accTbl, int* accCount)
/* find entries to copy based on number of versions and/or modDates.
 * Specify NULL to not use criteria */
{
/* scan by update to help minimize number of updates (by grouping) */
struct gbUpdate* update;
int localAccCount = 0;

if (verboseLevel() > 1)
    {
    fprintf(stderr, "findEntries: num=%d", numAccs);
    if (flags & FE_FULL)
        fprintf(stderr, " full");
    if (flags & FE_DAILY)
        fprintf(stderr, " daily");
    fprintf(stderr, " %s", gbFmtSelect(type|orgCats));
    if (versions != NULL)
        fprintf(stderr, " numVers=%d-%d", versions->minNum, versions->maxNum);
    if (modDates != NULL)
        fprintf(stderr, " numModDates=%d-%d", modDates->minNum, modDates->maxNum);
    fprintf(stderr, "\n");
    }

for (update = release->updates; (update != NULL) && (localAccCount < numAccs);
     update = update->next)
    {
    if ((update->isFull && (flags & FE_FULL))
        || (!update->isFull && (flags & FE_DAILY)))
        findInUpdate(numAccs, type, release, versions,  modDates,
                     flags, orgCats, update, accTbl, &localAccCount);
    }
(*accCount) += localAccCount;
verbose(1, "  found: %d entries\n", localAccCount);

}

void findAccs(int numAccs, unsigned type, struct gbRelease* release,
              struct hash* accTbl)
/* Get list of accessions to copy.  */ 
{
int accCount = 0;
int native3Versions, xeno3Versions, native2Versions, xeno2Versions;
int native2ModDates, xeno2ModDates, nativeNew, xenoNew, xenoRest;
struct numRange versions;
struct numRange modDates;

/* breakdown of types to extract is just hard coded for now */
native3Versions = (int)(numAccs*0.15);     /* 15% with 3 versions */
xeno3Versions = (int)(numAccs*0.05);       /* 5% with 3 versions */
native2Versions = (int)(numAccs*0.15);     /* 15% with 2 versions */
xeno2Versions = (int)(numAccs*0.05);       /* 5% with 2 versions */
native2ModDates = (int)(numAccs*0.10);     /* 10% with moddate change */
xeno2ModDates = (int)(numAccs*0.05);       /* 5% with moddate change */
nativeNew = (int)(numAccs*0.10);           /* 10% new */
xenoNew = (int)(numAccs*0.10);             /* 10% new */
xenoRest = (int)(numAccs*0.5);             /* 5% rest */

versions.minNum = 3;
versions.maxNum = 100;
findEntries(native3Versions, type, release, &versions, NULL,
            FE_FULL|FE_DAILY, GB_NATIVE,
            accTbl, &accCount);
findEntries(xeno3Versions, type, release, &versions, NULL,
            FE_FULL|FE_DAILY, GB_XENO,
            accTbl, &accCount);

versions.minNum = 2;
versions.maxNum = 2;
findEntries(native2Versions, type, release, &versions, NULL,
            FE_FULL|FE_DAILY, GB_NATIVE,
            accTbl, &accCount);
findEntries(xeno2Versions, type, release, &versions, NULL, 
            FE_FULL|FE_DAILY, GB_XENO,
            accTbl, &accCount);

modDates.minNum = 2;
modDates.maxNum = 100;
findEntries(native2ModDates, type, release, NULL, &modDates, 
            FE_FULL|FE_DAILY, GB_NATIVE,
            accTbl, &accCount);
findEntries(xeno2ModDates, type, release, NULL, &modDates, 
            FE_FULL|FE_DAILY, GB_XENO,
            accTbl, &accCount);

findEntries(nativeNew, type, release, NULL, NULL, 
            FE_DAILY, GB_NATIVE,
            accTbl, &accCount);
findEntries(xenoNew, type, release, NULL, NULL, 
            FE_DAILY, GB_XENO,
            accTbl, &accCount);

/* get the rest */
findEntries(xenoRest, type, release, NULL, NULL,
            FE_FULL|FE_DAILY, GB_NATIVE,
            accTbl, &accCount);
findEntries(numAccs-accCount, type, release, NULL, NULL, 
            FE_FULL|FE_DAILY, GB_XENO,
            accTbl, &accCount);
}

void getRequestedAccs(char* accFile, struct gbRelease* release,
                      struct hash* accTbl)
/* Mark for extraction the latest version of accs listed in a file */
{
struct slName* accList = loadAccList(accFile);
struct slName* acc;

for (acc = accList; acc != NULL; acc = acc->next)
    {
    struct gbEntry* entry = gbReleaseFindEntry(release, acc->name);
    if (entry != NULL)
        selectAcc(entry->processed, accTbl, NULL);
    }

slFreeList(&accList);
}

boolean readData(FILE *fh, char* fname, char* buf, int bufSize,
                 boolean mustGet)
/* read a line from the input file */
{
int len;
if (fgets(buf, bufSize, fh) == NULL)
    {
    if (ferror(fh))
        errnoAbort("reading %s", fname);
    if (mustGet)
        errAbort("unexpected EOF on %s", fname);
    return FALSE;
    }
len = strlen(buf);
if ((len > 0) && (buf[len-1] == '\n'))
    buf[len-1] = '\0';

return TRUE;
}

void extractAccFromGb(char *inName, char* outName, struct hash *accTbl)
/* Parse records of genBank file and print ones that match accession names.
 * (yanked from gbOneAcc, changed to use stdio so we can access compressed). */
{
enum {maxHeadLines=20, headLineSize=256 };
char *headLines[maxHeadLines];	/* Store stuff between locus and accession. */
char line[headLineSize];
FILE *inFh;
FILE *outFh = NULL;
int lineNum = 0;
int i;
char* acc;

verbose(1, "copying from %s\n", inName);

inFh = gzMustOpen(inName, "r");

for (i=0; i<maxHeadLines; ++i)
    headLines[i] = needMem(headLineSize);

while (TRUE)
    {
    boolean gotAcc = FALSE;
    boolean gotMyAcc = FALSE;
    int headLineCount = 0;
    /* Seek to LOCUS */
    for (;;)
	{
	if (!readData(inFh, inName, line, headLineSize, FALSE))
	    break;
        lineNum++;
	if (startsWith("LOCUS", line))
	    break;
	}
    if (feof(inFh))
        break;
    for (i=0; i<maxHeadLines; ++i)
	{
	++headLineCount;
	strcpy(headLines[i], line);
	readData(inFh, inName, line, headLineSize, TRUE);
        lineNum++;
	if (startsWith("ACCESSION", line))
	    {
	    gotAcc = TRUE;
	    break;
	    }
	}
    if (!gotAcc)
	errAbort("LOCUS without ACCESSION in %d lines at line %d of %s",
                 maxHeadLines, lineNum, inName);
    acc = lastWordInLine(line);
    gotMyAcc = (hashLookup(accTbl, acc) != NULL);
    if (gotMyAcc)
	{
        if (outFh == NULL)
            outFh = gbMustOpenOutput(outName);
	for (i=0; i<headLineCount; ++i)
	    {
	    fputs(headLines[i], outFh);
	    fputc('\n', outFh);
	    }
	fputs(line, outFh);
	fputc('\n', outFh);
	}
    for (;;)
	{
	readData(inFh, inName, line, headLineSize, TRUE);
        lineNum++;
	if (gotMyAcc)
	    {
	    fputs(line, outFh);
	    fputc('\n', outFh);
	    }
	if (startsWith("//", line))
	    break;
	}
    if ((outFh != NULL) && ferror(outFh))
        break;  /* write error */
    }
if (outFh != NULL)
    gbOutputRename(outName, &outFh);
gzClose(&inFh);
}


struct fileInfo* getGbFiles(struct gbUpdate* update, unsigned types)
/* generate list of genbank files for an update and type */
{
char relDir[PATH_LEN];
struct fileInfo* files = NULL; /* relative path, including directory */
char* updateDot = strchr(update->name, '.');

/* figure out path to input file */
strcpy(relDir, "download/");
strcat(relDir, update->release->name);
if (update->release->srcDb == GB_GENBANK)
    {
    /* genbank */
    if (update->isFull)
        {
        if (types & GB_MRNA)
            files = slCat(files, listDirX(relDir, "gbpri*.seq.gz", TRUE));
        if (types & GB_EST)
            files = slCat(files, listDirX(relDir, "gbest*.seq.gz", TRUE));
       }
    else
        {
        char dailyDir[PATH_LEN];
        char dailyFile[PATH_LEN];
        strcpy(dailyDir, relDir);
        strcat(dailyDir, "/daily-nc");
        strcpy(dailyFile, "nc");
        strcat(dailyFile, updateDot+1);
        strcat(dailyFile, ".flat.gz");
        files = slCat(files, listDirX(dailyDir, dailyFile, TRUE));
        }
    }
else
    {
    /* refseq */
    if (update->isFull)
        {
        char fullDir[PATH_LEN];
        strcpy(fullDir, relDir);
        strcat(fullDir, "/cumulative");
        files = slCat(files, listDirX(fullDir, "rscu.gbff.Z", TRUE));
        }
    else
        {
        /* need to reverse year and month-day parts in
         * rsnc.1231.2001.gbff.Z */
        char dailyDir[PATH_LEN];
        char dailyFile[PATH_LEN];
        int len;
        strcpy(dailyDir, relDir);
        strcat(dailyDir, "/daily");
        strcpy(dailyFile, "rsnc.");
        strcat(dailyFile, updateDot+6);
        len = strlen(dailyFile);
        strncpy(dailyFile+len, updateDot, 5);  /* include dot */
        dailyFile[len+5] = '\0';   /* strncpy didn't null term */
        strcat(dailyFile, ".gbff.Z");
        files = slCat(files, listDirX(dailyDir, dailyFile, TRUE));
        }
    }
if (files == NULL)
    errAbort("no input files found for release %s update %s",
             update->release->name, update->name);
return files;
}

char* parsePepGeneAcc(char* faHdr)
/* parse the gene acc from the header of a peptide fasta, return NULL
 * if not found. */
{
static char acc[GB_ACC_BUFSZ];
char *p1 = NULL, *p2 = NULL;
int len;

/* >gi|9629904|ref|NP_045938.1| (NC_001867) Pr gag */
p1 = strchr(faHdr, '(');
if (p1 != NULL)
    p2 = strchr(p1, ')');
if ((p1 == NULL) || (p2 == NULL))
    return NULL;
p1++;
len = p2-p1;
if (len > sizeof(acc)-1)
    len = sizeof(acc)-1;
strncpy(acc, p1, len);
return acc;
}

void copyRefSeqPepFa(struct gbUpdate* update,
                     char* outDir, char *gbFile)
/* copy a subset of the RefSeq peptide file for the select genes */
{
struct gbRelease* release = update->release;
char faInPath[PATH_LEN];
char faOutPath[PATH_LEN];
struct lineFile* inLf;
boolean copying = FALSE;
FILE* outFh;
char* line;

/* change the .gbff.Z suffix to .fsa.Z */
if (!endsWith(gbFile, ".gbff.Z"))
    errAbort("expected a file ending in .gbff.Z, got: %s", gbFile);
strcpy(faInPath, gbFile);
faInPath[strlen(faInPath)-7] = '\0';
strcat(faInPath, ".fsa.Z");

strcpy(faOutPath, outDir);
strcat(faOutPath, "/");
strcat(faOutPath, faInPath);

verbose(1, "copying from %s\n", faInPath);

/* copy selected, don't bother with fa readers */
inLf = gzLineFileOpen(faInPath);
outFh = gbMustOpenOutput(faOutPath);

while (lineFileNext(inLf, &line, NULL))
    {
    if (line[0] == '>')
        {
        char *geneAcc = parsePepGeneAcc(line);
        struct gbEntry* entry = NULL;
        if (geneAcc != NULL)
            entry = gbReleaseFindEntry(release, geneAcc);
        copying = ((entry != NULL) && (entry->selectVer > 0));
	verbose(2, "acc for pep: %s: %s\n", geneAcc,
		(copying ? "yes" : "no"));
        }
    if (copying)
        {
        fputs(line, outFh);
        fputc('\n', outFh);
        if (ferror(outFh))
            errnoAbort("write failed: %s: ", faOutPath);
        }
    }

gbOutputRename(faOutPath, &outFh);
gzLineFileClose(&inLf);
}

void copyUpdateAccs(struct gbUpdate* update, unsigned types,
                    struct hash *accTbl, char* outDir)
/* extract accessions for an update and type */
{
char outFile[PATH_LEN];
struct fileInfo* files = getGbFiles(update, types);
struct fileInfo* file;

for (file = files; file != NULL; file = file->next)
    {
    strcpy(outFile, outDir);
    strcat(outFile, "/");
    strcat(outFile, file->name);
    extractAccFromGb(file->name, outFile, accTbl);
    if (update->release->srcDb == GB_REFSEQ)
        copyRefSeqPepFa(update, outDir, file->name);
    }
}

void copyAccs(struct gbRelease* release, unsigned types, struct hash *accTbl,
              char* outDir)
/* copy select accessions for a release */
{
struct gbUpdate* update;
for (update = release->updates; update != NULL; update = update->next)
    if (update->selectProc)
        copyUpdateAccs(update, types, accTbl, outDir);
}

struct gbRelease* loadIndex(char* relName, unsigned types, char* database)
/* load processed section of index for release */
{
struct gbIndex* index = gbIndexNew(database, NULL);
struct gbSelect select;
ZeroVar(&select);
select.release = gbIndexMustFindRelease(index, relName);

if (types & GB_MRNA)
    {
    select.type = GB_MRNA;
    gbReleaseLoadProcessed(&select);
    }
if ((types & GB_EST) && (select.release->srcDb == GB_GENBANK))
    {
    struct slName* prefixes, *prefix;
    select.type = GB_EST;
    prefixes = gbReleaseGetAccPrefixes(select.release, GB_PROCESSED, GB_EST);
    for (prefix = prefixes; prefix != NULL; prefix = prefix->next)
        {
        select.accPrefix = prefix->name;
        gbReleaseLoadProcessed(&select);
        }
    select.accPrefix = NULL;
    slFreeList(&prefixes);
    }

return select.release;
}

void getTestSubset(int numMrnas, int numEsts, char *accList,
                   char* selectAccFile, char* relName, char* database,
                   char* outDir)
/* Get the test subset. */
{
struct gbRelease* release;
struct hash* accTbl = hashNew(20);
unsigned types = 0;

/* note, we find mRNAs and ESTs seperately, but extract in one pass, as
 * daily updates have both in a single file */
if (numMrnas > 0)
    types |= GB_MRNA;
if (numEsts > 0)
    types |= GB_EST;

release = loadIndex(relName, types, database);

if (selectAccFile != NULL)
    getRequestedAccs(selectAccFile, release, accTbl);

if (numMrnas > 0)
    findAccs(numMrnas, GB_MRNA, release, accTbl);
if (numEsts > 0)
    findAccs(numEsts, GB_EST, release, accTbl);

copyAccs(release, types, accTbl, outDir);

if (accList != NULL)
    {
    FILE *accListFh;
    struct hashCookie cookie;
    struct hashEl* hel;
    
    gbMakeFileDirs(accList);
    accListFh = mustOpen(accList, "w");
    cookie = hashFirst(accTbl);
    while ((hel = hashNext(&cookie)) != NULL)
        fprintf(accListFh, "%s\n", hel->name);
    carefulClose(&accListFh);
    }

hashFree(&accTbl);
gbIndexFree(&release->index);
}


int main(int argc, char* argv[])
/* parse command line */
{
int numMrnas;
int numEsts;
char *accList, *selectAccFile;
char *database, *relName, *outDir;

verboseSetLevel(0);
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
relName = argv[1];
database = argv[2];
outDir = argv[3];
numMrnas = optionInt("numMrnas", 10);
numEsts = optionInt("numEsts", 10);
accList = optionVal("accList",  NULL);
selectAccFile = optionVal("selectAcc",  NULL);
gbVerbInit(optionInt("verbose", 0));
if (verboseLevel() > 0)
    setlinebuf(stderr);

getTestSubset(numMrnas, numEsts, accList, selectAccFile,
              relName, database, outDir);

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

