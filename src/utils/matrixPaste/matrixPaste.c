/* matrixPaste - Paste together matrices - much like paste but sensible about labels.  
 * For unlabeled matrices just use paste. To paste vertically just use cat. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

boolean fit1 = FALSE;
boolean filePrefix = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixPaste - Concatenate matrices - much like paste but sensible about labels.  \n"
  "For unlabeled matrices just use paste.  Output goes to stdout\n"
  "usage:\n"
  "   matrixPaste matrix1 matrix2 ... matrixN\n"
  "options:\n"
  "   -fit1 - row labels are matrix1's and no problem if later matrices have more rows\n"
  "   -filePrefix - add the root part of the matrix file name as a prefix to column labels\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"fit1", OPTION_BOOLEAN},
   {"filePrefix", OPTION_BOOLEAN},
   {NULL, 0},
};

void matrixPaste(int count, char *files[])
/* matrixPaste - Concatenate matrices - much like paste but sensible about labels.   */
{
struct lineFile *lfs[count];
char *midFileNames[count];
FILE *f = stdout;

/* Open all inputs */
int i;
for (i=0; i<count; ++i)
    lfs[i] = lineFileOpen(files[i], TRUE);
struct lineFile *firstLf = lfs[0];

/* Figure out root file names for labels if need be */
for (i=0; i<count; ++i)
    {
    char name[FILENAME_LEN];
    splitPath(files[i], name, NULL, NULL);
    trimLastChar(name);
    midFileNames[i] = cloneString(name);
    }

/* Read through all the rows */
boolean isLabelRow = TRUE;
for (;;)
    {
    /* Get the next line from the first file.  If it's not there we are done but
     * let's make sure all ther rest are too before breaking out. */
    char *first;
    char *firstLabel = NULL;  
    if (!lineFileNext(firstLf, &first, NULL))
        {
	/* We are done with first file. Do some error checking that other files are at end too */
	if (!fit1)
	    {
	    int i;
	    for (i=1; i<count; ++i)
	        {
		struct lineFile *lf = lfs[i];
		char *line;
		if (lineFileNext(lf, &line, NULL))
		    errAbort("%s has more lines that %s\n", lf->fileName, firstLf->fileName);
		}
	    }
	break;   // One way or another we are done
	}

    /* Print out first file including labels */
    firstLabel = nextTabWord(&first);
    if (isLabelRow)
	{
	char *mid = midFileNames[0];
	if (filePrefix)
	    fprintf(f, "%s ", mid);
	fprintf(f, "%s", firstLabel);
	char *colLabel;
	while ((colLabel = nextTabWord(&first)) != NULL)
	    {
	    fputc('\t', f);
	    if (filePrefix)
		fprintf(f, "%s ", mid);
	    fprintf(f, "%s", colLabel);
	    }
	}
    else
        {
	fprintf(f, "%s\t%s", firstLabel, first);
	}
	
    /* Go through rest of files, not printing row labels (but still printing column labels on
     * first row */
    int i;
    for (i=1; i<count; ++i)
        {
	struct lineFile *lf = lfs[i];
	char *line;
	if (!lineFileNext(lf, &line, NULL))
	    errAbort("%s has fewer lines than %s\n", lf->fileName, firstLf->fileName);
	if (isLabelRow)
	    {
	    nextTabWord(&line);    // Middle labels from label rows vanish
	    char *mid = midFileNames[i];
	    char *colLabel;
	    while ((colLabel = nextTabWord(&line)) != NULL)
		{
		fputc('\t', f);
		if (filePrefix)
		    fprintf(f, "%s ", mid);
		fprintf(f, "%s", colLabel);
		}
	    }
	else
	    {
	    char *label = nextTabWord(&line); /* Skip over label */
	    if (!fit1)
		{
		if (!sameString(label, firstLabel))
		    errAbort("%s has %s as label for line %d, %s has %s instead",
			lf->fileName, label, lf->lineIx, firstLf->fileName, firstLabel);
		}
	    fprintf(f, "\t%s", line);
	    }
	}
    fputc('\n', f);
    isLabelRow = FALSE;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
fit1 = optionExists("fit1");
filePrefix = optionExists("filePrefix");
matrixPaste(argc-1, argv+1);
return 0;
}
