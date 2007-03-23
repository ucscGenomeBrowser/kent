/* hmmPfamToTab - Convert hmmPfam output to something simple and tab-delimited.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hmmPfamParse.h"

static char const rcsid[] = "$Id: hmmPfamToTab.c,v 1.1 2007/03/23 08:21:15 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hmmPfamToTab - Convert hmmPfam output to something simple and tab-delimited.\n"
  "usage:\n"
  "   hmmPfamToTab inFile out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hmmPfamToTab(char *inFile, char *outFile)
/* hmmPfamToTab - Convert hmmPfam output to something simple and tab-delimited.. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct hpfResult *hr;
while ((hr = hpfNext(lf)) != NULL)
     {
     struct hpfModel *mod;
     for (mod = hr->modelList; mod != NULL; mod = mod->next)
         {
	 struct hpfDomain *dom;
	 for (dom = mod->domainList; dom != NULL; dom = dom->next)
	     fprintf(f, "%s\t%d\t%d\t%s\n", hr->name, dom->qStart, dom->qEnd, mod->name);
	 }
     hpfResultFree(&hr);
     }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hmmPfamToTab(argv[1], argv[2]);
return 0;
}
