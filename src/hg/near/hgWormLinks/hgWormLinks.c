/* hgWormLinks - Create table that links worm ORF name to description and SwissProt. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: hgWormLinks.c,v 1.1 2003/09/25 00:05:28 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgWormLinks - Create table that links worm ORF name to description\n"
  "and SwissProt.  This works on a couple of WormBase dumps in Ace format\n"
  "I believe from Lincoln Stein.\n"
  "usage:\n"
  "   hgWormLinks swissProtWormPep.ace seqPepDesc.ace\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *wpToSp(char *fileName)
/* Make hash that maps from wormPep to SwissProt/Trembl. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char swissProt[64];
boolean gotIt = FALSE;
char *line, *word, *row[4];
struct hash *hash = newHash(17);
int count = 0;

while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (startsWith("Protein", line))
        gotIt = FALSE;
    if (startsWith("Database", line))
        {
	int wordCount = chopLine(line, row);
	if (wordCount >= 4)
	    {
	    gotIt = TRUE;
	    safef(swissProt, sizeof(swissProt), "%s", row[3]);
	    }
	}
    else if (startsWith("WORMPEP", line))
        {
	if (gotIt)
	    {
	    int wordCount = chopLine(line, row);
	    lineFileExpectWords(lf, 3, wordCount);
	    hashAdd(hash, row[2], cloneString(swissProt));
	    ++count;
	    }
	}
    }
if (count == 0)
   errAbort("%s doesn't seem to be a Ace dump of WormPep to SwissProt/TRembl",
   	fileName);
lineFileClose(&lf);
return hash;
}

void hgWormLinks(char *spWpFile, char *seqPepDescFile, char *outFile)
/* Parse out ace mappings to simple column format. */
{
struct hash *spHash = wpToSp(spWpFile);
struct lineFile *lf = lineFileOpen(seqPepDescFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char seq[64];
char pep[64];
char *description;
char *sp;
char *line, *word;

seq[0] = pep[0] = 0;
while (lineFileNext(lf, &line, NULL))
    {
    if ((word = nextWord(&line)) == NULL)
	{
	seq[0] = pep[0] = 0;
        continue;
	}
    if (sameString(word, "Sequence"))
        {
	word = nextWord(&line);
	safef(seq, sizeof(seq), "%s", word);
	}
    else if (sameWord(word, "WormPep"))
        {
	word = nextWord(&line);
	safef(pep, sizeof(pep), "%s", word);
	}
    else if (sameWord(word, "Description"))
        {
	description = skipLeadingSpaces(line);
	if (!parseQuotedString(description, description, &line))
	    errAbort("Expecting quoted string line %d of %s", 
	    	lf->lineIx, lf->fileName);
	sp = hashFindVal(spHash, pep);
	if (sp == NULL) sp = "";
	fprintf(f, "%s\t%s\t%s\n", seq, sp, description);
	seq[0] = pep[0] = 0;
	}
    }
carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgWormLinks(argv[1],argv[2], argv[3]);
return 0;
}
