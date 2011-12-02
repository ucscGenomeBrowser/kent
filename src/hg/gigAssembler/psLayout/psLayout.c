/* psLayout - generate cDNA alignments using SuperStitcher, PatSpace and fuzzyFinder. 
 * Output these in format suitable for layout (contig ordering and orienting)
 * programs. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "portable.h"
#include "errabort.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "cda.h"
#include "patSpace.h"
#include "supStitch.h"
#include "repMask.h"
#include "cheapcgi.h"
#include "psl.h"


/* Variables that can be set by command line options. */
int minMatch = 4;
int maxBad = 100;
int minBases = 25;

/* Stuff to help print meaningful error messages when on a
 * compute cluster. */
char *hostName = NULL;
char *aliSeqName = NULL;
boolean veryTight = FALSE;
boolean avoidSelfSelf = FALSE;

void warnHandler(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) 
    {
    if (hostName != NULL || aliSeqName != NULL)
	{
	fprintf(stderr, "\n[");
	if (hostName != NULL)
	    fprintf(stderr, "host %s ", hostName);
	if (aliSeqName != NULL)
	    fprintf(stderr, "seq %s", aliSeqName);
	fprintf(stderr, "] ");
	}
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    }
}


void usage()
/* Print usage and exit. */
{
errAbort("psLayout - generate alignments for contig layout programs.\n"
         "usage:\n"
         "    psLayout genomeListFile otherListFile type ignore.ooc outputFile [noHead]\n"
	 "The individual parameters are:\n"
	 "    genomeListFile - Either a .fa file or a a text file containing a list of .fa\n"
	 "        files.  These files are generally genomic.  This may contain up to 64 million\n"
	 "        bases total on a machine with enough RAM. (On 256 Meg machines it's best to\n"
	 "        limit it to 30 million bases. The program expects a .fa.out file (from \n"
	 "        RepeatMasker) to exist for each .fa file in this parameter.\n"
	 "    otherListFile - A .fa file or a list of .fa files that may be\n"
	 "        genomic or mRNA. These files may be of unlimited size, but the largest\n"
	 "        sequence in them should be no more than about 3 megabases.\n"
	 "    type - either the word 'mRNA' 'genomic' 'g2g' or 'asm'.\n"
	 "        mRNA - allows introns\n"
	 "        genomic - no introns. Finds matches of about 94%% or better\n"
	 "        g2g - no introns. Finds matches of about 98%% or better\n"
	 "        asm - no introns. Matches 98%% or better. Skips self alignments\n"
	 "    ignore.ooc - A list of overused tiles, typically 10.ooc. Can be 'none'.\n"
	 "    outputFile - Where to put the alignments\n"
	 "    noHead - if the word 'noHead' is present, this will suppress the header\n"
	 "             so that the output is a simple tab-delimited file\n"
	 "Options:\n"
	 "    -minMatch=N  Minimum number of tiles that must match.  Default 4\n"
	 "    -maxBad=N    Mismatch/inserts allowed in parts per thousand. Default 100\n"
	 "    -minBases=N  Minumum number of bases that must match. Default 25\n"
	 "                 (Repeat bases are worth 1/4 of a unique base in this parameter)\n");
}

struct repeatTracker
/* This keeps track of repeats, and is the value of the repeatHash. */
    {
    struct repeatTracker *next;
    struct dnaSeq *seq;
    UBYTE *repBytes;
    };

void repeatTrackerFree(struct repeatTracker **pRt)
/* Free a repeatTracker. */
{
struct repeatTracker *rt;
if ((rt = *pRt) == NULL)
    return;
freeMem(rt->repBytes);
freez(pRt);
}

void repeatTrackerFreeList(struct repeatTracker **pList)
/* Free a list of repeatTrackers. */
{
struct repeatTracker *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    repeatTrackerFree(&el);
    }
*pList = NULL;
}

int calcMilliBad(int qAliSize, int tAliSize, 
	int qNumInserts, int tNumInserts,
	int match, int repMatch, int misMatch, boolean isMrna)
/* Express mismatches and other problems in parts per thousand. */
{
int aliSize;
int milliBad;
int sizeDif;
int insertFactor;

aliSize = min(qAliSize, tAliSize);
sizeDif = qAliSize - tAliSize;
if (sizeDif < 0)
    {
    if (isMrna)
	sizeDif = 0;
    else
	sizeDif = -sizeDif;
    }
insertFactor = qNumInserts;
if (!isMrna)
    insertFactor += tNumInserts;

milliBad = (1000 * (misMatch + insertFactor + sizeDif)) / (match + repMatch);
return milliBad;
}


void oneAli(struct ffAli *left, struct dnaSeq *otherSeq, 
	struct repeatTracker *rt, boolean isRc, enum ffStringency stringency, FILE *out)
/* Analyse one alignment and if it looks good enough write it out to file. */
{
struct dnaSeq *genoSeq = rt->seq;
UBYTE *repBytes = rt->repBytes;
struct ffAli *ff, *nextFf;
struct ffAli *right = ffRightmost(left);
DNA *needle = otherSeq->dna;
DNA *hay = genoSeq->dna;
int nStart = left->nStart - needle;
int nEnd = right->nEnd - needle;
int hStart = left->hStart - hay;
int hEnd = right->hEnd - hay;
int nSize = nEnd - nStart;
int hSize = hEnd - hStart;
int nInsertBaseCount = 0;
int nInsertCount = 0;
int hInsertBaseCount = 0;
int hInsertCount = 0;
int matchCount = 0;
int mismatchCount = 0;
int repMatch = 0;
int countNs = 0;
DNA *np, *hp, n, h;
int blockSize;
int i;
int badScore;
int milliBad;
int passIt;

/* Count up matches, mismatches, inserts, etc. */
for (ff = left; ff != NULL; ff = nextFf)
    {
    int hStart;
    nextFf = ff->right;
    blockSize = ff->nEnd - ff->nStart;
    np = ff->nStart;
    hp = ff->hStart;
    hStart = hp - hay;
    for (i=0; i<blockSize; ++i)
	{
	n = np[i];
	h = hp[i];
	if (n == 'n' || h == 'n')
	    ++countNs;
	else
	    {
	    if (n == h)
		{
		if (repBytes[i+hStart])
		    ++repMatch;
		else
		    ++matchCount;
		}
	    else
		++mismatchCount;
	    }
	}
    if (nextFf != NULL)
	{
	if (ff->nEnd != nextFf->nStart)
	    {
	    ++nInsertCount;
	    nInsertBaseCount += nextFf->nStart - ff->nEnd;
	    }
	if (ff->hEnd != nextFf->hStart)
	    {
	    ++hInsertCount;
	    hInsertBaseCount += nextFf->hStart - ff->hEnd;
	    }
	}
    }

/* See if it looks good enough to output. */
milliBad = calcMilliBad(nEnd - nStart, hEnd - hStart, nInsertCount, hInsertCount, 
	matchCount, repMatch, mismatchCount, stringency == ffCdna);
if (veryTight)
    {
    passIt = (milliBad < 60 && 
	(matchCount >= 25 || 
	 (matchCount >= 15 && matchCount + repMatch >= 50) ||
	 (matchCount >= 5 && repMatch >= 100 && milliBad < 50)));
    }
else
    {
    passIt = (milliBad < maxBad && 
	(matchCount >= minBases || 
	 (matchCount >= minBases/2 && matchCount + repMatch >= 2*minBases) ||
	 (repMatch >= 4*minBases && milliBad < (maxBad/2))));
    }
if (passIt)
    {
    if (isRc)
	{
	int temp;
	int oSize = otherSeq->size;
	temp = nStart;
	nStart = oSize - nEnd;
	nEnd = oSize - temp;
	}
    fprintf(out, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
                 "%c\t"
		 "%s\t%d\t%d\t%d\t"
		 "%s\t%d\t%d\t%d\t%d\t",
	matchCount, mismatchCount, repMatch, countNs, nInsertCount, nInsertBaseCount, hInsertCount, hInsertBaseCount,
	(isRc ? '-' : '+'),
	otherSeq->name, otherSeq->size, nStart, nEnd,
	genoSeq->name, genoSeq->size, hStart, hEnd,
	ffAliCount(left));
    for (ff = left; ff != NULL; ff = ff->right)
	fprintf(out, "%d,", ff->nEnd - ff->nStart);
    fprintf(out, "\t");
    for (ff = left; ff != NULL; ff = ff->right)
	fprintf(out, "%d,", ff->nStart - needle);
    fprintf(out, "\t");
    for (ff = left; ff != NULL; ff = ff->right)
	fprintf(out, "%d,", ff->hStart - hay);
    fprintf(out, "\n");
    if (ferror(out))
	{
	perror("");
	errAbort("Write error to .psl");
	}
    }
}

void oneStrand(struct patSpace *ps, struct hash *repeatHash, struct dnaSeq *otherSeq,
    boolean isRc,  enum ffStringency stringency, FILE *out)
/* Search one strand of other sequence. */
{
struct ssBundle *bundleList, *bun;

bundleList = ssFindBundles(ps, otherSeq, otherSeq->name, stringency, avoidSelfSelf);
for (bun = bundleList; bun != NULL; bun = bun->next)
    {
    struct ssFfItem *ffi;
    struct dnaSeq *genoSeq = bun->genoSeq;
    struct repeatTracker *rt = hashLookup(repeatHash, genoSeq->name)->val;
    for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
	{
	struct ffAli *ff = ffi->ff;
	oneAli(ff, otherSeq, rt, isRc, stringency, out);
	}
    }
ssBundleFreeList(&bundleList);
}

void storeMasked(struct hash *repeatHash, char *faName)
/* Look for repeat masked file corresponding to faName. */
{
char fileName[512];
struct lineFile *lf;
char *line;
int lineSize;
struct repeatMaskOut rmo;
struct hashEl *hel;
struct repeatTracker *rt;
boolean ok;

sprintf(fileName, "%s.out", faName);
if (!fileExists(fileName))
    {
    warn("No mask file %s\n", fileName);
    return;
    }
lf = lineFileOpen(fileName, TRUE);
ok = lineFileNext(lf, &line, &lineSize);
if (!ok)
    warn("Empty mask file %s\n", fileName);
else 
    {
    if (!startsWith("There were no", line))
	{
	if (!startsWith("   SW", line))
	    errAbort("%s isn't a RepeatMasker .out file.", fileName);
	if (!lineFileNext(lf, &line, &lineSize) || !startsWith("score", line))
	    errAbort("%s isn't a RepeatMasker .out file.", fileName);
	lineFileNext(lf, &line, &lineSize);  /* Blank line. */
	while (lineFileNext(lf, &line, &lineSize))
	    {
	    char *words[32];
	    int wordCount;
	    int seqSize;
	    int repSize;
	    wordCount = chopLine(line, words);
	    if (wordCount < 14)
		errAbort("%s line %d - error in repeat mask .out file\n", fileName, lf->lineIx);
	    repeatMaskOutStaticLoad(words, &rmo);
	    /* If repeat is more than 15% divergent don't worry about it. */
	    if (rmo.percDiv + rmo.percDel + rmo.percInc <= 15.0)
		{
		if ((hel = hashLookup(repeatHash, rmo.qName)) == NULL)
		    errAbort("%s is in %s but not %s, files out of sync?\n", rmo.qName, fileName, faName);
		rt = hel->val;
		seqSize = rt->seq->size;
		if (rmo.qStart <= 0 || rmo.qStart > seqSize || rmo.qEnd <= 0 || rmo.qEnd > seqSize || rmo.qStart > rmo.qEnd)
		    {
		    warn("Repeat mask sequence out of range (%d-%d of %d in %s)\n",
			rmo.qStart, rmo.qEnd, seqSize, rmo.qName);
		    if (rmo.qStart <= 0)
			rmo.qStart = 1;
		    if (rmo.qEnd > seqSize)
			rmo.qEnd = seqSize;
		    }
		repSize = rmo.qEnd - rmo.qStart + 1;
		if (repSize > 0)
		    memset(rt->repBytes + rmo.qStart - 1, TRUE, repSize);
		}
	    }
	}
    }
lineFileClose(&lf);
}

void filterMissingFiles(char **files, int *pFileCount)
/* Remove files that don't exist from list.  Die if
 * no files left. */
{
int i;
int okFileCount = 0;
int oldCount = *pFileCount;
char *file;

for (i=0; i<oldCount; ++i)
    {
    file = files[i];
    if (fileExists(file))
	files[okFileCount++] = file;
    else
	warn("File %s doesn't exist", file);	
    }
*pFileCount = okFileCount;
}

void readAllWordsOrFa(char *fileName, char ***retFiles, int *retFileCount, 
   char **retBuf)
/* Open a file and check if it is .fa.  If so return just that
 * file in a list of one.  Otherwise read all file and treat file
 * as a list of filenames.  */
{
FILE *f = mustOpen(fileName, "r");
char c = fgetc(f);

fclose(f);
if (c == '>')
    {
    char **files;
    *retFiles = AllocArray(files, 1);
    *retBuf = files[0] = cloneString(fileName);
    *retFileCount = 1;
    return;
    }
else
    {
    readAllWords(fileName, retFiles, retFileCount, retBuf);
    }
}


int main(int argc, char *argv[])
{
char *genoListName;
char *otherListName;
char *oocFileName;
char *typeName;
char *outName;
struct patSpace *patSpace;
long startTime, endTime;
char **genoList;
int genoListSize;
char *genoListBuf;
char **otherList;
int otherListSize;
char *otherListBuf;
char *genoName;
int i;
int blockCount = 0;
struct dnaSeq **seqListList = NULL, *seq = NULL;
char *outRoot;
struct sqlConnection *conn;
enum ffStringency stringency = ffCdna;
int seedSize = 10;
FILE *out;
boolean noHead = FALSE;
struct repeatTracker *rt;
struct hash *repeatHash = newHash(10);

hostName = getenv("HOST");
pushWarnHandler(warnHandler);

startTime = clock1();
cgiSpoof(&argc, argv);
minMatch = cgiOptionalInt("minMatch", minMatch);
maxBad = cgiOptionalInt("maxBad", maxBad);
minBases = cgiOptionalInt("minBases", minBases);

dnaUtilOpen();

#ifdef DEBUG
/* Hard wire command line input so don't have to type it in each 
 * time run the stupid Gnu debugger. */

genoListName = "pFoo/geno.lst";
otherListName = "pFoo/bacend.lst";
typeName = "genomic";
oocFileName = "/d/biodata/human/10.ooc";
outName = "pFoo/pFoo.psl";

#else

if (argc != 6 && argc != 7)
    usage();

genoListName = argv[1];
otherListName = argv[2];
typeName = argv[3];
oocFileName = argv[4];
if (sameWord(oocFileName, "none"))
    oocFileName = NULL;
outName = argv[5];
if (argc == 7)
    {
    if (sameWord("noHead", argv[6]))
	noHead = TRUE;
    else
	usage();
    }

#endif 

if (sameWord(typeName, "mRNA") || sameWord(typeName, "cDNA"))
    {
    stringency = ffCdna;
    }
else if (sameWord(typeName, "genomic"))
    {
    stringency = ffTight;
    }
else if (sameWord(typeName, "g2g"))
    {
    stringency = ffTight;
    veryTight = TRUE;
    seedSize = 11;
    }
else if (sameString(typeName, "asm"))
    {
    stringency = ffTight;
    avoidSelfSelf = TRUE;
    }
else
    {
    warn("Unrecognized otherType %s\n", typeName);
    usage();
    }

readAllWordsOrFa(genoListName, &genoList, &genoListSize, &genoListBuf);
filterMissingFiles(genoList, &genoListSize);
if (genoListSize <= 0)
    errAbort("There are no files that exist in %s\n", genoListName);
readAllWordsOrFa(otherListName, &otherList, &otherListSize, &otherListBuf);
if (otherListSize <= 0)
    errAbort("There are no files that exist in %s\n", otherListName);
filterMissingFiles(otherList, &otherListSize);
out = mustOpen(outName, "w");
if (!noHead)
    pslWriteHead(out);

AllocArray(seqListList, genoListSize);
for (i=0; i<genoListSize; ++i)
    {
    genoName = genoList[i];
    if (!startsWith("#", genoName)  )
        seqListList[i] = seq = faReadAllDna(genoName);
    for (;seq != NULL; seq = seq->next)
	{
	int size = seq->size;
	char *name = seq->name;
	struct hashEl *hel;
	AllocVar(rt);
	AllocArray(rt->repBytes, size);
	rt->seq = seq;
	if ((hel = hashLookup(repeatHash, name)) != NULL)
	    errAbort("Duplicate %s in %s\n", name, genoName);
	hashAdd(repeatHash, name, rt);
	}
    storeMasked(repeatHash, genoName);
    }

patSpace = makePatSpace(seqListList, genoListSize, seedSize, oocFileName, minMatch, 2000);
endTime = clock1();
printf("Made index in %ld seconds\n",  (endTime-startTime));
startTime = endTime;

for (i=0; i<otherListSize; ++i)
    {
    FILE *f;
    char *otherName;
    int c;
    int dotCount = 0;
    struct dnaSeq otherSeq;
    ZeroVar(&otherSeq);

    otherName = otherList[i];
    if (startsWith("#", otherName)  )
	continue;
    f = mustOpen(otherName, "r");
    while ((c = fgetc(f)) != EOF)
	if (c == '>')
	    break;
    printf("%s\n", otherName);
    fflush(stdout);
    while (faFastReadNext(f, &otherSeq.dna, &otherSeq.size, &otherSeq.name))
        {
	aliSeqName = otherSeq.name;
	oneStrand(patSpace, repeatHash, &otherSeq, FALSE, stringency, out);
	reverseComplement(otherSeq.dna, otherSeq.size);
	oneStrand(patSpace, repeatHash, &otherSeq, TRUE, stringency, out);
	aliSeqName = NULL;
        }
    fclose(f);
    }
freePatSpace(&patSpace);
endTime = clock1();
printf("Alignment time is %ld sec\n", (endTime-startTime));
startTime = endTime;
fclose(out);
return 0;
}

