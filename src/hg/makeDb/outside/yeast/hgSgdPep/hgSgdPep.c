/* hgSgdPep - Parse yeast protein fasta files into format we can load. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSgdPep - Parse yeast protein fasta files into format we can load\n"
  "usage:\n"
  "   hgSgdPep input.fa output.fa symbol.txt\n"
  "where:\n"
  "   input.fa is a file in SGD chr*.peptides.fsa format\n"
  "   output.fa is output fasta with ORF ID\n"
  "   symbol.txt is two columns: ORF GeneSymbol\n"
  "options:\n"
  "   -restrict=file Restrict to ORFs in first column of file\n"
  );
}

static struct optionSpec options[] = {
   {"restrict", OPTION_STRING},
   {NULL, 0},
};

struct hash *hashFirstWord(char *fileName)
/* Hash first word in each line. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
struct hash *hash = newHash(16);
while (lineFileNextReal(lf, &line))
    {
    word = nextWord(&line);
    hashAdd(hash, word, NULL);
    }
lineFileClose(&lf);
return hash;
}

void hgSgdPep(char *inName, char *outName, char *symbolName)
/* hgSgdPep - Parse yeast protein fasta files into format we can load. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
FILE *symF = mustOpen(symbolName, "w");
boolean inPep = FALSE;
char *line;
char *pattern = "ORFP:";
int patternSize = strlen(pattern);
struct hash *restrictHash = NULL;

if (optionExists("restrict"))
    restrictHash = hashFirstWord(optionVal("restrict", NULL));
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	char *orf, *gene;
	inPep = FALSE;
	if ((line = stringIn(pattern, line)) != NULL)
	    {
	    line += patternSize;
	    orf = nextWord(&line);
	    gene = nextWord(&line);
	    if (orf == NULL)
	        errAbort("Expecting ORF name starting with Y line %d of %s",
			lf->lineIx, lf->fileName);
	    if (gene != NULL)
		fprintf(symF, "%s\t%s\n", orf, gene);
	    if (restrictHash == NULL || hashLookup(restrictHash, orf))
		{
		fprintf(f, ">%s\n", orf);
		inPep = TRUE;
		}
	    }
	else
	    {
	    errAbort("No %s line %d of %s\n", 
	    	pattern, lf->lineIx, lf->fileName);
	    }
	}
    else if (inPep)
        fprintf(f, "%s\n", line);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgSgdPep(argv[1], argv[2], argv[3]);
return 0;
}
