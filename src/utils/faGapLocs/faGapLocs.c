/* faGapLocs - report location of gaps and sequences in a FASTA file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "fa.h"
#include "dnautil.h"
#include "options.h"


/* command line options */
static struct optionSpec optionSpecs[] =
{
    {NULL, 0}
};

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "faGapLocs - report location of gaps and sequences in a FASTA file\n"
  "usage:\n"
  "   faGapLocs in.fa out.lift\n"
  "options:\n"

  "Output is in the format of a lift file, with entries named in the form:\n"
  "   seqid_gap_N\n"
  "   seqid_blk_N\n"
  );
}

static int processGap(char *seqId, int seqSize, DNA *seq, int iSeq, int nGap, FILE *liftFh)
/* write out one gap and return next position in sequence */
{
int len = strspn(seq+iSeq, "n");
fprintf(liftFh, "%d\t%s_gap_%d\t%d\t%s\t%d\n",
        iSeq, seqId, nGap, len, seqId, seqSize);
return iSeq+len;
}

static int processBlk(char *seqId, int seqSize, DNA *seq, int iSeq, int nBlk, FILE *liftFh)
/* write out one block of sequence and return next position in sequence */
{
int len = strcspn(seq+iSeq, "n");
fprintf(liftFh, "%d\t%s_seq_%d\t%d\t%s\t%d\n",
        iSeq, seqId, nBlk, len, seqId, seqSize);
return iSeq+len;
}


static void processSeq(char *seqId, int seqSize, DNA *seq, FILE *liftFh)
/* process one fasta sequence, writing gap and blk information to file */
{
int nGap = 0, nBlk = 0, iSeq;

for (iSeq = 0; iSeq < seqSize; )
    {
    if (seq[iSeq] == 'n')
        iSeq = processGap(seqId, seqSize, seq, iSeq, nGap++, liftFh);
    else
        iSeq = processBlk(seqId, seqSize, seq, iSeq, nBlk++, liftFh);
    }
}

static void faGapLocs(char *faIn, char* liftOut)
/* faGapLocs - report location of gaps and sequences in a FASTA file. */
{
struct lineFile *faLf = lineFileOpen(faIn, TRUE);
FILE *liftFh = mustOpen(liftOut, "w");
char *seqId;
int seqSize;
DNA *seq;

while (faSpeedReadNext(faLf, &seq, &seqSize, &seqId))
    processSeq(seqId, seqSize, seq, liftFh);

carefulClose(&liftFh);
lineFileClose(&faLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
faGapLocs(argv[1], argv[2]);
return 0;
}
