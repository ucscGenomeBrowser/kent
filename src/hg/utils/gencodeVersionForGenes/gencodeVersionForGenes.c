/* gencodeVersionForGenes - Figure out which version of a gencode gene set a set of gene 
 * identifiers best fits. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"
#include "sqlNum.h"
#include "sqlList.h"

boolean clUpperCase = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gencodeVersionForGenes - Figure out which version of a gencode gene set a set of gene \n"
  "identifiers best fits\n"
  "usage:\n"
  "   gencodeVersionForGenes genes.txt geneSymVer.tsv\n"
  "where:\n"
  "   genes.txt is a list of gene symbols or identifiers, one per line\n"
  "   geneSymVer.tsv is output of gencodeGeneSymVer, usually /hive/data/inside/geneSymVerTx.tsv\n"
  "options:\n"
  "   -bed=output.bed - Create bed file for mapping genes to genome via best gencode fit\n"
  "   -upperCase - Force genes to be upper case\n"
  "   -allBed=outputDir - Output beds for all versions in geneSymVer.tsv\n"
  "   -geneToId=geneToId.tsv - Output two column file with symbol from gene.txt and gencode\n"
  "                  gene names as second. Symbols with no gene found are omitted\n"
  "   -miss=output.txt - unassigned genes are put here, one per line\n"
  "   -target=ucscDb - something like hg38 or hg19.  If set this will use most recent\n"
  "                version of each gene that exists for the assembly in symbol mode\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bed", OPTION_STRING},
   {"allBed", OPTION_STRING},
   {"geneToId", OPTION_STRING},
   {"miss", OPTION_STRING},
   {"target", OPTION_STRING},
   {"upperCase", OPTION_BOOLEAN},
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
if (clUpperCase)
   touppers(ret->symbol);
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

void saveGsvtAsBed(struct gsvt *gsvt, char *name, char *symbol, FILE *f)
/* Print out one gsvt as a bed record to f*/
{
fprintf(f, "%s\t%d\t%d\t%s\t", 
    gsvt->chrom, gsvt->chromStart, gsvt->chromEnd, name);
fprintf(f, "%d\t%s\t%d\t%d\t", gsvt->score, gsvt->strand, gsvt->thickStart, gsvt->thickEnd);
fprintf(f, "0\t%u\t", gsvt->blockCount);

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
fprintf(f, "\t%s", symbol);
fprintf(f, "\n");
}

void writeOneVersionOfGvstAsBed(struct gsvt *list, char *version, char *bedOut)
/* If they ask for BED give them the whole geneset for their best version */
{
if (bedOut != NULL)
{
FILE *f = mustOpen(bedOut, "w");
struct gsvt *gsvt;
for (gsvt = list; gsvt != NULL; gsvt = gsvt->next)
	{
	if (sameString(gsvt->gencodeVersion, version))
	    saveGsvtAsBed(gsvt, gsvt->gene, gsvt->symbol, f);
	}
    carefulClose(&f);
    }
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

int versionCmp(const void *va, const void *vb)
/* Compare to sort so most recent version comes last. */
{
const struct version *a = *((struct version **)va);
const struct version *b = *((struct version **)vb);
int diff = cmpStringsWithEmbeddedNumbers(a->ucscDb, b->ucscDb);
if (diff == 0)
   diff = cmpStringsWithEmbeddedNumbers(a->name, b->name);
return diff;
}

boolean isSortedByDb(struct version *list)
/* ErrAbort with a message if list is not sorted by db */
{
struct version *prev = list;
if (prev == NULL)
  return TRUE;
for (;;)
    {
    struct version *next = prev->next;
    if (next == NULL)
        return TRUE;
    if (strcmp(prev->ucscDb, next->ucscDb) < 0)
        return FALSE;
    prev = next;
    }
}

boolean isAccForm(char *s, char *prefix, int prefixSize, int digitCount, boolean withVersion)
/* Return TRUE if s is form <prefix><digitCount digits>[.version] */
{
if (!startsWith(prefix, s))
    return FALSE;
s += prefixSize;
int digits = countLeadingDigits(s);
if (digits != digitCount)
    return FALSE;
s += digitCount;
if (withVersion)
    {
    if (s[0] != '.')
        return FALSE;
    s += 1;
    digits = countLeadingDigits(s);
    if (s[digits] != 0)
        return FALSE;
    }
else
    {
    if (s[0] != 0)
        return FALSE;
    }
return TRUE;
}

int countEnsgForms(struct slName *geneList, boolean withVersion)
/* Count # of elements on list that are of form ENSG + 11 digits + maybe .digits */
{
/* Define constants for ENSG type stuff */
char *prefix = "ENSG";
int digitCount = 11;
int prefixSize = strlen(prefix);

int count = 0;
struct slName *gene;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    if (isAccForm(gene->name, prefix, prefixSize, digitCount, withVersion))
        ++count;
    }
return count;
}

struct hash *combinedDbHash(struct version *versionList, char *targetDb, boolean isSym)
/* Make a hash that will return the most recent matching gsvt assuming versions sorted
 * from oldest to most recent */
{
struct hash *combinedHash = hashNew(0);
struct hash *geneVerHash = hashNew(0);
struct version *v;
for (v = versionList; v != NULL; v = v->next)
    {
    if (!sameString(targetDb, v->ucscDb))
        continue;
    struct hash *verHash = (isSym ?v->symHash : v->idHash);
    struct hashEl *hel;
    struct hashCookie cookie = hashFirst(verHash);
    while ((hel = hashNext(&cookie)) != NULL)
	{
	/* If this is the version where we have first seen this gene, then
	 * add it. */
	struct version *geneVer = hashFindVal(geneVerHash, hel->name);
	if (geneVer == NULL)
	    {
	    geneVer = v;
	    hashAdd(geneVerHash, hel->name, geneVer);
	    }
	if (geneVer == v)
	    {
	    hashAdd(combinedHash, hel->name, hel->val);
	    }
	}
    }
hashFree(&geneVerHash);
return combinedHash;
}

int countHashHits(struct hash *hash, struct slName *list)
/* Return count of number of things on list that are in hash */
{
int count = 0;
struct slName *el;
for (el = list; el != NULL; el = el->next)
    if (hashLookup(hash, el->name))
        ++count;
return count;
}

void gencodeVersionForGenes(char *geneListFile, char *geneSymVerTxFile, char *targetDb,
    char *geneToIdOut, char *bedOut, char *allBedOut, char *missesOut)
/* gencodeVersionForGenes - Figure out which version of a gencode gene set a 
 * set of gene identifiers best fits. */
{
/* Read up gene list file */
struct slName *geneList = readAllLines(geneListFile);
int geneCount = slCount(geneList);
verbose(2, "Read %d from %s\n", geneCount, geneListFile);
if (geneCount <= 0)
    errAbort("%s is empty", geneListFile);

/* Optionally uppercase genes. */
if (clUpperCase)
   {
   struct slName *el;
   for (el = geneList; el != NULL; el = el->next)
      touppers(el->name);
   }

/* Read geneSymVerTx file and create versions from it */
struct version *versionList = NULL;
struct gsvt *gsvt, *gsvtList = gsvtLoadAll(geneSymVerTxFile);
verbose(2, "Got %d genes from many versions in %s\n", slCount(gsvtList), geneSymVerTxFile);

/* Classify it as dotted/undotted/symboled. Strip dots off of gene list if needs undotting */
int dottedCount = countEnsgForms(geneList, TRUE);
int undottedCount = countEnsgForms(geneList, FALSE);
boolean isUndotted = (undottedCount > geneCount/2);
if (isUndotted)
    {
    for (gsvt = gsvtList; gsvt!=NULL; gsvt = gsvt->next)
        chopSuffix(gsvt->gene);
    }
    
verbose(2, "dottedCount %d, undottedCount %d, isUndotted %d\n", 
    dottedCount, undottedCount, isUndotted);

struct hash *versionHash = hashNew(0);
for (gsvt = gsvtList; gsvt!=NULL; gsvt = gsvt->next)
    {
    char versionString[256];
    safef(versionString, sizeof(versionString), "%s %s", gsvt->ucscDb, gsvt->gencodeVersion);
    struct version *version = hashFindVal(versionHash, versionString);
    if (version == NULL)
        {
	AllocVar(version);
	version->idHash = hashNew(0);
	version->symHash = hashNew(0);
	version->ucscDb = cloneString(gsvt->ucscDb);
	version->name = cloneString(gsvt->gencodeVersion);
	hashAdd(versionHash, versionString, version);
	slAddHead(&versionList, version);
	}
    hashAdd(version->idHash, gsvt->gene, gsvt);
    hashAdd(version->symHash, gsvt->symbol, gsvt);
    }
slSort(&versionList, versionCmp);
slReverse(&versionList);
verbose(1, "examining %d versions of gencode and refseq\n", versionHash->elCount);

if (allBedOut != NULL)
    {
    makeDirsOnPath(allBedOut);
    struct version *v;
    for (v = versionList; v != NULL; v = v->next)
        {
	char fileName[PATH_LEN];
	char *name = cloneString(v->name);
	subChar(name, 'V', '-');
	safef(fileName, sizeof(fileName), "%s/%s.%s.bed", allBedOut, v->ucscDb, name);
	writeOneVersionOfGvstAsBed(gsvtList, v->name, fileName);
	}
    }

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

/* Report best version, maybe not best one we will actually use though */
if (bestVersion == NULL)
    errAbort("Can't find any matches to any versions of gencode");
verbose(1, "best is %s as %s on %s with %d of %d (%g%%) hits\n", bestVersion->name, 
    (bestIsSym?"sym":"id"), bestVersion->ucscDb, bestCount, geneCount, 100.0 * bestCount/geneCount);

/* Now we want to make a hash that will give us a gsvt for our genes. We start with the hash
 * for the best gencode version. */
struct hash *gsvtHash = (bestIsSym ?bestVersion->symHash : bestVersion->idHash);
struct hash *targetHash = NULL;


/* If they have a target db, we do more processing, making a hash out of all versions that
 * target that database instead. */
if (targetDb != NULL)
    {
    gsvtHash = targetHash = combinedDbHash(versionList, targetDb, bestIsSym);
    int hitCount = countHashHits(targetHash, geneList);
    verbose(1, "on %s %d of %d (%g%%) hit across versions\n", targetDb, 
	hitCount, geneCount, 100.0 * hitCount/geneCount);
    }

if (bedOut != NULL)
    {
    FILE *f = mustOpen(bedOut, "w");
    struct slName *gene;
    for (gene = geneList; gene != NULL; gene = gene->next)
        {
	/* There may actually be more than one mapping of the same gene, especially
	 * if we're using symbols.   So output each mapping, not just the first. */
	struct hashEl *hel;
        for (hel = hashLookup(gsvtHash, gene->name); hel != NULL; hel = hashLookupNext(hel))
	    {
	    struct gsvt *gsvt = hel->val;
	    if (gsvt != NULL)
		{
		if (bestIsSym)
		    saveGsvtAsBed(gsvt, gene->name, gsvt->gene, f);
		else
		    saveGsvtAsBed(gsvt, gene->name, gsvt->symbol, f);
		}
	    }
	}
    carefulClose(&f);
    }

if (geneToIdOut != NULL)
    {
    FILE *f = mustOpen(geneToIdOut, "w");
    struct slName *gene;
    for (gene = geneList; gene != NULL; gene = gene->next)
        {
	struct gsvt *gsvt = hashFindVal(gsvtHash, gene->name);
	if (gsvt != NULL)
	    fprintf(f, "%s\t%s\n", gene->name, gsvt->gene);
	}
    carefulClose(&f);
    }

if (missesOut != NULL)
    {
    FILE *f = mustOpen(missesOut, "w");
    struct slName *gene;
    for (gene = geneList; gene != NULL; gene = gene->next)
        {
	if (hashLookup(gsvtHash, gene->name) == NULL)
	    fprintf(f, "%s\n", gene->name);
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
clUpperCase = optionExists("upperCase");
gencodeVersionForGenes(argv[1],argv[2], optionVal("target", NULL),
    optionVal("geneToId", NULL), optionVal("bed", NULL), 
    optionVal("allBed", NULL), optionVal("miss", NULL));
return 0;
}
