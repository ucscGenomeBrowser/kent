/* tagStormScalarToArray - Do conversion of earlier version purely scalar tag storm to one that allows comma separated lists, requiring quotes around real commas. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormScalarToArray - Do conversion of earlier version purely scalar tag storm to one that allows comma separated lists, requiring quotes around real commas\n"
  "usage:\n"
  "   tagStormScalarToArray scalar_input.tags array_output.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void rewriteVal(FILE *f, char *val)
/* Rewrite val, which has some quotes or commas in it, in a way to be more compatable with
 * csv list representation */
{
val = trimSpaces(val);
int valLen = strlen(val);

/* Strip surrounding quotes if any */
if (valLen > 2 && val[0] == '"' && lastChar(val) == '"')
     {
     val[valLen-1] = 0;
     val += 1;
     }

/* If there are no commas just output it */
if (strchr(val, ',') == NULL)
    {
    fputs(val, f);
    return;
    }

/* If there are no more quotes and no spaces, treat it as a decent array as is. */
if (strchr(val, '"') == NULL && strchr(val, ' ') == NULL && strchr(val, '\t') == NULL)
    {
    fputs(val, f);
    return;
    }

/* Otherwise put quotes around it and output, escaping internal quotes with double quotes */
fputc('"', f);
char c;
while ((c = *val++) != 0)
    {
    if (c == '"')
	fputc('"', f);
    fputc(c, f);
    }
fputc('"', f);
}

void tagStormScalarToArray(char *input, char *output)
/* tagStormScalarToArray - Do conversion of earlier version purely scalar tag storm to one that allows comma 
 * separated lists, requiring quotes around real commas. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *firstWord = skipLeadingSpaces(line);
    if (isEmpty(firstWord) || firstWord[0] == '#') // Pass commas and blank lines through unmolested
        fprintf(f, "%s\n", line);
    else if (strchr(firstWord, ',') == NULL && strchr(firstWord, '"') == NULL)  // no , or " no problem
        fprintf(f, "%s\n", line);
    else
        {
	char *afterTag = skipToSpaces(firstWord);
	char *startVal = skipLeadingSpaces(afterTag);
	if (startVal == NULL)
	    errAbort("Empty %s tag line %d of %s", firstWord, lf->lineIx, lf->fileName);
	mustWrite(f, line, startVal-line);	// write out indent
	rewriteVal(f, startVal);
	fputc('\n', f);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tagStormScalarToArray(argv[1], argv[2]);
return 0;
}
