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
  "   orthologBySynteny from-db to-db geneTable netTable chrom chainFile\n"
  "options:\n"
  "   -name=field name in gene table to get name from [default: name]\n"
  "   -track=name of track in to-db - find gene overlapping with syntenic location\n"
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
void chainPrintHead(struct chain *chain, FILE *f, char *ortholog, char *name, char *type, double score)
{
if (chain->qStrand == '+')
    fprintf(f, "chain %s %d + %d %d %s %d %c %d %d id %d score %1.0f %s %s %s\n", 
        chain->tName, chain->tSize, chain->tStart, chain->tEnd,
        chain->qName, chain->qSize, chain->qStrand, chain->qStart, chain->qEnd,
        chain->id,score, name, ortholog, type);
else
    fprintf(f, "chain %s %d + %d %d %s %d %c %d %d id %d score %1.0f %s %s %s\n", 
        chain->tName, chain->tSize, chain->tStart, chain->tEnd,
        chain->qName, chain->qSize, chain->qStrand, chain->qSize - chain->qEnd, chain->qSize - chain->qStart,
        chain->id,score, name, ortholog, type);
}

char *mapOrtholog(char *geneTable, char *chrom, int gStart, int gEnd)
    /* finds orthlogous gene name based on syntenic cordinates */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr;
char **row;
int i = 0;
char query[512];

    conn = hAllocConn2();
    sprintf(query, "select name, cdsStart, cdsEnd from %s where chrom = '%s' and txStart <= %d and txEnd >= %d",geneTable,chrom,gEnd, gStart);
    printf("%s \n",query);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char *name = (row[0]);
        int geneStart = atoi(row[1]);
        int geneEnd = atoi(row[2]);
        /*printf("gene %s %d-%d %d-%d\n",name, gStart, gEnd, geneStart, geneEnd); */
    return(name);
        }
    return(NULL);
}

void printChain(char *geneTable, char *netTable, char *chrom, struct hash *chainHash)
/* Return subset ofchain at syntenic location. */
{
char *fieldName = optionVal("name", "name");
char *syntenicGene = optionVal("track", "knownGene");
struct hash *hash = newHash(0);
struct chain *aChain;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int i = 0;
char query[512];
char prevName[255] = "x";
char *ortholog = NULL;
struct chain *retSubChain;
struct chain *retChainToFree;
/*int chainArr[MAXCHAINARRAY];*/

sprintf(query, "select chainId, cdsStart, cdsEnd, %s, type, level, qName, qStart, qEnd from %s g, %s_%s n where n.tName = '%s' and n.tStart <= g.cdsEnd and n.tEnd >=  g.cdsStart and chrom = '%s' and type <> 'gap' order by cdsStart, score desc",fieldName, geneTable,chrom,netTable, chrom,chrom );
    printf("%s \n",query);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *chainId = (row[0]);
    int geneStart = sqlUnsigned(row[1]);
    int geneEnd = sqlUnsigned(row[2]);
    char *name = row[3];
    /*assert(i<MAXCHAINARRARY);*/
    if ((atoi(row[0]) != 0) && strcmp(prevName,name))
        {
        aChain = hashMustFindVal(chainHash, chainId);
        chainSubsetOnT(aChain, geneStart, geneEnd, &retSubChain,  &retChainToFree);
        if(retSubChain != NULL)
            {
            assert(atoi(row[0]) > 0);
            assert(atoi(row[1]) > 0);
            assert(atoi(row[2]) > 0);
               printf("gene %s %d %d \n",retSubChain->qName, retSubChain->qStart, retSubChain->qEnd);
            if (syntenicGene)
                ortholog = mapOrtholog(syntenicGene, retSubChain->qName, retSubChain->qStart, retSubChain->qEnd);
            chainPrintHead(retSubChain, stdout, ortholog, name, row[4], aChain->score);
            }
        else
            {
            /*printf("***null chain*** ");
            chainPrintHead(aChain, stdout);*/
            }
        chainFree(&retChainToFree);
        }
    sprintf(prevName , "%s",name);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void orthologBySyntenyChrom(char *geneTable, char *netTable, char *chrom, struct hash *chainHash)
{
    printChain(geneTable, netTable, chrom, chainHash);

}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *chainHash;

optionHash(&argc, argv);
if (argc != 7)
    usage();
hSetDb(argv[1]);
hSetDb2(argv[2]);
chainHash = allChains(argv[6]);
orthologBySyntenyChrom(argv[3], argv[4], argv[5], chainHash);
return 0;
}
