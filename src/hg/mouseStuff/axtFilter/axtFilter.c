/* axtFilter - Filter axt files. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtFilter - Filter axt files.  Output goes to standard out.\n"
  "usage:\n"
  "   axtFilter file(s)\n"
  "options:\n"
  "   -q=chr1,chr2 - restrict query side sequence to those named\n"
  "   -notQ=chr1,chr2 - restrict query side sequence to those not named\n"
  "   -notQ_random - restrict query side sequence, no *_random to be used\n"
  "   -notQ_hap - restrict query side sequence, no *_hap|*_alt to be used\n"
  "   -t=chr1,chr2 - restrict target side sequence to those named\n"
  "   -notT=chr1,chr2 - restrict target side sequence to those not named\n"
  "   -notT_random - restrict target side sequence, no *_random to be used\n"
  "   -notT_anyRandom - restrict target side sequence, no *_random/chrUn/NA/U\n"
  "   -notT_hap - restrict target side sequence, no *_hap|*_hap to be used\n"
  "   -minScore=N - restrict to those scoring at least N\n"
  "   -maxScore=N - restrict to those scoring less than N\n"
  "   -qStartMin=N - restrict to those with qStart at least N\n"
  "   -qStartMax=N - restrict to those with qStart less than N\n"
  "   -tStartMin=N - restrict to those with tStart at least N\n"
  "   -tStartMax=N - restrict to those with tStart less than N\n"
  "   -qEndMin=N - restrict to those with qEnd at least N\n"
  "   -qEndMax=N - restrict to those with qEnd less than N\n"
  "   -tEndMin=N - restrict to those with tEnd at least N\n"
  "   -tEndMax=N - restrict to those with tEnd less than N\n"
  "   -strand=?    -restrict strand (to + or -)\n"
  "   -qStartsWith=?  -restrict query side sequence to those starting with\n"
  "   -tStartsWith=?  -restrict target side sequence to those starting with\n"
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

void axtFilter(int inCount, char *inNames[])
/* axtFilter - Filter axt files. */
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
int qEndMin = optionInt("qEndMin", -BIGNUM);
int qEndMax = optionInt("qEndMax", BIGNUM);
int tEndMin = optionInt("tEndMin", -BIGNUM);
int tEndMax = optionInt("tEndMax", BIGNUM);
char *strand = optionVal("strand", NULL);
char *qStartsWith = optionVal("qStartsWith", NULL);
char *tStartsWith = optionVal("tStartsWith", NULL);
boolean notQ_random = optionExists("notQ_random");
boolean notQ_hap = optionExists("notQ_hap");
boolean notT_random = optionExists("notT_random");
boolean notT_anyRandom = optionExists("notT_anyRandom");
boolean notT_hap = optionExists("notT_hap");
int i;

for (i=0; i<inCount; ++i)
    {
    char *fileName = inNames[i];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct axt *axt;
    lineFileSetMetaDataOutput(lf, stdout);
    while ((axt = axtRead(lf)) != NULL)
        {
	boolean writeIt = TRUE;
	if (qHash != NULL && !hashLookup(qHash, axt->qName))
	    writeIt = FALSE;
	if (notQHash != NULL && hashLookup(notQHash, axt->qName))
	    writeIt = FALSE;
	if (notQ_random && endsWith(axt->qName, "_random"))
	    writeIt = FALSE;
	if (notQ_hap && haplotype(axt->qName))
	    writeIt = FALSE;
	if (notT_random && endsWith(axt->tName, "_random"))
	    writeIt = FALSE;
	if (notT_anyRandom && (endsWith(axt->tName, "_random") ||
			       startsWith("chrUn", axt->tName) || /* galGal */
			       sameWord("chrNA", axt->tName) || /* danRer */
			       sameWord("chrU", axt->tName) ))  /* dm */
	    writeIt = FALSE;
	if (notT_hap && haplotype(axt->tName))
	    writeIt = FALSE;
	if (tHash != NULL && !hashLookup(tHash, axt->tName))
	    writeIt = FALSE;
	if (notTHash != NULL && hashLookup(notTHash, axt->tName))
	    writeIt = FALSE;
	if (axt->score < minScore || axt->score >= maxScore)
	    writeIt = FALSE;
	if (axt->qStart < qStartMin || axt->qStart >= qStartMax)
	    writeIt = FALSE;
	if (axt->tStart < tStartMin || axt->tStart >= tStartMax)
	    writeIt = FALSE;
	if (axt->qEnd < qEndMin || axt->qEnd >= qEndMax)
	    writeIt = FALSE;
	if (axt->tEnd < tEndMin || axt->tEnd >= tEndMax)
	    writeIt = FALSE;
	if (strand != NULL && strand[0] != axt->qStrand)
	    writeIt = FALSE;
        if (qStartsWith != NULL && !startsWith(qStartsWith, axt->qName))
	    writeIt = FALSE;
        if (tStartsWith != NULL && !startsWith(tStartsWith, axt->tName))
	    writeIt = FALSE;
	if (writeIt)
	    axtWrite(axt, stdout);
	axtFree(&axt);
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
axtFilter(argc-1, argv+1);
return 0;
}
