/* chainFilter - Filter chain files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainBlock.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainFilter - Filter chain files.  Output goes to standard out.\n"
  "usage:\n"
  "   chainFilter file(s)\n"
  "options:\n"
  "   -q=chr1,chr2 - restrict query side sequence to those named\n"
  "   -notQ=chr1,chr2 - restrict query side sequence to those not named\n"
  "   -t=chr1,chr2 - restrict target side sequence to those named\n"
  "   -notT=chr1,chr2 - restrict target side sequence to those not named\n"
  "   -minScore=N - restrict to those scoring at least N\n"
  "   -maxScore=N - restrict to those scoring less than N\n"
  "   -qStartMin=N - restrict to those with qStart at least N\n"
  "   -qStartMax=N - restrict to those with qStart less than N\n"
  "   -tStartMin=N - restrict to those with tStart at least N\n"
  "   -tStartMax=N - restrict to those with tStart less than N\n"
  "   -strand=?    -restrict strand (to + or -)\n"
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

void chainFilter(int inCount, char *inNames[])
/* chainFilter - Filter chain files. */
{
struct hash *qHash = hashCommaOption("q");
struct hash *tHash = hashCommaOption("t");
struct hash *notQHash = hashCommaOption("notQ");
struct hash *notTHash = hashCommaOption("notT");
int minScore = optionInt("minScore", -BIGNUM);
int maxScore = optionInt("maxScore", BIGNUM);
int qStartMin = optionInt("qStartMin", -BIGNUM);
int qStartMax = optionInt("qStartMax", BIGNUM);
int tStartMin = optionInt("tStartMin", -BIGNUM);
int tStartMax = optionInt("tStartMax", BIGNUM);
char *strand = optionVal("strand", NULL);
int i;

for (i=0; i<inCount; ++i)
    {
    char *fileName = inNames[i];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct chain *chain;
    while ((chain = chainRead(lf)) != NULL)
        {
	boolean writeIt = TRUE;
	if (qHash != NULL && !hashLookup(qHash, chain->qName))
	    writeIt = FALSE;
	if (notQHash != NULL && hashLookup(notQHash, chain->qName))
	    writeIt = FALSE;
	if (tHash != NULL && !hashLookup(tHash, chain->tName))
	    writeIt = FALSE;
	if (notTHash != NULL && hashLookup(notTHash, chain->tName))
	    writeIt = FALSE;
	if (chain->score < minScore || chain->score >= maxScore)
	    writeIt = FALSE;
	if (chain->qStart < qStartMin || chain->qStart >= qStartMax)
	    writeIt = FALSE;
	if (chain->tStart < tStartMin || chain->tStart >= tStartMax)
	    writeIt = FALSE;
	if (strand != NULL && strand[0] != chain->qStrand)
	    writeIt = FALSE;
	if (writeIt)
	    chainWrite(chain, stdout);
	chainFree(&chain);
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
chainFilter(argc-1, argv+1);
return 0;
}
