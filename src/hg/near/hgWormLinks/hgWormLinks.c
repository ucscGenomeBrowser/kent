/* hgWormLinks - Create table that links worm ORF name to description and SwissProt. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"

static char const rcsid[] = "$Id: hgWormLinks.c,v 1.3 2003/10/26 08:17:08 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgWormLinks - Create table that links worm ORF name to description\n"
  "and SwissProt.  This works on a couple of WormBase dumps, in Ace format\n"
  "I believe, from Lincoln Stein.\n"
  "usage:\n"
  "   hgWormLinks swissProtWormPep.ace seqPepDesc.ace wbConcise.txt wbGeneClass.txt orfToGene.txt output.txt\n"
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


struct hash *readOrfToGene(char *fileName)
/* Read two column orf/gene file and return a hash 
 * keyed by orf with gene values. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = newHash(16);
while (lineFileRow(lf, row))
    {
    char *orf = row[0];
    char *gene = row[1];
    if (!strchr(orf, '.') || !strchr(gene, '-'))
        errAbort("%s doesn't seem to be in ORF<tab>gene<CR> format", fileName);
    hashAdd(hash, orf, cloneString(gene));
    }
lineFileClose(&lf);
return hash;
}

struct hash *readDescriptionFile(char *fileName)
/* Return two column file keyed by first column with
 * values in second column.  Strip any tabs from values. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct hash *hash = newHash(16);
while (lineFileNextReal(lf, &line))
    {
    char *key = nextWord(&line);
    char *val = skipLeadingSpaces(line);
    if (val != NULL && val[0] != 0)
        {
	subChar(val, '\t', ' ');
	stripChar(val, '\r');
	hashAdd(hash, key, cloneString(val));
	}
    }
lineFileClose(&lf);
return hash;
}

char *updateDescription(char *description, char *orf, 
		struct hash *orfToGene, struct hash *classDesc, struct hash *geneDesc)
/* The descriptions I got from Lincoln of known genes are quite
 * poor - only containing the gene name.  Here we update the
 * description with something better if possible. */
{
char *update = NULL;
char *geneName = hashFindVal(orfToGene, orf);
if (geneName != NULL)
    {
    update = hashFindVal(geneDesc, geneName);
    if (update == NULL)
        {
	char *class = cloneString(geneName);
	char *desc;
	subChar(class, '-', 0);
	desc = hashFindVal(classDesc, class);
	if (desc != NULL)
	    {
	    struct dyString *dy = dyStringNew(0);
	    dyStringPrintf(dy, "%s - a member of the gene class ", geneName);
	    dyStringAppend(dy, desc);
	    update = cloneString(dy->string);
	    dyStringFree(&dy);
	    }
	}
    }
if (update != NULL)
    return update;
else
    return description;
}

void hgWormLinks(char *spWpFile, char *seqPepDescFile, 
	char *wbConciseFile, char *wbGeneClassFile, 
	char *orfToGeneFile, char *outFile)
/* Parse out ace mappings to simple column format. */
{
struct hash *spHash = wpToSp(spWpFile);
struct hash *orfToGeneHash = readOrfToGene(orfToGeneFile);
struct hash *classDesc = readDescriptionFile(wbGeneClassFile);
struct hash *geneDesc = readDescriptionFile(wbConciseFile);
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
	description = updateDescription(description, seq, 
		orfToGeneHash, classDesc, geneDesc);
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
if (argc != 7)
    usage();
hgWormLinks(argv[1],argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
