/* blat - Standalone BLAT fast sequence search command line tool. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nib.h"
#include "psl.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "genoFind.h"

/* Variables that can be set from command line. */
int tileSize = 10;
int minMatch = 3;
int minBases = 20;
int maxGap = 2;
int repMatch = 1024;
boolean noHead = FALSE;
char *ooc = NULL;
boolean isProt = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blat - Standalone BLAT fast sequence search command line tool\n"
  "usage:\n"
  "   blat database query output.psl\n"
  "where:\n"
  "   database is either a .fa file, a .nib file, or a list of .fa or .nib files\n"
  "   query is similarly a .fa, .nib, or list of .fa or .nib files\n"
  "   output.psl is where to put the output.\n"
  "options:\n"
  "   -tileSize=N sets the size of perfectly matches.  Usually between 8 and 12\n"
  "               Default is 10\n"
  "   -minMatch=N sets the number of perfect tile matches.  Usually set from 2 to 4\n"
  "               Default is 3\n"
  "   -minBases=N sets minimum number of matching bases.  Default is 20\n"
  "   -maxGap=N   sets the size of maximum gap between tiles in a clump.  Usually set\n"
  "               from 0 to 3.  Default is 2.\n"
  "   -repMatch=N sets the number of repetitions of a tile allowed before\n"
  "               it is masked.  Typically this is 1024 for tileSize 12,\n"
  "               4096 for tile size 11, 16384 for tile size 10.\n"
  "               Default is 16384\n"
  "   -noHead     suppress .psl header (so it's just a tab-separated file)\n"
  "   -ooc=N.ooc  Use overused tile file N.ooc\n"
  "   -prot       Query sequence is protein\n"
  );
}

boolean isNib(char *fileName)
/* Return TRUE if file is a nib file. */
{
return endsWith(fileName, ".nib") || endsWith(fileName, ".NIB");
}

void getFileArray(char *fileName, char ***retFiles, int *retFileCount)
/* Check if file if .fa or .nib.  If so return just that
 * file in a list of one.  Otherwise read all file and treat file
 * as a list of filenames.  */
{
boolean gotSingle = FALSE;
char *buf;		/* This will leak memory but won't matter. */

/* Detect nib files by suffix since that is standard. */
if (isNib(fileName))
    gotSingle = TRUE;
/* Detect .fa files (where suffix is not standardized)
 * by first character being a '>'. */
else
    {
    FILE *f = mustOpen(fileName, "r");
    char c = fgetc(f);
    fclose(f);
    if (c == '>')
        gotSingle = TRUE;
    }
if (gotSingle)
    {
    char **files;
    *retFiles = AllocArray(files, 1);
    files[0] = cloneString(fileName);
    *retFileCount = 1;
    return;
    }
else
    {
    readAllWords(fileName, retFiles, retFileCount, &buf);
    }
}

struct dnaSeq *getSeqList(int fileCount, char *files[], struct hash *hash)
/* From an array of .fa and .nib file names, create a
 * list of dnaSeqs. */
{
int i;
char *fileName;
struct dnaSeq *seqList = NULL, *seq;
int count = 0; 
unsigned long totalSize = 0;

for (i=0; i<fileCount; ++i)
    {
    fileName = files[i];
    if (isNib(fileName))
        {
	FILE *f;
	int size;

	nibOpenVerify(fileName, &f, &size);
	seq = nibLdPart(fileName, f, size, 0, size);
	seq->name = fileName;
	carefulClose(&f);
	slAddHead(&seqList, seq);
	hashAddUnique(hash, seq->name, seq);
	totalSize += size;
	count += 1;
	}
    else
        {
	struct dnaSeq *list, *next;
	list = faReadAllDna(fileName);
	for (seq = list; seq != NULL; seq = next)
	    {
	    next = seq->next;
	    slAddHead(&seqList, seq);
	    hashAddUnique(hash, seq->name, seq);
	    totalSize += seq->size;
	    count += 1;
	    }
	}
    }
printf("Indexed %lu bases in %d sequences\n", totalSize, count);
slReverse(&seqList);
return seqList;
}

void searchOneStrand(struct dnaSeq *seq, struct genoFind *gf, FILE *psl, boolean isRc)
/* Search for seq in index, align it, and write results to psl. */
{
struct gfClump *clumpList = gfFindClumps(gf, seq);
gfAlignSeqClumps(clumpList, seq, isRc, ffCdna, minBases, gfSavePsl, psl);
gfClumpFreeList(&clumpList);
}

#ifdef SOON
void searchOneProtStrand(struct dnaSeq *seq, struct genoFind *gf, FILE *psl)
/* Search for protein seq in index and write results to psl. */
{
struct gfClump *clump, *clumpList = gfPepFindClumps(gf, seq);
uglyf("Found %d clumps in %s\n", slCount(clumpList), seq->name);
for (clump = clumpList; clump != NULL; clump = clump->next)
    gfClumpDump(gf, clump, uglyOut);
}
#endif

void searchOne(struct dnaSeq *seq, struct genoFind *gf, FILE *psl)
/* Search for seq on either strand in index. */
{
#ifdef SOON
if (isProt)
    {
    searchOneProtStrand(seq, gf, psl);
    }
else
#endif
    {
    searchOneStrand(seq, gf, psl, FALSE);
    reverseComplement(seq->dna, seq->size);
    searchOneStrand(seq, gf, psl, TRUE);
    reverseComplement(seq->dna, seq->size);
    }
}

void searchIndex(int fileCount, char *files[], struct genoFind *gf, char *pslOut)
/* Search all sequences in all files against genoFind index. */
{
int i;
char *fileName;
struct dnaSeq *seqList = NULL, *targetSeq;
int count = 0; 
unsigned long totalSize = 0;
FILE *psl = mustOpen(pslOut, "w");

if (!noHead)
    pslWriteHead(psl);
for (i=0; i<fileCount; ++i)
    {
    fileName = files[i];
    if (isNib(fileName))
        {
	FILE *f;
	int size;
	struct dnaSeq *seq;

	nibOpenVerify(fileName, &f, &size);
	seq = nibLdPart(fileName, f, size, 0, size);
	freez(&seq->name);
	seq->name = cloneString(fileName);
	carefulClose(&f);
	searchOne(seq, gf, psl);
	freeDnaSeq(&seq);
	totalSize += size;
	count += 1;
	}
    else
        {
	static struct dnaSeq seq;
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	while (faSomeSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name, !isProt))
	    {
	    searchOne(&seq, gf, psl);
	    totalSize += seq.size;
	    count += 1;
	    }
	lineFileClose(&lf);
	}
    }
carefulClose(&psl);
printf("Searched %lu bases in %d sequences\n", totalSize, count);
}


void blat(char *dbFile, char *queryFile, char *pslOut)
/* blat - Standalone BLAT fast sequence search command line tool. */
{
char **dbFiles, **queryFiles;
int dbCount, queryCount;
struct dnaSeq *dbSeqList, *seq;
struct hash *dbHash = newHash(16);
struct genoFind *gf;

getFileArray(dbFile, &dbFiles, &dbCount);
getFileArray(queryFile, &queryFiles, &queryCount);
dbSeqList = getSeqList(dbCount, dbFiles, dbHash);
#ifdef SOON
if (isProt)
    gf = gfPepIndexNibs(dbCount, dbFiles, 3, 0, 4, 4096);
else
#endif
    gf = gfIndexSeq(dbSeqList, minMatch, maxGap, tileSize, repMatch, ooc, FALSE);
searchIndex(queryCount, queryFiles, gf, pslOut);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
tileSize = cgiOptionalInt("tileSize", tileSize);
if (tileSize < 6 || tileSize > 15)
    errAbort("tileSize must be between 6 and 15");
minMatch = cgiOptionalInt("minMatch", minMatch);
if (minMatch < 0)
    errAbort("minMatch must be at least 1");
minBases = cgiOptionalInt("minBases", minBases);
maxGap = cgiOptionalInt("maxGap", maxGap);
if (cgiVarExists("prot"))
    isProt = TRUE;
if (maxGap > 100)
    errAbort("maxGap must be less than 100");
if (cgiVarExists("repMatch"))
    repMatch = cgiInt("repMatch");
else
    {
    if (tileSize == 15)
        repMatch = 16;
    else if (tileSize == 14)
        repMatch = 64;
    else if (tileSize == 13)
        repMatch = 256;
    else if (tileSize == 12)
        repMatch = 1024;
    else if (tileSize == 11)
        repMatch = 4*1024;
    else if (tileSize == 10)
        repMatch = 16*1024;
    else if (tileSize == 9)
        repMatch = 64*1024;
    else if (tileSize == 8)
        repMatch = 256*1024;
    else if (tileSize == 7)
        repMatch = 1024*1024;
    else if (tileSize == 6)
        repMatch = 4*1024*1024;
    }
noHead = (cgiVarExists("noHead") || cgiVarExists("nohead"));
ooc = cgiOptionalString("ooc");

blat(argv[1], argv[2], argv[3]);
return 0;
}
