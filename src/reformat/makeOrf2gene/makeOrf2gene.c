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
  "   makeOrf2gene in orf2gene syn\n"
  "where orf2gene and syn are outputs in two formats\n");
}

struct orf2gene
/* Info on a orf/gene relationship. */
    {
    struct orf2gene *next;
    char *orf;		/* Name of orf. */
    char *gene;		/* Name of gene. */
    };

int orf2geneCmpOrf(const void *va, const void *vb)
/* Compare two orf2genes by orf. */
{
const struct orf2gene *a = *((struct orf2gene **)va);
const struct orf2gene *b = *((struct orf2gene **)vb);
return strcmp(a->orf, b->orf);
}

int orf2geneCmpGene(const void *va, const void *vb)
/* Compare two orf2genes by gene. */
{
const struct orf2gene *a = *((struct orf2gene **)va);
const struct orf2gene *b = *((struct orf2gene **)vb);
return strcmp(a->gene, b->gene);
}

void makeOrf2gene(char *inName, char *outName, char *synName)
/* makeOrf2gene - Convert from Lincoln Stein's orf2gene dump to our simple format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f; 
char *words[8], *s;
int wordCount;
char geneName[128], orfName[256];
boolean gotLocus = FALSE;
struct orf2gene *list = NULL, *el, *lastEl = NULL;

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

/* Write orf2gene file. */
slSort(&list, orf2geneCmpOrf);
f  = mustOpen(outName, "w");
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s %s\n", el->orf, el->gene);
carefulClose(&f);

/* Write syn file. */
slSort(&list, orf2geneCmpGene);
f = mustOpen(synName, "w");
for (el = list; el != NULL; el = el->next)
    {
    if (lastEl == NULL || differentString(el->gene, lastEl->gene))
        {
	if (lastEl != NULL)
	   fprintf(f, "\n");
	fprintf(f, "%s", el->gene);
	}
    fprintf(f, " %s", el->orf);
    lastEl = el;
    }
fprintf(f, "\n");
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
makeOrf2gene(argv[1], argv[2], argv[3]);
return 0;
}
