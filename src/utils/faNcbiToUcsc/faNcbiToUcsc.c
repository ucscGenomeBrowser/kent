/* faNcbiToUcsc - Convert FA file from NCBI to UCSC format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faNcbiToUcsc - Convert FA file from NCBI to UCSC format.\n"
  "usage:\n"
  "   faNcbiToUcsc inFile outFile\n"
  "options:\n"
  "   -split - split into separate files\n"
  "   -ntLast - look for NT_ on last bit\n"
  );
}

void faNcbiToUcsc(char *inFile, char *out)
/* faNcbiToUcsc - Convert FA file from NCBI to UCSC format.. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char outName[512];
char *line;
boolean split = cgiBoolean("split");
boolean ntLast = cgiBoolean("ntLast");
struct dnaSeq seq;
FILE *f = NULL;

if (split)
    makeDir(out);
else
    f = mustOpen(out, "w");
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
	{
	if (ntLast)
	    {
	    char *nt = stringIn("NT_", line);
	    char *e;
	    if (nt == NULL)
		errAbort("Expecting NT_ in %s", line);
	    e = strchr(nt, '|');
	    if (e != NULL) *e = 0;
	    e = strchr(nt, ' ');
	    if (e != NULL) *e = 0;
	    if (split)
		{
		sprintf(outName, "%s/%s.fa", out, nt);
		carefulClose(&f);
		f = mustOpen(outName, "w");
		}
	    fprintf(f, ">%s\n", nt);
	    }
	else
	    {
	    char *words[8];
	    int wordCount, i;
	    char *accession = NULL;
	    wordCount = chopString(line+1, "|", words, ArraySize(words));
	    if (sameString(words[0], "ri"))
		{
		accession = words[2];
		}
	    else
		{
		for (i=0; i<wordCount-1; ++i)
		    {
		    if (sameString(words[i], "gb"))
			{
			accession = words[i+1];
			break;
			}
		    }
		}
	    if (accession == NULL)
	        errAbort("Couldn't find 'gb' line %d of %s", lf->lineIx, lf->fileName);
	    chopSuffix(accession);
	    fprintf(f, ">%s\n", accession);
	    }
	}
    else
        {
	fprintf(f, "%s\n", line);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
faNcbiToUcsc(argv[1], argv[2]);
return 0;
}
