/* blat - Standalone BLAT fast sequence search command line tool. */
/* Copyright 2001-2004 Jim Kent.  All rights reserved. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "bits.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nib.h"
#include "twoBit.h"
#include "psl.h"
#include "sig.h"
#include "options.h"
#include "obscure.h"
#include "genoFind.h"
#include "trans3.h"
#include "gfClientLib.h"


/* Variables shared with other modules.  Set in this module, read only
 * elsewhere. */
char *databaseName;		/* File name of database. */
int databaseSeqCount = 0;	/* Number of sequences in database. */
unsigned long databaseLetters = 0;	/* Number of bases in database. */

enum constants {
    qWarnSize = 5000000, /* Warn if more than this many bases in one query. */
    };

/* Variables that can be set from command line. */
int tileSize = 11;
int stepSize = 0;	/* Default (same as tileSize) */
int minMatch = 2;
int minScore = 30;
int maxGap = 2;
int repMatch = 1024*4;
int dotEvery = 0;
boolean oneOff = FALSE;
boolean noHead = FALSE;
boolean trimA = FALSE;
boolean trimHardA = FALSE;
boolean trimT = FALSE;
boolean fastMap = FALSE;
char *makeOoc = NULL;
char *ooc = NULL;
enum gfType qType = gftDna;
enum gfType tType = gftDna;
char *mask = NULL;
char *repeats = NULL;
char *qMask = NULL;
double minRepDivergence = 15;
double minIdentity = 90;
char *outputFormat = "psl";


void usage()
/* Explain usage and exit. */
{
printf(
  "blat - Standalone BLAT v. %s fast sequence search command line tool\n"
  "usage:\n"
  "   blat database query [-ooc=11.ooc] output.psl\n"
  "where:\n"
  "   database and query are each either a .fa, .nib or .2bit file,\n"
  "      or a list of these files with one file name per line.\n"
  "   -ooc=11.ooc tells the program to load over-occurring 11-mers from\n"
  "      an external file.  This will increase the speed\n"
  "      by a factor of 40 in many cases, but is not required.\n"
  "   output.psl is the name of the output file.\n"
  "   Subranges of .nib and .2bit files may be specified using the syntax:\n"
  "      /path/file.nib:seqid:start-end\n"
  "   or\n"
  "      /path/file.2bit:seqid:start-end\n"
  "   or\n"
  "      /path/file.nib:start-end\n"
  "   With the second form, a sequence id of file:start-end will be used.\n"
  "options:\n"
  "   -t=type        Database type.  Type is one of:\n"
  "                    dna - DNA sequence\n"
  "                    prot - protein sequence\n"
  "                    dnax - DNA sequence translated in six frames to protein\n"
  "                  The default is dna.\n"
  "   -q=type        Query type.  Type is one of:\n"
  "                    dna - DNA sequence\n"
  "                    rna - RNA sequence\n"
  "                    prot - protein sequence\n"
  "                    dnax - DNA sequence translated in six frames to protein\n"
  "                    rnax - DNA sequence translated in three frames to protein\n"
  "                  The default is dna.\n"
  "   -prot          Synonymous with -t=prot -q=prot.\n"
  "   -ooc=N.ooc     Use overused tile file N.ooc.  N should correspond to \n"
  "                  the tileSize.\n"
  "   -tileSize=N    Sets the size of match that triggers an alignment.  \n"
  "                  Usually between 8 and 12.\n"
  "                  Default is 11 for DNA and 5 for protein.\n"
  "   -stepSize=N    Spacing between tiles. Default is tileSize.\n"
  "   -oneOff=N      If set to 1, this allows one mismatch in tile and still\n"
  "                  triggers an alignment.  Default is 0.\n"
  "   -minMatch=N    Sets the number of tile matches.  Usually set from 2 to 4.\n"
  "                  Default is 2 for nucleotide, 1 for protein.\n"
  "   -minScore=N    Sets minimum score.  This is the matches minus the \n"
  "                  mismatches minus some sort of gap penalty.  Default is 30.\n"
  "   -minIdentity=N Sets minimum sequence identity (in percent).  Default is\n"
  "                  90 for nucleotide searches, 25 for protein or translated\n"
  "                  protein searches.\n"
  "   -maxGap=N      Sets the size of maximum gap between tiles in a clump.  Usually\n"
  "                  set from 0 to 3.  Default is 2. Only relevent for minMatch > 1.\n"
  "   -noHead        Suppresses .psl header (so it's just a tab-separated file).\n"
  "   -makeOoc=N.ooc Make overused tile file. Target needs to be complete genome.\n"
  "   -repMatch=N    Sets the number of repetitions of a tile allowed before\n"
  "                  it is marked as overused.  Typically this is 256 for tileSize\n"
  "                  12, 1024 for tile size 11, 4096 for tile size 10.\n"
  "                  Default is 1024.  Typically comes into play only with makeOoc.\n"
  "                  Also affected by stepSize: when stepSize is halved, repMatch is\n"
  "                  doubled to compensate.\n"
  "   -mask=type     Mask out repeats.  Alignments won't be started in masked region\n"
  "                  but may extend through it in nucleotide searches.  Masked areas\n"
  "                  are ignored entirely in protein or translated searches. Types are:\n"
  "                    lower - mask out lower-cased sequence\n"
  "                    upper - mask out upper-cased sequence\n"
  "                    out   - mask according to database.out RepeatMasker .out file\n"
  "                    file.out - mask database according to RepeatMasker file.out\n"
  "   -qMask=type    Mask out repeats in query sequence.  Similar to -mask above, but\n"
  "                  for query rather than target sequence.\n"
  "   -repeats=type  Type is same as mask types above.  Repeat bases will not be\n"
  "                  masked in any way, but matches in repeat areas will be reported\n"
  "                  separately from matches in other areas in the psl output.\n"
  "   -minRepDivergence=NN   Minimum percent divergence of repeats to allow \n"
  "                  them to be unmasked.  Default is 15.  Only relevant for \n"
  "                  masking using RepeatMasker .out files.\n"
  "   -dots=N        Output dot every N sequences to show program's progress.\n"
  "   -trimT         Trim leading poly-T.\n"
  "   -noTrimA       Don't trim trailing poly-A.\n"
  "   -trimHardA     Remove poly-A tail from qSize as well as alignments in \n"
  "                  psl output.\n"
  "   -fastMap       Run for fast DNA/DNA remapping - not allowing introns, \n"
  "                  requiring high %%ID. Query sizes must not exceed %d.\n"
  "   -out=type      Controls output file format.  Type is one of:\n"
  "                    psl - Default.  Tab-separated format, no sequence\n"
  "                    pslx - Tab-separated format with sequence\n"
  "                    axt - blastz-associated axt format\n"
  "                    maf - multiz-associated maf format\n"
  "                    sim4 - similar to sim4 format\n"
  "                    wublast - similar to wublast format\n"
  "                    blast - similar to NCBI blast format\n"
  "                    blast8- NCBI blast tabular format\n"
  "                    blast9 - NCBI blast tabular format with comments\n"
  "   -fine          For high-quality mRNAs, look harder for small initial and\n"
  "                  terminal exons.  Not recommended for ESTs.\n"
  "   -maxIntron=N  Sets maximum intron size. Default is %d.\n"
  "   -extendThroughN  Allows extension of alignment through large blocks of Ns.\n"
  , gfVersion, MAXSINGLEPIECESIZE, ffIntronMaxDefault
  );
exit(-1);
}


struct optionSpec options[] = {
   {"t", OPTION_STRING},
   {"q", OPTION_STRING},
   {"prot", OPTION_BOOLEAN},
   {"ooc", OPTION_STRING},
   {"tileSize", OPTION_INT},
   {"stepSize", OPTION_INT},
   {"oneOff", OPTION_INT},
   {"minMatch", OPTION_INT},
   {"minScore", OPTION_INT},
   {"minIdentity", OPTION_FLOAT},
   {"maxGap", OPTION_INT},
   {"noHead", OPTION_BOOLEAN},
   {"makeOoc", OPTION_STRING},
   {"repMatch", OPTION_INT},
   {"mask", OPTION_STRING},
   {"qMask", OPTION_STRING},
   {"repeats", OPTION_STRING},
   {"minRepDivergence", OPTION_FLOAT},
   {"dots", OPTION_INT},
   {"trimT", OPTION_BOOLEAN},
   {"noTrimA", OPTION_BOOLEAN},
   {"trimHardA", OPTION_BOOLEAN},
   {"fastMap", OPTION_BOOLEAN},
   {"out", OPTION_STRING},
   {"fine", OPTION_BOOLEAN},
   {"maxIntron", OPTION_INT},
   {"extendThroughN", OPTION_BOOLEAN},
   {NULL, 0},
};


/* Stuff to support various output formats. */
struct gfOutput *gvo;		/* Overall output controller */

void searchOneStrand(struct dnaSeq *seq, struct genoFind *gf, FILE *psl, 
	boolean isRc, struct hash *maskHash, Bits *qMaskBits)
/* Search for seq in index, align it, and write results to psl. */
{
if (fastMap && (seq->size > MAXSINGLEPIECESIZE))
    errAbort("Maximum single piece size (%d) exceeded by query %s of size (%d). "
	"Larger pieces will have to be split up until no larger than this limit "
	"when the -fastMap option is used."	
	, MAXSINGLEPIECESIZE, seq->name, seq->size);
gfLongDnaInMem(seq, gf, isRc, minScore, qMaskBits, gvo, fastMap, optionExists("fine"));
}


void searchOneProt(aaSeq *seq, struct genoFind *gf, FILE *f)
/* Search for protein seq in index and write results to psl. */
{
int hitCount;
struct lm *lm = lmInit(0);
struct gfClump *clumpList = gfFindClumps(gf, seq, lm, &hitCount);
gfAlignAaClumps(gf, clumpList, seq, FALSE, minScore, gvo);
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
    }
else
    {
    gvo->maskHash = maskHash;
    searchOneStrand(seq, gf, f, FALSE, maskHash, qMaskBits);
    reverseComplement(seq->dna, seq->size);
    searchOneStrand(seq, gf, f, TRUE, maskHash, qMaskBits);
    reverseComplement(seq->dna, seq->size);
    }
gfOutputQuery(gvo, f);
}

void trimSeq(struct dnaSeq *seq, struct dnaSeq *trimmed)
/* Copy seq to trimmed (shallow copy) and optionally trim
 * off polyA tail or polyT head. */
{
DNA *dna = seq->dna;
int size = seq->size;
*trimmed = *seq;
if (trimT)
    maskHeadPolyT(dna, size);
if (trimA || trimHardA)
    {
    int trimSize = maskTailPolyA(dna, size);
    if (trimHardA)
	{
	trimmed->size -= trimSize;
	dna[size-trimSize] = 0;
	}
    }
}


Bits *maskQuerySeq(struct dnaSeq *seq, boolean isProt, 
	boolean maskQuery, boolean lcMask)
/* Massage query sequence a bit, converting it to correct
 * case (upper for protein/lower for DNA) and optionally
 * returning upper/lower case info , and trimming poly A. */
{
Bits *qMaskBits = NULL;
verbose(2, "%s\n", seq->name);
if (isProt)
    faToProtein(seq->dna, seq->size);
else
    {
    if (maskQuery)
	{
	if (lcMask)
	    toggleCase(seq->dna, seq->size);
	qMaskBits = maskFromUpperCaseSeq(seq);
	}
    faToDna(seq->dna, seq->size);
    }
if (seq->size > qWarnSize)
    {
    warn("Query sequence %s has size %d, it might take a while.",
	 seq->name, seq->size);
    }
return qMaskBits;
}
	    
void searchOneMaskTrim(struct dnaSeq *seq, boolean isProt,
		       struct genoFind *gf, FILE *outFile,
		       struct hash *maskHash,
		       long long *retTotalSize, int *retCount)
/* Search a single sequence against a single genoFind index. */
{
boolean maskQuery = (qMask != NULL);
boolean lcMask = (qMask != NULL && sameWord(qMask, "lower"));
Bits *qMaskBits = maskQuerySeq(seq, isProt, maskQuery, lcMask);
struct dnaSeq trimmedSeq;
ZeroVar(&trimmedSeq);
trimSeq(seq, &trimmedSeq);
if (qType == gftRna || qType == gftRnaX)
   memSwapChar(trimmedSeq.dna, trimmedSeq.size, 'u', 't');
searchOne(&trimmedSeq, gf, outFile, isProt, maskHash, qMaskBits);
*retTotalSize += seq->size;
*retCount += 1;
bitFree(&qMaskBits);
}

void searchOneIndex(int fileCount, char *files[], struct genoFind *gf, char *outName, 
	boolean isProt, struct hash *maskHash, FILE *outFile, boolean showStatus)
/* Search all sequences in all files against single genoFind index. */
{
int i;
char *fileName;
int count = 0; 
long long totalSize = 0;

gfOutputHead(gvo, outFile);
for (i=0; i<fileCount; ++i)
    {
    fileName = files[i];
    if (nibIsFile(fileName))
        {
	struct dnaSeq *seq;

	if (isProt)
	    errAbort("%s: Can't use .nib files with -prot or d=prot option\n", fileName);
	seq = nibLoadAllMasked(NIB_MASK_MIXED, fileName);
	freez(&seq->name);
	seq->name = cloneString(fileName);
	searchOneMaskTrim(seq, isProt, gf, outFile,
			  maskHash, &totalSize, &count);
	freeDnaSeq(&seq);
	}
    else if (twoBitIsSpec(fileName))
	{
	struct twoBitSpec *tbs = twoBitSpecNew(fileName);
	struct twoBitFile *tbf = twoBitOpen(tbs->fileName);
	if (isProt)
	    errAbort("%s is a two bit file, which doesn't work for proteins.", 
	    	fileName);
	if (tbs->seqs != NULL)
	    {
	    struct twoBitSeqSpec *ss = NULL;
	    for (ss = tbs->seqs;  ss != NULL;  ss = ss->next)
		{
		struct dnaSeq *seq = twoBitReadSeqFrag(tbf, ss->name,
						       ss->start, ss->end);
		searchOneMaskTrim(seq, isProt, gf, outFile,
				  maskHash, &totalSize, &count);
		dnaSeqFree(&seq);
		}
	    }
	else
	    {
	    struct twoBitIndex *index = NULL;
	    for (index = tbf->indexList; index != NULL; index = index->next)
		{
		struct dnaSeq *seq = twoBitReadSeqFrag(tbf, index->name, 0, 0);
		searchOneMaskTrim(seq, isProt, gf, outFile,
				  maskHash, &totalSize, &count);
		dnaSeqFree(&seq);
		}
	    }
	twoBitClose(&tbf);
	}
    else
        {
	static struct dnaSeq seq;
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	    {
	    searchOneMaskTrim(&seq, isProt, gf, outFile,
			      maskHash, &totalSize, &count);
	    }
	lineFileClose(&lf);
	}
    }
carefulClose(&outFile);
if (showStatus)
    printf("Searched %lld bases in %d sequences\n", totalSize, count);
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
gvo->reportTargetStrand = TRUE;
gfFindAlignAaTrans(gfs, qSeq, t3Hash, dbIsRc, minScore, gvo);
}

void transTripleSearch(struct dnaSeq *qSeq, struct genoFind *gfs[3], struct hash *t3Hash, 
	boolean dbIsRc, boolean qIsDna, FILE *f)
/* Translate qSeq three ways and look for each in three frames of index. */
{
int qIsRc;
gvo->reportTargetStrand = TRUE;
for (qIsRc = 0; qIsRc <= qIsDna; qIsRc += 1)
    {
    gfLongTransTransInMem(qSeq, gfs, t3Hash, qIsRc, dbIsRc, !qIsDna, minScore, gvo);
    if (qIsDna)
        reverseComplement(qSeq->dna, qSeq->size);
    }
}

void bigBlat(struct dnaSeq *untransList, int queryCount, char *queryFiles[], char *outFile, boolean transQuery, boolean qIsDna, FILE *out, boolean showStatus)
/* Run query against translated DNA database (3 frames on each strand). */
{
int frame, i;
struct dnaSeq *seq, trimmedSeq;
struct genoFind *gfs[3];
aaSeq *dbSeqLists[3];
struct trans3 *t3List = NULL;
int isRc;
struct lineFile *lf = NULL;
struct hash *t3Hash = NULL;
boolean forceUpper = FALSE;
boolean forceLower = FALSE;
boolean toggle = FALSE;
boolean maskUpper = FALSE;

ZeroVar(&trimmedSeq);
if (showStatus)
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

if (gvo->fileHead != NULL)
    gvo->fileHead(gvo, out);

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
		repMatch, ooc, TRUE, oneOff, FALSE, stepSize);
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
		warn("Query sequence %s has size %d, it might take a while.",
		     qSeq.name, qSeq.size);
		}
	    trimSeq(&qSeq, &trimmedSeq);
	    if (transQuery)
	        transTripleSearch(&trimmedSeq, gfs, t3Hash, isRc, qIsDna, out);
	    else
		tripleSearch(&trimmedSeq, gfs, t3Hash, isRc, out);
	    gfOutputQuery(gvo, out);
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
struct genoFind *gf;
boolean tIsProt = (tType == gftProt);
boolean qIsProt = (qType == gftProt);
boolean bothSimpleNuc = (tType == gftDna && (qType == gftDna || qType == gftRna));
boolean bothSimpleProt = (tIsProt && qIsProt);
FILE *f = mustOpen(outName, "w");
boolean showStatus = (f != stdout);

databaseName = dbFile;
gfClientFileArray(dbFile, &dbFiles, &dbCount);
if (makeOoc != NULL)
    {
    gfMakeOoc(makeOoc, dbFiles, dbCount, tileSize, repMatch, tType);
    if (showStatus)
	printf("Done making %s\n", makeOoc);
    exit(0);
    }
gfClientFileArray(queryFile, &queryFiles, &queryCount);
dbSeqList = gfClientSeqList(dbCount, dbFiles, tIsProt, tType == gftDnaX, repeats, 
	minRepDivergence, showStatus);
databaseSeqCount = slCount(dbSeqList);
for (seq = dbSeqList; seq != NULL; seq = seq->next)
    databaseLetters += seq->size;

gvo = gfOutputAny(outputFormat, minIdentity*10, qIsProt, tIsProt, noHead, 
	databaseName, databaseSeqCount, databaseLetters, minIdentity, f);

if (bothSimpleNuc || bothSimpleProt)
    {
    struct hash *maskHash = NULL;

    /* Save away masking info for output. */
    if (repeats != NULL)
	{
	maskHash = newHash(0);
	for (seq = dbSeqList; seq != NULL; seq = seq->next)
	    {
	    Bits *maskedBits = maskFromUpperCaseSeq(seq);
	    hashAdd(maskHash, seq->name, maskedBits);
	    }
	}

    /* Handle masking and indexing.  If masking is off, we want the indexer
     * to see unmasked sequence, otherwise we want it to see masked.  However
     * after indexing we always want it unmasked, because things are always
     * unmasked for the extension phase. */
    if (mask == NULL && !bothSimpleProt)
        gfClientUnmask(dbSeqList);
    gf = gfIndexSeq(dbSeqList, minMatch, maxGap, tileSize, repMatch, ooc, 
    	tIsProt, oneOff, FALSE, stepSize);
    if (mask != NULL)
        gfClientUnmask(dbSeqList);

    searchOneIndex(queryCount, queryFiles, gf, outName, tIsProt, maskHash, f, showStatus);
    freeHash(&maskHash);
    }
else if (tType == gftDnaX && qType == gftProt)
    {
    bigBlat(dbSeqList, queryCount, queryFiles, outName, FALSE, TRUE, f, showStatus);
    }
else if (tType == gftDnaX && (qType == gftDnaX || qType == gftRnaX))
    {
    bigBlat(dbSeqList, queryCount, queryFiles, outName, TRUE, qType == gftDnaX, f, showStatus);
    }
else
    {
    errAbort("Unrecognized combination of target and query types\n");
    }
if (dotEvery > 0)
    printf("\n");
freeDnaSeqList(&dbSeqList);
}

int main(int argc, char *argv[])
/* Process command line into global variables and call blat. */
{
boolean tIsProtLike, qIsProtLike;

#ifdef DEBUG
{
char *cmd = "blat hCrea.geno hCrea.mrna foo.psl -t=dnax -q=rnax";
char *words[16];

printf("Debugging parameters\n");
cmd = cloneString(cmd);
argc = chopLine(cmd, words);
argv = words;
}
#endif /* DEBUG */

optionInit(&argc, argv, options);
if (argc != 4)
    usage();

/* Get database and query sequence types and make sure they are
 * legal and compatable. */
if (optionExists("prot"))
    qType = tType = gftProt;
if (optionExists("t"))
    tType = gfTypeFromName(optionVal("t", NULL));
trimA = optionExists("trimA") || optionExists("trima");
trimT = optionExists("trimT") || optionExists("trimt");
trimHardA = optionExists("trimHardA");
switch (tType)
    {
    case gftProt:
    case gftDnaX:
        tIsProtLike = TRUE;
	break;
    case gftDna:
        tIsProtLike = FALSE;
	break;
    default:
	tIsProtLike = FALSE;
        errAbort("Illegal value for 't' parameter");
	break;
    }
if (optionExists("q"))
    qType = gfTypeFromName(optionVal("q", NULL));
if (qType == gftRnaX || qType == gftRna)
    trimA = TRUE;
if (optionExists("noTrimA"))
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
if ((tIsProtLike ^ qIsProtLike) != 0)
    errAbort("t and q must both be either protein or dna");

/* Set default tile size for protein-based comparisons. */
if (tIsProtLike)
    {
    tileSize = 5;
    minMatch = 1;
    oneOff = FALSE;
    maxGap = 0;
    }

/* Get tile size and related parameters from user and make sure
 * they are within range. */
tileSize = optionInt("tileSize", tileSize);
stepSize = optionInt("stepSize", tileSize);
minMatch = optionInt("minMatch", minMatch);
oneOff = optionExists("oneOff");
fastMap = optionExists("fastMap");
minScore = optionInt("minScore", minScore);
maxGap = optionInt("maxGap", maxGap);
minRepDivergence = optionFloat("minRepDivergence", minRepDivergence);
minIdentity = optionFloat("minIdentity", minIdentity);
gfCheckTileSize(tileSize, tIsProtLike);
if (minMatch < 0)
    errAbort("minMatch must be at least 1");
if (maxGap > 100)
    errAbort("maxGap must be less than 100");



/* Set repMatch parameter from command line, or
 * to reasonable value that depends on tile size. */
if (optionExists("repMatch"))
    repMatch = optionInt("repMatch", repMatch);
else
    repMatch = gfDefaultRepMatch(tileSize, stepSize, tIsProtLike);

/* Gather last few command line options. */
noHead = optionExists("noHead");
ooc = optionVal("ooc", NULL);
makeOoc = optionVal("makeOoc", NULL);
mask = optionVal("mask", NULL);
qMask = optionVal("qMask", NULL);
repeats = optionVal("repeats", NULL);
if (repeats != NULL && mask != NULL && differentString(repeats, mask))
    errAbort("The -mask and -repeat settings disagree.  "
             "You can just omit -repeat if -mask is on");
if (mask != NULL)	/* Mask setting will also set repeats. */
    repeats = mask;
outputFormat = optionVal("out", outputFormat);
dotEvery = optionInt("dots", 0);
/* set global for fuzzy find functions */
setFfIntronMax(optionInt("maxIntron", ffIntronMaxDefault));
setFfExtendThroughN(optionExists("extendThroughN"));


/* Call routine that does the work. */
blat(argv[1], argv[2], argv[3]);
return 0;
}
