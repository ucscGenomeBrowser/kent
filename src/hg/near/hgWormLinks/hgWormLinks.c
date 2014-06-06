/* hgWormLinks - Create table that links worm ORF name to description and SwissProt. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgWormLinks - Create table that links worm ORF name to description\n"
  "and SwissProt.  This works on a WormBase dump, in Ace format\n"
  "I believe, from Lincoln Stein.\n"
  "usage:\n"
  "   hgWormLinks swissprot_dump.txt wbConcise.txt wbGeneClass.txt orfToGene.txt output.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0}
};

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
        char *desc = hashFindVal(hash, key);
	subChar(val, '\t', ' ');
	stripChar(val, '\r');
        /* if gene exists in hash */
	if (desc != NULL) 
            {
	    struct dyString *dy = dyStringNew(1024);
            dyStringPrintf(dy, "%s  ", desc);
            dyStringAppend(dy, val);
            val = cloneString(dy->string);
            dyStringFree(&dy); 
            }
        /* add to gene and description to hash */
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
    /* if this does not exist in the hash or the entry is "NULL" */
    if ((update == NULL) || sameString(update, "NULL") )
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

void hgWormLinks(char *spFile, char *wbConciseFile, char *wbGeneClassFile, 
	char *orfToGeneFile, char *outFile)
/* Parse out ace mappings to simple column format. */
{
struct hash *orfToGeneHash = readOrfToGene(orfToGeneFile);
struct hash *classDesc = readDescriptionFile(wbGeneClassFile);
struct hash *geneDesc = readDescriptionFile(wbConciseFile);
struct lineFile *lf = lineFileOpen(spFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char seq[64];
char pep[64];
char sp[64];
char *description;
char *line, *word, *row[4];

description = "";
seq[0] = pep[0] = sp[0] = 0;
while (lineFileNext(lf, &line, NULL))
    {
     if ((word = nextWord(&line)) == NULL)
        {
        seq[0] = pep[0] = description[0] = sp[0] = 0;
        continue;
        }
    /* if it is the column headings line then skip */
    if (sameString(word, "#WormPep"))
	{
        continue;
        }                                
    else 
        {
        int wordCount = chopByChar(line, '\t', row, ArraySize(row));
        /* get WormPep ID */
	safef(pep, sizeof(pep), "%s", word);
        /* get CDS or seq name */
        safef(seq, sizeof(seq), "%s", row[0]);
        /* get description if available */
        if (wordCount == 4) 
            {
            description = row[3];
            }
        else {
            description = "";
        }
        /* get Swiss-Prot protein accession */
        safef(sp, sizeof(sp), "%s", row[2]);
        /* try to improve description if possible */
        description = updateDescription(description, seq, 
                orfToGeneHash, classDesc, geneDesc);
        fprintf(f, "%s\t%s\t%s\n", seq, sp, description);
        seq[0] = pep[0] = sp[0] = 0;
        }
    }
   
carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
hgWormLinks(argv[1],argv[2], argv[3], argv[4], argv[5]);
return 0;
}
