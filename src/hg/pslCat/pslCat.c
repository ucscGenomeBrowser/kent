/* pslCat - concatenate psl files. */
#include "common.h"
#include "cheapcgi.h"
#include "portable.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCat - concatenate psl files\n"
  "usage:\n"
  "   pslCat file(s)\n"
  "options:\n"
  "   -check parses input.  Detects more errors but slower\n"
  "   -nohead omit psl header\n"
  "   -dir  files are directories (concatenate all in dirs)\n"
  "   -out=file put output to file rather than stdout\n"
  "   -ext=.xxx  limit files in directories to those with extension\n");
}

void catOnePsl(char *fileName, boolean check, FILE *out)
/* Write one .psl file to standard out.  Don't write header. */
{
struct lineFile *lf = pslFileOpen(fileName);
char *line;
int lineSize;

if (check)
    {
    struct psl *psl;

    while ((psl = pslNext(lf)) != NULL)
        {
	pslTabOut(psl, out);
	pslFree(&psl);
	}
    }
else
    {
    while (lineFileNext(lf, &line, &lineSize))
	{
	mustWrite(stdout, line, lineSize-1);
	fputc('\n', out);
	}
    }
lineFileClose(&lf);
}

void pslCat(int fileCount, char *fileNames[], 
	boolean check, boolean doDir, char *extension, char *out)
/* pslCat - concatenate psl files. */
{
int i;
char *fileName;
FILE *f = stdout;

if (out != NULL)
    f = mustOpen(out, "w");
if (!cgiBoolean("nohead"))
    pslWriteHead(f);
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    if (doDir)
        {
	struct fileInfo *list, *el;
	list = listDirX(fileName, NULL, TRUE);
	for (el = list; el != NULL; el = el->next)
	    {
	    if (extension == NULL || endsWith(el->name, extension))
	        catOnePsl(el->name, check, f);
	    }
	slFreeList(&list);
	}
    else
        {
	catOnePsl(fileName, check, f);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 2)
    usage();
pslCat(argc-1, argv+1, 
	cgiBoolean("check"), 
	cgiBoolean("dir"), cgiOptionalString("ext"),
	cgiOptionalString("out"));
return 0;
}
