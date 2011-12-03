/* ooCloneInfo - Merge info on clone size, phase, etc.. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hCommon.h"
#include "ooUtils.h"


struct clone
/* Info on one clone. */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Name (accession) */
    int version;        /* Version (of accession.version) */
    int phase;          /* HTG phase 0, 1, 2 (ordered), 3 (finished) */
    int approxSize;     /* Approximate size (includes some N's) */
    char *startFrag;    /* First frag in accession.fa file. */
    char *endFrag;      /* Last frag in accession.fa file. */
    int size;		/* Exact size. */
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooCloneInfo - Merge info on clone size, phase, etc.\n"
  "usage:\n"
  "   ooCloneInfo gsDir ooDir\n");
}

void readSeqInfo(char *fileName, struct clone **retList, struct hash **retHash)
/* Read NCBI sequence.inf file into list/hash. */
{
struct hash *hash = newHash(15);
struct clone *cloneList = NULL, *clone;
char *row[8], *cloneName, *ver;
struct lineFile *lf = lineFileOpen(fileName, TRUE);

while (lineFileRow(lf, row))
    {
    /* Parse out accession.version into two separate parts. */
    cloneName = row[0];
    ver = strchr(cloneName, '.');
    if (ver == NULL || !isdigit(ver[1]))
        errAbort("Expecting accession.version field 1 line %d of %s", 
		lf->lineIx, lf->fileName);
    *ver++ = 0;

    AllocVar(clone);
    slAddHead(&cloneList, clone);
    if (hashLookup(hash, cloneName))
	errAbort("Duplicate %s line %d of %s", cloneName, lf->lineIx, lf->fileName);
    hashAddSaveName(hash, cloneName, clone, &clone->name);
    clone->version = atoi(ver);
    clone->phase = lineFileNeedNum(lf, row, 3);
    if (clone->phase < 0 || clone->phase > 3)
        errAbort("Expecting number between 0 and 3 in field 4 line %d of %s",
	    lf->lineIx, lf->fileName);
    clone->approxSize = lineFileNeedNum(lf, row, 2);
    }
lineFileClose(&lf);
slReverse(&cloneList);
*retList = cloneList;
*retHash = hash;
}

struct clone *addSizeInfo(char *fileName, struct hash *hash)
/* Add in information from cloneSizes file.  This may involve
 * some "nt" contigs that are not already in hash.  Return
 * a list of these, and add to hash. */
{
struct clone *ntList = NULL, *clone;
char *row[4], *cloneName;
struct lineFile *lf = lineFileOpen(fileName, TRUE);

while (lineFileRow(lf, row))
    {
    cloneName = row[0];
    if (cloneName[0] == 'N' && cloneName[1] == 'T')
        {
	AllocVar(clone);
	slAddHead(&ntList, clone);
	if (hashLookup(hash, cloneName))
	    errAbort("Duplicate %s line %d of %s", cloneName, lf->lineIx, lf->fileName);
	hashAddSaveName(hash, cloneName, clone, &clone->name);
	clone->version = 0;
	clone->phase = 3;
	clone->approxSize = 0;
	}
    else
        {
	clone = hashFindVal(hash, cloneName);
	if (clone == NULL)
	    errAbort("Couldn't find %s in hash of sequence.inf", cloneName);
	}
    clone->size = lineFileNeedNum(lf, row, 1);
    clone->startFrag = cloneString(row[2]);
    clone->endFrag = cloneString(row[3]);
    }
lineFileClose(&lf);
slReverse(&ntList);
return ntList;
}

struct hash *cloneHash = NULL;
int contigCount = 0;
int emptyContigCount = 0;

void doContig(char *dir, char *chrom, char *contig)
/* Process geno.lst and create cloneInfo for a single contig. */
{
char fileName[512];
struct lineFile *lf;
FILE *f;
char *row[1], *line;
int lineSize;
char sDir[256], sFile[128], sExt[64];
char *faName;
struct clone *clone;

sprintf(fileName, "%s/geno.lst", dir);
lf = lineFileOpen(fileName, TRUE);
if (lineFileNext(lf, &line, &lineSize))	/* Check for empty. */
    {
    ++contigCount;
    lineFileReuse(lf);
    sprintf(fileName, "%s/cloneInfo", dir);
    f = mustOpen(fileName, "w");
    fprintf(f, "#name    \tversion\tsize\tphase\tstartFragment\tendFragment\tsequenceFile\n");
    while (lineFileRow(lf, row))
	{
	faName = row[0];
	splitPath(faName, sDir, sFile, sExt);
	clone = hashMustFindVal(cloneHash, sFile);
	fprintf(f, "%s\t.%d\t%d\t%d\t%s\t%s\t%s\n",
		clone->name, clone->version, clone->size, clone->phase,
		clone->startFrag, clone->endFrag, faName);
	if (ferror(f))
	    {
	    perror("");
	    errAbort("Write error on %s", fileName);
	    }
	}
    fclose(f);
    }
else
    ++emptyContigCount;
lineFileClose(&lf);
}

void ooCloneInfo(char *gsDir, char *ooDir)
/* ooCloneInfo - Merge info on clone size, phase, etc.. */
{
char fileName[512];
struct clone *cloneList = NULL, *ntCloneList, *clone;

sprintf(fileName, "%s/ffa/sequence.inf", gsDir);
readSeqInfo(fileName, &cloneList, &cloneHash);
printf("Read %d clones in %s\n", slCount(cloneList), fileName);

sprintf(fileName, "%s/cloneSizes", gsDir);
ntCloneList = addSizeInfo(fileName, cloneHash);
printf("Added %d nt clones from %s\n", slCount(ntCloneList), fileName);
cloneList = slCat(cloneList, ntCloneList);
ntCloneList = NULL;

ooToAllContigs(ooDir, doContig);
printf("Wrote cloneInfo file to %d contigs.  (%d contigs are empty)\n", 
    contigCount, emptyContigCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
ooCloneInfo(argv[1], argv[2]);
return 0;
}
