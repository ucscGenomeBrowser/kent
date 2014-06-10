/* rnaPhylo - rna phylogenetic analysis. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "phyloTree.h"
#include "rnautil.h"
#include "bits.h"
#include "dystring.h"


#define ADDSEQ 0
// issues: 
// - GU -> GC pair is only 1 change, whereas GC->CG or others are two changes
// - how to count the evolutionary distance
// - how to count the 
#define SS_KEY "#=GC"
#define NUM_PAIRS 6
#define REF_ID "refId"

#ifndef MJP
#define MJP(v) verbose((v),"%s[%3d]: ", __func__, __LINE__)
#endif

int dna[256];
int pairId[4][4];
boolean printMaxVariants = FALSE;
boolean hideMissing = FALSE;
boolean hideNodes = FALSE;

struct stats {
    Bits *variants;
    int events;
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rnaPhylo - rna phylogenetic analysis\n"
  "usage:\n"
  "   rnaPhylo refSpp tree.nh align.mfa [align2.mfa [align3.mfa ...]]\n"
  "options:\n"
  "   -genomes=file.txt   : File with db and genome name separated by tab for human-readable display\n"
  "   -maxVariants        : Just output the filename and maximum number of variants found\n"
  "   -hideMissing        : Hide (dont show) species where sequence is missing\n"
  "   -hideNodes          : Hide ancestral nodes from output\n"
  );
}

static struct optionSpec options[] = {
    {"genomes", OPTION_STRING},
    {"maxVariants", OPTION_BOOLEAN},
    {"hideMissing", OPTION_BOOLEAN},
    {"hideNodes", OPTION_BOOLEAN},
    {NULL, 0},
};

void init()
{
int i,j;
for (i=0 ; i<256 ; ++i)
    dna[i] = -1;
dna['A'] = dna['a'] = 0;
dna['C'] = dna['c'] = 1;
dna['G'] = dna['g'] = 2;
dna['T'] = dna['t'] = 3;
dna['U'] = dna['u'] = 3;
for (i=0 ; i<4 ; ++i)
    for (j=0 ; j<4 ; ++j)
	pairId[i][j] = -1;
pairId[2][3] = 0; // GU
pairId[3][2] = 1; // UG
pairId[0][3] = 2; // AU
pairId[3][0] = 3; // UA
pairId[1][2] = 4; // CG
pairId[2][1] = 5; // GC
}

boolean faReadNext(struct lineFile *lf, char **retSeq, int *retSize, char **retName)
/* Read in DNA or Peptide FA record in mixed case.   Allow any upper or lower case
 * letter, or the dash character in. */
{
int seqLen = 0, len;
static char name[512];
static char seq[10000];
int lineSize;
char *line;

/* Read first line, make sure it starts with '>', and read first word
 * as name of sequence. */
name[0] = 0;
seq[0] = 0;
if (!lineFileNext(lf, &line, &lineSize))
    {
    *retSeq = NULL;
    *retSize = 0;
    return FALSE;
    }
if (line[0] == '>')
    {
    line = firstWordInLine(skipLeadingSpaces(line+1));
    if (line == NULL)
        errAbort("Expecting sequence name after '>' line %d of %s", lf->lineIx, lf->fileName);
    strncpy(name, line, sizeof(name));
    name[sizeof(name)-1] = '\0'; /* Just to make sure name is NULL terminated. */
    }
else
    {
    errAbort("Expecting '>' line %d of %s", lf->lineIx, lf->fileName);
    }
/* Read until next '>' */
for (;;)
    {
    if (!lineFileNext(lf, &line, &lineSize))
        break;
    if (line[0] == '>')
        {
	lineFileReuse(lf);
	break;
	}
    len = strlen(line);
    if (seqLen + len >= sizeof(seq))
	errAbort("sequence too long\n");
    MJP(2);verbose(2,"# adding %5d [%s]\n", len, line);
    safecpy(seq+seqLen, sizeof(seq), line);
    MJP(2);verbose(2,"# seq=[%s]\n", seq);
    seqLen += len;
    }
seq[seqLen] = 0;
*retSeq = seq;
*retSize = seqLen;
*retName = name;
return TRUE;
}

struct hash *readMfa(char *alignfile, char *refSpp)
{
char *name, *seq, *dot;
int size;
struct hash *h = NULL;
struct lineFile *lf = lineFileOpen(alignfile, TRUE);
boolean first = TRUE;
while (faReadNext(lf, &seq, &size, &name))
    {
    if ( (dot = strchr(name, '.')) )
	*dot = '\0';
    MJP(2);verbose(2,"hashAdd(h,%s,%s)\n", name, seq);
    if (first)
	{
	h = newHash(0);
	hashAdd(h, refSpp, cloneString(seq));
	hashAdd(h, REF_ID, cloneString(name));
	first = FALSE;
	}
    else
	hashAdd(h, name, cloneString(seq));
    }
lineFileClose(&lf);
return h;
}

int findMaxDepth2(struct phyloTree *tree, int depth, int maxDepth)
// returns the max depth of the tree
{
if (tree->numEdges == 0)
    return depth > maxDepth ? depth : maxDepth;
else if (tree->numEdges == 2)
    {
    maxDepth = findMaxDepth2(tree->edges[0], depth+1, maxDepth);
    return findMaxDepth2(tree->edges[1], depth+1, maxDepth);
    }
else
    {
    errAbort("Found %d edges, expecting 0 (leaf) or 2 (node)\n", tree->numEdges);
    return maxDepth; // stop compiler warning
    }
}

int findMaxDepth(struct phyloTree *tree)
{
    return findMaxDepth2(tree, 0, 0);
}

void treeSpaces(FILE *f, char branch, char prefix, int depth, int maxDepth)
{
  spaceOut(f, depth);
  putc(branch, f);
  spaceOut(f, maxDepth-depth);
  putc(prefix, f);
}

char *nodeName(struct phyloTree *node, struct hash *genomes)
{
char *name = "";
if (node && node->ident && node->ident->name)
    name = (char *)hashOptionalVal(genomes, node->ident->name, node->ident->name);
return name;
}

char *ssEncode(char *seq, char *ss, int *pair, char *ref)
{
char *s = cloneString(seq);
int i = 0, p;
while (s[i])
    {
    if (pair[i] < 0) // unpaired
	{
	if (ref && dna[(int)seq[i]] >= 0 && dna[(int)ref[i]] == dna[(int)seq[i]])
	    s[i] = '.';
	}
    else // paird
	{
	if (dna[(int)seq[i]] >= 0 && dna[(int)seq[pair[i]]] >= 0) // both are bases, not '-'
	    {
	    p = pairId[dna[(int)seq[i]]][dna[(int)seq[pair[i]]]];
	    s[i] = (p >= 0 ? '0' + p : (i < pair[i] ? '{' : '}'));
	    }
	else 
	    {
	    if (dna[(int)seq[i]] < 0) // this one is '-'
		s[i] = (i < pair[i] ? '<' : '>');
	    else // its a base
		s[i] = (i < pair[i] ? '[' : ']');
	    }
	//MJP(2);verbose(2,"(i=%d,pair=%d) p=%d %c %c\n", i, pair[i], p, seq[i], seq[pair[i]]);
	}
    ++i;
    }
return s;
}

void printTreeDepth(struct hash *aln, struct phyloTree *tree, char *refSpp, struct hash *genomes, char *ss, int *pair, char branch, int depth, int maxDepth, FILE *f)
// returns the max depth of the tree
{
char *seq;
char *ref;
char *name;
char *ssEnc = NULL;
//int ii;
MJP(2);verbose(2,"edges=%d tree=%p genomes=%p\n", tree->numEdges, tree, genomes);
name = nodeName(tree, genomes);
MJP(2);verbose(2,"nodeName=[%s]\n", name);
if (tree->numEdges == 0)
    {
    ref = (char *)hashMustFindVal(aln, refSpp);
    if ( sameOk(refSpp, tree->ident->name) )
	{
	treeSpaces(f, branch, ' ', depth, maxDepth);
	//printf("%-15s [%s]\n", name, ref);
	ssEnc = ssEncode(ref, ss, pair, ref);
	printf("%-15s  %s\n", name, ssEnc);
	}
    else if ( (seq = (char *)hashFindVal(aln, tree->ident->name)) && ref )
	{
	treeSpaces(f, branch, ' ', depth, maxDepth);
	ssEnc = ssEncode(seq, ss, pair, ref);
	printf("%-15s  %s\n", name, ssEnc);
	if (ADDSEQ)
	    {
	    treeSpaces(f, branch, ' ', depth, maxDepth);
	    printf("%-15s  ", name);
	    int i;
	    for (i = 0 ; seq[i] ; ++i)
		printf("%c", (dna[(int)ref[i]] >= 0 && dna[(int)seq[i]] == dna[(int)ref[i]]) ? '.' : seq[i]);
	    printf("\n");
	    }
	}
    else if (!hideMissing)
	{
	treeSpaces(f, branch, ' ', depth, maxDepth);
	printf("%-15s |\n", name);
	}
    }
else if (tree->numEdges == 2)
    {
    printTreeDepth(aln, tree->edges[0], refSpp, genomes, ss, pair, '/',  depth+1, maxDepth, f);
    if (!hideNodes)
	{
	treeSpaces(f, '+', '*', depth, maxDepth);
	printf("%-15s  %s\n", name, "");
	}
    printTreeDepth(aln, tree->edges[1], refSpp, genomes, ss, pair, '\\', depth+1, maxDepth, f);
    }
else
    {
    errAbort("Found %d edges, expecting 0 (leaf) or 2 (node)\n", tree->numEdges);
    }
freez(&ssEnc);
}

void printTree(struct hash *aln, struct phyloTree *tree, char *refSpp, struct hash *genomes, char *ss, int *pair)
{
if (aln && tree)
    printTreeDepth(aln, tree, refSpp, genomes, ss, pair, '+', 0, findMaxDepth(tree), stdout);
}

struct stats *newStats()
{
struct stats *ret;
AllocVar(ret);
ret->variants = bitAlloc(NUM_PAIRS);
return ret;
}

void freeStats(struct stats **pStats)
{
struct stats *s = *pStats;
bitFree(&(s->variants));
freez(pStats);
}

struct stats *evoColumnStats(int a, int b, struct hash *aln, struct phyloTree *tree)
{
char *seq, A, B;
struct stats *ret, *l, *r;
int needExplaining, unexplained;
if (tree->numEdges == 0) // leaf
    {
    ret = newStats();
    seq = (char *)hashFindVal(aln, tree->ident->name);
    if (seq) // species at this leaf is in alignment
	{
	A = dna[(int)seq[a]]; B = dna[(int)seq[b]];
	if (A >= 0 && B >= 0 && pairId[(int)A][(int)B] >= 0) // basepaired sequences
	    bitSetOne(ret->variants, pairId[(int)A][(int)B]);
	}
    else 
	{
	// species is not in alignment, just ignore
	}
    return ret;
    }
else if (tree->numEdges == 2)// node
    {
    l = evoColumnStats(a, b, aln, tree->edges[0]);
    r = evoColumnStats(a, b, aln, tree->edges[1]);
    bitOr(l->variants, r->variants, NUM_PAIRS); // accumulate the different pairs we have seen
    // covariation events which need explaining are the total different pairs we have seen
    needExplaining = bitCountRange(l->variants, 0, NUM_PAIRS);
    // unexplained events are those needing explanation less the events in left & right sub-trees
    unexplained = needExplaining - (l->events + r->events);
    if (unexplained < 0)
	unexplained = 0;
    // total events in this tree are the events in the left & right subtrees, plus the unexplained ones
    l->events = l->events + r->events + unexplained;
    freeStats(&r);
    return l;
    }
errAbort("Found %d edges, expecting 0 (leaf) or 2 (node)\n", tree->numEdges);
return NULL;
}


void mkPairStats(int *pairList, int size, int *stats, int statsSize, char *tens, char *ones)
{
/* Make a symbol string indicating pairing partner 
 * Size gives size of input pairList and output tens and ones
 * There is one stats entry for every pair entry > -1 */
int i;
int index;
MJP(2);verbose(2,"size=%d statsSize=%d\n", size, statsSize);
for (i = 0, index = 0; i < size; i++)
    {
    ones[i] = tens[i] = ' ';
    MJP(2);verbose(2,"pairList(i=%d)=%d stats[index=%d]=%d\n", i, pairList[i], index, stats[index]);
    if (pairList[i] >= 0) // then there is stats for this
	{
	if (pairList[i] < i) // then we are on the other side of the pair
	    {
            --index;
	    if (index<0)
		errAbort("not enough stats (%d < 0)\n", index);
	    }
	tens[i] = ((stats[index] < 100) ? ((stats[index] / 10) > 0 ? '0' + (stats[index] / 10) : ' ') : 9);
	ones[i] = '0' + ((stats[index] < 100) ? stats[index] % 10 : 9);
	if (pairList[i] > i) // we are on the leading strand
	    {
            ++index;
	    if (index>statsSize)
		errAbort("not enough stats (%d >= %d)\n", index, statsSize);
	    }
	}
    }
}

int countPairs(int *pairs, int size)
{
int i, n = 0;
for (i = 0 ; i<size ; ++i)
    if (pairs[i] >= 0 && i < pairs[i])
	++n;
return n;
}

struct hash *readGenomes(char *file)
{
struct hash *h = NULL;
char *line, *db;
int lineSize;
struct lineFile *lf = lineFileOpen(file, TRUE);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (!h)
	h = newHash(0);
    db = nextWord(&line);
    if (strlen(line) > 15)
	line[15] = 0;
    MJP(2);verbose(2,"readGenomes(%s,%s)\n", db, line);
    hashAdd(h, db, cloneString(line));
    }
return h;
}

void rnaPhylo(char *refSpp, char *treefile, char *genomesfile, char *alignfiles[], int numFiles)
/* rnaPhylo - rna phylogenetic analysis. */
{
char *ss, *tens, *ones;
int *pair, pairCount, ssLen, a, b, s0;
int i, *events, *variants;
char *pairSymbols;
int maxDepth, maxVariants, sumVariants, sumSqVariants;

MJP(2);verbose(2,"phyloOpenTree(%s)\n", treefile);
struct phyloTree *t = phyloOpenTree(treefile);
MJP(2);verbose(2,"findMaxDepth()\n");
maxDepth = findMaxDepth(t);
MJP(2);verbose(2,"d=%d; readGenomes(%s)\n", maxDepth, genomesfile);
struct hash *genomes = readGenomes(genomesfile);
MJP(2);verbose(2,"mfa loop(%d files)\n", numFiles);
for (i = 0 ; i < numFiles ; ++i)
    {
    struct hash *aln = readMfa(alignfiles[i], refSpp);
    if (!aln)
	{
	warn("%s: no alignment found", alignfiles[i]);
	continue;
	}
    ss = (char *)hashFindVal(aln, SS_KEY);
    ssLen = strlen(ss);
    fold2pairingList(ss, ssLen, &pair);
    pairCount = countPairs(pair, ssLen);
    AllocArray(events, pairCount);
    AllocArray(variants, pairCount);
    s0 = maxVariants = sumVariants = sumSqVariants = 0;
    for (a = 0; a < ssLen ; ++a)
	{
	b = pair[a];
	if (b < 0 || b < a)
	    continue;
	struct stats *s = evoColumnStats(a, b, aln, t);
	events[s0] = s->events;
	variants[s0] = bitCountRange(s->variants, 0, NUM_PAIRS);
	maxVariants = max(maxVariants, variants[s0]);
	sumVariants += variants[s0];
	sumSqVariants += variants[s0]*variants[s0];
	++s0;
	freeStats(&s);
	}
    if (pairCount != s0)
	errAbort("Pair count != s0 (%d != %d)\n", pairCount, s0);
    if (printMaxVariants)
	{
	printf("%s,pairs,%d,maxVariants,%d,sumVariants,%d,sumSqVariants,%d,avgVariants,%.2f\n", alignfiles[i], s0, maxVariants, sumVariants, sumSqVariants, (float)sumVariants/s0);
	}
    else
	{
	AllocArray(pairSymbols,ssLen+1);
	mkPairPartnerSymbols(pair, pairSymbols, ssLen);
	printf("%s: Structure analysis for sequence [%s] in genome [%s]\n", alignfiles[i], (char *)hashMustFindVal(aln, REF_ID), refSpp);
	printf("legend: {}=base paired with non-canonical base; <>=gap paired with base; []=base paired with gap\n");
	printf("      : 0=GU 1=UG 2=AU 3=UA 4=CG 5=GC; '-'=gap; '.'=unpaired, same as ref; [ACGT]=unpaired, different to ref\n");
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "Reference seq.", (char *)hashMustFindVal(aln, refSpp));
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "Structure", ss);
	printTree(aln, t, refSpp, genomes, ss, pair);
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "Structure", ss);
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "Pairs", pairSymbols);
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "Reference seq.", (char *)hashMustFindVal(aln, refSpp));
	AllocArray(tens, ssLen+1);
	AllocArray(ones, ssLen+1);
	mkPairStats(pair, ssLen, variants, pairCount, tens, ones);
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "Variants", tens);
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "(cont.)", ones);
	freez(&tens);
	freez(&ones);
	AllocArray(tens, ssLen+1);
	AllocArray(ones, ssLen+1);
	mkPairStats(pair, ssLen, events, pairCount, tens, ones);
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "Events", tens);
	spaceOut(stdout, maxDepth+1); 
	printf("%15s  [%s]\n", "(cont.)", ones);
	freez(&tens);
	freez(&ones);
	freez(&pairSymbols);
	}
    struct hashEl *helList = hashElListHash(aln);
    hashElFreeList(&helList);
    freeHash(&aln);
    freez(&pair);
    freez(&events);
    freez(&variants);
    }
MJP(2);verbose(2,"done\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
init();
--argc;
++argv;
printMaxVariants = optionExists("maxVariants");
hideMissing = optionExists("hideMissing");
hideNodes = optionExists("hideNodes");
if (argc < 3)
    usage();
rnaPhylo(argv[0], argv[1], optionVal("genomes", NULL), &argv[2], argc-2);
return 0;
}
