/* pslToPslx - Convert from psl to xa alignment format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "dnaseq.h"
#include "fa.h"
#include "nibTwo.h"
#include "psl.h"


/* command line options */
static struct optionSpec optionSpecs[] = {
    {"masked", OPTION_BOOLEAN},
    {NULL, 0}
};
static boolean gMasked = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslToPslx - Convert from psl to pslx format, which includes sequences\n"
  "usage:\n"
  "   pslToPslx [options] in.psl qSeqSpec tSeqSpec out.pslx\n"
  "\n"
  "qSeqSpec and tSeqSpec can be nib directory, a 2bit file, or a FASTA file.\n"
  "FASTA files should end in .fa, .fa.gz, .fa.Z, or .fa.bz2 and are read into\n"
  "memory.\n"
  "\n"
  "Options:\n"
  "  -masked - if specified, repeats are in lower case cases, otherwise entire\n"
  "            sequence is loader case.\n"
  );
}

struct seqReader
/* Access to sequences from a seqSpec */
{
    struct seqFile *next;
    char *spec;                  /* file specification */
    boolean isFasta;
    struct nibTwoCache *nibTwo;  /* object for accessing nib or 2bit files */
    struct hash *seqTbl;       /* hash of dnaSeq objects from a FASTA file */
    };

static void seqReaderLoadFasta(struct seqReader *seqReader)
/* load a FASTA file into a seqReader object */
{
seqReader->seqTbl = hashNew(21);
struct dnaSeq *seq, *seqs = faReadAllMixed(seqReader->spec);
while ((seq = slPopHead(&seqs)) != NULL)
    hashAdd(seqReader->seqTbl, seq->name, seq);
}

static struct seqReader *seqReaderNew(char *seqSpec)
/* construct a new seqReader object */
{
struct seqReader *seqReader;
AllocVar(seqReader);
seqReader->spec = cloneString(seqSpec);
if (endsWith(seqSpec, ".fa") || endsWith(seqSpec, ".fa.gz") || endsWith(seqSpec, ".fa.Z") || endsWith(seqSpec, ".fa.bz2"))
    seqReaderLoadFasta(seqReader);
else
    seqReader->nibTwo = nibTwoCacheNew(seqSpec);
return seqReader;
}

static struct dnaSeq *seqReaderTblGet(struct seqReader *seqReader, char *seqName, int start, int end,
                                      int *retFullSeqSize)
/* get a sequence from the seqTbl */
{
struct dnaSeq *fullSeq = hashFindVal(seqReader->seqTbl, seqName);
if (fullSeq == NULL)
    errAbort("can't find sequence %s in %s", seqName, seqReader->spec);
if (end > fullSeq->size)
    errAbort("range %d-%d longer than sequence %s length %d in %s",
             start, end, seqName, fullSeq->size, seqReader->spec);
if (retFullSeqSize != NULL)
    *retFullSeqSize = fullSeq->size;
int len = (end-start);
struct dnaSeq *seq = newDnaSeq(cloneStringZ(fullSeq->dna+start, len), len, seqName);
if (!gMasked)
    tolowers(seq->dna);
return seq;
}

static struct dnaSeq *seqReaderGet(struct seqReader *seqReader, char *seqName, int start, int end,
                                   int *retFullSeqSize)
/* return part of a sequence from a seqReader.  dnaSeqFree result when
 * complete. */
{
assert(start <= end);
if (seqReader->nibTwo != NULL)
    return nibTwoCacheSeqPartExt(seqReader->nibTwo, seqName, start, end-start, gMasked, retFullSeqSize);
else
    return seqReaderTblGet(seqReader, seqName, start, end, retFullSeqSize);
}

static void outputBlocks(FILE *pslOutFh, struct seqReader *seqReader, char *seqName, char strand,
                         int start, int end, int blockCount, unsigned *starts, unsigned *sizes)
/* output sequences for of the sequence columns */
{
int fullSeqSize = 0;;
struct dnaSeq *seq = seqReaderGet(seqReader, seqName, start, end, &fullSeqSize);
int seqOff = start;
if (strand == '-')
    {
    reverseComplement(seq->dna, seq->size);
    seqOff = fullSeqSize - end;
    }
int iBlk;
for (iBlk = 0; iBlk < blockCount; iBlk++)
    {
    mustWrite(pslOutFh, seq->dna+starts[iBlk]-seqOff, sizes[iBlk]);
    fputc(',', pslOutFh);
    }
dnaSeqFree(&seq);
}

static void writePslx(FILE *pslOutFh, struct seqReader *qSeqReader, struct seqReader *tSeqReader, struct psl *psl)
/* output a pslx, cheating in not creating the whole pslx to avoid more sequence
 * manipulation */
{
if (pslIsProtein(psl))
    errAbort("doesn't support protein PSLs: qName: %s", psl->qName);
pslOutput(psl, pslOutFh, '\t', '\t');
outputBlocks(pslOutFh, qSeqReader, psl->qName, psl->strand[0],
             psl->qStart, psl->qEnd, psl->blockCount, psl->qStarts, psl->blockSizes);
fputc('\t', pslOutFh);
outputBlocks(pslOutFh, tSeqReader, psl->tName, psl->strand[1],
             psl->tStart, psl->tEnd, psl->blockCount, psl->tStarts, psl->blockSizes);
fputc('\n', pslOutFh);
}

static void pslToPslx(char *inPslFile, char *qSeqSpec, char *tSeqSpec, char *outPslFile)
/* pslToPslx - Convert from psl to pslx alignment format. */
{
struct lineFile *pslInLf = pslFileOpen(inPslFile);
struct seqReader *qSeqReader = seqReaderNew(qSeqSpec);
struct seqReader *tSeqReader = seqReaderNew(tSeqSpec);
FILE *pslOutFh = mustOpen(outPslFile, "w");
struct psl *psl;
while ((psl = pslNext(pslInLf)) != NULL)
    {
    writePslx(pslOutFh, qSeqReader, tSeqReader, psl);
    pslFree(&psl);
    }
lineFileClose(&pslInLf);
carefulClose(&pslOutFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
gMasked = optionExists("masked");
pslToPslx(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
