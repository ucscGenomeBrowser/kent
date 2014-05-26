/* faToTab - convert fa file into tab separated file */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "linefile.h"
#include "fa.h"

int clDots = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToTab - convert fa file to tab separated file\n"
  "usage:\n"
  "   faToTab infileName outFileName\n"
  "options:\n"
  "     -type=seqType   sequence type, dna or protein, default is dna\n"
  "     -keepAccSuffix - don't strip dot version off of sequence id, keep as is\n");
}

void dotOut()
/* Print dot to standard error and flush it so user can
 * see it right away. */
 {
 fputc('.', stderr);
 fflush(stderr);
 }

char *unburyAcc(struct lineFile *lf, char *longNcbiName)
/* Return accession given a long style NCBI name.  
 * Cannibalizes the longNcbiName. */
{
char *parts[5];
int partCount;

partCount = chopByChar(longNcbiName, '|', parts, ArraySize(parts));
if (partCount < 4) 
    errAbort("Badly formed longNcbiName line %d of %s\n", lf->lineIx, lf->fileName);
chopSuffix(parts[3]);
return parts[3];
}

char *accWithoutSuffix(char *acc) 
/* 
Function to strip the suffix from an accession in order to make it consistent
with our notation here. We ignore the suffix.
Eg. NM_123456.1 becomes NM_123456
*/
{
char *fixedAcc = acc;
char *dotIndex = strchr(acc, '.');

if (dotIndex)
    {
    char *accNum = NULL;
    int dotPos = dotIndex - acc; /* stupid C pointer arith. No other way to do get the string
                                    length up to the period. */
    accNum = needMem(dotPos + 1);
    strncpy(accNum, acc, dotPos);
    accNum[dotPos] = 0; /* Null terminate */
    fixedAcc = accNum;
    }

return fixedAcc;
}

void writeSeqTable(char *faName, FILE *out, boolean unburyAccession, boolean isDna,
                   boolean keepAccSuffix)
/* Write out contents of fa file to name/sequence pairs in tabbed out file. */
{
struct lineFile *lf = lineFileOpen(faName, TRUE);
bioSeq seq;
int dotMod = 0;

fprintf(stderr, "Reading %s\n", faName);
while (faSomeSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name, isDna))
    {
    if (clDots > 0 && ++dotMod == clDots )
        {
	dotMod = 0;
	dotOut();
	}
    if (unburyAccession) 
        {
	seq.name = unburyAcc(lf, seq.name);
        }
    if (!keepAccSuffix)
        seq.name = accWithoutSuffix(seq.name);
    fprintf(out, "%s\t%s\n", seq.name, seq.dna);
    }
if (clDots) fprintf(stderr, "\n");
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
static struct optionSpec optionSpecs[] = {
    {"type", OPTION_STRING},
    {"keepAccSuffix", OPTION_BOOLEAN},
    {NULL, 0}
    };
char *infileName, *outfileName;
FILE *outf;
char *seqType = "dna";        /* sequence type */

if (argc < 3) usage();
optionInit(&argc, argv, optionSpecs);

infileName  = argv[1];
outfileName = argv[2];
seqType = optionVal("type", seqType);
boolean keepAccSuffix = optionExists("keepAccSuffix");


outf = mustOpen(outfileName,  "w");
writeSeqTable(infileName, outf, FALSE, sameWord(seqType, "dna"),
              keepAccSuffix);
fclose(outf);
fprintf(stderr, "%s created.\n", outfileName);
return 0;
}
