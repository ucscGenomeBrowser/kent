/* blat - Standalone BLAT fast sequence search command line tool. */
/* Copyright 2001-2002 Jim Kent.  All rights reserved. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "bits.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nib.h"
#include "psl.h"
#include "sig.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "genoFind.h"
#include "trans3.h"
#include "repMask.h"

/* Variables shared with other modules.  Set in this module, read only
 * elsewhere. */
int version = 14;	/* Blat version number. */
char *databaseName;		/* File name of database. */
int databaseSeqCount = 0;	/* Number of sequences in database. */
unsigned long databaseLetterCount = 0;	/* Number of bases in database. */

enum constants {
	qWarnSize = 5000000, /* Warn if more than this many bases in one query. */
	};

/* Variables that can be set from command line. */
int tileSize = 11;
int minMatch = 2;
int minScore = 30;
int maxGap = 2;
int repMatch = 1024*4;
int dotEvery = 0;
boolean oneOff = FALSE;
boolean noHead = FALSE;
boolean trimA = FALSE;
boolean trimT = FALSE;
char *makeOoc = NULL;
char *ooc = NULL;
enum gfType qType = gftDna;
enum gfType tType = gftDna;
char *mask = NULL;
char *qMask = NULL;
double minRepDivergence = 15;
double minIdentity = 90;
char *outputFormat = "psl";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "blat - Standalone BLAT v. %d fast sequence search command line tool\n"
  "usage:\n"
  "   blat database query [-ooc=11.ooc] output.psl\n"
  "where:\n"
  "   database is either a .fa file, a .nib file, or a list of .fa or .nib\n"
  "   files, query is similarly a .fa, .nib, or list of .fa or .nib files\n"
  "   -ooc=11.ooc tells the program to load over-occurring 11-mers from\n"
  "               and external file.  This will increase the speed\n"
  "               by a factor of 40 in many cases, but is not required\n"
  "   output.psl is where to put the output.\n"
  "options:\n"
  "   -tileSize=N sets the size of match that triggers an alignment.  \n"
  "               Usually between 8 and 12\n"
  "               Default is 11 for DNA and 5 for protein.\n"
  "   -oneOff=N   If set to 1 this allows one mismatch in tile and still\n"
  "               triggers an alignments.  Default is 0.\n"
  "   -minMatch=N sets the number of tile matches.  Usually set from 2 to 4\n"
  "               Default is 2 for nucleotide, 1 for protein.\n"
  "   -minScore=N sets minimum score.  This is twice the matches minus the \n"
  "               mismatches minus some sort of gap penalty.  Default is 30\n"
  "   -minIdentity=N Sets minimum sequence identity (in percent).  Default is\n"
  "               90 for nucleotide searches, 25 for protein or translated\n"
  "               protein searches.\n"
  "   -maxGap=N   sets the size of maximum gap between tiles in a clump.  Usually\n"
  "               set from 0 to 3.  Default is 2. Only relevent for minMatch > 1.\n"
  "   -noHead     suppress .psl header (so it's just a tab-separated file)\n"
  "   -ooc=N.ooc  Use overused tile file N.ooc.  N should correspond to \n"
  "               the tileSize\n"
  "   -makeOoc=N.ooc Make overused tile file\n"
  "   -repMatch=N sets the number of repetitions of a tile allowed before\n"
  "               it is marked as overused.  Typically this is 256 for tileSize\n"
  "               12, 1024 for tile size 11, 4096 for tile size 10.\n"
  "               Default is 1024.  Typically only comes into play with makeOoc\n"
  "   -mask=type  Mask out repeats.  Alignments won't be started in masked region\n"
  "               but may extend through it in nucleotide searches.  Masked areas\n"
  "               are ignored entirely in protein or translated searches. Types are\n"
  "                 lower - mask out lower cased sequence\n"
  "                 upper - mask out upper cased sequence\n"
  "                 out   - mask according to database.out RepeatMasker .out file\n"
  "                 file.out - mask database according to RepeatMasker file.out\n"
  "   -qMask=type Mask out repeats in query sequence.  Similar to -mask above but\n"
  "               for query rather than target sequence.\n"
  "   -minRepDivergence=NN - minimum percent divergence of repeats to allow them to be\n"
  "               unmasked.  Default is 15.  Only relevant for RepeatMasker .out files.\n"
  "   -out=type   Controls output file format.  Type is one of:\n"
  "                   psl - Default.  Tab separated format without actual sequence\n"
  "                   pslx - Tab separated format with sequence\n"
  "                   axt - blastz-like axt format (only for nucleotides)\n"
  "                   wu-blast - similar to wu-blast format\n"
  "   -dots=N     Output dot every N sequences to show program's progress\n"
  "   -t=type     Database type.  Type is one of:\n"
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
  "   -trimT      Trim leading poly-T\n"
  "   -noTrimA    Don't trim trailing poly-A\n"
  , version
  );
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

void toggleCase(char *s, int size)
/* toggle upper and lower case chars in string. */
{
char c;
int i;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (isupper(c))
        c = tolower(c);
    else if (islower(c))
        c = toupper(c);
    s[i] = c;
    }
}

void upperToN(char *s, int size)
/* Turn upper case letters to N's. */
{
char c;
int i;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (isupper(c))
        s[i] = 'n';
    }
}

void unmaskNucSeqList(struct dnaSeq *seqList)
/* Unmask all sequences. */
{
struct dnaSeq *seq;
for (seq = seqList; seq != NULL; seq = seq->next)
    faToDna(seq->dna, seq->size);
}

void maskFromOut(struct dnaSeq *seqList, char *outFile)
/* Mask DNA sequence by putting bits more than 85% identical to
 * repeats as defined in RepeatMasker .out file to upper case. */
{
struct lineFile *lf = lineFileOpen(outFile, TRUE);
struct hash *hash = newHash(0);
struct dnaSeq *seq;
char *line;

for (seq = seqList; seq != NULL; seq = seq->next)
    hashAdd(hash, seq->name, seq);
if (!lineFileNext(lf, &line, NULL))
    errAbort("Empty mask file %s\n", lf->fileName);
if (!startsWith("There were no", line))	/* No repeats is ok. Not much work. */
    {
    if (!startsWith("   SW", line))
	errAbort("%s isn't a RepeatMasker .out file.", lf->fileName);
    if (!lineFileNext(lf, &line, NULL) || !startsWith("score", line))
	errAbort("%s isn't a RepeatMasker .out file.", lf->fileName);
    lineFileNext(lf, &line, NULL);  /* Blank line. */
    while (lineFileNext(lf, &line, NULL))
	{
	char *words[32];
	struct repeatMaskOut rmo;
	int wordCount;
	int seqSize;
	int repSize;
	wordCount = chopLine(line, words);
	if (wordCount < 14)
	    errAbort("%s line %d - error in repeat mask .out file\n", lf->fileName, lf->lineIx);
	repeatMaskOutStaticLoad(words, &rmo);
	/* If repeat is more than 15% divergent don't worry about it. */
	if (rmo.percDiv + rmo.percDel + rmo.percInc <= minRepDivergence)
	    {
	    if((seq = hashFindVal(hash, rmo.qName)) == NULL)
		errAbort("%s is in %s but not corresponding sequence file, files out of sync?\n", 
			rmo.qName, lf->fileName);
	    seqSize = seq->size;
	    if (rmo.qStart <= 0 || rmo.qStart > seqSize || rmo.qEnd <= 0 
	    	|| rmo.qEnd > seqSize || rmo.qStart > rmo.qEnd)
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
		toUpperN(seq->dna + rmo.qStart - 1, repSize);
	    }
	}
    }
freeHash(&hash);
lineFileClose(&lf);
}

void maskNucSeqList(struct dnaSeq *seqList, char *seqFileName, char *maskType,
	boolean hardMask)
/* Apply masking to simple nucleotide sequence by making masked nucleotides
 * upper case (since normal DNA sequence is lower case for us. */
{
struct dnaSeq *seq;
DNA *dna;
int size, i;
char *outFile = NULL, outNameBuf[512];

if (sameWord(maskType, "upper"))
    {
    /* Already has dna to be masked in upper case. */
    }
else if (sameWord(maskType, "lower"))
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
	toggleCase(seq->dna, seq->size);
    }
else
    {
    /* Masking from a RepeatMasker .out file. */
    if (sameWord(maskType, "out"))
	{
	sprintf(outNameBuf, "%s.out", seqFileName);
	outFile = outNameBuf;
	}
    else
	{
	outFile = maskType;
	}
    unmaskNucSeqList(seqList);
    maskFromOut(seqList, outFile);
    }
if (hardMask)
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
	upperToN(seq->dna, seq->size);
    }
}

bioSeq *getSeqList(int fileCount, char *files[], struct hash *hash, 
	boolean isProt, boolean isTransDna, char *maskType)
/* From an array of .fa and .nib file names, create a
 * list of dnaSeqs. */
{
int i;
char *fileName;
bioSeq *seqList = NULL, *seq;
int count = 0; 
unsigned long totalSize = 0;
boolean maskWarned = FALSE;
boolean doMask = (maskType != NULL);

for (i=0; i<fileCount; ++i)
    {
    struct dnaSeq *list = NULL, sseq;
    ZeroVar(&sseq);
    fileName = files[i];
    if (isNib(fileName))
        {
	FILE *f;
	int size;

	if (maskType != NULL && !maskWarned)
	    {
	    if (sameWord(maskType, "lower") || sameWord(maskType, "upper"))
	        warn("Warning: mask=lower and mask=upper don't work with .nib files.");
	    }
	nibOpenVerify(fileName, &f, &size);
	seq = nibLdPart(fileName, f, size, 0, size);
	seq->name = fileName;
	carefulClose(&f);
	slAddHead(&list, seq);
	hashAddUnique(hash, seq->name, seq);
	totalSize += size;
	count += 1;
	}
    else
        {
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	while (faMixedSpeedReadNext(lf, &sseq.dna, &sseq.size, &sseq.name))
	    {
	    seq = cloneDnaSeq(&sseq);
	    if (!doMask)
	        {
		if (isProt)
		    faToProtein(seq->dna, seq->size);
		else
		    faToDna(seq->dna, seq->size);
		}
	    hashAddUnique(hash, seq->name, seq);
	    slAddHead(&list, seq);
	    totalSize += seq->size;
	    count += 1;
	    }
	faFreeFastBuf();
	}

    /* If necessary mask sequence from file. */
    if (doMask)
	{
	slReverse(&list);
	maskNucSeqList(list, fileName, maskType, isTransDna);
	slReverse(&list);
	}

    /* Move local list to head of bigger list. */
    seqList = slCat(list, seqList);
    }
printf("Loaded %lu letters in %d sequences\n", totalSize, count);
slReverse(&seqList);
return seqList;
}

struct gfVirtualOutput
/* A polymorphic object to help us write many file types. */
    {
    struct gfVirtualOutput *next;
    void *data;		/* Type-specific data pointer. */
    GfSaveAli out;   /* Main output function - called for each ffAli. */
    void (*queryOut)(FILE *f, void *data);  /* Called for each query. */
    void (*fileHead)(FILE *f);	/* Write file header if any */
    void (*fileTail)(FILE *f);	/* Write file tail if any */
    };

/* Stuff to support various output formats. */
struct gfVirtualOutput gvo;		/* Overall output controller */
struct gfSavePslxData pslxOutData;	/* Data for psl/pslx format. */
struct gfSaveAxtData axtOutData;	/* Data for blast and other formats. */


void axtQueryOut(FILE *f, void *data)
/* Do axt oriented output - at end of processing query. */
{
struct gfSaveAxtData *aod = data;
struct axtBundle *gab;
struct axtScoreScheme *ss = axtScoreSchemeDefault();
for (gab = aod->bundleList; gab != NULL; gab = gab->next)
    {
    struct axt *axt;
    for (axt = gab->axtList; axt != NULL; axt = axt->next)
	{
	axt->score = axtScore(axt, ss);
	axtWrite(axt, f);
	}
    }
axtBundleFreeList(&aod->bundleList);
}

void axtBlastOut(struct axtBundle *abList, int queryIx, boolean isProt, FILE *f, 
	char *databaseName, int databaseSeqCount, double databaseLetterCount, 
	boolean isWu, char *ourId);

void wublastQueryOut(FILE *f, void *data)
/* Output wublast on query. */
{
char blatName[16];
struct gfSaveAxtData *aod = data;
++aod->queryIx;
sprintf(blatName, "blat %d", version);
axtBlastOut(aod->bundleList, aod->queryIx, aod->qIsProt, f,
	databaseName, databaseSeqCount, databaseLetterCount,
	TRUE, blatName);
axtBundleFreeList(&aod->bundleList);
}


void initBasicOutput(char *format, FILE *f, int minIdentity, boolean qIsProt, boolean tIsProt)
/* Initialize output according to format.  Additional initialiazation
 * of pslOutData/axtOutData may be necessary. */
{
int goodPpt = round(10*minIdentity);
ZeroVar(&gvo);
if (sameWord(format, "psl") || sameWord(format, "pslx"))
    {
    gvo.out = gfSavePslx;
    gvo.data = &pslxOutData;
    if (!noHead)
        gvo.fileHead = pslWriteHead;
    pslxOutData.minGood = goodPpt;
    pslxOutData.saveSeq = sameWord(format, "pslx");
    pslxOutData.f = f;
    pslxOutData.qIsProt = qIsProt;
    pslxOutData.tIsProt = tIsProt;
    }
else if (sameWord(format, "wu-blast") || sameWord(format, "axt"))
    {
    gvo.out = gfSaveAxtBundle;
    gvo.data = &axtOutData;
    axtOutData.minGood = goodPpt;
    axtOutData.qIsProt = qIsProt;
    axtOutData.tIsProt = tIsProt;
    if (qIsProt || tIsProt)
        errAbort("Sorry, at the moment %s output doesn't support protein alignments", format);
    if (sameWord(format, "wu-blast"))
        {
	gvo.queryOut = wublastQueryOut;
	}
    else if (sameWord(format, "axt"))
        {
	gvo.queryOut = axtQueryOut;
	}
    }
else
    errAbort("Unrecognized output format '%s'", format);
}

void finishBasicOutput(FILE *f)
/* Write out stuff to file */
{
if (gvo.queryOut != NULL)
    gvo.queryOut(f, gvo.data);
}


void searchOneStrand(struct dnaSeq *seq, struct genoFind *gf, FILE *psl, 
	boolean isRc, struct hash *maskHash, Bits *qMaskBits)
/* Search for seq in index, align it, and write results to psl. */
{
gfLongDnaInMem(seq, gf, isRc, minScore, qMaskBits, gvo.out, gvo.data);
}


void searchOneProt(aaSeq *seq, struct genoFind *gf, FILE *f)
/* Search for protein seq in index and write results to psl. */
{
int hitCount;
struct lm *lm = lmInit(0);
struct gfClump *clump, *clumpList = gfFindClumps(gf, seq, lm, &hitCount);
initBasicOutput(outputFormat, f, minIdentity, TRUE, TRUE);
gfAlignAaClumps(gf, clumpList, seq, FALSE, minScore, gvo.out, gvo.data);
gfClumpFreeList(&clumpList);
lmCleanup(&lm);
}

void dotOut()
/* Put out a dot every now and then if user want's to. */
{
static int mod = 1;
if (dotEvery > 0)
    {
    if (--mod <= 0)
	{
	fputc('.', stdout);
	fflush(stdout);
	mod = dotEvery;
	}
    }
}

void searchOne(bioSeq *seq, struct genoFind *gf, FILE *f, boolean isProt, struct hash *maskHash, Bits *qMaskBits)
/* Search for seq on either strand in index. */
{
dotOut();
if (isProt)
    {
    searchOneProt(seq, gf, f);
    finishBasicOutput(f);
    }
else
    {
    initBasicOutput(outputFormat, f, minIdentity, FALSE, FALSE);
    pslxOutData.maskHash = maskHash;
    searchOneStrand(seq, gf, f, FALSE, maskHash, qMaskBits);
    reverseComplement(seq->dna, seq->size);
    searchOneStrand(seq, gf, f, TRUE, maskHash, qMaskBits);
    reverseComplement(seq->dna, seq->size);
    finishBasicOutput(f);
    }
}

void trimSeq(struct dnaSeq *seq, struct dnaSeq *trimmed)
/* Copy seq to trimmed (shallow copy) and optionally trim
 * off polyA tail or polyT head. */
{
DNA *dna = seq->dna;
int i, size = seq->size;
*trimmed = *seq;
if (trimT)
    {
    int tSize = 0;
    for (i=0; i<size; ++i)
        {
	DNA b = dna[i];
	if (b == 't' || b == 'T')
	    ++tSize;
	else
	    break;
	}
    if (tSize >= 4)
	memset(dna, 'n', tSize);
    }
if (trimA)
    {
    int aSize = 0;
    for (i=size-1; i>=0; --i)
        {
	DNA b = dna[i];
	if (b == 'a' || b == 'A')
	    ++aSize;
	else
	    break;
	}
    if (aSize >= 4)
	memset(dna + size - aSize, 'n', aSize);
    }
}

void searchOneIndex(int fileCount, char *files[], struct genoFind *gf, char *outName, 
	boolean isProt, struct hash *maskHash)
/* Search all sequences in all files against single genoFind index. */
{
int i;
char *fileName;
bioSeq *seqList = NULL, *targetSeq;
int count = 0; 
unsigned long totalSize = 0;
FILE *outFile = mustOpen(outName, "w");
boolean maskQuery = (qMask != NULL);
boolean lcMask = (qMask != NULL && sameWord(qMask, "lower"));
struct dnaSeq trimmedSeq;
ZeroVar(&trimmedSeq);

if (gvo.fileHead != NULL)
    gvo.fileHead(outFile);
for (i=0; i<fileCount; ++i)
    {
    fileName = files[i];
    if (isNib(fileName))
        {
	FILE *f;
	struct dnaSeq *seq;

	if (isProt)
	    errAbort("%s: Can't use .nib files with -prot or d=prot option\n", fileName);
	seq = nibLoadAll(fileName);
	freez(&seq->name);
	seq->name = cloneString(fileName);
	trimSeq(seq, &trimmedSeq);
	carefulClose(&f);
	searchOne(&trimmedSeq, gf, outFile, isProt, maskHash, NULL);
	totalSize += seq->size;
	freeDnaSeq(&seq);
	count += 1;
	}
    else
        {
	static struct dnaSeq seq;
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	    {
	    Bits *qMaskBits = NULL;
	    if (isProt)
		faToProtein(seq.dna, seq.size);
	    else
	        {
		if (maskQuery)
		    {
		    if (lcMask)
		        toggleCase(seq.dna, seq.size);
		    qMaskBits = maskFromUpperCaseSeq(&seq);
		    }
		faToDna(seq.dna, seq.size);
		}
	    if (seq.size > qWarnSize)
	        {
		warn("Query sequence %d has size %d, it might take a while.", seq.name, seq.size);
		}
	    trimSeq(&seq, &trimmedSeq);
	    searchOne(&trimmedSeq, gf, outFile, isProt, maskHash, qMaskBits);
	    totalSize += seq.size;
	    count += 1;
	    bitFree(&qMaskBits);
	    }
	lineFileClose(&lf);
	}
    }
carefulClose(&outFile);
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

void tripleSearch(aaSeq *qSeq, struct genoFind *gfs[3], struct hash *t3Hash, boolean dbIsRc, FILE *f)
/* Look for qSeq in indices for three frames.  Then do rest of alignment. */
{
initBasicOutput(outputFormat, f, minIdentity, TRUE, FALSE);
pslxOutData.t3Hash = t3Hash;
pslxOutData.targetRc = dbIsRc;
pslxOutData.reportTargetStrand = TRUE;
gfFindAlignAaTrans(gfs, qSeq, t3Hash, minScore, gvo.out, gvo.data);
}

void transTripleSearch(struct dnaSeq *qSeq, struct genoFind *gfs[3], struct hash *t3Hash, 
	boolean dbIsRc, boolean qIsDna, FILE *f)
/* Translate qSeq three ways and look for each in three frames of index. */
{
int qIsRc;
initBasicOutput(outputFormat, f, minIdentity, FALSE, FALSE);
pslxOutData.t3Hash = NULL;
pslxOutData.reportTargetStrand = TRUE;
pslxOutData.targetRc = dbIsRc;

for (qIsRc = 0; qIsRc <= qIsDna; qIsRc += 1)
    {
    gfLongTransTransInMem(qSeq, gfs, t3Hash, qIsRc, !qIsDna, minScore, gvo.out, gvo.data);
    if (qIsDna)
	{
        reverseComplement(qSeq->dna, qSeq->size);
	}
    }
}

void bigBlat(struct dnaSeq *untransList, int queryCount, char *queryFiles[], char *outFile, boolean transQuery, boolean qIsDna)
/* Run query against translated DNA database (3 frames on each strand). */
{
int frame, i;
struct dnaSeq *seq, trimmedSeq;
struct genoFind *gfs[3];
aaSeq *dbSeqLists[3];
struct trans3 *t3List = NULL, *t3;
int isRc;
FILE *out = mustOpen(outFile, "w");
struct lineFile *lf = NULL;
struct hash *t3Hash = NULL;
boolean forceUpper = FALSE;
boolean forceLower = FALSE;
boolean toggle = FALSE;
boolean maskUpper = FALSE;

ZeroVar(&trimmedSeq);
printf("Blatx %d sequences in database, %d files in query\n", slCount(untransList), queryCount);

/* Figure out how to manage query case.  Proteins want to be in
 * upper case, generally, nucleotides in lower case.  But there
 * may be repeatMasking based on case as well. */
if (transQuery)
    {
    if (qMask == NULL)
       forceLower = TRUE;
    else
       {
       maskUpper = TRUE;
       toggle = !sameString(qMask, "upper");
       }
    }
else
    {
    forceUpper = TRUE;
    }

if (gvo.fileHead != NULL)
    gvo.fileHead(out);

for (isRc = FALSE; isRc <= 1; ++isRc)
    {
    /* Initialize local pointer arrays to NULL to prevent surprises. */
    for (frame = 0; frame < 3; ++frame)
	{
	gfs[frame] = NULL;
	dbSeqLists[frame] = NULL;
	}

    t3List = seqListToTrans3List(untransList, dbSeqLists, &t3Hash);
    for (frame = 0; frame < 3; ++frame)
	{
	gfs[frame] = gfIndexSeq(dbSeqLists[frame], minMatch, maxGap, tileSize, 
		repMatch, ooc, TRUE, oneOff, FALSE);
	}

    for (i=0; i<queryCount; ++i)
        {
	aaSeq qSeq;

	lf = lineFileOpen(queryFiles[i], TRUE);
	while (faMixedSpeedReadNext(lf, &qSeq.dna, &qSeq.size, &qSeq.name))
	    {
	    dotOut();
	    /* Put it into right case and optionally mask on case. */
	    if (forceLower)
	        toLowerN(qSeq.dna, qSeq.size);
	    else if (forceUpper)
	        toUpperN(qSeq.dna, qSeq.size);
	    else if (maskUpper)
	        {
		if (toggle)
		    toggleCase(qSeq.dna, qSeq.size);
		upperToN(qSeq.dna, qSeq.size);
		}
	    if (qSeq.size > qWarnSize)
	        {
		warn("Query sequence %d has size %d, it might take a while.", qSeq.name, qSeq.size);
		}
	    trimSeq(&qSeq, &trimmedSeq);
	    if (transQuery)
	        transTripleSearch(&trimmedSeq, gfs, t3Hash, isRc, qIsDna, out);
	    else
		tripleSearch(&trimmedSeq, gfs, t3Hash, isRc, out);
	    }
	lineFileClose(&lf);
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
carefulClose(&out);
}


void blat(char *dbFile, char *queryFile, char *outName)
/* blat - Standalone BLAT fast sequence search command line tool. */
{
char **dbFiles, **queryFiles;
int dbCount, queryCount;
struct dnaSeq *dbSeqList, *seq;
struct hash *dbHash = newHash(16);
struct genoFind *gf;
boolean dbIsProt = (tType == gftProt);
boolean bothSimpleNuc = (tType == gftDna && (qType == gftDna || qType == gftRna));

databaseName = dbFile;
getFileArray(dbFile, &dbFiles, &dbCount);
if (makeOoc != NULL)
    {
    gfMakeOoc(makeOoc, dbFiles, dbCount, tileSize, repMatch, tType);
    printf("Done making %s\n", makeOoc);
    exit(0);
    }
getFileArray(queryFile, &queryFiles, &queryCount);
dbSeqList = getSeqList(dbCount, dbFiles, dbHash, dbIsProt, tType == gftDnaX, mask);
databaseSeqCount = slCount(dbSeqList);
for (seq = dbSeqList; seq != NULL; seq = seq->next)
    databaseLetterCount += seq->size;

if (bothSimpleNuc || (tType == gftProt && qType == gftProt))
    {
    struct hash *maskHash = NULL;
    /* Build index, possibly upper-case masked. */
    gf = gfIndexSeq(dbSeqList, minMatch, maxGap, tileSize, repMatch, ooc, 
    	dbIsProt, oneOff, mask != NULL);
    if (mask != NULL)
	{
	int i;
	struct gfSeqSource *source = gf->sources;
	maskHash = newHash(0);
	for (i=0; i<gf->sourceCount; ++i)
	    if (source[i].maskedBits)
		hashAdd(maskHash, source[i].seq->name, source[i].maskedBits);
        unmaskNucSeqList(dbSeqList);
	}
    searchOneIndex(queryCount, queryFiles, gf, outName, dbIsProt, maskHash);
    freeHash(&maskHash);
    }
else if (tType == gftDnaX && qType == gftProt)
    {
    bigBlat(dbSeqList, queryCount, queryFiles, outName, FALSE, TRUE);
    }
else if (tType == gftDnaX && (qType == gftDnaX || qType == gftRnaX))
    {
    bigBlat(dbSeqList, queryCount, queryFiles, outName, TRUE, qType == gftDnaX);
    }
else
    {
    errAbort("Unrecognized combination of target and query types\n");
    }
if (dotEvery > 0)
    printf("\n");
freeDnaSeqList(&dbSeqList);
hashFree(&dbHash);
}

int main(int argc, char *argv[])
/* Process command line into global variables and call blat. */
{
boolean cmpIsProt;	/* True if comparison takes place in protein space. */
boolean dIsProtLike, qIsProtLike;

#ifdef DEBUG
{
char *cmd = "blat hCrea.geno hCrea.mrna foo.psl -t=dnax -q=rnax";
char *words[16];

uglyf("Debugging parameters\n");
cmd = cloneString(cmd);
argc = chopLine(cmd, words);
argv = words;
}
#endif /* DEBUG */

cgiFromCommandLine(&argc, argv, FALSE);
if (argc != 4)
    usage();

/* Get database and query sequence types and make sure they are
 * legal and compatable. */
if (cgiVarExists("prot"))
    qType = tType = gftProt;
if (cgiVarExists("t"))
    tType = gfTypeFromName(cgiString("t"));
trimA = cgiBoolean("trimA") || cgiBoolean("trima");
trimT = cgiBoolean("trimT") || cgiBoolean("trimt");
switch (tType)
    {
    case gftProt:
    case gftDnaX:
        dIsProtLike = TRUE;
	break;
    case gftDna:
        dIsProtLike = FALSE;
	break;
    default:
	dIsProtLike = FALSE;
        errAbort("Illegal value for 't' parameter");
	break;
    }
if (cgiVarExists("q"))
    qType = gfTypeFromName(cgiString("q"));
if (qType == gftRnaX || qType == gftRna)
    trimA = TRUE;
if (cgiVarExists("noTrimA"))
    trimA = FALSE;
switch (qType)
    {
    case gftProt:
    case gftDnaX:
    case gftRnaX:
	minIdentity = 25;
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
    tileSize = 5;
    minMatch = 1;
    oneOff = FALSE;
    maxGap = 0;
    }

/* Get tile size and related parameters from user and make sure
 * they are within range. */
tileSize = cgiOptionalInt("tileSize", tileSize);
minMatch = cgiOptionalInt("minMatch", minMatch);
oneOff = cgiOptionalInt("oneOff", oneOff);
minScore = cgiOptionalInt("minScore", minScore);
maxGap = cgiOptionalInt("maxGap", maxGap);
minRepDivergence = cgiUsualDouble("minRepDivergence", minRepDivergence);
minIdentity = cgiUsualDouble("minIdentity", minIdentity);
gfCheckTileSize(tileSize, dIsProtLike);
if (minMatch < 0)
    errAbort("minMatch must be at least 1");
if (maxGap > 100)
    errAbort("maxGap must be less than 100");



/* Set repMatch parameter from command line, or
 * to reasonable value that depends on tile size. */
if (cgiVarExists("repMatch"))
    repMatch = cgiInt("repMatch");
else
    {
    if (dIsProtLike)
	{
	if (tileSize == 3)
	    repMatch = 600000;
	else if (tileSize == 4)
	    repMatch = 30000;
	else if (tileSize == 5)
	    repMatch = 1500;
	else if (tileSize == 6)
	    repMatch = 75;
	else if (tileSize <= 7)
	    repMatch = 10;
	}
    else
	{
	if (tileSize == 15)
	    repMatch = 16;
	else if (tileSize == 14)
	    repMatch = 32;
	else if (tileSize == 13)
	    repMatch = 128;
	else if (tileSize == 12)
	    repMatch = 256;
	else if (tileSize == 11)
	    repMatch = 4*256;
	else if (tileSize == 10)
	    repMatch = 16*256;
	else if (tileSize == 9)
	    repMatch = 64*256;
	else if (tileSize == 8)
	    repMatch = 256*256;
	else if (tileSize == 7)
	    repMatch = 1024*256;
	else if (tileSize == 6)
	    repMatch = 4*1024*256;
	}
    }

/* Gather last few command line options. */
noHead = cgiBoolean("noHead");
ooc = cgiOptionalString("ooc");
makeOoc = cgiOptionalString("makeOoc");
mask = cgiOptionalString("mask");
qMask = cgiOptionalString("qMask");
outputFormat = cgiUsualString("out", outputFormat);
dotEvery = cgiOptionalInt("dots", 0);

/* Call routine that does the work. */
blat(argv[1], argv[2], argv[3]);
return 0;
}
