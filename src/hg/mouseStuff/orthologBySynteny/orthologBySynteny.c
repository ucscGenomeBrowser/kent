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
  "orthologBySynteny - Find syntenic location for a list of gene predictions on a single chromosome\n"
  "usage:\n"
  "   orthologBySynteny from-db to-db geneTable netTable chrom chainFile\n"
  "options:\n"
  "   -name=field name in gene table (from-db) to get name from [default: name]\n"
  "   -tName=field name in gene table (to-db) to get name from [default: name]\n"
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
void chainPrintHead(struct chain *chain, FILE *f, char *ortholog, char *name, char *type, double score, int level)
{
if (chain->qStrand == '+')
    fprintf(f, "%s\t%s\t%d\t%s\t%d\t+\t%d\t%d\t%s\t%d\t%c\t%d\t%d\t%d\t%1.0f\t%s\n", 
        name, type, level, chain->tName, chain->tSize, chain->tStart, chain->tEnd,
        chain->qName, chain->qSize, chain->qStrand, chain->qStart, chain->qEnd,
        chain->id,score, ortholog);
else
    fprintf(f, "%s\t%s\t%d\t%s\t%d\t+\t%d\t%d\t%s\t%d\t%c\t%d\t%d\t%d\t%1.0f\t%s\n", 
        name, type, level, chain->tName, chain->tSize, chain->tStart, chain->tEnd,
        chain->qName, chain->qSize, chain->qStrand, chain->qSize - chain->qEnd, chain->qSize - chain->qStart,
        chain->id,score, ortholog);
}

char *mapOrtholog(struct sqlConnection *conn,  char *geneTable, char *chrom, int gStart, int gEnd)
    /* finds orthlogous gene name based on syntenic cordinates */
{
struct sqlResult *sr;
char **row;
int i = 0;
char query[512];
static char retName[255];
char *tFieldName = optionVal("tName", "name");

    sprintf(query, "select %s, cdsStart, cdsEnd from %s where chrom = '%s' and txStart <= %d and txEnd >= %d",tFieldName, geneTable,chrom,gEnd, gStart);
    /*printf("%s\n",query);*/
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char *name = (row[0]);
        int geneStart = atoi(row[1]);
        int geneEnd = atoi(row[2]);
        sprintf(retName, "%s",name);
        sqlFreeResult(&sr);
        return(retName);
        }
    sqlFreeResult(&sr);
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
struct sqlConnection *conn2 = hAllocConn2();
struct sqlResult *sr;
char **row;
int i = 0;
char query[512];
char prevName[255] = "x";
char *ortholog = NULL;
struct chain *retSubChain;
struct chain *retChainToFree;
/*int chainArr[MAXCHAINARRAY];*/

sprintf(query, "select chainId, cdsStart, cdsEnd, %s, type, level, qName, qStart, qEnd from %s g, %s_%s n where n.tName = '%s' and n.tStart <= g.cdsEnd and n.tEnd >=  g.cdsStart and chrom = '%s' and type <> 'gap' order by name, cdsStart, score desc",fieldName, geneTable,chrom,netTable, chrom,chrom );
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *chainId = (row[0]);
    int geneStart = sqlUnsigned(row[1]);
    int geneEnd = sqlUnsigned(row[2]);
    int level = sqlUnsigned(row[5]);
    char *name = row[3];
    /*assert(i<MAXCHAINARRARY);*/
    if ((atoi(row[0]) != 0) && strcmp(prevName,name))
        {
        /*printf("name %s prev %s\n",name, prevName);*/
        aChain = hashMustFindVal(chainHash, chainId);
        chainSubsetOnT(aChain, geneStart, geneEnd, &retSubChain,  &retChainToFree);
        if(retSubChain != NULL)
            {
            assert(atoi(row[0]) > 0);
            assert(atoi(row[1]) > 0);
            assert(atoi(row[2]) > 0);
            /* if -track option on then look for an ortholog */
            if (syntenicGene)
                {
                if (retSubChain->qStrand == '+')
                    ortholog = mapOrtholog(conn2, syntenicGene, retSubChain->qName, retSubChain->qStart, retSubChain->qEnd );
                else
                    ortholog = mapOrtholog(conn2, syntenicGene, retSubChain->qName, retSubChain->qSize - retSubChain->qEnd, retSubChain->qSize - retSubChain->qStart );
                }
            chainPrintHead(retSubChain, stdout, ortholog, name, row[4], aChain->score, level);
            }
        else
            {
            /* gap in net/chain skip to next level */
            chainFree(&retChainToFree);
            continue;
            }
        chainFree(&retChainToFree);
        }
    sprintf(prevName , "%s",name);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn2(&conn2);
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
