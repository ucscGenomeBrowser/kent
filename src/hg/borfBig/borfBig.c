/* borfBig - Run Victor Solovyev's bestorf repeatedly. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "fa.h"
#include "dystring.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "borfBig - Run Victor Solovyev's bestorf repeatedly\n"
  "usage:\n"
  "   borfBig in.fa out.tab\n"
  "options:\n"
  "   -exe=borf - where exe file belongs\n"
  "   -tmpFa=file - where to put temp .fa file\n"
  "   -tmpOrf=file - where to put temp bestorf output file\n"
  );
}

char *mustFindLine(struct lineFile *lf, char *pat)
/* Find line that starts (after skipping white space)
 * with pattern or die trying.  Skip over pat in return*/
{
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (startsWith(pat, line))
	{
	line += strlen(pat);
	return skipLeadingSpaces(line);
	}
    }
errAbort("Couldn't find %s in %s\n", pat, lf->fileName);
return NULL;
}

void convertBorfOutput(char *fileName, FILE *f)
/* Convert bestorf output to our database format. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line = mustFindLine(lf, "BESTORF");
char *word;
char *row[13];
struct dnaSeq seq;
int seqSize;
ZeroVar(&seq);

line = mustFindLine(lf, "Seq name:");
word = nextWord(&line);
fprintf(f, "%s\t", word);                            /* Name */
line = mustFindLine(lf, "Length of sequence:");
seqSize = atoi(line);                                /* Size */
fprintf(f, "%d\t", seqSize);
lineFileNeedNext(lf, &line, NULL);
line = skipLeadingSpaces(line);
if (startsWith("no reliable", line))
    {
    fprintf(f, ".\tn/a\t0\t0\t0.0\t0\t0\t0\t0\t\n");
    }
else
    {
    line = mustFindLine(lf, "G Str F");
    lineFileSkip(lf, 1);
    lineFileRow(lf, row);

    fprintf(f, "%s\t", row[1]);                          /* Strand */
    fprintf(f, "%s\t", row[3]);                          /* Feature */
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 4) - 1); /* cdsStart */
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 6));     /* cdsEnd */
    fprintf(f, "%s\t", row[7]);                          /* score */
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 8) - 1); /* orfStart */
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 10));    /* orfEnd */
    fprintf(f, "%s\t", row[11]);                         /* cdsSize */
    fprintf(f, "%s\t", row[12]);                         /* frame */
    line = mustFindLine(lf, "Predicted protein");
    if (!faPepSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	errAbort("Can't find peptide in %s", lf->fileName);
    fprintf(f, "%s\n", seq.dna);
    }
lineFileClose(&lf);
}

void borfBig(char *inName, char *outName)
/* borfBig - Run Victor Solovyev's bestorf repeatedly. */
{
char *exe = optionVal("exe", "borf");
char *tmpFa = optionVal("tmpFa", cloneString(rTempName("/tmp", "borf", ".fa")));
char *tmpOrf = optionVal("tmpOrf", cloneString(rTempName("/tmp", "borf", ".out")));
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct dnaSeq seq;
struct dyString *cmd = newDyString(256);
ZeroVar(&seq);

while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    faWrite(tmpFa, seq.name, seq.dna, seq.size);
    dyStringClear(cmd);
    dyStringPrintf(cmd, "%s %s > %s", exe, tmpFa, tmpOrf);
    mustSystem(cmd->string);
    convertBorfOutput(tmpOrf, f);
    printf(".");
    fflush(stdout);
    }
printf("\n");
remove(tmpFa);
remove(tmpOrf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
borfBig(argv[1], argv[2]);
return 0;
}
