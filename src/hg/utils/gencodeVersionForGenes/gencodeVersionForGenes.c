/* gencodeVersionForGenes - Figure out which version of a gencode gene set a set of gene 
 * identifiers best fits. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "sqlList.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gencodeVersionForGenes - Figure out which version of a gencode gene set a set of gene \n"
  "identifiers best fits\n"
  "usage:\n"
  "   gencodeVersionForGenes genes.txt geneSymVer.tsv\n"
  "options:\n"
  "   -bed=output.bed - Create bed file for mapping genes to best genome\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bed", OPTION_STRING},
   {NULL, 0},
};

struct gsvt
/* Gene symbol version bestTx - output of gencodeGeneSymVerTx */
    {
    struct gsvt *next;  /* Next in singly linked list. */
    char *gene;	/* Gene id */
    char *symbol;	/* HUGO or similar symbol */
    char *gencodeVersion;	/* gencodeV# */
    char *ucscDb;	/* hg19, hg38, something like that */
    char *chrom;	/* chromosome */
    unsigned chromStart;	/* Start position in chromosome */
    unsigned chromEnd;	/* End position in chromosome */
    char *transcript;	/* name of best transcript */
    int score;	/* usually 0 */
    char strand[2];	/* Strand - either plus or minus */
    unsigned thickStart;	/* Start position in chromosome */
    unsigned thickEnd;	/* End position in chromosome */
    char *itemRgb;	/* Used as itemRgb */
    unsigned blockCount;	/* Number of blocks in alignment */
    unsigned *blockSizes;	/* Size of each block */
    unsigned *blockStarts;	/* Start of each block in query. */
    };

struct gsvt *gsvtLoad(char **row)
/* Load a gsvt from row fetched with select * from gsvt
 * from database.  Dispose of this with gsvtFree(). */
{
struct gsvt *ret;

AllocVar(ret);
ret->blockCount = sqlUnsigned(row[13]);
ret->gene = cloneString(row[0]);
ret->symbol = cloneString(row[1]);
ret->gencodeVersion = cloneString(row[2]);
ret->ucscDb = cloneString(row[3]);
ret->chrom = cloneString(row[4]);
ret->chromStart = sqlUnsigned(row[5]);
ret->chromEnd = sqlUnsigned(row[6]);
ret->transcript = cloneString(row[7]);
ret->score = sqlSigned(row[8]);
safecpy(ret->strand, sizeof(ret->strand), row[9]);
ret->thickStart = sqlUnsigned(row[10]);
ret->thickEnd = sqlUnsigned(row[11]);
ret->itemRgb = cloneString(row[12]);
int sizeOne;
sqlUnsignedDynamicArray(row[14], &ret->blockSizes, &sizeOne);
assert(sizeOne == ret->blockCount);
sqlUnsignedDynamicArray(row[15], &ret->blockStarts, &sizeOne);
assert(sizeOne == ret->blockCount);
return ret;
}

struct gsvt *gsvtLoadAll(char *fileName) 
/* Load all gsvt from a whitespace-separated file.
 * Dispose of this with gsvtFreeList(). */
{
struct gsvt *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[16];

while (lineFileRow(lf, row))
    {
    el = gsvtLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct version
/* Keep info on one version */
    {
    struct version *next;
    char *name;
    char *ucscDb;
    int symHit;
    int idHit;
    struct hash *idHash;
    struct hash *symHash;
    };

void gencodeVersionForGenes(char *geneListFile, char *geneSymVerTxFile, char *bedOut)
/* gencodeVersionForGenes - Figure out which version of a gencode gene set a 
 * set of gene identifiers best fits. */
{
/* Read up gene list file */
struct slName *geneList = readAllLines(geneListFile);
int geneCount = slCount(geneList);
verbose(2, "Read %d from %s\n", geneCount, geneListFile);
if (geneCount <= 0)
    errAbort("%s is empty", geneListFile);

/* Read geneSymVerTx file and create versions from it */
struct version *versionList = NULL;
struct gsvt *gsvt, *gsvtList = gsvtLoadAll(geneSymVerTxFile);
verbose(2, "Got %d genes from many versions in %s\n", slCount(gsvtList), geneSymVerTxFile);
struct hash *versionHash = hashNew(0);
for (gsvt = gsvtList; gsvt!=NULL; gsvt = gsvt->next)
    {
    char *versionString = gsvt->gencodeVersion;
    struct version *version = hashFindVal(versionHash, versionString);
    if (version == NULL)
        {
	AllocVar(version);
	version->idHash = hashNew(0);
	version->symHash = hashNew(0);
	version->ucscDb = cloneString(gsvt->ucscDb);
	hashAddSaveName(versionHash, versionString, version, &version->name);
	slAddHead(&versionList, version);
	}
    hashAdd(version->idHash, gsvt->gene, gsvt);
    hashAdd(version->symHash, gsvt->symbol, gsvt);
    }
verbose(1, "examining %d versions of gencode\n", versionHash->elCount);

/* Loop through each version counting up genes etc. */
struct version *bestVersion = NULL;
int bestCount = FALSE;
boolean bestIsSym = FALSE;

struct version *v;
for (v = versionList; v != NULL; v = v->next)
    {
    struct slName *gene;
    int idHits = 0, symHits = 0;
    for (gene = geneList; gene != NULL; gene = gene->next)
        {
	if (hashLookup(v->idHash, gene->name))
	   ++idHits;
	if (hashLookup(v->symHash, gene->name))
	   ++symHits;
	}
    verbose(2, "%s\t%s\t%d\t%d\n", v->name, v->ucscDb, idHits, symHits);
    if (symHits > bestCount)
        {
	bestVersion = v;
	bestCount = symHits;
	bestIsSym = TRUE;
	}
    if (idHits > bestCount)
        {
	bestVersion = v;
	bestCount = idHits;
	bestIsSym = FALSE;
	}
    }
verbose(1, "best is %s as %s with %d of %d (%g%%) hits\n", bestVersion->name, 
    (bestIsSym?"sym":"id"),   bestCount, geneCount, 100.0 * bestCount/geneCount);

if (bedOut != NULL)
    {
    FILE *f = mustOpen(bedOut, "w");
    struct version *v = bestVersion;
    struct slName *gene;
    for (gene = geneList; gene != NULL; gene = gene->next)
        {
	char *geneName = gene->name;
	struct gsvt *gsvt = (bestIsSym ? hashFindVal(v->symHash, geneName) 
				       : hashFindVal(v->idHash, geneName) );
	if (gsvt != NULL)
	    {
	    fprintf(f, "%s\t%d\t%d\t%s\t", gsvt->chrom, gsvt->chromStart, gsvt->chromEnd, geneName);
	    fprintf(f, "%d\t%s\t%d\t", gsvt->score, gsvt->strand, gsvt->thickStart);
	    fprintf(f, "%u\t", gsvt->blockCount);

	    /* Print exon-by-exon fields. */
	    int i;

	    /* Print block sizes */
	    for (i=0; i<gsvt->blockCount; ++i)
		fprintf(f, "%u,", gsvt->blockSizes[i]);
	    fprintf(f, "\t");

	    /* Print block starts */
	    for (i=0; i<gsvt->blockCount; ++i)
		fprintf(f, "%u,", gsvt->blockStarts[i]);

	    /* Print human readable form of gene symbol */
	    fprintf(f, "\t%s", gsvt->symbol);
	    fprintf(f, "\n");

	    }
	}
    carefulClose(&f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
gencodeVersionForGenes(argv[1],argv[2], optionVal("bed", NULL));
return 0;
}
