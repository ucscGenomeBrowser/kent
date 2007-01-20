/* faCat - concatenate fa records and add gaps between */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "fa.h"

static char const rcsid[] = "$Id: faCat.c,v 1.1 2007/01/20 23:28:52 baertsch Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort("faCat - concatenate fa records and add gaps between \n"
         "usage:\n"
         "   faCat [options] in.fa out.fa\n"
         "\n"
         "Options:\n"
         "    -gapSize=N - size of gap to insert between each sequence. Default 25.\n"
         "\n"
         );
}

static struct optionSpec options[] = {
    {"gapSize", OPTION_INT},
    {NULL, 0},
};

/* command line options */
char *namePat = NULL;
boolean vOption = FALSE;
int minSize = -1;
int maxSize = -1;
int gapSize = 0;

boolean matchName(char *seqHeader)
/* see if the sequence name matches */
{
/* find end of name */
char *nameSep = skipToSpaces(seqHeader);
char sepChr = '\0';
boolean isMatch = FALSE;

if (nameSep != NULL)
    {
    sepChr = *nameSep; /* terminate name */
    *nameSep = '\0';
    }
isMatch = wildMatch(namePat, seqHeader);
if (nameSep != NULL)
    *nameSep = sepChr;
return isMatch;
}

boolean recMatches(DNA *seq, int seqSize, char *seqHeader)
/* check if a fasta record matches the sequence constraints */
{
if ((minSize >= 0) && (seqSize < minSize))
    return FALSE;
if ((maxSize >= 0) && (seqSize > maxSize))
    return FALSE;
if ((namePat != NULL) && !matchName(seqHeader))
    return FALSE;
return TRUE;
}

void faCat(char *inFile, char *outFile)
/* faCat - Filter out fa records that don't match expression. */
{
struct lineFile *inLf = lineFileOpen(inFile, TRUE);
FILE *outFh = mustOpen(outFile, "w");
DNA *seq;
int seqSize;
char *seqHeader;
char name[256] = ">chrUn";

mustWrite(outFh, name, sizeof(name));
while (faMixedSpeedReadNext(inLf, &seq, &seqSize, &seqHeader))
    {
//    if (vOption ^ recMatches(seq, seqSize, seqHeader))
    //    faWriteNext(outFh, seqHeader, seq, seqSize);
    writeSeqWithBreaks(outFh, seq, seqSize, 50);
    }
lineFileClose(&inLf);
carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
//namePat = optionVal("name", NULL);
//vOption = optionExists("v");
//minSize = optionInt("minSize", -1);
gapSize = optionInt("gapSize", 25);
faCat(argv[1],argv[2]);
return 0;
}
