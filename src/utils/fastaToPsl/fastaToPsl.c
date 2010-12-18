/* fastaToPsl - Convert FASTA alignments to PSL format. */
#include "common.h"
#include "fa.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "errabort.h"
#include "psl.h"
#include "sqlNum.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fastaToPsl - Convert FASTA pair alignments to PSL format\n"
  "usage:\n"
  "   fastaToPsl [options] in.fasta out.psl\n"
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
DNA *tmpSeq;
char *tmpHeader;
int tmpSize;
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

inLF  = lineFileOpen(inName, TRUE);

/* read the query sequence */
read = faMixedSpeedReadNext(inLF, &tmpSeq, &qSize, &tmpHeader);
if (!read)
    errAbort("Could not read query FASTA entry.");
qSeq    = cloneString(tmpSeq);
qSeqLen = countNonDash(qSeq, qSize);
qHeader = cloneString(tmpHeader);
verbose(2, "Query sequence header: %s\n", qHeader);
verbose(3, "Query sequence alignment length: %d\n", qSize);
verbose(3, "Query sequence length: %d\n", qSeqLen);

/* read the target sequence */
read = faMixedSpeedReadNext(inLF, &tmpSeq, &tSize, &tmpHeader);
if (!read)
    errAbort("Could not read target FASTA entry.");
tSeq    = cloneString(tmpSeq);
tSeqLen = countNonDash(tSeq, tSize);
tHeader = cloneString(tmpHeader);
verbose(2, "Target sequence header: %s\n", tHeader);
verbose(3, "Target sequence alignment length: %d\n", tSize);
verbose(3, "Target sequence length: %d\n", tSeqLen);

/* try to read next sequence to see if there is one */
read = faMixedSpeedReadNext(inLF, &tmpSeq, &tmpSize, &tmpHeader);
if (read)
    verbose(0, "Found more than two FASTA entries, only processing the first two.\n");

pslAlign = pslFromAlign(qHeader, qSeqLen, 0, qSeqLen, qSeq,
                        tHeader, tSeqLen, 0, tSeqLen, tSeq,
                        "+", 0);
pslWriteAll(pslAlign, outName, TRUE);

lineFileClose(&inLF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 3)
    usage();
//gUntranslated = optionExists("untranslated");
fastaToPsl(argv[1], argv[2]);
return 0;
}
