/* fetchSeq  */
/* Input is bed 6. */
/* Output adds sequence as column 7. */
/* The key thing here is doing a reverse complement for data on the negative strand. */
/* Not checking for coords off the end of the chrom. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dnaseq.h"
#include "linefile.h"
#include "sqlNum.h"
#include "twoBit.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
    "fetchSeq - append sequence to bed 6\n"
    "usage:\n"
    "    fetchSeq input sequence output\n");
}


void doFetch(char *inputFileName, char *sequenceFileName, char *outputFileName)
/* lookup sequence for each line */
{
struct lineFile *lf = NULL;
char *line;
char *row[6];
int elementCount;
struct twoBitFile *tbf;

char *fileChrom = NULL;
int start = 0;
int end = 0;
char *name = NULL;
int score = 0;
char *strand = NULL;

struct dnaSeq *chunk = NULL;

FILE *outputFileHandle = mustOpen(outputFileName, "w");

tbf = twoBitOpen(sequenceFileName);

lf = lineFileOpen(inputFileName, TRUE);
while (lineFileNext(lf, &line, NULL))
    {
    elementCount = chopString(line, "\t", row, ArraySize(row));
    if (elementCount != 6) continue;

    fileChrom = cloneString(row[0]);
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    name = cloneString(row[3]);
    score = sqlUnsigned(row[4]);
    strand = cloneString(row[5]);

    if (start == end) continue;
    assert (end > start);

    chunk = twoBitReadSeqFrag(tbf, fileChrom, start, end);
    touppers(chunk->dna);
    if (sameString(strand, "-"))
        reverseComplement(chunk->dna, chunk->size);
    fprintf(outputFileHandle, "%s\t%d\t%d\t%s\t%d\t%s\t%s\n", fileChrom, start, end, name, score, strand, chunk->dna);
    dnaSeqFree(&chunk);
    }

lineFileClose(&lf);
carefulClose(&outputFileHandle);
}


int main(int argc, char *argv[])
/* read bed 6 file, look up sequence */
{
if (argc != 4)
    usage();

if (!fileExists(argv[1]))
    errAbort("can't find %s\n", argv[1]);
if (!fileExists(argv[2]))
    errAbort("can't find %s\n", argv[2]);

doFetch(argv[1], argv[2], argv[3]);

return 0;
}
