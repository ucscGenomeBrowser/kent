/* tabixFetch - Test lineFileTabixMayOpen and lineFileSetTabixRegion (compare with cmd line tabix output). */
#include "common.h"
#include "linefile.h"
#include "options.h"

#ifdef USE_TABIX

#include "sqlNum.h"
#include "udc.h"
#include "knetUdc.h"

void usage()
/* Explain usage and exit. */
{
errAbort(

  "tabixFetch - Test lineFileTabixMayOpen and lineFileSetTabixRegion (compare\n"
  "             with command-line tabix output)\n"

  "usage:\n"
  "   tabixFetch fileOrUrl chrom:start-end\n"
  "Fetch lines from the specified position range from a line-based file\n"
  "with genomic positions that has been compressed and indexed by tabix.\n"
  "This duplicates the fetching functionality of the tabix program -- it\n"
  "was written solely to test lib/lineFile.c's tabix-wrapper mode.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

#define BAD_POS_FORMAT "Expecting position format: chrom:N-M where N and M are integers.  " \
		       "\"%s\" doesn't match."
static void parsePosition(const char *position, char **retChrom, int *retStart, int *retEnd)
/* Do we have a lib routine for this somewhere?  Too lazy to look. */
{
char *posDupe = cloneString(position);
char *words[3];
int wordCount = chopByChar(posDupe, ':', words, ArraySize(words));
if (wordCount != 2)
    errAbort(BAD_POS_FORMAT, position);
*retChrom = posDupe;

stripChar(words[1], ',');
char *range = cloneString(words[1]);
wordCount = chopByChar(range, '-', words, ArraySize(words));
if (wordCount != 2)
    errAbort(BAD_POS_FORMAT, position);
*retStart = sqlUnsigned(words[0]) - 1;
*retEnd = sqlUnsigned(words[1]);
}

void tabixFetch(char *fileOrUrl, char *position)
/* tabixFetch - Test lineFileTabixMayOpen and lineFileSetTabixRegion
 * (compare with cmd line tabix output). */
{
udcSetDefaultDir("/data/tmp/angie/udcCache");
knetUdcInstall();
struct lineFile *lf = lineFileTabixMayOpen(fileOrUrl, TRUE);
if (lf == NULL)
    exit(1);
int lineSize;
char *line = NULL;
boolean gotLine = FALSE;
if (0)
do
    {
    gotLine = lineFileNext(lf, &line, &lineSize);
    puts(line);
    }
while (gotLine && (line[0] == '#' || isEmpty(line)));

char *chrom = NULL;
int start, end;
parsePosition(position, &chrom, &start, &end);
boolean success = lineFileSetTabixRegion(lf, chrom, start, end);
if (!success)
    printf("*** Could not set position to %s\n", position);
else
    {
    printf("*** First (up to) 10 lines from query on %s:\n", position);
    int i;
    for (i=0;  i < 10;  i++)
	{
	if (lineFileNext(lf, &line, &lineSize))
	    puts(line);
	else break;
	}
    if (i == 0)
	printf("*** No lines returned from query on %s\n", position);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tabixFetch(argv[1], argv[2]);
return 0;
}

#else // no USE_TABIX
int main(int argc, char *argv[])
/* Process command line. */
{
errAbort(COMPILE_WITH_TABIX, "tabixFetch");
return 1;
}

#endif // no USE_TABIX
