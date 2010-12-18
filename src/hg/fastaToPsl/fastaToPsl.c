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

static char const rcsid[] = "$Id: spideyToPsl.c,v 1.4 2008/02/21 02:13:24 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fastaToPsl - Convert FASTA pair alignments to PSL format\n"
  "usage:\n"
  "   fastaToPsl [options] in.fa out.psl\n"
  "\n"
  "where in.fa is the FASTA pair alignment. \n"
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
char *qHeader;

DNA *tSeq;
int tSize;
char *tHeader;

inLF  = lineFileOpen(inName, TRUE);

/* read the query sequence */
read = faMixedSpeedReadNext(inLF, &tmpSeq, &qSize, &tmpHeader);
if (!read) errAbort("Could not read query FASTA entry.");
qSeq    = cloneString(tmpSeq);
qHeader = cloneString(tmpHeader);

/* read the target sequence */
read = faMixedSpeedReadNext(inLF, &tmpSeq, &tSize, &tmpHeader);
if (!read) errAbort("Could not read target FASTA entry.");
tSeq    = cloneString(tmpSeq);
tHeader = cloneString(tmpHeader);

/* try to read next sequence to see if there is one */
read = faMixedSpeedReadNext(inLF, &tmpSeq, &tmpSize, &tmpHeader);
if (read) warn("WARN: Found more than two FASTA entries, only processing the first two.");

pslAlign = pslFromAlign(qHeader, countNonDash(qSeq, qSize), 0, qSize, qSeq,
                        tHeader, countNonDash(tSeq, tSize), 0, tSize, tSeq,
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
