/* crawlForHogs - Look through new style access logs for hogs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "apacheLog.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "crawlForHogs - Look through new style access logs for hogs.\n"
  "usage:\n"
  "   crawlForHogs accessLog outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct trough
/* A trough is where the hogs are most likely to be found.  Some hogs even manage
 * to be at multiple troughs all at the same time.  */
    {
    boolean (*identify)(struct apacheAccessLog *a); 	/* Return true if in trough */
    char *name;					/* Name of output file for trough. */
    char *style;					/* Style for track - color etc. */
    FILE *f;						/* open file. */
    };

boolean msnbotIdentify(struct apacheAccessLog *a)
/* Return true if msnbot. */
{
return startsWith("msnbot", a->program);
}

boolean err500Identify(struct apacheAccessLog *a)
/* Return true if err500 */
{
return a->status == 500;
}

boolean msnbot500Identify(struct apacheAccessLog *a)
/* Return true if err500 and msnbot */
{
return msnbotIdentify(a) && err500Identify(a);
}


boolean hgTracksIdentify(struct apacheAccessLog *a)
/* Return true if hgTracks. */
{
return startsWith("/cgi-bin/hgTracks", a->url);
}

boolean hgTablesIdentify(struct apacheAccessLog *a)
/* Return true if hgTables. */
{
return startsWith("/cgi-bin/hgTables", a->url);
}

boolean hgBlatIdentify(struct apacheAccessLog *a)
/* Return true if hgBlat. */
{
return startsWith("/cgi-bin/hgBlat", a->url);
}

boolean hgcIdentify(struct apacheAccessLog *a)
/* Return true if hgc. */
{
return startsWith("/cgi-bin/hgc", a->url);
}

boolean hgNearIdentify(struct apacheAccessLog *a)
/* Return true if hgNear. */
{
return startsWith("/cgi-bin/hgNear", a->url);
}


boolean hgGeneIdentify(struct apacheAccessLog *a)
/* Return true if hgGene. */
{
return startsWith("/cgi-bin/hgGene", a->url);
}


boolean ip1Identify(struct apacheAccessLog *a)
/* Return true if ip1. */
{
return startsWith("130.225.0.210", a->ip);
}

boolean ip2Identify(struct apacheAccessLog *a)
/* Return true if ip2. */
{
return startsWith("193.62.203.214", a->ip);
}

boolean hgwdevIdentify(struct apacheAccessLog *a)
/* Return true if hgwdev. */
{
return startsWith("128.114.50.189", a->ip);
}


struct trough troughs[] = 
   {
    { msnbotIdentify, "msnbot", "color=0,200,0"},
    { err500Identify, "err500", "color=222,0,0"},
    { msnbot500Identify, "msnbot500", "color=250,100,0"},
    { hgTracksIdentify, "hgTracks", "color=0,0,200"},
    { hgTablesIdentify, "hgTables", "color=0,0,200"},
    { hgNearIdentify, "hgNear", "color=0,0,200"},
    { hgGeneIdentify, "hgGene", "color=0,0,200"},
    { hgBlatIdentify, "hgBlat", "color=0,0,200"}, 
    { hgcIdentify, "hgc", "color=0,0,200"},
    { ip1Identify, "ip1", "color=150,150,0"},
    { ip2Identify, "ip2", "color=150,150,0"},
    { hgwdevIdentify, "hgwdev", "color=150,150,0"},
   };

void crawlForHogs(char *inputLog, char *outDir)
/* crawlForHogs - Look through new style access logs for hogs.. */
{
struct lineFile *lf = lineFileOpen(inputLog, TRUE);
char *line;
int troughCount = ArraySize(troughs);
int i;
makeDir(outDir);
for (i=0; i<troughCount; ++i)
    {
    char path[PATH_LEN];
    char *name = troughs[i].name;
    safef(path, sizeof(path), "%s/%s", outDir, name);
    troughs[i].f = mustOpen(path, "w");
    }

while (lineFileNext(lf, &line, NULL))
    {
    struct apacheAccessLog *a = apacheAccessLogParse(line, lf->fileName, lf->lineIx);
    if (a != NULL)
	{
	for (i=0; i<troughCount; ++i)
	    if ((troughs[i].identify)(a))
		{
		FILE *f = troughs[i].f;
		fprintf(f, "%s\n", line);
		}
	apacheAccessLogFree(&a);
	}
    }

for (i=0; i<troughCount; ++i)
    carefulClose(&troughs[i].f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
crawlForHogs(argv[1], argv[2]);
return 0;
}
