/* shortRepeatMerge - Merge files made with shortRepeatFind.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "dnautil.h"

static char const rcsid[] = "$Id: shortRepeatMerge.c,v 1.1 2008/10/23 20:03:59 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "shortRepeatMerge - Merge files made with shortRepeatFind.\n"
  "usage:\n"
  "   shortRepeatMerge input(s).srf output.srf\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct nameCount
/* A file and a line from that file. */
    {
    struct nameCount *next;
    struct lineFile *lf; /* Open file. */
    char *name;		 /* First column - name */
    int count;		 /* Second column, count */
    };

boolean nameCountNext(struct nameCount *nc)
/* Update nameCount with next line in file.  Return FALSE if at EOF. */
{
char *row[2];
if (!lineFileRow(nc->lf, row))
    return FALSE;
nc->name = row[0];
nc->count = sqlUnsigned(row[1]);
return TRUE;
}

struct nameCount *lowestLine(struct nameCount *ncList)
/* Return alphabetically lowest one in list. */
{
struct nameCount *first = ncList;
struct nameCount *nc;
for (nc = first->next; nc != NULL; nc = nc->next)
    {
    if (cmpDnaStrings(nc->name, first->name) < 0)
        first = nc;
    }
return first;
}

void shortRepeatMerge(int inCount, char *inputs[], char *output)
/* shortRepeatMerge - Merge files made with shortRepeatFind.. */
{
int i;
struct nameCount *ncList = NULL, *nc;

/* Open all files and read first line. */
for (i=0; i<inCount; ++i)
    {
    AllocVar(nc);
    nc->lf = lineFileOpen(inputs[i], TRUE);
    if (nameCountNext(nc))
        {
	slAddHead(&ncList, nc);
	}
    }
slReverse(&ncList);
verbose(2, "Opened %d files\n", inCount);

/* Open output and start looping through files. */
FILE *f = mustOpen(output, "w");
while (ncList != NULL)
    {
    char target[50];
    char *lowest = lowestLine(ncList)->name; 
    strcpy(target, lowest);
    int total = 0;
    boolean gotEof = FALSE;
    for (nc = ncList; nc != NULL; nc = nc->next)
        {
	if (sameString(nc->name, lowest))
	    {
	    total += nc->count;
	    if (!nameCountNext(nc))
	        {
		gotEof = TRUE;
		nc->lf = NULL;
		}
	    }
	}
    if (gotEof)
        {
	struct nameCount *newList = NULL, *next;
	for (nc = ncList; nc != NULL; nc = next)
	    {
	    next = nc->next;
	    if (nc->lf)
	        {
		slAddHead(&newList, nc);
		}
	    }
	slReverse(&newList);
	ncList = newList;
	}
    fprintf(f, "%s %d\n", target, total); 
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dnaUtilOpen();
if (argc < 3)
    usage();
shortRepeatMerge(argc-2, argv+1, argv[argc-1]);
return 0;
}
