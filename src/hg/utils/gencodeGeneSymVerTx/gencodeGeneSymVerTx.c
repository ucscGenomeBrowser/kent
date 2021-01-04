/* gencodeGeneSymVerTx - Create a tab separated file with gene ID/symbol/best transcript mapping for many 
 * version of gencode.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "genePred.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gencodeGeneSymVerTx - Create a tab separated file with gene ID/symbol/best transcript mapping \n"
  "for many version of gencode.\n"
  "        /hive/data/genomes/hg*/bed/gencodeV17/data/gencode.tsv\n"
  "containing exactly the files we need for input.\n"
  "usage:\n"
  "   gencodeGeneSymVerTx input.list\toutput.tsv\n"
  "where input.lst is files to be run on.  The file names are parsed and should be\n"
  "of the form:\n"
  "        /hive/data/genomes/hg*/bed/gencodeV17/data/gencode.tsv\n"
  "and there needs to be a gencode.gp in same dir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct transcript
/* Collects info for a single transcript */
    {
    struct transcript *next;
    char *name;	    /* ENST000... */
    char *status;
    char *type;
    struct slName *tags;
    };

struct gene
/* Collects info from multiple transcripts */
    {
    struct gene *next;
    char *name;			/* ENSG000... */
    char *symbol;	/* HUGO name */
    struct transcript *txList;	/* List of transcripts */
    struct transcript *bestTx;  /* Best transcript */
    };

struct version
/* One record per gencode version */
    {
    struct version *next;	/* Next in list */
    char *name;			/* gencodeV19 gencodeV34 etc */
    char *metaName;		/* Full path to gencode.tsv file */
    char *genePredName;		/* Full path to gencode.gp file */
    char *ucscDb;		/* hg19, hg38, etc */
    struct gene *geneList;	/* List of genes */
    struct hash *gpHash;	/* Hash of genePredictions keyed by transcript ID */
    };

int scoreTranscript(struct transcript *tx)
/* Return a score for transcripts, higher is better.  Weights are ad hoc fit to data */
{
int score = 0;
struct slName *tag;
for (tag = tx->tags; tag != NULL; tag = tag->next)
     {
     char *name = tag->name;
     if (sameString("appris_principal", name))
          score += 30000;
     if (sameString("appris_principal_1", name))
          score += 30000;
     else if (startsWith("appris_principal", name))
          score += 15000;
     else if (sameString("appris_candidate_highest_score", name))
          score += 20000;
     else if (sameString("appris_candidate_longest_ccds", name))
          score += 15100;
     else if (sameString("appris_candidate_longest", name))
          score += 15000;
     else if (sameString("appris_candidate_longest_seq", name))
          score += 15000;
     else if (sameString("appris_candidate_ccds", name))
          score += 14000;
     else if (sameString("appris_candidate", name))
          score += 10000;
     else if (startsWith("appris_alternative", name))
          score += 0;
     else if (startsWith("appris_", name))
          errAbort("Unknown appris %s", name);
     else if (sameString("CCDS", name))
          score += 3000;
      }
char *status = tx->status;
if (sameString("KNOWN", status))
    score += 300;
else if (sameString("NOVEL", status))
    score += 200;

char *type = tx->type;
if (sameString("protein_coding", type))
    score += 300;
else if (sameString("antisense", type))
    score -= 100;
else if (sameString("sense_intronic", type))
    score -= 100;
else if (endsWith("pseudogene", type))
    score -= 1000;
return score;
}

struct slName *parseTags(struct hash *tagHash, char *tagString)
/* Parse tagString to reveal string list. */
{
struct slName *tagList = slNameListFromString(tagString, ',');
struct slName *tag;
for (tag = tagList; tag != NULL; tag = tag->next)
    hashIncInt(tagHash, tag->name);
return tagList;
}

struct hash *hashGenePredFile(char *fileName)
/* Load in a genepred into a hash keyed by name */
{
struct genePred *gp, *gpList = genePredLoadAll(fileName);
struct hash *hash = hashNew(0);
for (gp = gpList; gp != NULL; gp = gp->next)
    hashAdd(hash, gp->name, gp);
return hash;
}


struct gene *makeGeneList(struct version *v)
/* makeGeneList - Create a table that links gene with a canonical transcript to 
 *  represent the gene for a particular version of gencode. */
{
char *metaIn = v->metaName;

/* Read in table and set up indexes into required fields */
static char *required[] = {"geneId", "geneName", "transcriptId", "tags", 
	"transcriptStatus", "transcriptType"};
struct fieldedTable *table = fieldedTableFromTabFile(metaIn, metaIn, required, ArraySize(required));
verbose(1, "Read %s with %d rows and %d fields\n", metaIn, table->rowCount, table->fieldCount);
int geneIx = fieldedTableFindFieldIx(table, "geneId");
int geneNameIx = fieldedTableFindFieldIx(table, "geneName");
int transcriptIx = fieldedTableFindFieldIx(table, "transcriptId");
int tagsIx = fieldedTableFindFieldIx(table, "tags");
int statusIx = fieldedTableFindFieldIx(table, "transcriptStatus");
int typeIx = fieldedTableFindFieldIx(table, "transcriptType");

struct hash *statusHash = hashNew(0);   // Null val uniqueness cache for status
struct hash *typeHash = hashNew(0);	// Null val uniqueness cache for type
struct hash *tagHash = hashNew(0);	// Null val uniqueness cache for tag
struct hash *txHash = hashNew(0);	// Null val uniqueness cache for transcripts

struct hash *geneHash = hashNew(0);	// gene valued
struct gene *geneList = NULL;

/* Scan through table building up transcripts and genes. */
struct fieldedRow *row;
for (row = table->rowList; row != NULL; row = row->next)
    {
    char *geneName = row->row[geneIx];
    struct gene *gene = hashFindVal(geneHash, geneName);
    if (gene == NULL)
        {
	AllocVar(gene);
	gene->symbol = cloneString(row->row[geneNameIx]);
	hashAddSaveName(geneHash, geneName, gene, &gene->name);
	slAddHead(&geneList, gene);
	}
    struct transcript *tx;
    AllocVar(tx);
    tx->status = hashStoreName(statusHash, row->row[statusIx]);
    tx->type = hashStoreName(typeHash,row->row[typeIx]);
    tx->tags = parseTags(tagHash, row->row[tagsIx]);
    tx->name = hashStoreName(txHash, row->row[transcriptIx]);
    slAddHead(&gene->txList, tx);
    }
slReverse(&geneList);
verbose(1, "Have %d genes, %d transcripts, %d status, %d type, %d tags\n", 
    geneHash->elCount, txHash->elCount, statusHash->elCount, typeHash->elCount, tagHash->elCount);

/* Find best for each gene */
struct gene *gene;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    slReverse(&gene->txList);
    struct transcript *bestTx = NULL, *tx = NULL;
    int bestScore = -BIGNUM;
    for (tx = gene->txList; tx != NULL; tx = tx->next)
        {
	int score = scoreTranscript(tx);
	if (score > bestScore)
	     {
	     bestScore = score;
	     bestTx = tx;
	     }
	}
    gene->bestTx = bestTx;
    }
verbose(1, "Found best genes\n");
return geneList;
}



struct version *versionsFromFile(char *input)
// Read in input file which is a list of things like
//      /hive/data/genomes/hg*/bed/gencodeV17/data/gencode.tsv
// and return a list of versions */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
struct version *list = NULL, *v;
while (lineFileNextReal(lf, &line))
    {
    line = trimSpaces(line);
    if (!fileExists(line))
        errAbort("%s in %s doesn't exist", line, input);
    AllocVar(v);
    if ((v->name = stringBetween("/bed/", "/data/", line)) == NULL)
        errAbort("Couldn't parse gencode version from %s", line);
    if ((v->ucscDb = stringBetween("/genomes/", "/bed/", line)) == NULL)
        errAbort("Couldn't parse ucscDb version from %s", line);
    v->metaName = cloneString(line);

    /* Make .gp file name from metaName*/
    char buf[PATH_LEN];
    chopSuffix(line);
    safef(buf, sizeof(buf), "%s.gp", line);
    v->genePredName = cloneString(buf);
    slAddHead(&list, v);
    }
slReverse(&list);
return list;
}

void gencodeGeneSymVerTx(char *input, char *output)
/* gencodeGeneSymVerTx - Create a tab separated file with gene ID/symbol/best transcript 
 * mapping for many version of gencode. */
{
struct version *v, *versionList = versionsFromFile(input);
verbose(1, "Read %d versions\n", slCount(versionList));

/* Read them all in before making output.  */
for (v = versionList; v != NULL; v = v->next)
    {
    v->geneList = makeGeneList(v);
    v->gpHash = hashGenePredFile(v->genePredName);
    }

/* Now make output */
FILE *f = mustOpen(output, "w");
fprintf(f, "#gene\tsymbol\tgencodeVersion\tucscDb\tchrom\tchromStart\tchromEnd\ttranscript\tscore\tstrand\tthickStart\tthickEnd\titemRgb\tblockCount\tblockSizes\tblockStarts\n");
for (v = versionList; v != NULL; v = v->next)
    {
    struct gene *gene;
    for (gene = v->geneList; gene != NULL; gene = gene->next)
        {
	struct hashEl *hel;
	for (hel = hashLookup(v->gpHash, gene->bestTx->name); hel != NULL; hel = hashLookupNext(hel))
	    {
	    fprintf(f, "%s\t%s\t%s\t%s\t",   
		gene->name, gene->symbol, v->name, v->ucscDb);
	    struct genePred *gp = hel->val;
	    /* Print scalar bed fields. */
	    fprintf(f, "%s\t", gp->chrom);
	    fprintf(f, "%u\t", gp->txStart);
	    fprintf(f, "%u\t", gp->txEnd);
	    fprintf(f, "%s\t", gene->bestTx->name);
	    fprintf(f, "%u\t", 0);
	    fprintf(f, "%s\t", gp->strand);
	    fprintf(f, "%u\t", gp->cdsStart);
	    fprintf(f, "%u\t", gp->cdsEnd);
	    fprintf(f, "%u\t", 0);
	    fprintf(f, "%u\t", gp->exonCount);

	    /* Print exon-by-exon fields. */
	    int i;

	    /* Print exon sizes */
	    for (i=0; i<gp->exonCount; ++i)
		fprintf(f, "%u,", gp->exonEnds[i] - gp->exonStarts[i]);
	    fprintf(f, "\t");

	    /* Print exons starts */
	    for (i=0; i<gp->exonCount; ++i)
		fprintf(f, "%u,", gp->exonStarts[i] - gp->txStart);
	    fprintf(f, "\n");
	    }
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
gencodeGeneSymVerTx(argv[1], argv[2]);
return 0;
}
