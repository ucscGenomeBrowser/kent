/* letterChain - Make Markov chain of letters in text. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dlist.h"
#include "options.h"


int maxChainSize = 16;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "letterChain - Make Markov chain of letters in text\n"
  "usage:\n"
  "   letterChain in.txt out.chain\n"
  "options:\n"
  "   -size=N Size of chains (default %d)\n"
  , maxChainSize
  );
}

static struct optionSpec options[] = {
   {"size", OPTION_INT},
   {NULL, 0},
};

struct trie 
/* A tree of letters. */
    {
    UBYTE letter;
    UBYTE branchCount;
    int useCount;
    struct trie *singleFollowing;	/* Single following chain. */
    struct trie **multipleFollowing;	/* Multiple following chains. */
    };

void addToTrie(struct trie *trie, struct dlList *ll)
/* Add chain of letters to trie. */
{
struct dlNode *node;
struct trie *next;
trie->useCount += 1;
for (node = ll->head; !dlEnd(node); node = node->next)
    {
    UBYTE *s = node->val;
    UBYTE c = *s;
    if (trie->branchCount)
        {
	if (trie->singleFollowing)
	    {
	    next = trie->singleFollowing;
	    if (next->letter != c)
	        {
		/* Move from single to multiple followers. */
		trie->singleFollowing = NULL;
		AllocArray(trie->multipleFollowing, 256);
		trie->multipleFollowing[next->letter] = next;

		/* Allocate and initialize new follower. */
		AllocVar(next);
		next->letter = c;
		trie->multipleFollowing[c] = next;
		trie->branchCount += 1;
		}
	    }
	else
	    {
	    next = trie->multipleFollowing[c];
	    if (next == NULL)
	        {
		AllocVar(next);
		next->letter = c;
		trie->multipleFollowing[c] = next;
		trie->branchCount += 1;
		}
	    }
	}
    else
        {
	AllocVar(next);
	next->letter = c;
	trie->singleFollowing = next;
	trie->branchCount = 1;
	}
    next->useCount += 1;
    trie = next;
    }
}

static char dumpPrefix[256];

void rDumpTrie(int level, int total, struct trie *trie, FILE *f)
/* Dump out trie */
{
int i;

dumpPrefix[level] = trie->letter;
for (i=1; i<=level; ++i)
    {
    char c = dumpPrefix[i];
    if (c == '\n')
         fprintf(f, "$");
    else
         fprintf(f, "%c", c);
    }
fprintf(f, " %d %1.1f%% %d\n", trie->useCount, 100.0*trie->useCount/total, trie->branchCount);
if (trie->branchCount > 0)
    {
    if (trie->singleFollowing)
        rDumpTrie(level+1, trie->useCount, trie->singleFollowing, f);
    else
        {
	struct trie *next;
	int i;
	for (i=0; i<256; ++i)
	    {
	    if ((next = trie->multipleFollowing[i]) != NULL)
	        {
		rDumpTrie(level+1, trie->useCount, next, f);
		}
	    }
	}
    }
}

void letterChain(char *inFile, char *outFile, int maxSize)
/* letterChain - Make Markov chain of letters in text. */
{
struct dlList *ll = dlListNew();
int llSize = 0;
int c;
FILE *in = mustOpen(inFile, "r");
FILE *out;
struct dlNode *node;
UBYTE *s;
struct trie *trie;

AllocVar(trie);

while ((c = getc(in)) >= 0)
    {
    if (llSize < maxSize)
        {
	s = needMem(1);
	*s = c;
	dlAddValTail(ll, s);
	++llSize;
	if (llSize == maxSize)
	    addToTrie(trie, ll);
	}
    else
        {
	node = dlPopHead(ll);
	s = node->val;
	*s = c;
	dlAddTail(ll, node);
	addToTrie(trie, ll);
	}
    }
if (llSize < maxSize)
    addToTrie(trie, ll);
while ((node = dlPopHead(ll)) != NULL)
    {
    addToTrie(trie, ll);
    freeMem(node->val);
    freeMem(node);
    }
dlListFree(&ll);
carefulClose(&in);

out = mustOpen(outFile, "w");
rDumpTrie(0, trie->useCount, trie, out);

carefulClose(&out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
maxChainSize = optionInt("size", maxChainSize);
letterChain(argv[1], argv[2], maxChainSize);
return 0;
}
