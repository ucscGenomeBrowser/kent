/* netFilter - Filter out parts of net.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainNet.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netFilter - Filter out parts of net.  What passes\n"
  "filter goes to standard output.  Note a net is a\n"
  "recursive data structure.  If a parent fails to pass\n"
  "the filter, the children are not even considered.\n"
  "usage:\n"
  "   netFilter in.net(s)\n"
  "options:\n"
  "   -q=chr1,chr2 - restrict query side sequence to those named\n"
  "   -notQ=chr1,chr2 - restrict query side sequence to those not named\n"
  "   -t=chr1,chr2 - restrict target side sequence to those named\n"
  "   -notT=chr1,chr2 - restrict target side sequence to those not named\n"
  "   -minScore=N - restrict to those scoring at least N\n"
  "   -maxScore=N - restrict to those scoring less than N\n"
  "   -minGap=N  - restrict to those with gap size (tSize) >= minSize\n"
  "   -syn        - do filtering based on synteny.  \n"
  );
}

struct hash *hashCommaString(char *s)
/* Make hash out of comma separated string. */
{
char *e;
struct hash *hash = newHash(8);
while (s != NULL && s[0] != 0)
    {
    e = strchr(s, ',');
    if (e != NULL)
        *e = 0;
    hashAdd(hash, s, NULL);
    if (e != NULL)
	e += 1;
    s = e;
    }
return hash;
}

struct hash *hashCommaOption(char *opt)
/* Make hash out of optional value. */
{
char *s = optionVal(opt, NULL);
if (s == NULL)
    return NULL;
return hashCommaString(s);
}


struct hash *tHash, *notTHash;	/* Target chromosomes. */
struct hash *qHash, *notQHash;	/* Query chromosomes. */
double minScore, maxScore;	/* Min/max score. */
boolean doSyn;		/* Do synteny based filtering. */
double minSynScore = 200000;  /* Minimum score for block to be syntenic regardless. */
double minSynSize = 20000;    /* Minimum size for syntenic block. */
double minSynAli = 10000;     /* Minimum alignment size. */
double maxFar = 200000;  /* Maximum distance to allow synteny. */
int minGap = 0;		      /* Minimum gap size. */

boolean synFilter(struct cnFill *fill)
/* Filter based on synteny */
{
if (fill->type == NULL)
    errAbort("No type field, please run input net through netSyntenic");
if (fill->score >= minSynScore && fill->tSize >= minSynSize && fill->ali >= minSynAli)
    return TRUE;
if (sameString(fill->type, "top") || sameString(fill->type, "nonSyn"))
    return FALSE;
if (fill->qFar > maxFar)
    return FALSE;
return TRUE;
}

boolean filterOne(struct cnFill *fill)
/* Return TRUE if fill passes filter. */
{
if (qHash != NULL && !hashLookup(qHash, fill->qName))
    return FALSE;
if (notQHash != NULL && hashLookup(notQHash, fill->qName))
    return FALSE;
if (fill->chainId)
    {
    if (fill->score < minScore || fill->score > maxScore)
	return FALSE;
    if (doSyn && !synFilter(fill))
	return FALSE;
    }
else
    {
    if (fill->tSize < minGap)
        return FALSE;
    }
return TRUE;
}

struct  cnFill *cnPrune(struct cnFill *fillList)
/* Get rid of parts of fillList that don't pass filter. 
 * Return what's left. */
{
struct cnFill *newList = NULL, *fill, *next;

for (fill = fillList; fill != NULL; fill = next)
    {
    next = fill->next;
    if (filterOne(fill))
	{
	slAddHead(&newList, fill);
	if (fill->children)
	    fill->children = cnPrune(fill->children);
	}
    else
	{
	cnFillFree(&fill);
	}
    }
slReverse(&newList);
return newList;
}

void writeFiltered(struct chainNet *net, FILE *f)
/* Write out parts of net that pass filter to file. */
{
if ((net->fillList = cnPrune(net->fillList)) != NULL)
    {
    chainNetWrite(net, f);
    }
}

void netFilter(int inCount, char *inFiles[])
/* netFilter - Filter out parts of net.. */
{
FILE *f = stdout;
int i;

tHash = hashCommaOption("t");
notTHash = hashCommaOption("notT");
qHash = hashCommaOption("q");
notQHash = hashCommaOption("notQ");
minScore = optionInt("minScore", -BIGNUM);
maxScore = optionInt("maxScore", BIGNUM);
doSyn = optionExists("syn");
minGap = optionInt("minGap", minGap);

for (i=0; i<inCount; ++i)
    {
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
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 2)
    usage();
netFilter(argc-1, argv+1);
return 0;
}
