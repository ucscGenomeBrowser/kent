/* txGeneAlias - Make kgAlias and kgProtAlias tables.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "spDb.h"
#include "txRnaAccs.h"
#include "kgXref.h"


static char const rcsid[] = "$Id: txGeneAlias.c,v 1.4 2008/08/29 01:37:00 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneAlias - Make kgAlias and kgProtAlias tables.\n"
  "usage:\n"
  "   txGeneAlias genomeDb uniProtDb genes.xref genes.ev oldToNew.tab alias.tab protAlias.tab\n"
  "where:\n"
  "   genomeDb is a genome database, like hg19 or something\n"
  "   uniProtDb is a uniProt database, like sp0702020\n"
  "   genes.xref is the output of txGeneXref\n"
  "   genes.ev is one of txWalk's products that's been accessioned\n"
  "   alias.tab is a two column output: geneId,alias\n"
  "   protAlias.tab is a three column output: geneId,uniProtId,alias\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void outAlias(FILE *f, char *id, char *alias)
/* Output one alias to file. */
{
if (alias[0] != 0)
    fprintf(f, "%s\t%s\n", id, alias);
}

void outProt(FILE *f, char *id, char *protId, char *alias)
/* Output one protein alias to file */
{
if (alias[0] != 0)
    fprintf(f, "%s\t%s\t%s\n", id, protId, alias);
}

struct hash *loadNewToOldHash(char *oldToNewFile)
/* Read through 4 column file <position> <old> <new> <type> and make hash of old accessions 
 * keyed by new accession, only containing elements where new and old are different. */
{
struct lineFile *lf = lineFileOpen(oldToNewFile, TRUE);
char *row[4];
struct hash *hash = hashNew(16);
while (lineFileRowTab(lf, row))
    {
    char *oldAcc = row[1], *newAcc = row[2];
    if (newAcc[0] != 0 && !sameString(oldAcc, newAcc))
	hashAdd(hash, newAcc, cloneString(oldAcc));
    }
return hash;
}
 
void txGeneAlias(char *genomeDb, char *uniProtDb, char *xrefFile, 
	char *evFile, char *oldToNew, char *aliasFile, char *protAliasFile)
/* txGeneAlias - Make kgAlias and kgProtAlias tables.. */
{
/* Read and hash oldToNew */
struct hash *newToOldHash = loadNewToOldHash(oldToNew);

/* Load evidence into hash */
struct hash *evHash = newHash(18);
struct txRnaAccs *ev, *evList = txRnaAccsLoadAll(evFile);
for (ev = evList; ev != NULL; ev = ev->next)
    hashAdd(evHash, ev->name, ev);

/* Open connections to our databases */
struct sqlConnection *gConn = sqlConnect(genomeDb);
struct sqlConnection *uConn = sqlConnect(uniProtDb);
struct sqlResult *sr;
char **row;
char query[256];

/* Open files. */
struct lineFile *lf = lineFileOpen(xrefFile, TRUE);
FILE *fAlias = mustOpen(aliasFile, "w");
FILE *fProt = mustOpen(protAliasFile, "w");

/* Stream through xref file, which has much of the info we need,
 * and which contains a line for each gene. */
char *words[KGXREF_NUM_COLS];
while (lineFileRowTab(lf, words))
    {
    /* Load the xref, and output most of it's fields as aliases. */
    struct kgXref *x = kgXrefLoad(words);
    char *id = x->kgID;
    outAlias(fAlias, id, x->kgID);
    outAlias(fAlias, id, x->mRNA);
    outAlias(fAlias, id, x->spID);
    outAlias(fAlias, id, x->spDisplayID);
    outAlias(fAlias, id, x->geneSymbol);
    outAlias(fAlias, id, x->refseq);
    outAlias(fAlias, id, x->protAcc);
    char *old = hashFindVal(newToOldHash, id);
    if (old != NULL)
        outAlias(fAlias, id, old);

    /* If we've got a uniProt ID, use that to get more info from uniProt. */
    char *acc = x->spID;
    if (acc[0] != 0)
        {
	/* Get current accession and output a bunch of easy protein aliases. */
	acc = spLookupPrimaryAcc(uConn, acc);
	outProt(fProt, id, acc, acc);
	outProt(fProt, id, acc, x->spDisplayID);
	outProt(fProt, id, acc, x->geneSymbol);
	outProt(fProt, id, acc, x->protAcc);
	if (old != NULL)
	    outProt(fProt, id, acc, old);

	/* Throw in old swissProt accessions. */
	safef(query, sizeof(query), "select val from otherAcc where acc = '%s'", acc);
	sr = sqlGetResult(uConn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    outAlias(fAlias, id, row[0]);
	    outProt(fProt, id, acc, row[0]);
	    }

	/* Throw in gene names that SwissProt knows about */
	struct slName *gene, *geneList = spGenes(uConn, acc);
	for (gene = geneList; gene != NULL; gene = gene->next)
	    {
	    outAlias(fAlias, id, gene->name);
	    outProt(fProt, id, acc, gene->name);
	    }
	slFreeList(&geneList);
	}
    /* Throw in gene names from genbank. */
    /* At some point we may want to restrict this to the primary transcript in a cluster. */
    ev = hashFindVal(evHash,  id);
    if (ev != NULL)
	{
	int i;
	for (i=0; i<ev->accCount; ++i)
	    {
	    safef(query, sizeof(query), "select geneName from gbCdnaInfo where acc='%s'", acc);
	    int nameId = sqlQuickNum(gConn, query);
	    if (nameId != 0)
		{
		char name[64];
		safef(query, sizeof(query), "select name from geneName where id=%d", nameId);
		if (sqlQuickQuery(gConn, query, name, sizeof(name)))
		    outAlias(fAlias, id, name);
		}
	    }
	}

    kgXrefFree(&x);
    }

carefulClose(&fAlias);
carefulClose(&fProt);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 8)
    usage();
txGeneAlias(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
return 0;
}
