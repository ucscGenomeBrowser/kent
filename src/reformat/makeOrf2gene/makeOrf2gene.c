/* makeOrf2gene - Convert from Lincoln Stein's orf2gene dump to our simple format. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeOrf2gene - Convert from Lincoln Stein's orf2gene dump to our simple format\n"
  "usage:\n"
  "   makeOrf2gene in out\n");
}

struct orf2gene
/* Info on a orf/gene relationship. */
    {
    struct orf2gene *next;
    char *orf;		/* Name of orf. */
    char *gene;		/* Name of gene. */
    };

int orf2geneCmpOrf(const void *va, const void *vb)
/* Compare two orf2genes. */
{
const struct orf2gene *a = *((struct orf2gene **)va);
const struct orf2gene *b = *((struct orf2gene **)vb);
return strcmp(a->orf, b->orf);
}

void makeOrf2gene(char *inName, char *outName)
/* makeOrf2gene - Convert from Lincoln Stein's orf2gene dump to our simple format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *words[8], *s;
int wordCount;
char geneName[128], orfName[256];
boolean gotLocus = FALSE;
struct orf2gene *list = NULL, *el;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (sameString("Locus", words[0]))
        {
	if (wordCount < 3)
	    errAbort("Expect at least 3 space delimited words in Locus line %d of %s",
	    	lf->lineIx, lf->fileName);
	if (words[2][0] != '"')
	    errAbort("Expecting quoted string in Locus line %d of %s", 
	    	lf->lineIx, lf->fileName);
        if (!parseQuotedString(words[2], geneName, &s))
	    errAbort("Missing end quote line %d of %s", 
	    	lf->lineIx, lf->fileName);
	gotLocus = TRUE;
	}
    else if (sameString("Genomic_Sequence", words[0]))
	{
	if (!gotLocus)
	    errAbort("Expecting Locus line before Genomic_Sequence line %d of %s",
	    	lf->lineIx, lf->fileName);
	if (wordCount < 2)
	    errAbort("Expect at least 2 space delimited words in Genomic_Sequence line %d of %s",
	    	lf->lineIx, lf->fileName);
	if (words[1][0] != '"')
	    errAbort("Expecting quoted string in Genomic_Sequence line %d of %s", 
	    	lf->lineIx, lf->fileName);
        if (!parseQuotedString(words[1], orfName, &s))
	    errAbort("Missing end quote line %d of %s", 
	    	lf->lineIx, lf->fileName);
	AllocVar(el);
	el->orf = cloneString(orfName);
	el->gene = cloneString(geneName);
	slAddHead(&list, el);
	}
    }
lineFileClose(&lf);
printf("Read %d orf/gene pairs\n", slCount(list));

slSort(&list, orf2geneCmpOrf);
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s %s\n", el->orf, el->gene);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
makeOrf2gene(argv[1], argv[2]);
return 0;
}
