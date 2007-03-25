/* hmmPfamToTab - Convert hmmPfam output to something simple and tab-delimited.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hmmPfamParse.h"

static char const rcsid[] = "$Id: hmmPfamToTab.c,v 1.2 2007/03/25 18:44:47 kent Exp $";

double eVal = 0.0001;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hmmPfamToTab - Convert hmmPfam output to something simple and tab-delimited.\n"
  "usage:\n"
  "   hmmPfamToTab inFile out.tab\n"
  "options:\n"
  "   -eVal=0.N - Set eVal threshold. Default %g\n"
  , eVal
  );
}

static struct optionSpec options[] = {
   {"eVal", OPTION_DOUBLE},
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
	     {
	     if (dom->eVal <= eVal)
		 fprintf(f, "%s\t%d\t%d\t%s\n", hr->name, dom->qStart, dom->qEnd, mod->name);
	     }
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
eVal = optionDouble("eVal", eVal);
hmmPfamToTab(argv[1], argv[2]);
return 0;
}
