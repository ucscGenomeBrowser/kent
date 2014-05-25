/* vegaBuildInfo - build vegaInfo table */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "vegaInfo.h"


void usage()
/* Explain usage and exit. */
{
errAbort("usage: vegaBuildInfo in.gtf out.tab\n"
"\tvegaBuildInfo grabs info strings out of vega.gtf and writes\n"
"\tout one per transcript\n"); 
}

char * clearString(char *str)
{
char *p;

assert(*str == '"');
str++;
p = strchr(str, '"');
assert(p != NULL);
*p = 0;

return str;
}

void vegaBuildInfo( char *gtfName, char *outName)
{
struct hash *hash = newHash(0);
FILE *out = mustOpen(outName, "w");
struct lineFile *lf = lineFileOpen(gtfName, TRUE);
struct vegaInfo vi;
char *row[30];
char *p;
int wordCount;
int ii;

while ((wordCount = lineFileChopNext(lf, row, 30)) > 0)
    {
    vi.method = row[1];
    for(ii=8; ii< wordCount; ii++)
	{
	if (sameString("gene_id", row[ii]))
		vi.geneId = clearString(row[++ii]);
	else if (sameString("transcript_id", row[ii]))
		vi.transcriptId = clearString(row[++ii]);
	else if (sameString("otter_transcript_id", row[ii]))
		vi.otterId = clearString(row[++ii]);
	else if (sameString("gene_description", row[ii]))
	    {
	    p =  row[++ii];
	    p++;
	    vi.geneDesc = p;
	    while(*p != '"')
		{
		if (*p == 0)
		    *p = ' ';
		p++;
		}
	    *p = 0;
	    }
	else 
	    ii++;
	}
    if (vi.geneDesc == NULL)
	vi.geneDesc = "N/A";

    if (!hashLookup(hash, vi.transcriptId)) 
	vegaInfoTabOut(&vi, out);
    hashAdd(hash, vi.transcriptId, vi.geneId);

    vi.geneDesc = NULL;
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
vegaBuildInfo(argv[1], argv[2]);
return 0;
}
