/* kgBioCyc1 - Create tab separated files for bioCyc tables.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: kgBioCyc1.c,v 1.2 2008/08/27 22:34:32 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgBioCyc1 - Create tab separated files for bioCyc tables.\n"
  "usage:\n"
  "   kgBioCyc1 genes.tab pathways.tab database bioCycPathway.tab bioCycMapDesc.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void checkWordMatch(char *fileName, char **words, int lineIx, int wordIx, char *expecting)
/* CHeck that words[wordIx] is same as expecting or report error and abort. */
{
if (!sameWord(words[wordIx], expecting))
    errAbort("Expecting %s, got %s, line %d word %d of %s", expecting, words[wordIx],
    	lineIx+1, wordIx+1, fileName);
}

struct hash *hashBioCycGenes(char *geneFile, char *db)
/* Build up a hash of bioCycGenes keyed by bioCyc gene name and with ucscGene values. */
{
/* Open up file, read first line, and chop it into tab separated words. */
struct lineFile *lf = lineFileOpen(geneFile, TRUE);
char *words[256];
int firstWordCount = lineFileChopNextTab(lf, words, ArraySize(words));
struct sqlConnection *conn = sqlConnect(db);
char query[1024];
struct hash *hash = hashNew(16);

/* Verify that the first fields of the first line are what we think.  THese are the
 * labels for the table. */
if (firstWordCount < 9)
    errAbort("Not enough fields in %s\n", geneFile);
checkWordMatch(geneFile, words, 0, 0, "UNIQUE-ID");
checkWordMatch(geneFile, words, 0, 1, "NAME");
checkWordMatch(geneFile, words, 0, 2, "PRODUCT-NAME");
checkWordMatch(geneFile, words, 0, 3, "SWISS-PROT-ID");
checkWordMatch(geneFile, words, 0, 4, "REPLICON");
checkWordMatch(geneFile, words, 0, 5, "START-BASE");
checkWordMatch(geneFile, words, 0, 6, "END-BASE");

/* Loop through each gene, trying to find a UCSC gene that fits. */
int wordCount;
while ((wordCount = lineFileChopNextTab(lf, words, ArraySize(words))) > 0)
    {
    lineFileExpectWords(lf, firstWordCount, wordCount);
    char *bioCycId = words[0];
    char *symbol = words[1];
    char *knownGeneId = NULL;

    /* BioCycId is really an ENSEMBL Gene ID.  Use following SQL to look it up. */
    safef(query, sizeof(query), 
       "select distinct(knownCanonical.transcript)  "
       "from ensGtp,knownToEnsembl,knownIsoforms,knownCanonical "
       "where ensGtp.gene = '%s' "
       "and ensGtp.transcript = knownToEnsembl.value "
       "and knownToEnsembl.name=knownIsoforms.transcript "
       "and knownCanonical.clusterId = knownIsoforms.clusterId", bioCycId);
    knownGeneId = sqlQuickString(conn, query);

    /* If we don't have it, make an attempt to get it from gene symbol instead. */
    if (knownGeneId == NULL)
        {
	char *escSym = makeEscapedString(symbol, '\'');
	safef(query, sizeof(query), "select distinct(knownCanonical.transcript) "
	"from kgXref,knownIsoforms,knownCanonical "
	"where kgXref.geneSymbol = '%s' "
	"and kgXref.kgID=knownIsoforms.transcript "
	"and knownIsoforms.clusterId = knownCanonical.clusterId", escSym);
	knownGeneId = sqlQuickString(conn, query);
	freeMem(escSym);
	}

    if (knownGeneId != NULL)
        hashAdd(hash, bioCycId, knownGeneId);
    }
sqlDisconnect(&conn);
return hash;
}

boolean allBlank(char **words, int start, int end)
/* Return true if all words from start to end are empty. */
{
int i;
for (i=start; i<end; ++i)
    if (!isEmpty(words[i]))
        return FALSE;
return TRUE;
}

void kgBioCyc1(char *inGenes, char *inPathways, 
	char *inDatabase, char *outPathway, char *outDesc)
/* kgBioCyc1 - Create tab separated files for bioCyc tables.. */
{
struct hash *hash = hashBioCycGenes(inGenes, inDatabase);
uglyf("Got %d elements in %s\n", hash->elCount, inGenes);

/* Open, read in first line of pathways file (which contains column labels)
 * and sanity check it. */
struct lineFile *lf = lineFileOpen(inPathways, TRUE);
char *words[64*1024];
int firstWordCount = lineFileChopNextTab(lf, words, ArraySize(words));
if (firstWordCount < 3)
    errAbort("Not enough fields in %s\n", inPathways);
checkWordMatch(inPathways, words, 0, 0, "UNIQUE-ID");
checkWordMatch(inPathways, words, 0, 1, "NAME");

/* Figure out the first and last columns that have GENE-ID */
int firstGeneId = 0, lastGeneId = 0;
int i;
for (i=2; i<firstWordCount; ++i)
    {
    if (sameWord("GENE-ID", words[i]))
       {
       if (firstGeneId == 0) 
	   {
           firstGeneId = i;
	   lastGeneId = i;
	   }
       else
           {
	   if (lastGeneId != i-1)
	       errAbort("Expecting GENE-ID to be all together in line 1 of %s", inPathways);
	   lastGeneId = i;
	   }
       }
    }
if (firstGeneId == 0)
    errAbort("Found no GENE-ID in first line of %s", inPathways);

/* Open output files. */
FILE *fDesc = mustOpen(outDesc, "w");
FILE *fPathway = mustOpen(outPathway, "w");

/* GEt a database connection. */
struct sqlConnection *conn = sqlConnect(inDatabase);
char query[512];

/* Loop through remaining lines, one for each pathway. */
int wordCount;
while ((wordCount = lineFileChopNextTab(lf, words, ArraySize(words))) > 0)
    {
    char *pathwayId = words[0];
    char *description = words[1];
    fprintf(fDesc, "%s\t%s\n", pathwayId, description);

    /* Do a little error checking, and then write out pathway/description file. */
    if (firstWordCount != wordCount)
        {
	/* The lines with no genes in the pathway seem to be missing some tabs.... */
	if (!allBlank(words, 2, wordCount))
	    errAbort("Expecting %d words got %d line %d of %s.", 
		    firstWordCount, wordCount, lf->lineIx, lf->fileName);
	continue;
	}

    /* Write out a line in pathway file for each non-empty gene-id */
    int i;
    for (i=firstGeneId; i <= lastGeneId; ++i)
        {
	char *gene = words[i];
	if (!isEmpty(gene))
	   {
	   char *ucscId = hashFindVal(hash, gene);
	   if (ucscId != NULL)
	       {
	       safef(query, sizeof(query), "select geneSymbol from kgXref where kgID = '%s'",
		    ucscId);
	       char *geneSymbol = sqlQuickString(conn, query);
	       fprintf(fPathway, "%s\t%s\t%s\n", ucscId, geneSymbol, pathwayId);
	       freeMem(geneSymbol);
	       }
	   }
	}
    }

/* Clean up. */
carefulClose(&fDesc);
carefulClose(&fPathway);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
kgBioCyc1(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
