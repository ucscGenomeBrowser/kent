/* orthologBySynteny - Find syntenic location for a list of locations */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "dnautil.h"
#include "chain.h"

#define MAXCHAINARRAY 5000; /* max genes per chromosome */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orthologBySynteny - Find syntenic location for a list of locations\n"
  "usage:\n"
  "   orthologBySynteny from-database geneTable netTable chrom chainFile\n"
  "options:\n"
  "   -name=field name in gene table to get name from [default: name]"
  );
}

struct hash *allChains(char *fileName)
{
    struct hash *hash = newHash(0);
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct chain *chain;
    char *chainId;

    while ((chain = chainRead(lf)) != NULL)
        {
        AllocVar(chainId);
        sprintf(chainId, "%d",chain->id);
        hashAddUnique(hash, chainId, chain);
        }
    lineFileClose(&lf);
    return hash;
}
void chainPrintHead(struct chain *chain, FILE *f)
{
if (chain->qStrand == '+')
    fprintf(f, "chain %s %d + %d %d %s %d %c %d %d id %d score %1.0f\n", 
        chain->tName, chain->tSize, chain->tStart, chain->tEnd,
        chain->qName, chain->qSize, chain->qStrand, chain->qStart, chain->qEnd,
        chain->id,chain->score);
else
    fprintf(f, "chain %s %d + %d %d %s %d %c %d %d id %d score %1.0f\n", 
        chain->tName, chain->tSize, chain->tStart, chain->tEnd,
        chain->qName, chain->qSize, chain->qStrand, chain->qSize - chain->qEnd, chain->qSize - chain->qStart,
        chain->id,chain->score);
}

void printChain(char *geneTable, char *netTable, char *chrom, struct hash *chainHash)
/* Return subset ofchain at syntenic location. */
{
char *fieldName = optionVal("name", "name");
struct hash *hash = newHash(0);
struct chain *aChain;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int i = 0;
char query[512];
struct chain *retSubChain;
struct chain *retChainToFree;
/*int chainArr[MAXCHAINARRAY];*/

sprintf(query, "select chainId, cdsStart, cdsEnd, %s, type, level, qName, qStart, qEnd from %s g, %s_%s n where n.tName = '%s' and n.tStart <= g.cdsEnd and n.tEnd >=  g.cdsStart and chrom = '%s' and type <> 'gap' ",fieldName, geneTable,chrom,netTable, chrom,chrom );
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *chainId = (row[0]);
    int geneStart = atoi(row[1]);
    int geneEnd = atoi(row[2]);
    /*assert(i<MAXCHAINARRARY);*/
    if (atoi(row[0]) != 0)
        {
        aChain = hashMustFindVal(chainHash, chainId);
        chainSubsetOnT(aChain, geneStart, geneEnd, &retSubChain,  &retChainToFree);
        if(retSubChain != NULL)
            {
            assert(atoi(row[0]) > 0);
            assert(atoi(row[1]) > 0);
            assert(atoi(row[2]) > 0);
            printf("gene %s %s %s ",row[3], row[4], row[5]);
            chainPrintHead(retSubChain, stdout);
            }
        else
            {
            /*printf("***null chain*** ");
            chainPrintHead(aChain, stdout);*/
            }
        chainFree(&retChainToFree);
        }
    else
        {
            printf("***zero chain*** row0 %s %s %s %s %s %s\n",row[0],row[1],row[2],row[3],row[4],row[5]);

        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void orthologBySyntenyChrom(char *geneTable, char *netTable, char *chrom, struct hash *chainHash)
{
    printChain(geneTable, netTable, chrom, chainHash);

 
 
 
 
        /*
    struct lineFile *lf = lineFileOpen(inFiles[i], TRUE);
	struct chainNet *net;
	while ((net = chainNetRead(lf)) != NULL)
	    {
	    boolean writeIt = TRUE;
	    if (tHash != NULL && !hashLookup(tHash, net->name))
		writeIt = FALSE;
	    if (notTHash != NULL && hashLookup(notTHash, net->name))
		writeIt = FALSE;
	    if (writeIt)
		{
		writeFiltered(net, f);
		}
	    chainNetFree(&net);
	    }
    lineFileClose(&lf);
    */
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *chainHash;

optionHash(&argc, argv);
if (argc != 6)
    usage();
hSetDb(argv[1]);
chainHash = allChains(argv[4]);
orthologBySyntenyChrom(argv[2],argv[3], argv[5], chainHash);
return 0;
}
