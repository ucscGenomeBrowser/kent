/* crawlForPosAndTime - Look through logs and make file that gives time of day, hgTracks window size, and hgTracks response time.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "apacheLog.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "crawlForPosAndTime - Look through logs and make file that gives time of day, hgTracks window size, and hgTracks response time.\n"
  "usage:\n"
  "   crawlForPosAndTime input.log output.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *cgiOneVar(char *cgiString, char *varName)
/* Go list of CGI variables looking for "varName=value" and returning value. */ 
{
char *name, *endName, *val, *endVal;
int nameSize = strlen(varName);
for (name = cgiString; name != NULL; name = endVal)
    {
    endName = strchr(name, '=');
    if (endName == NULL)
	break;
    val = endName + 1;
    endVal = strchr(val, '&');
    if (endName-name == nameSize)
        {
	if (memcmp(varName, name, nameSize) == 0)
	    {
	    int valSize;
            if (endVal == NULL)
	        valSize = strlen(val);
	    else
	        valSize = endVal - val;
	    return cloneStringZ(val, valSize);
	    }
	}
    }
return NULL;
}

void crawlForPosAndTime(char *input, char *output)
/* crawlForPosAndTime - Look through logs and make file that gives time of day, hgTracks window size, and hgTracks response time.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    struct apacheAccessLog *a = apacheAccessLogParse(line, lf->fileName, lf->lineIx);
    if (a != NULL)
	{
	if (startsWith("/cgi-bin/hgTracks", a->url))
	    {
	    char *q = strchr(a->url, '?');
	    if (q != NULL)
		{
		char *position = cgiOneVar(q+1, "position");
		if (position != NULL)
		    {
		    char *chrom;
		    int start,end;
		    if (hgParseChromRange(NULL, position, &chrom, &start, &end))
		        {
			fprintf(f, "%ld\t%d\t%d\n", a->tick, a->runTime, end - start);
			}
		    freez(&position);
		    }
		}
	    }
	apacheAccessLogFree(&a);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
crawlForPosAndTime(argv[1], argv[2]);
return 0;
}
