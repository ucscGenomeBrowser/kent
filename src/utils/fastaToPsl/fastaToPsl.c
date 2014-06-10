/* fastaToPsl - Convert FASTA alignments to PSL format. */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "fa.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "errAbort.h"
#include "psl.h"
#include "sqlNum.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fastaToPsl - Convert FASTA pair alignments to PSL format\n"
  "usage:\n"
  "   fastaToPsl  in.fasta out.psl\n"
  "\n"
  "If there are more than two sequences in the FASTA alignment.\n"
  "the first sequence is use as the query and the rest of the\n"
  "sequences are iterated over as the target to create a series\n"
  "of pair-wise alignments.\n"
  );
}


/* command line */
static struct optionSpec optionSpec[] = {
    {NULL, 0}
};

void fastaToPsl(char *inName, char *outName)
/* fastaToPsl - Convert axt to psl format. */
{
struct lineFile *inLF;
FILE *outFh;
boolean read;
struct psl* pslAlign;

DNA *qSeq;
int qSize;
int qSeqLen;
char *qHeader;

DNA *tSeq;
int tSize;
int tSeqLen;
char *tHeader;

int queryCounter;

inLF  = lineFileOpen(inName, TRUE);
outFh = mustOpen(outName, "w");

/* read the target sequence */
read = faMixedSpeedReadNext(inLF, &qSeq, &qSize, &qHeader);
if (!read)
    errAbort("Could not read target FASTA entry.");
qSeq    = cloneString(qSeq);
qSeqLen = countNonDash(qSeq, qSize);
qHeader = cloneString(qHeader);
verbose(2, "Query sequence header: %s\n", qHeader);
verbose(3, "Query sequence alignment length: %d\n", qSize);
verbose(3, "Query sequence length: %d\n", qSeqLen);
verbose(4, "Query sequence: %s\n", qSeq);

/* read the rest of the sequences */
queryCounter = 1;
pslWriteHead(outFh);
while (faMixedSpeedReadNext(inLF, &tSeq, &tSize, &tHeader))
    {
    tSeqLen = countNonDash(tSeq, tSize);

    verbose(2, "Target sequence (%d) header: %s\n", queryCounter, tHeader);
    verbose(3, "Target sequence (%d) length: %d\n", queryCounter, tSeqLen);
    verbose(4, "Target sequence (%d): %s\n", queryCounter, tSeq);

    pslAlign = pslFromAlign(qHeader, qSeqLen, 0, qSeqLen, qSeq,
                            tHeader, tSeqLen, 0, tSeqLen, tSeq,
                            "+", 0);
    pslTabOut(pslAlign, outFh);

    ++queryCounter;
    }

lineFileClose(&inLF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 3)
    usage();
fastaToPsl(argv[1], argv[2]);
return 0;
}
