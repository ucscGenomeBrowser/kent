/* netChainSubset - Create chain file with subset of chains that appear in the net. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"
#include "chainNet.h"

char *type = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netChainSubset - Create chain file with subset of chains that appear in the net\n"
  "usage:\n"
  "   netChainSubset in.net in.chain out.chain\n"
  "options:\n"
  "   -gapOut=gap.tab - Output gap sizes to file\n"
  "   -type=XXX - Restrict output to particular type in net file\n"
  );
}

struct hash *chainReadAll(char *fileName)
/* Read chains into a hash keyed by id. */
{
char nameBuf[16];
struct hash *hash = hashNew(18);
struct chain *chain;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int count = 0;

while ((chain = chainRead(lf)) != NULL)
    {
    sprintf(nameBuf, "%x", chain->id);
    if (hashLookup(hash, nameBuf))
        errAbort("Duplicate chain %d ending line %d of %s", 
		chain->id, lf->lineIx, lf->fileName);
    hashAdd(hash, nameBuf, chain);
    ++count;
    }
lineFileClose(&lf);
fprintf(stderr, "read %d chains in %s\n", count, fileName);
return hash;
}

struct chain *chainLookup(struct hash *hash, int id)
/* Find chain in hash. */
{
char nameBuf[16];
sprintf(nameBuf, "%x", id);
return hashMustFindVal(hash, nameBuf);
}

void gapWrite(struct chain *chain, FILE *f)
/* Write gaps to simple two column file. */
{
struct boxIn *a, *b;
a = chain->blockList;
for (b = a->next; b != NULL; b = b->next)
    {
    fprintf(f, "%d\t%d\n", b->tStart - a->tEnd, b->qStart - a->qEnd);
    a = b;
    }
}

void convertFill(struct cnFill *fill, 
	struct chain *chain, FILE *f, FILE *gapFile)
/* Convert subset of chain as defined by fill to axt. */
{
struct chain *subChain, *chainToFree;

if (type != NULL)
    {
    if (!sameString(type, fill->type))
        return;
    }
chainSubsetOnT(chain, fill->tStart, fill->tStart + fill->tSize, 
	&subChain, &chainToFree);
assert(subChain != NULL);
chainWrite(subChain, f);
if (gapFile != NULL)
    gapWrite(subChain, gapFile);
chainFree(&chainToFree);
}

void rConvert(struct cnFill *fillList, 
	struct hash *chainHash, FILE *f, FILE *gapFile)
/* Recursively output chains in net as axt. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        convertFill(fill, 
		chainLookup(chainHash, fill->chainId), f, gapFile);
    if (fill->children)
        rConvert(fill->children, chainHash, f, gapFile);
    }
}

void netChainSubset(char *netIn, char *chainIn, char *chainOut)
/* netChainSubset - Create chain file with subset of *
 * chains that appear in the net. */
{
struct hash *chainHash;
struct chainNet *net;
struct lineFile *lf = lineFileOpen(netIn, TRUE);
FILE *f = mustOpen(chainOut, "w");
char *gapFileName = optionVal("gapOut", NULL);
FILE *gapFile = NULL;

if (gapFileName)
    gapFile = mustOpen(gapFileName, "w");
chainHash = chainReadAll(chainIn);
while ((net = chainNetRead(lf)) != NULL)
    {
    fprintf(stderr, "Processing %s\n", net->name);
    rConvert(net->fillList, chainHash, f, gapFile);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
type = optionVal("type", type);
if (argc != 4)
    usage();
netChainSubset(argv[1], argv[2], argv[3]);
return 0;
}
