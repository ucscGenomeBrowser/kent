/* faFilter - Filter out fa records that don't match expression. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "fa.h"

static char const rcsid[] = "$Id: faFilter.c,v 1.4 2007/12/19 00:33:51 lowec Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort("faFilter - Filter fa records, selecting ones that match the specified conditions\n"
         "usage:\n"
         "   faFilter [options] in.fa out.fa\n"
         "\n"
         "Options:\n"
         "    -name=wildCard  - Only pass records where name matches wildcard\n"
         "                      * matches any string or no character.\n"
         "                      ? matches any single character.\n"
         "                      anything else etc must match the character exactly\n"
         "                      (these will will need to be quoted for the shell)\n"
         "    -namePatList=filename - A list of regular expressions, one per line, that\n"
         "                            will be applied to the fasta name the same as -name\n"
         "    -v - invert match, select non-matching records.\n"
         "    -minSize=N - Only pass sequences at least this big.\n"
         "    -maxSize=N - Only pass sequences this size or smaller.\n"
         "\n"
         "All specified conditions must pass to pass a sequence.  If no conditions are\n"
         "specified, all records will be passed.\n"
         );
}

static struct optionSpec options[] = {
    {"namePatList", OPTION_STRING},
    {"name", OPTION_STRING},
    {"v", OPTION_BOOLEAN},
    {"minSize", OPTION_INT},
    {"maxSize", OPTION_INT},
    {NULL, 0},
};

/* command line options */
char *optNamePatList = NULL;
char *namePat = NULL;
boolean vOption = FALSE;
int minSize = -1;
int maxSize = -1;

boolean matchName(char *seqHeader, char *pattern)
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
isMatch = wildMatch(pattern, seqHeader);
if (nameSep != NULL)
    *nameSep = sepChr;
return isMatch;
}

struct slName *readInPatterns(char *filename)
{
if(filename == NULL){return(NULL);}

struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
struct slName *ret = NULL, *curr = NULL;

while (lineFileNext(lf, &line, NULL))
    {
    curr = newSlName(line);
    slAddHead(&ret, curr);
    }
return(ret);
}

boolean recMatches(DNA *seq, int seqSize, char *seqHeader, struct slName *patternsList)
/* check if a fasta record matches the sequence constraints */
{
boolean matchList = FALSE;
struct slName *currPattern = NULL;

if ((minSize >= 0) && (seqSize < minSize))
    return FALSE;
if ((maxSize >= 0) && (seqSize > maxSize))
    return FALSE;
if ((namePat != NULL) && !matchName(seqHeader, namePat))
    return FALSE;
for(currPattern = patternsList; currPattern != NULL; currPattern = currPattern->next)
    {
    if (matchName(seqHeader, currPattern->name)) {matchList = TRUE;}
    }
if ((patternsList != NULL) && (!matchList))
    return FALSE;
return TRUE;
}

void faFilter(char *inFile, char *outFile)
/* faFilter - Filter out fa records that don't match expression. */
{
struct slName *patternsList = readInPatterns(optNamePatList);
struct lineFile *inLf = lineFileOpen(inFile, TRUE);
FILE *outFh = mustOpen(outFile, "w");
DNA *seq;
int seqSize;
char *seqHeader;

while (faMixedSpeedReadNext(inLf, &seq, &seqSize, &seqHeader))
    {
    if (vOption ^ recMatches(seq, seqSize, seqHeader, patternsList))
        faWriteNext(outFh, seqHeader, seq, seqSize);
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
optNamePatList = optionVal("namePatList", NULL);
namePat = optionVal("name", NULL);
vOption = optionExists("v");
minSize = optionInt("minSize", -1);
maxSize = optionInt("maxSize", -1);
faFilter(argv[1],argv[2]);
return 0;
}
