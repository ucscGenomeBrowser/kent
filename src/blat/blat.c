/* blat - Standalone BLAT fast sequence search command line tool. */
#include "common.h"
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

enum constants {
	qWarnSize = 5000000,	/* Warn if more than this many bases in one query. */
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
  "blat - Standalone BLAT fast sequence search command line tool\n"
  "usage:\n"
  "   blat database query [-ooc=11.ooc] output.psl\n"
  "where:\n"
  "   database is either a .fa file, a .nib file, or a list of .fa or .nib files\n"
  "   query is similarly a .fa, .nib, or list of .fa or .nib files\n"
  "   -ooc=11.ooc tells the program to load over-occurring 11-mers from\n"
  "               and external file.  This will increase the speed\n"
  "               by a factor of 40 in many cases, but is not required\n"
  "   output.psl is where to put the output.\n"
  "options:\n"
  "   -tileSize=N sets the size of match that triggers an alignment.  \n"
  "               Usually between 8 and 12\n"
  "               Default is 11 for DNA and 8 for protein.\n"
  "   -oneOff=N   If set to 1 this allows one mismatch in tile and still\n"
  "               triggers an alignments.  Default is 1 for protein, 0 for DNA.\n"
  "   -minMatch=N sets the number of tile matches.  Usually set from 2 to 4\n"
  "               Default is 2 for nucleotide, 1 for protein.\n"
  "   -minScore=N sets minimum score.  This is twice the matches minus the mismatches\n"
  "               minus some sort of gap penalty.  Default is 30\n"
  "   -minIdentity=N Sets minimum sequence identity (in percent).  Default is 90 for\n"
  "               nucleotide searches, 25 for protein or translated protein searches.\n"
  "   -maxGap=N   sets the size of maximum gap between tiles in a clump.  Usually set\n"
  "               from 0 to 3.  Default is 2.\n"
  "   -repMatch=N sets the number of repetitions of a tile allowed before\n"
  "               it is masked.  Typically this is 256 for tileSize 12,\n"
  "               1024 for tile size 11, 4096 for tile size 10.\n"
  "               Default is 1024\n"
  "   -noHead     suppress .psl header (so it's just a tab-separated file)\n"
  "   -ooc=N.ooc  Use overused tile file N.ooc.  N should correspond to \n"
  "               the tileSize\n"
  "   -makeOoc=N.ooc Make overused tile file\n"
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
  "   -mask=type  Mask out repeats.  Alignments won't be started in masked region but\n"
  "               may extend through it.  Masking only works for nucleotides. Mask types are:\n"
  "                 lower - mask out lower cased sequence\n"
  "                 upper - mask out upper cased sequence\n"
  "                 out   - mask according to database.out RepeatMasker .out file\n"
  "                 file.out - mask database according to RepeatMasker file.out\n"
  "   -qMask=type Mask out repeats in query sequence.  Alignments won't be started in masked\n"
  "               region but may extend through it.  Masking only works for nucleotides. Mask:\n"
  "               types are:\n"
  "                 lower - mask out lower cased sequence\n"
  "                 upper - mask out upper cased sequence\n"
  "   -minRepDivergence=NN - minimum percent divergence of repeats to allow them to be\n"
  "               unmasked.  Default is 15\n"
  "   -out=type   Controls output file format.  Type is one of:\n"
  "                   psl - Default.  Tab separated format without actual sequence\n"
  "                   pslx - Tab separated format with sequence\n"
  "   -dots=N     Output dot every N sequences to show program's progress\n"
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

void maskNucSeqList(struct dnaSeq *seqList, char *seqFileName, char *maskType)
/* Apply masking to simple nucleotide sequence by making masked nucleotides
 * upper case (since normal DNA sequence is lower case for us. */
{
struct dnaSeq *seq;
DNA *dna;
int size, i;
char *outFile = NULL, outNameBuf[512];

if (sameWord(maskType, "upper"))
    {
    /* This is easy - user has done it for us. */
    return;
    }
else if (sameWord(maskType, "lower"))
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
	toggleCase(seq->dna, seq->size);
    return;
    }
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

bioSeq *getSeqList(int fileCount, char *files[], struct hash *hash, boolean isProt, boolean simpleNuc, char *maskType)
/* From an array of .fa and .nib file names, create a
 * list of dnaSeqs. */
{
int i;
char *fileName;
bioSeq *seqList = NULL, *seq;
int count = 0; 
unsigned long totalSize = 0;
boolean maskWarned = FALSE;
boolean softMask = (simpleNuc && maskType != NULL);

for (i=0; i<fileCount; ++i)
    {
    struct dnaSeq *list = NULL, sseq;
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
	    if (!softMask)
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
    if (softMask)
	{
	slReverse(&list);
	maskNucSeqList(list, fileName, maskType);
	slReverse(&list);
	}

    /* Move local list to head of bigger list. */
    seqList = slCat(list, seqList);
    }
printf("Loaded %lu letters in %d sequences\n", totalSize, count);
slReverse(&seqList);
return seqList;
}

void outputOptions(struct gfSavePslxData *outForm, FILE *f)
/* Set up main output data structure. */
{
ZeroVar(outForm);
outForm->f =f;
outForm->minGood = round(10*minIdentity);
outForm->saveSeq = sameWord(outputFormat, "pslx");
}

void searchOneStrand(struct dnaSeq *seq, struct genoFind *gf, FILE *psl, 
	boolean isRc, struct hash *maskHash, Bits *qMaskBits)
/* Search for seq in index, align it, and write results to psl. */
{
struct gfSavePslxData data;
outputOptions(&data, psl);
data.maskHash = maskHash;
gfLongDnaInMem(seq, gf, isRc, minScore, qMaskBits, gfSavePslx, &data);
}

void searchOneProt(aaSeq *seq, struct genoFind *gf, FILE *psl)
/* Search for protein seq in index and write results to psl. */
{
int hitCount;
struct lm *lm = lmInit(0);
struct gfSavePslxData outForm;
struct gfClump *clump, *clumpList = gfFindClumps(gf, seq, lm, &hitCount);
outputOptions(&outForm, psl);
outForm.qIsProt = TRUE;
outForm.tIsProt = TRUE;
gfAlignAaClumps(gf, clumpList, seq, FALSE, minScore, gfSavePslx, &outForm);
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

void searchOne(bioSeq *seq, struct genoFind *gf, FILE *psl, boolean isProt, struct hash *maskHash, Bits *qMaskBits)
/* Search for seq on either strand in index. */
{
dotOut();
if (isProt)
    {
    searchOneProt(seq, gf, psl);
    }
else
    {
    searchOneStrand(seq, gf, psl, FALSE, maskHash, qMaskBits);
    reverseComplement(seq->dna, seq->size);
    searchOneStrand(seq, gf, psl, TRUE, maskHash, qMaskBits);
    reverseComplement(seq->dna, seq->size);
    }
}

void searchOneIndex(int fileCount, char *files[], struct genoFind *gf, char *pslOut, 
	boolean isProt, struct hash *maskHash)
/* Search all sequences in all files against single genoFind index. */
{
int i;
char *fileName;
bioSeq *seqList = NULL, *targetSeq;
int count = 0; 
unsigned long totalSize = 0;
FILE *psl = mustOpen(pslOut, "w");
boolean maskQuery = (qMask != NULL);
boolean lcMask = (qMask != NULL && sameWord(qMask, "lower"));

if (!noHead)
    pslWriteHead(psl);
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
	carefulClose(&f);
	searchOne(seq, gf, psl, isProt, maskHash, NULL);
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
	    searchOne(&seq, gf, psl, isProt, maskHash, qMaskBits);
	    totalSize += seq.size;
	    count += 1;
	    bitFree(&qMaskBits);
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

void tripleSearch(aaSeq *qSeq, struct genoFind *gfs[3], struct hash *t3Hash, boolean dbIsRc, FILE *f)
/* Look for qSeq in indices for three frames.  Then do rest of alignment. */
{
static struct gfSavePslxData outForm;
outputOptions(&outForm, f);
outForm.t3Hash = t3Hash;
outForm.targetRc = dbIsRc;
outForm.reportTargetStrand = TRUE;
outForm.qIsProt = TRUE;
gfFindAlignAaTrans(gfs, qSeq, t3Hash, minScore, gfSavePslx, &outForm);
}

void transTripleSearch(struct dnaSeq *qSeq, struct genoFind *gfs[3], struct hash *t3Hash, 
	boolean dbIsRc, boolean qIsDna, FILE *f)
/* Translate qSeq three ways and look for each in three frames of index. */
{
int qIsRc;
static struct gfSavePslxData outForm;
outputOptions(&outForm, f);
outForm.t3Hash = NULL;
outForm.reportTargetStrand = TRUE;
outForm.targetRc = dbIsRc;

for (qIsRc = 0; qIsRc <= qIsDna; qIsRc += 1)
    {
    gfLongTransTransInMem(qSeq, gfs, t3Hash, qIsRc, !qIsDna, minScore, gfSavePslx, &outForm);
    if (qIsDna)
        reverseComplement(qSeq->dna, qSeq->size);
    }
}

void bigBlat(struct dnaSeq *untransList, int queryCount, char *queryFiles[], char *outFile, boolean transQuery, boolean qIsDna)
/* Run query against translated DNA database (3 frames on each strand). */
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
boolean forceUpper = FALSE;
boolean forceLower = FALSE;
boolean toggle = FALSE;
boolean maskUpper = FALSE;

printf("Blatx %d sequences in database, %d files in query\n", slCount(untransList), queryCount);

/* Figure out how to manage query case. */
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

if (!noHead)
    pslWriteHead(pslOut);

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
	    if (transQuery)
	        transTripleSearch(&qSeq, gfs, t3Hash, isRc, qIsDna, pslOut);
	    else
		tripleSearch(&qSeq, gfs, t3Hash, isRc, pslOut);
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
boolean dbIsProt = (tType == gftProt);
boolean bothSimpleNuc = (tType == gftDna && (qType == gftDna || qType == gftRna));
getFileArray(dbFile, &dbFiles, &dbCount);
if (makeOoc != NULL)
    {
    gfMakeOoc(makeOoc, dbFiles, dbCount, tileSize, repMatch, tType);
    printf("Done making %s\n", makeOoc);
    exit(0);
    }
getFileArray(queryFile, &queryFiles, &queryCount);
dbSeqList = getSeqList(dbCount, dbFiles, dbHash, dbIsProt, bothSimpleNuc, mask);

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
    searchOneIndex(queryCount, queryFiles, gf, pslOut, dbIsProt, maskHash);
    freeHash(&maskHash);
    }
else if (tType == gftDnaX && qType == gftProt)
    {
    bigBlat(dbSeqList, queryCount, queryFiles, pslOut, FALSE, TRUE);
    }
else if (tType == gftDnaX && (qType == gftDnaX || qType == gftRnaX))
    {
    bigBlat(dbSeqList, queryCount, queryFiles, pslOut, TRUE, qType == gftDnaX);
    }
else
    {
    errAbort("Unrecognized combination of target and query types\n");
    }
if (dotEvery > 0)
    printf("\n");
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
    tileSize = 8;
    minMatch = 1;
    oneOff = TRUE;
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
