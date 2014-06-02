/* netFilter - Filter out parts of net.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
  "   -minAli=N - restrict to those with at least given bases aligning\n"
  "   -maxAli=N - restrict to those with at most given bases aligning\n"
  "   -minSizeT=N - restrict to those at least this big on target\n"
  "   -minSizeQ=N - restrict to those at least this big on query\n"
  "   -qStartMin=N - restrict to those with qStart at least N\n"
  "   -qStartMax=N - restrict to those with qStart less than N\n"
  "   -qEndMin=N - restrict to those with qEnd at least N\n"
  "   -qEndMax=N - restrict to those with qEnd less than N\n"
  "   -tStartMin=N - restrict to those with tStart at least N\n"
  "   -tStartMax=N - restrict to those with tStart less than N\n"
  "   -tEndMin=N - restrict to those with tEnd at least N\n"
  "   -tEndMax=N - restrict to those with tEnd less than N\n"
  "   -qOverlapStart=N - restrict to those where the query overlaps a region starting here\n"
  "   -qOverlapEnd=N - restrict to those where the query overlaps a region ending here\n"
  "   -tOverlapStart=N - restrict to those where the target overlaps a region starting here\n"
  "   -tOverlapEnd=N - restrict to those where the target overlaps a region ending here\n"
  "   -type=XXX - restrict to given type, maybe repeated to allow several types\n"

  "   -syn        - do filtering based on synteny (tuned for human/mouse).  \n"
  "   -minTopScore=N - Minimum score for top level alignments. default 300000\n"
  "   -minSynScore=N - Min syntenic block score (def=200,000). \n"
  "                      Default covers 27,000 bases including 9,000 \n"
  "                      aligning--a very stringent requirement. \n"
  "   -minSynSize=N - Min syntenic block size (def=20,000). -\n"
  "   -minSynAli=N  - Min syntenic alignment size(def=10,000). -\n"
  "   -maxFar=N     - Max distance to allow synteny (def=200,000). \n"
  "   -nonsyn     - do inverse filtering based on synteny (tuned for human/mouse).  \n"
  "   -chimpSyn   - do filtering based on synteny (tuned for human/chimp).  \n"
  "   -fill - Only pass fills, not gaps. Only useful with -line.\n"
  "   -gap  - Only pass gaps, not fills. Only useful with -line.\n"
  "   -line - Do this a line at a time, not recursing\n"
  "   -noRandom      - suppress chains involving 'random' chromosomes\n"
  "   -noHap         - suppress chains involving chromosome names inc '_hap|_alt'\n"
  );
}

struct optionSpec options[] = {
   {"q", OPTION_STRING},
   {"notQ", OPTION_STRING},
   {"t", OPTION_STRING},
   {"notT", OPTION_STRING},
   {"minScore", OPTION_FLOAT},
   {"maxScore", OPTION_FLOAT},
   {"minGap", OPTION_INT},
   {"minAli", OPTION_INT},
   {"maxAli", OPTION_INT},
   {"minSizeT", OPTION_INT},
   {"minSizeQ", OPTION_INT},
   {"qStartMin", OPTION_INT},
   {"qStartMax", OPTION_INT},
   {"qEndMin", OPTION_INT},
   {"qEndMax", OPTION_INT},
   {"tStartMin", OPTION_INT},
   {"tStartMax", OPTION_INT},
   {"tEndMin", OPTION_INT},
   {"tEndMax", OPTION_INT},
   {"qOverlapStart", OPTION_INT},
   {"qOverlapEnd", OPTION_INT},
   {"tOverlapStart", OPTION_INT},
   {"tOverlapEnd", OPTION_INT},
   {"syn", OPTION_BOOLEAN},
   {"minTopScore", OPTION_FLOAT},
   {"minSynScore", OPTION_FLOAT},
   {"minSynSize", OPTION_FLOAT},
   {"minSynAli", OPTION_FLOAT},
   {"maxFar", OPTION_FLOAT},
   {"chimpSyn", OPTION_BOOLEAN},
   {"nonsyn", OPTION_BOOLEAN},
   {"type", OPTION_STRING|OPTION_MULTI},
   {"fill", OPTION_BOOLEAN},
   {"gap", OPTION_BOOLEAN},
   {"line", OPTION_BOOLEAN},
   {"noRandom", OPTION_BOOLEAN},
   {"noHap", OPTION_BOOLEAN},
   {NULL, 0},
};

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
int qStartMin;  /* query range */
int qStartMax;
int qEndMin;
int qEndMax;
int tStartMin;  /* target range */
int tStartMax;
int tEndMin;
int tEndMax;
int qOverlapStart; /* query overlap region */
int qOverlapEnd;
int tOverlapStart; /* query overlap region */
int tOverlapEnd;
boolean doSyn;		/* Do synteny based filtering. */
boolean doChimpSyn;	/* Do chimp synteny based filtering. */
boolean doNonSyn;		/* Do synteny based filtering. */
double minTopScore = 300000;  /* Minimum score for top level alignments. */
/* changed to 300000 from 100000 to cut down on pseudogenes - Robert*/
double minSynScore = 200000;  /* Minimum score for block to be syntenic 
                               * regardless.  On average in the human/mouse
			       * net a score of 200,000 will cover 27000 
			       * bases including 9000 aligning bases - more
			       * than all but the heartiest of processed
			       * pseudogenes. */
double minSynSize = 20000;    /* Minimum size for syntenic block. */
double minSynAli = 10000;     /* Minimum alignment size. */
double maxFar = 200000;  /* Maximum distance to allow synteny. */
int minGap = 0;		      /* Minimum gap size. */
int minAli = 0;			/* Minimum ali size. */
int maxAli = 0;			/* Maximum ali size. */
int minSizeT = 0;		/* Minimum target size. */
int minSizeQ = 0;		/* Minimum query size. */
boolean fillOnly = FALSE;	/* Only pass fills? */
boolean gapOnly = FALSE;	/* Only pass gaps? */
struct slName *types = NULL;	/* Only pass given types */
boolean noRandom = FALSE;	/* Only pass non-random chromosomes. */
boolean noHap = FALSE;		/* Only pass non-hap chromosomes. */

boolean synFilter(struct cnFill *fill)
/* Filter based on synteny - tuned for human/mouse */
{
if (fill->type == NULL)
    errAbort("No type field, please run input net through netSyntenic");
if (fill->score >= minSynScore && fill->tSize >= minSynSize && fill->ali >= minSynAli)
    return TRUE;
if (sameString(fill->type, "top"))
    return fill->score >= minTopScore;
if (sameString(fill->type, "nonSyn"))
    return FALSE;
if (fill->qFar > maxFar)
    return FALSE;
/* For all others, assume syntenic.  This keeps tandem dupes, small inversion,
 * and translocations. */
return TRUE;
}

boolean chimpSynFilter(struct cnFill *fill)
/* Filter based on synteny  - tuned for human/chimp. */
{
if (fill->type == NULL)
    errAbort("No type field, please run input net through netSyntenic");
if (fill->ali >= 50)
    return TRUE;
if (fill->ali < 15)
    return FALSE;
if (sameString(fill->type, "top"))
    return FALSE;
if (sameString(fill->type, "nonSyn"))
    return FALSE;
if (fill->qFar > 1000)
    return FALSE;
/* For all others, assume syntenic.  This keeps tandem dupes, small inversion,
 * and translocations. */
return TRUE;
}


boolean filterOne(struct cnFill *fill)
/* Return TRUE if fill passes filter. */
{
if (qHash != NULL && !hashLookup(qHash, fill->qName))
    return FALSE;
if (notQHash != NULL && hashLookup(notQHash, fill->qName))
    return FALSE;
if (types != NULL)
    {
    struct slName *t;
    if (fill->type == NULL)
        return FALSE;
    for (t = types; t != NULL; t = t->next)
        {
        if (sameString(t->name, fill->type))
            break;  /* found it */
        }
    if (t == NULL)
        return FALSE;  /* not found in list of types */
    }
if (fill->chainId)
    {
    if (gapOnly)
        return FALSE;
    if (fill->qSize < minSizeQ)
	return FALSE;
    if (fill->tSize < minSizeT)
	return FALSE;
    if (fill->score < minScore || fill->score > maxScore)
	return FALSE;
    if (fill->ali < minAli)
        return FALSE;
    if ((fill->ali > maxAli) && (maxAli != 0))
        return FALSE;
    if (noRandom)
        {
	if (endsWith(fill->qName, "_random") 
	    || startsWith("chrUn", fill->qName)
	    || sameWord("chrNA", fill->qName) /* danRer */
	    || sameWord("chrU", fill->qName)) /* dm */
	    return FALSE;
	}
    if (noHap && haplotype(fill->qName))
	return FALSE;
    if (fill->qStart < qStartMin || fill->qStart >= qStartMax)
        return FALSE;
    if ((fill->qStart+fill->qSize) < qEndMin || (fill->qStart+fill->qSize) >= qEndMax)
        return FALSE;
    if (fill->tStart < tStartMin || fill->tStart >= tStartMax)
        return FALSE;
    if ((fill->tStart+fill->tSize) < tEndMin || (fill->tStart+fill->tSize) >= tEndMax)
        return FALSE;
    if ((fill->qStart+fill->qSize) < qOverlapStart || fill->qStart >= qOverlapEnd)
        return FALSE;
    if ((fill->tStart+fill->tSize) < tOverlapStart || fill->tStart >= tOverlapEnd)
        return FALSE;
    if (doSyn && !synFilter(fill))
	return FALSE;
    if (doNonSyn && synFilter(fill))
	return FALSE;
    if (doChimpSyn && !chimpSynFilter(fill))
        return FALSE;
    }
else
    {
    if (fillOnly)
        return FALSE;
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

void netLineFilter(struct lineFile *lf, FILE *f)
/* Do filter one line at a time. */
{
struct hash *nameHash = newHash(0);
char *line, *l;
int d;

while (lineFileNext(lf, &line, NULL))
    {
    d = countLeadingChars(line, ' ');
    l = line + d;
    if (startsWith("fill", l) || startsWith("gap", l))
        {
	struct cnFill *fill = cnFillFromLine(nameHash, lf, l);
	if (filterOne(fill))
	    cnFillWrite(fill, f, d);
	cnFillFree(&fill);
	}
    else
        {
	fprintf(f, "%s\n", line);
	}
    }

hashFree(&nameHash);
}

void netFilter(int inCount, char *inFiles[])
/* netFilter - Filter out parts of net.. */
{
FILE *f = stdout;
int i;
boolean doLine = optionExists("line");

tHash = hashCommaOption("t");
notTHash = hashCommaOption("notT");
qHash = hashCommaOption("q");
notQHash = hashCommaOption("notQ");
minScore = optionInt("minScore", -BIGNUM);
maxScore = optionFloat("maxScore", 9e99);
qStartMin = optionInt("qStartMin", -BIGNUM);
qStartMax = optionInt("qStartMax", BIGNUM);
qEndMin = optionInt("qEndMin", -BIGNUM);
qEndMax = optionInt("qEndMax", BIGNUM);
tStartMin = optionInt("tStartMin", -BIGNUM);
tStartMax = optionInt("tStartMax", BIGNUM);
tEndMin = optionInt("tEndMin", -BIGNUM);
tEndMax = optionInt("tEndMax", BIGNUM);
qOverlapStart = optionInt("qOverlapStart", -BIGNUM);
qOverlapEnd = optionInt("qOverlapEnd", BIGNUM);
tOverlapStart = optionInt("tOverlapStart", -BIGNUM);
tOverlapEnd = optionInt("tOverlapEnd", BIGNUM);
doSyn = optionExists("syn");
minTopScore = optionFloat("minTopScore", minTopScore);
minSynScore = optionFloat("minSynScore", minSynScore);
minSynSize = optionFloat("minSynSize", minSynSize);
minSynAli = optionFloat("minSynAli", minSynAli);
maxFar = optionFloat("maxFar", maxFar);
doChimpSyn = optionExists("chimpSyn");
doNonSyn = optionExists("nonsyn");
minGap = optionInt("minGap", minGap);
minAli = optionInt("minAli", minAli);
maxAli = optionInt("maxAli", maxAli);
minSizeT = optionInt("minSizeT", minSizeT);
minSizeQ = optionInt("minSizeQ", minSizeQ);
fillOnly = optionExists("fill");
gapOnly = optionExists("gap");
types = optionMultiVal("type", types);
noRandom = optionExists("noRandom");
noHap = optionExists("noHap");

for (i=0; i<inCount; ++i)
    {
    struct lineFile *lf = lineFileOpen(inFiles[i], TRUE);
    if (doLine)
        {
	netLineFilter(lf, f);
	}
    else
	{
	struct chainNet *net;
	while ((net = chainNetRead(lf)) != NULL)
	    {
	    boolean writeIt = TRUE;
	    if (tHash != NULL && !hashLookup(tHash, net->name))
		writeIt = FALSE;
	    if (notTHash != NULL && hashLookup(notTHash, net->name))
		writeIt = FALSE;
	    if (noRandom && (endsWith(net->name, "_random")
			     || startsWith("chrUn", net->name)
			     || sameWord("chrNA", net->name) /* danRer */
			     || sameWord("chrU", net->name)))  /* dm */
	        writeIt = FALSE;
	    if (noHap && haplotype(net->name))
		writeIt = FALSE;
	    if (writeIt)
		{
		writeFiltered(net, f);
		}
	    chainNetFree(&net);
	    }
	}
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
netFilter(argc-1, argv+1);
return 0;
}
