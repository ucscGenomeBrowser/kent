/* faNcbiToUcsc - Convert FA file from NCBI to UCSC format.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
  "   -wordBefore=xx The word before the accession, default 'gb'\n"
  "   -wordIx=N The word (starting at zero) the accession is in\n"
  "   -encode  - use ENCODE region name as fasta sequence name\n"
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
boolean encode = cgiBoolean("encode");
struct dnaSeq seq;
FILE *f = NULL;
char *wordBefore = cgiUsualString("wordBefore", "gb");
int wordIx = cgiUsualInt("wordIx", -1);
char *e = NULL;
char *nt = NULL;
ZeroVar(&seq);

if (split)
    makeDir(out);
else
    f = mustOpen(out, "w");
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
	{
	if (ntLast || encode)
	    {
	    nt = NULL;
            if (ntLast)
                {
		e = NULL;
                nt = stringIn("NT_", line);
                if (nt == NULL)
                    nt = stringIn("NG_", line);
                if (nt == NULL)
                    nt = stringIn("NC_", line);
                if (nt == NULL)
                    errAbort("Expecting NT_ NG_ or NC_in '%s'", line);
                e = strchr(nt, '|');
                if (e != NULL) *e = 0;
                e = strchr(nt, ' ');
                if (e != NULL) *e = 0;
                }
            else 
                {
                nt = stringIn("|EN", line);
                if (nt == NULL)
                    errAbort("Expecting EN in %s", line);
                nt++;
                nt = firstWordInLine(nt);
                }
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
	    char *words[32];
	    int wordCount, i;
	    char *accession = NULL;
	    wordCount = chopString(line+1, "|", words, ArraySize(words));
	    if (wordIx >= 0)
		{
		if (wordIx >= wordCount)
		    errAbort("Sorry only %d words", wordCount);
	        accession = words[wordIx];
		}
	    else
		{
		for (i=0; i<wordCount-1; ++i)
		    {
		    if (sameString(words[i], wordBefore))
			{
			accession = words[i+1];
			break;
			}
		    }
		if (accession == NULL)
		    errAbort("Couldn't find '%s' line %d of %s", 
			    wordBefore, lf->lineIx, lf->fileName);
		}
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
