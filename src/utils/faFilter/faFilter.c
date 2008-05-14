/* faFilter - Filter out fa records that don't match expression. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "fa.h"
#include "hash.h"
#include "dystring.h"

static char const rcsid[] = "$Id: faFilter.c,v 1.5 2008/05/14 07:19:22 markd Exp $";

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
         "    -uniq - Removes duplicate sequence ids, keeping the first.\n"
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
    {"uniq", OPTION_BOOLEAN},
    {NULL, 0},
};

/* command line options */
char *optNamePatList = NULL;
char *namePat = NULL;
boolean vOption = FALSE;
int minSize = -1;
int maxSize = -1;
struct hash *uniqHash = NULL;

char *parseSeqName(char *seqHeader)
/* parse sequence id from a header seqHeader. 
 * WARNING: static return. */
{
static struct dyString *buf = NULL;
if (buf == NULL)
    buf = dyStringNew(32);
dyStringClear(buf);
dyStringAppend(buf, seqHeader);
char *sep = skipToSpaces(buf->string);
if (sep != NULL)
    *sep = '\0';
return buf->string;
}

struct slName *readInPatterns(char *filename)
{
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

boolean matchesAPattern(char *name, struct slName *patternsList)
/* does the sequence match one of the patterns */
{
struct slName *pat;
for (pat = patternsList; pat != NULL; pat = pat->next)
    {
    if (wildMatch(pat->name, name))
        return TRUE;
    }
return FALSE;
}

boolean recMatches(DNA *seq, int seqSize, char *seqHeader, struct slName *patternsList)
/* check if a fasta record matches the sequence constraints */
{
char *name = parseSeqName(seqHeader);
if ((minSize >= 0) && (seqSize < minSize))
    return FALSE;
if ((maxSize >= 0) && (seqSize > maxSize))
    return FALSE;
if ((patternsList != NULL) && !matchesAPattern(name, patternsList))
    return FALSE;
if (uniqHash != NULL) 
    {
    if (hashLookup(uniqHash, name) != NULL)
        return FALSE;  // already seen
    hashAdd(uniqHash, name, NULL);
    }
return TRUE;
}

void faFilter(char *inFile, char *outFile)
/* faFilter - Filter out fa records that don't match expression. */
{
struct slName *patternsList = NULL;
if (optNamePatList != NULL)
    readInPatterns(optNamePatList);
if (namePat != NULL)
    slSafeAddHead(&patternsList, slNameNew(namePat));
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
if (optionExists("uniq"))
    uniqHash = hashNew(24);
faFilter(argv[1],argv[2]);
return 0;
}
