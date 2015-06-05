/* stripEvidence - strip evidence tags from UniProt text file*/
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "stripEvidence -  strip evidence tags from UniProt text file"
  "usage:\n"
  "   stripEvidence input output\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void stripEvidence(char *inputFile, char *outputFile)
/* stripEvidence - collapse continued UniProt text file prefix lines into one long line. */
{
struct lineFile *lf = lineFileOpen(inputFile, TRUE);
FILE *f = mustOpen(outputFile, "w");

int size;
char *start;
boolean inComment = FALSE;

while(lineFileNext(lf, &start, &size ))
    {
    assert(size >= 2);
    if (inComment)
	{
	// delete this line up until closing curly (and maybe a period afterward)
	char *ptr = &start[5];
	int length = strlen(ptr)+1;

	for(; *ptr != '}' && *ptr; ptr++)
	    length--;

	if (*ptr == '}')
	    {
	    inComment = FALSE;
	    ptr++;
	    if (*ptr == '.')
		{
		ptr++;
		length--;
		}
	    }
	memmove(&start[5], ptr, length);
	}

    char *match = NULL;
    while ((match = strstr(start, "{ECO")) != NULL)
	{
	// for all openings of evidence packet, delete to closing curly or end of line
	char *ptr = match;
	int length = strlen(ptr)+1;

	for(; *ptr != '}' && *ptr; ptr++)
	    length--;
	if (*ptr == '}')
	    {
	    ptr++;
	    if (*ptr == '.')
		{
		ptr++;
		length--;
		}
	    }
	else
	    inComment = TRUE;
	memmove(match, ptr, length );
	}

    // sometimes there are spaces after these curly brackets
    eraseTrailingSpaces(start);
    int length = strlen(start);

    if ((length > 4) || (strcmp(start, "//") == 0))
	{
	fwrite(start, 1, length,  f);
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
stripEvidence(argv[1], argv[2]);
return 0;
}
