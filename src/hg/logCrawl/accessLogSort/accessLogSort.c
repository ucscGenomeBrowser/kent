/* accessLogSort - Sort access logs by a field, by timestamp by default.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "apacheLog.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "accessLogSort - Sort access logs by timestamp.\n"
  "usage:\n"
  "   accessLogSort in.log out.log\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct logLine
/* A log and the line it's on. */
    {
    struct logLine *next;
    struct apacheAccessLog *log;
    char *line;
    };

int logLineCmp(const void *va, const void *vb)
/* Do comparison for sort. */
{
const struct logLine *a = *((struct logLine **)va);
const struct logLine *b = *((struct logLine **)vb);
return apacheAccessLogCmpTick(&a->log, &b->log);
}

void accessLogSort(char *input, char *output)
/* accessLogSort - Sort access logs by a field, by timestamp by default.. */
{
struct logLine *el, *list = NULL;
struct apacheAccessLog *a;
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    {
    a = apacheAccessLogParse(line, lf->fileName, lf->lineIx);
    if (a != NULL)
        {
	AllocVar(el);
	el->log = a;
	el->line = cloneString(line);
	slAddHead(&list, el);
	}
    }
slSort(&list, logLineCmp);
FILE *f = mustOpen(output, "w");
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s\n", el->line);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
accessLogSort(argv[1], argv[2]);
return 0;
}
