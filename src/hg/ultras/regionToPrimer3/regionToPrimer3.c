/* regionToPrimer3 - Convert fasta region file to primer3 input. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "dnaSeq.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regionToPrimer3 - Convert fasta region file to primer3 input\n"
  "usage:\n"
  "   regionToPrimer3 input output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
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

void regionToPrimer3(char *inFile, char *outFile)
/* regionToPrimer3 - Convert fasta region file to primer3 input. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *dna;
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

    /* Mask out G quartets and whatever RepeatMasker finds. */
    polyToLower(dna, pOpen-dna, 'G', 4);
    polyToLower(pClose+1, dna+size - pClose - 1, 'C', 4);
    lowerToN(dna, size);

    /* Create input record for primer3. */
    fprintf(f, "PRIMER_SEQUENCE_ID=%s\n", name);
    fprintf(f, "SEQUENCE=%s\n", dna);
    fprintf(f, "TARGET=%d,%d\n", startSize+1, midSize);
    fprintf(f, "PRIMER_EXPLAIN_FLAG=1\n");
    fprintf(f, "PRIMER_PRODUCT_SIZE_RANGE=%d-%d\n", midSize, midSize+300);
    fprintf(f, "PRIMER_PRODUCT_OPT_SIZE=%d\n", midSize+32);
    fprintf(f, "=\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
regionToPrimer3(argv[1], argv[2]);
return 0;
}
