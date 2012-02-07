/* accessLogMerge - Merge multiple time sorted apache access logs into a single time sorted log.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "accessLogMerge - Merge multiple time sorted apache access logs into a single time sorted log.\n"
  "usage:\n"
  "   accessLogMerge log1 log2 ... logN\n"
  "Output goes to stdout.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

boolean addSource = FALSE;

static struct optionSpec options[] = {
   {"addSource", OPTION_BOOLEAN},
   {NULL, 0},
};

time_t stampFromLine(struct lineFile *lf, char *line)
/* Figure out time stamp corresponding to apache log line. */
{
char *s = strchr(line, '[');
if (s == NULL)
    {
    warn("Couldn't find time stamp in %s\nline %d of %s", line, lf->lineIx, lf->fileName);
    return 0;
    }
struct tm tm;
char *res = strptime(s+1, "%d/%b/%Y:%T", &tm);
if (res == NULL)
    {
    warn("Badly formatted time stamp in %s\nline %d of %s", line, lf->lineIx, lf->fileName);
    return 0;
    }
return mktime(&tm);
}

void accessLogMerge(int logCount, char *logNames[], char *outFile)
/* accessLogMerge - Merge multiple time sorted apache access logs into a single time sorted log.. */
{
FILE *out = mustOpen(outFile, "w");
struct lineFile *lfs[logCount];
char *lines[logCount];
time_t stamps[logCount];
int i;
for (i=0; i<logCount; ++i)
    {
    lfs[i] = lineFileOpen(logNames[i], TRUE);
    lines[i] = NULL;
    stamps[i] = 0;
    }

for (;;)
    {
    /* Fetch next line from all open files.  End program if all files closed. */
    boolean gotAny = FALSE;
    for (i=0; i<logCount; ++i)
        {
	struct lineFile *lf = lfs[i];
	if (lf != NULL && lines[i] == NULL)
	    {
	    char *line;
	    if (lineFileNextReal(lf, &line))
	        {
		lines[i] = line;
		stamps[i] = stampFromLine(lf, line);
		gotAny = TRUE;	/* Set here so that even error lines make things keep going. */
		}
	    else
	        {
		lineFileClose(&lfs[i]);
		stamps[i] = 0;
		}
	    }
	}
    /* Figure out minimum time stamp. */
    time_t minTime = 0;
    for (i=0; i<logCount; ++i)
        {
	time_t stamp = stamps[i];
	if (stamp != 0)
	    {
	    if (minTime == 0 || stamp < minTime)
	        minTime = stamp;
	    gotAny = TRUE;
	    }
	}

    if (!gotAny)
        break;


    /* Output all that match stamp. */
    for (i=0; i<logCount; ++i)
        {
	if (stamps[i] == minTime)
	    {
	    fprintf(out, "%s", lines[i]);
	    if (addSource)
	       fprintf(out, " %s", logNames[i]);
	    fputc('\n', out);
	    lines[i] = NULL;
	    }
	}
    }
carefulClose(&out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
addSource = optionExists("addSource");
accessLogMerge(argc-2, argv+1, argv[argc-1]);
return 0;
}
