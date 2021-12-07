/* regionToPrimer3 - Convert fasta region file to primer3 input. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"

char *exe = "primer3_core";
double minTm = 57.0;
double maxTm = 63.0;
double optTm = 60.0;

char *tempInName = "rtp_tmp.in";
char *tempOutName = "rtp_tmp.out";
int primerMinSize = 16;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regionToPrimer3 - Convert fasta region file to primer3 input\n"
  "usage:\n"
  "   regionToPrimer3 input output\n"
  "options:\n"
  "   -exe=/path/to/primer3_core - Where to find primer3_core\n"
  "   -minTm=X - minimum primer melting temp (default %4.1f)\n"
  "   -maxTm=X - maximum primer melting temp (default %4.1f)\n"
  "   -optTm=X - optimum primer melting temp (default %4.1f)\n"
  , minTm, maxTm, optTm
  );
}

static struct optionSpec options[] = {
   {"exe", OPTION_STRING},
   {"minTm", OPTION_DOUBLE},
   {"maxTm", OPTION_DOUBLE},
   {"optTm", OPTION_DOUBLE},
   {NULL, 0},
};

void polyToLower(char *s, int size, char b, int minRun)
/* Switch runs of b minRun or longer to lower case. */
{
boolean inRun = FALSE;
int startRun = 0;
int i;
char c;
for (i=0; i<size; ++i)
    {
    c = toupper(s[i]);
    if (inRun )
        {
	if (c != b)
	    {
	    inRun = FALSE;
	    if (i-startRun >= minRun)
		toLowerN(s+startRun, i-startRun);
	    }
	}
    else 
        {
	if (c == b)
	    {
	    inRun = TRUE;
	    startRun = i;
	    }
	}
    }
if (inRun && size-startRun >= minRun)
    toLowerN(s+startRun, size-startRun);
}

boolean regionReadNext(struct lineFile *lf, char **retDna, int *retSize, char **retName)
/* Return region - basically DNA with parens. */
{
char *name, *dna;
static struct dyString *dy;
char *line, c;

/* Get dynamic string buffer. */
if (dy == NULL)
     dy = dyStringNew(4*1024);

/* Get opening '>'. */
if (!lineFileNextReal(lf, &line))
    return FALSE;
if (line[0] != '>')
    errAbort("Not fasta format line %d of %s", lf->lineIx, lf->fileName);
name = cloneString(line+1);

/* Read in DNA until next line that starts with '>' or EOF */
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	lineFileReuse(lf);
	break;
	}
    while ((c = *line++) != 0)
        {
	if (c == '(' || c == ')' || isalpha(c))
	    dyStringAppendC(dy, c);
	}
    }
dna = cloneString(dy->string);
*retName = name;
*retDna = dna;
*retSize = dy->stringSize;
dyStringClear(dy);
return TRUE;
}

boolean parseWritePrimer3Output(char *seqName, int midSize, char *inFile, FILE *f)
/* Parse inFile.  If it looks like it has valid primers write out 
 * tab-separated result and return TRUE. */
{
boolean ok = FALSE;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *tag, *val;
char *left, *right, *size, *leftTm, *rightTm;

left = right = size = leftTm = rightTm = NULL;

while (lineFileNextReal(lf, &tag))
    {
    char *val = strchr(tag, '=');
    if (val == tag)
        break;
    *val++ = 0;
    if (sameString(tag, "PRIMER_LEFT_SEQUENCE"))
        left = cloneString(val);
    else if (sameString(tag, "PRIMER_RIGHT_SEQUENCE"))
        right = cloneString(val);
    else if (sameString(tag, "PRIMER_PRODUCT_SIZE"))
        size = cloneString(val);
    if (sameString(tag, "PRIMER_LEFT_TM"))
        leftTm = cloneString(val);
    if (sameString(tag, "PRIMER_RIGHT_TM"))
        rightTm = cloneString(val);
    }
lineFileClose(&lf);

if (left != NULL && right != NULL & size != NULL && leftTm != NULL && rightTm != NULL)
    {
    ok = TRUE;
    fprintf(f, "%s", seqName);
    fprintf(f, "\t%s", left);
    fprintf(f, "\t%s", right);
    fprintf(f, "\t%s", size);
    fprintf(f, "\t%d", midSize);
    fprintf(f, "\t%s", leftTm);
    fprintf(f, "\t%s", rightTm);
    fprintf(f, "\n");
    }
freeMem(left);
freeMem(right);
freeMem(size);
freeMem(leftTm);
freeMem(rightTm);
return ok;
}


boolean primer3Works(FILE *out, char *name, int primerMinSize, DNA *dna, int startSize, int midSize, int endSize)
/* Make system call to primer 3 to build primers that flank mid-region.
 * Return TRUE and write result to file if it succeeds.  Otherwise
 * return FALSE. */
{
char command[512];
int totalSize = startSize + midSize + endSize;

/* Create input file for primer3 */
FILE *f = mustOpen(tempInName, "w");
fprintf(f, "PRIMER_SEQUENCE_ID=%s\n", name);
fprintf(f, "SEQUENCE=");
mustWrite(f, dna, totalSize);
fprintf(f, "\n");
fprintf(f, "TARGET=%d,%d\n", startSize+1, midSize);
fprintf(f, "PRIMER_EXPLAIN_FLAG=1\n");
fprintf(f, "PRIMER_PRODUCT_SIZE_RANGE=%d-%d\n", midSize, totalSize);
fprintf(f, "PRIMER_MIN_TM=%5.2f\n", minTm);
fprintf(f, "PRIMER_MAX_TM=%5.2f\n", maxTm);
fprintf(f, "PRIMER_OPT_TM=%5.2f\n", optTm);
fprintf(f, "PRIMER_MIN_SIZE=%d\n", primerMinSize);
fprintf(f, "=\n");
carefulClose(&f);

/* Make system call to primer3 that will hopefully result in temp output. */
remove(tempOutName);
safef(command, sizeof(command), "%s < %s > %s", exe, tempInName, tempOutName);
system(command);

return parseWritePrimer3Output(name, midSize + 2*primerMinSize, tempOutName, out);
}

boolean findPrimers(FILE *f, char *name, DNA *dna, int startSize, int midSize, int endSize)
/* Call primer3 to find primer that will amplify midSize region, and write results to file. */
{
int extra;
int totalSize = startSize + midSize + endSize;
for (extra = 25; 
    extra <= startSize+primerMinSize || extra <= endSize + primerMinSize;
    extra += 25)
    {
    int sStart = startSize + primerMinSize - extra;
    int sEnd = startSize + midSize - primerMinSize + extra;
    if (sStart < 0)
        sStart = 0;
    if (sEnd > totalSize)
        sEnd = totalSize;
    if (primer3Works(f, name, primerMinSize, dna+sStart, 
    	(startSize + primerMinSize) - sStart, midSize - 2*primerMinSize,
	sEnd - (startSize + midSize - primerMinSize)))
	return TRUE;
    }
warn("Couldn't find primers for %s", name);
return FALSE;
}



void regionToPrimer3(char *inFile, char *outFile)
/* regionToPrimer3 - Convert fasta region file to primer3 input. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
DNA *dna;
int size;
char *name;
int startSize, midSize, endSize;

while (regionReadNext(lf, &dna, &size, &name))
    {
    /* Parse out parenthesis to get target and flanking regions. */
    char *pOpen = strchr(dna, '(');
    char *pClose = strchr(dna, ')');
    if (pOpen == NULL || pClose == NULL)
         errAbort("Missing parens in %s", name);
    startSize = pOpen - dna;
    midSize = pClose - (pOpen + 1);
    endSize = dna + size - (pClose + 1);
    stripChar(dna, '(');
    stripChar(dna, ')');

    /* Mask out G quartets and whatever else is in lower case,  */
    /* such as regions that RepeatMasker finds.                 */
    polyToLower(dna, pOpen-dna + primerMinSize, 'G', 4);
    polyToLower(pClose+1 - primerMinSize, dna+size - pClose - 1 + primerMinSize, 'C', 4);
    lowerToN(dna, size);

    findPrimers(f, name, dna, startSize, midSize, endSize);
    verboseDot();
    }
verbose(1, "\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
exe = optionVal("exe", exe);
minTm = optionDouble("minTm", minTm);
maxTm = optionDouble("maxTm", maxTm);
optTm = optionDouble("optTm", optTm);
regionToPrimer3(argv[1], argv[2]);
return 0;
}
