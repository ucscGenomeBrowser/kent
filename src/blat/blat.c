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
#include "trans3.h"


/* Variables that can be set from command line. */
int tileSize = 10;
int minMatch = 3;
int minBases = 20;
int maxGap = 2;
int repMatch = 1024;
boolean noHead = FALSE;
char *ooc = NULL;
enum gfType qType = gftDna;
enum gfType dType = gftDna;


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
  "   -d=type     Database type.  Type is one of:\n"
  "                 dna - DNA sequence\n"
  "                 prot - protein sequence\n"
  "                 dnax - DNA sequence translated in six frames to protein\n"
  "               The default is dna\n"
  "   -q=type     Query type.  Type is one of:\n"
  "                 dna - DNA sequence\n"
  "                 rna - RNA sequence\n"
  "                 prot - protein sequence\n"
  "                 dnax - DNA sequence translated in six frames to protein\n"
  "                 rnax - DNA sequence translated in three frames to protein\n"
  "               The default is dna\n"
  "   -prot       Synonymous with -d=prot -q=prot\n"
  );
}

enum gfType gftFromString(char *string)
/* Return blatType that corresponds to string. */
{
if (sameWord(string, "dna")) return gftDna;
else if (sameWord(string, "rna")) return gftRna;
else if (sameWord(string, "prot")) return gftProt;
else if (sameWord(string, "dnax")) return gftDnaX;
else if (sameWord(string, "rnax")) return gftRnaX;
else 
    errAbort("Unrecognized BLAT type '%s'", string);
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

bioSeq *getSeqList(int fileCount, char *files[], struct hash *hash, boolean isProt)
/* From an array of .fa and .nib file names, create a
 * list of dnaSeqs. */
{
int i;
char *fileName;
bioSeq *seqList = NULL, *seq;
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
	list = faReadAllSeq(fileName, !isProt);
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
printf("Loaded %lu letters in %d sequences\n", totalSize, count);
slReverse(&seqList);
return seqList;
}

void searchOneStrand(struct dnaSeq *seq, struct genoFind *gf, FILE *psl, boolean isRc)
/* Search for seq in index, align it, and write results to psl. */
{
struct gfClump *clumpList = gfFindClumps(gf, seq);
gfAlignDnaClumps(clumpList, seq, isRc, ffCdna, minBases, gfSavePsl, psl);
gfClumpFreeList(&clumpList);
}

void searchOneProt(aaSeq *seq, struct genoFind *gf, FILE *psl)
/* Search for protein seq in index and write results to psl. */
{
struct gfClump *clump, *clumpList = gfFindClumps(gf, seq);
gfAlignAaClumps(gf, clumpList, seq, FALSE, ffCdna, minBases, gfSavePsl, psl);
gfClumpFreeList(&clumpList);
}

void searchOne(bioSeq *seq, struct genoFind *gf, FILE *psl, boolean isProt)
/* Search for seq on either strand in index. */
{
uglyf("Searching for hits to %s\n", seq->name);
if (isProt)
    {
    searchOneProt(seq, gf, psl);
    }
else
    {
    searchOneStrand(seq, gf, psl, FALSE);
    reverseComplement(seq->dna, seq->size);
    searchOneStrand(seq, gf, psl, TRUE);
    reverseComplement(seq->dna, seq->size);
    }
}

void searchOneIndex(int fileCount, char *files[], struct genoFind *gf, char *pslOut, boolean isProt)
/* Search all sequences in all files against single genoFind index. */
{
int i;
char *fileName;
bioSeq *seqList = NULL, *targetSeq;
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

	if (isProt)
	    errAbort("%s: Can't use .nib files with -prot or d=prot option\n", fileName);
	nibOpenVerify(fileName, &f, &size);
	seq = nibLdPart(fileName, f, size, 0, size);
	freez(&seq->name);
	seq->name = cloneString(fileName);
	carefulClose(&f);
	searchOne(seq, gf, psl, isProt);
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
	    searchOne(&seq, gf, psl, isProt);
	    totalSize += seq.size;
	    count += 1;
	    }
	lineFileClose(&lf);
	}
    }
carefulClose(&psl);
printf("Searched %lu bases in %d sequences\n", totalSize, count);
}

struct trans3 *seqListToTrans3List(struct dnaSeq *seqList, aaSeq *transLists[3], struct hash **retHash)
/* Convert sequence list to a trans3 list and lists for each of three frames. */
{
int frame;
struct dnaSeq *seq;
struct trans3 *t3List = NULL, *t3;
struct hash *hash = newHash(0);

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    t3 = trans3New(seq);
    hashAddUnique(hash, t3->name, t3);
    slAddHead(&t3List, t3);
    for (frame = 0; frame < 3; ++frame)
        {
	slAddHead(&transLists[frame], t3->trans[frame]);
	}
    }
slReverse(&t3List);
for (frame = 0; frame < 3; ++frame)
    {
    slReverse(&transLists[frame]);
    }
*retHash = hash;
return t3List;
}

void tripleSearch(aaSeq *qSeq, struct genoFind *gfs[3], struct hash *t3Hash, boolean isRc, FILE *f)
/* Look for qSeq in indices for three frames.  Then do rest of alignment. */
{
struct gfClump *clumps[3];
int frame;
struct gfSavePslxData data;

data.f = f;
data.t3Hash = t3Hash;
uglyf("\nTriple search %s on %c strand\n", qSeq->name, (isRc ? '-' : '+'));
gfFindAlignAaTrans3Clumps(gfs, qSeq, t3Hash, isRc, ffCdna, minBases, gfSavePslx, &data);
}

void blatx(struct dnaSeq *untransList, int queryCount, char *queryFiles[], char *outFile)
/* Query is protein.  Run it against translated DNA database 
 * (3 frames on each strand). */
{
int frame, i;
struct dnaSeq *seq;
struct genoFind *gfs[3];
aaSeq *dbSeqLists[3];
struct trans3 *t3List = NULL, *t3;
int isRc;
FILE *pslOut = mustOpen(outFile, "w");
struct lineFile *lf = NULL;
struct hash *t3Hash = NULL;

uglyf("Blatx %d sequences in database, %d files in query\n", slCount(untransList), queryCount);

for (isRc = FALSE; isRc <= 1; ++isRc)
    {
    /* Initialize local pointer arrays to NULL to prevent surprises. */
    for (frame = 0; frame < 3; ++frame)
	{
	gfs[frame] = NULL;
	dbSeqLists[frame] = NULL;
	}

    t3List = seqListToTrans3List(untransList, dbSeqLists, &t3Hash);
    uglyf("Translated database.\n");
    for (frame = 0; frame < 3; ++frame)
	{
	gfs[frame] = gfIndexSeq(dbSeqLists[frame], minMatch, maxGap, tileSize, repMatch, ooc, TRUE);
	}
    uglyf("indexed database.\n");

    for (i=0; i<queryCount; ++i)
        {
	aaSeq qSeq;

	lf = lineFileOpen(queryFiles[i], TRUE);
	while (faPepSpeedReadNext(lf, &qSeq.dna, &qSeq.size, &qSeq.name))
	    {
	    tripleSearch(&qSeq, gfs, t3Hash, isRc, pslOut);
	    }
	}

    /* Clean up time. */
    trans3FreeList(&t3List);
    freeHash(&t3Hash);
    for (frame = 0; frame < 3; ++frame)
	{
	genoFindFree(&gfs[frame]);
	}

    for (seq = untransList; seq != NULL; seq = seq->next)
        {
	reverseComplement(seq->dna, seq->size);
	}
    }
carefulClose(&pslOut);
}

void blat(char *dbFile, char *queryFile, char *pslOut)
/* blat - Standalone BLAT fast sequence search command line tool. */
{
char **dbFiles, **queryFiles;
int dbCount, queryCount;
struct dnaSeq *dbSeqList, *seq;
struct hash *dbHash = newHash(16);
struct genoFind *gf;

boolean dbIsProt = (dType == gftProt);
getFileArray(dbFile, &dbFiles, &dbCount);
getFileArray(queryFile, &queryFiles, &queryCount);
dbSeqList = getSeqList(dbCount, dbFiles, dbHash, dbIsProt);

if ((dType == gftDna && (qType == gftDna || qType == gftRna))
 || (dType == gftProt && qType == gftProt))
    {
    gf = gfIndexSeq(dbSeqList, minMatch, maxGap, tileSize, repMatch, ooc, dbIsProt);
    searchOneIndex(queryCount, queryFiles, gf, pslOut, dbIsProt);
    }
else if (dType == gftDnaX && qType == gftProt)
    {
    blatx(dbSeqList, queryCount, queryFiles, pslOut);
    }
else
    {
    uglyAbort("Don't handle all translated types yet\n");
    }
}

int main(int argc, char *argv[])
/* Process command line into global variables and call blat. */
{
boolean cmpIsProt;	/* True if comparison takes place in protein space. */
boolean dIsProtLike, qIsProtLike;

cgiSpoof(&argc, argv);
if (argc != 4)
    usage();

/* Get database and query sequence types and make sure they are
 * legal and compatable. */
if (cgiVarExists("prot"))
    qType = dType = gftProt;
if (cgiVarExists("d"))
    dType = gftFromString(cgiString("d"));
switch (dType)
    {
    case gftProt:
    case gftDnaX:
        dIsProtLike = TRUE;
	break;
    case gftDna:
        dIsProtLike = FALSE;
	break;
    default:
        errAbort("Illegal value for 'd' parameter");
	break;
    }
if (cgiVarExists("q"))
    qType = gftFromString(cgiString("q"));
switch (qType)
    {
    case gftProt:
    case gftDnaX:
    case gftRnaX:
        qIsProtLike = TRUE;
	break;
    default:
        qIsProtLike = FALSE;
	break;
    }
if ((dIsProtLike ^ qIsProtLike) != 0)
    errAbort("d and q must both be either protein or dna");

/* Set default tile size for protein-based comparisons. */
if (dIsProtLike)
    {
    tileSize = 4;
    minMatch = 3;
    maxGap = 0;
    }

/* Get tile size and related parameters from user and make sure
 * they are within range. */
tileSize = cgiOptionalInt("tileSize", tileSize);
minMatch = cgiOptionalInt("minMatch", minMatch);
minBases = cgiOptionalInt("minBases", minBases);
maxGap = cgiOptionalInt("maxGap", maxGap);
if (dIsProtLike)
    {
    if (tileSize < 2 || tileSize > 6)
	errAbort("protein tileSize must be between 2 and 6");
    }
else
    {
    if (tileSize < 6 || tileSize > 15)
	errAbort("DNA tileSize must be between 6 and 15");
    }
if (minMatch < 0)
    errAbort("minMatch must be at least 1");
if (maxGap > 100)
    errAbort("maxGap must be less than 100");


/* Set repMatch parameter from command line, or
 * to reasonable value that depends on tile size.
 * ~~~ Need to add proper support for this for protein case. */
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

/* Gather last few command line options. */
noHead = (cgiVarExists("noHead") || cgiVarExists("nohead"));
ooc = cgiOptionalString("ooc");

/* Call routine that does the work. */
blat(argv[1], argv[2], argv[3]);
return 0;
}
