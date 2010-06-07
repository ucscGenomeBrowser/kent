/* raSort - Sort a .ra file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

char *sortKey = "priority";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raSort - Sort a .ra file\n"
  "usage:\n"
  "   raSort input.ra output.ra\n"
  "options:\n"
  "   -key=name [default %s] Field to sort on.\n"
  , sortKey
  );
}

static struct optionSpec options[] = {
   {"key", OPTION_STRING},
   {NULL, 0},
};

struct ra
/* A ra record. */
    {
    struct ra *next;
    struct slName *lines;
    double priority;
    };

int raCmp(const void *va, const void *vb)
{
const struct ra *a = *((struct ra **)va);
const struct ra *b = *((struct ra **)vb);
double diff = a->priority - b->priority;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else
    return 0;
}

struct ra *nextRa(struct lineFile *lf, char *priorityField)
/* Return next ra record from file, including any comments
 * and return it.  Return NULL at end of file. */
{
int size;
char *line;
struct slName *slList = NULL, *sl;
boolean gotData = FALSE, gotPriority = FALSE;
struct ra *ra;
int priorityFieldSize = strlen(priorityField);
while (lineFileNext(lf, &line, &size))
    {
    char *s = skipLeadingSpaces(line);
    char c = s[0];
    if (c == '#' || c == 0)
        {
	sl = slNameNew(line);
	slAddHead(&slList, sl);
	}
    else
	{
	gotData = TRUE;
	lineFileReuse(lf);
        break;
	}
    }
if (!gotData)
    return NULL;
AllocVar(ra);

while (lineFileNext(lf, &line, &size))
    {
    char *s = skipLeadingSpaces(line);
    char c = s[0];
    if (c == 0)
        {
	break;
	}
    if (c != '#')
        {
	if (startsWith(priorityField, line) && isspace(line[priorityFieldSize]))
	    {
	    ra->priority = atof(line + priorityFieldSize);
	    gotPriority = TRUE;
	    }
	}
    sl = slNameNew(line);
    slAddHead(&slList, sl);
    }
if (!gotPriority)
    errAbort("Missing %s field line %d of %s", priorityField, lf->lineIx, lf->fileName);

slReverse(&slList);
ra->lines = slList;
return ra;
}

void raSort(char *inName, char *outName, char *key)
/* raSort - Sort a .ra file. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct ra *raList = NULL, *ra;

while ((ra = nextRa(lf, key)) != NULL)
    slAddHead(&raList, ra);

slSort(&raList, raCmp);
for (ra = raList; ra != NULL; ra = ra->next)
    {
    struct slName *sl;
    for (sl = ra->lines; sl != NULL; sl = sl->next)
	fprintf(f, "%s\n", sl->name);
    fprintf(f, "\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
sortKey = optionVal("key", sortKey);
raSort(argv[1], argv[2], sortKey);
return 0;
}
