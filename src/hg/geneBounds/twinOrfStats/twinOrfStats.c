/* twinOrfStats - Collect stats on refSeq cDNAs aligned to another species via axtForEst. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"
#include "ra.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "twinOrfStats - Collect stats on refSeq cDNAs aligned to another \n"
  "species via axtForEst\n"
  "usage:\n"
  "   twinOrfStats refSeqAli.axt refSeq.ra twinOrf.stats\n"
  "options:\n"
  "   -predict=predict.out - Predict start codon position on same set\n"
  "   -threshold=N.N - default 4\n"
  );
}

double threshold = 4.0;

boolean parseCds(char *gbCds, int start, int end, int *retStart, int *retEnd)
/* Parse genBank style cds string into something we can use... */
{
char *s = gbCds;
boolean gotStart = FALSE, gotEnd = TRUE;

gotStart = isdigit(s[0]);
s = strchr(s, '.');
if (s == NULL || s[1] != '.')
    errAbort("Malformed GenBank cds %s", gbCds);
s[0] = 0;
s += 2;
gotEnd = isdigit(s[0]);
if (gotStart) start = atoi(gbCds) - 1;
if (gotEnd) end = atoi(s);
*retStart = start;
*retEnd = end;
return gotStart && gotEnd;
}


struct refSeqInfo
/* Info on a refSeq. */
    {
    struct refSeqInfo *next;
    char *acc;		/* Accession - allocated in hash */
    boolean hasCds;	/* True if has CDS */
    int cdsStart;	/* Start of CDS */
    int cdsEnd;		/* End of CDS */
    int size;		/* Overall RNA size. */
    };

struct hash *readRefRa(char *fileName)
/* Read in refSeq ra file and return bits we're interested
 * in in a hash full of refSeqInfos. */
{
struct hash *hash = newHash(16);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *ra;
int count = 0, cdsCount = 0;

while ((ra = raNextRecord(lf)) != NULL)
    {
    char *acc = hashFindVal(ra, "acc");
    if (acc != NULL)
        {
	char *cds = hashFindVal(ra, "cds");
	char *siz = hashFindVal(ra, "siz");
	struct refSeqInfo *rsi;
	if (siz == NULL)
	    {
	    warn("No size for %s, skipping", acc);
	    continue;
	    }
	AllocVar(rsi);
	hashAddSaveName(hash, acc, rsi, &rsi->acc);
	rsi->size = atoi(siz);
	if (cds != NULL)
	    {
	    rsi->hasCds = parseCds(cds, 0, rsi->size, 
	    	&rsi->cdsStart, &rsi->cdsEnd);
	    if (rsi->hasCds)
	        ++cdsCount;
	    }
	++count;
	}
    hashFree(&ra);
    }
lineFileClose(&lf);
printf("Got %d cds of %d in %s\n", cdsCount, count, fileName);
return hash;
}

char ixToSym[] = {'T', 'C', 'A', 'G', 'N', '-', '.'};
char lixToSym[] = {'t', 'c', 'a', 'g', 'n', '-', '.'};
int symToIx[256];

void initSymToIx()
/* Initialize lookup table. */
{
int i;
for (i=0; i<ArraySize(symToIx); ++i)
    symToIx[i] = -1;
for (i=0; i<ArraySize(ixToSym); ++i)
    {
    symToIx[(int)ixToSym[i]] = i;
    symToIx[(int)lixToSym[i]] = i;
    }
}

struct countMatrix
/* A position matrix */
    {
    int counts[7][7];
    };

struct oddsMatrix
/* Log odds matrix */
    {
    double odds[7][7];
    };

struct c2Counts
/* Counts of adjacent columns in alignment */
    {
    int counts[7*7][7*7];
    };

struct codonCounts
/* A matrix for codon counts */
   {
   int counts[7*7*7][7*7*7];
   };

struct codonOdds
/* A matrix for codon odds */
   {
   double odds[7*7*7][7*7*7];
   };


void initCounts(struct codonCounts *cm, int count)
/* Initialize count matrix with values */
{
int i, j;
for (i=0; i<7*7*7; ++i)
    for (j=0; j<7*7*7; ++j)
	cm->counts[i][j] = count;
}

int countMatrixTotal(struct countMatrix *cm)
/* Count total number in matrix. */
{
int i, j;
int count = 0;
for (i=0; i<7; ++i)
    for (j=0; j<7; ++j)
        count += cm->counts[i][j];
return count;
}

int c2CountsTotal(struct c2Counts *m)
/* Count total number in matrix */
{
int i,j;
int count = 0;
for (i=0; i<7*7; ++i)
    for (j=0; j<7*7; ++j)
        count += m->counts[i][j];
return count;
}

int countCodonMatrixTotal(struct codonCounts *cm)
/* Count total number in matrix. */
{
int i, j;
int count = 0;
for (i=0; i<7*7*7; ++i)
    for (j=0; j<7*7*7; ++j)
        count += cm->counts[i][j];
return count;
}

void countToOdds(struct countMatrix *fore, struct countMatrix *back, 
	struct oddsMatrix *om)
/* Make matrix that is log odds base on foreground and background counts. */
{
double foreTotal = countMatrixTotal(fore);
double backTotal = countMatrixTotal(back);
int i,j;
for (i=0; i<7; ++i)
    for (j=0; j<7; ++j)
        {
	double fp = fore->counts[i][j]/foreTotal;
	double bp = back->counts[i][j]/backTotal;
	double odds;
	if (bp <= 0.00001)
	    odds = 0.0;
	else if (fp <= 0)
	    odds = -100.0;
	else 
	    odds = log(fp/bp);
	if (odds < -100.0)
	    odds = -100.0;
	if (odds > 100.0)
	    odds = 100.0;
	om->odds[i][j] = odds;
	}
}

int tIxFromSymIx(struct axt *axt, int symIx)
/* Return target position of given symbol. */
{
int tPos = axt->tStart;
int i;
for (i=0; i<symIx; ++i)
    {
    char t = axt->tSym[i];
    if (t != '.' && t != '-')
        ++tPos;
    }
return tPos;
}

int tIxToSymIx(struct axt *axt, int tIx)
/* Returns index in axt of tIx base in target sequence. */
{
int tPos = axt->tStart;
int symCount = axt->symCount;
char *tSym = axt->tSym;
int i;
if (tIx > axt->tEnd)
    errAbort("tIx >= tEnd tIxToSymIx(%s %s %d)", axt->qName, axt->tName, tIx);
if (tIx == axt->tEnd)
    return symCount;
for (i=0; i<symCount; ++i)
    {
    if (tSym[i] != '-')
	{
	if (tPos == tIx)
	    return i;
        ++tPos;
	}
    }
errAbort("Couldn't tIxToSymIx(%s %s %d)", axt->qName, axt->tName, tIx);
return -1;
}

boolean checkAtg(struct axt *axt, int cdsStart)
/* Return TRUE if starts with atg, otherwise
 * FALSE */
{
static char expect[4] = "ATG", got[4];
int i, symIx;
for (i=0; i<3; ++i)
    {
    symIx = tIxToSymIx(axt, cdsStart + i);
    got[i] = axt->tSym[symIx];
    }
touppers(got);
if (!sameString(expect, got))
    {
    printf("%s %s %d\n", axt->tName, got, cdsStart);
    return FALSE;
    }
return TRUE;
}

void addCodons(struct codonCounts *cm, struct axt *axt, int startT, int endT)
/* Add range of values to matrix. */
{
int symIx = tIxToSymIx(axt, startT);
int tPos = startT;
char t,q;
int tIx, qIx;
static char tCod[4], qCod[4];
int codIx = 0, i;

for (;symIx < axt->symCount; ++symIx)
    {
    if (tPos >= endT)
        break;
    t = axt->tSym[symIx];
    q = axt->qSym[symIx];
    if (t != '-' && t != '.')
       {
       tCod[codIx] = t;
       qCod[codIx] = q;
       ++codIx;
       if (codIx == 3)
          {
	  tIx = qIx = 0;
	  for (i=0; i<3; ++i)
	      {
	      tIx = tIx*7 + symToIx[(int)tCod[i]];
	      qIx = qIx*7 + symToIx[(int)qCod[i]];
	      }
	  cm->counts[tIx][qIx] += 1;
	  codIx = 0;
	  }
       ++tPos;
       }
    }
}

void addRange(struct countMatrix *cm, struct c2Counts *m,
	struct axt *axt, int startT, int endT)
/* Add range of values to matrix. */
{
int symIx = tIxToSymIx(axt, startT);
int tPos = startT;
char t,q;
int tIx, qIx, lastTix = -1, lastQix = -1;

for (;symIx < axt->symCount; ++symIx)
    {
    if (tPos >= endT)
        break;
    t = axt->tSym[symIx];
    q = axt->qSym[symIx];
    tIx = symToIx[(int)t];
    qIx = symToIx[(int)q];
    if (tIx >= 0 && qIx >= 0)
	{
	cm->counts[tIx][qIx] += 1;
	if (lastTix >= 0 && lastQix >= 0)
	    {
	    m->counts[lastTix*7+lastQix][tIx*7+qIx] += 1;
	    }
	}
    if (t != '-' && t != '.')
       ++tPos;
    lastTix = tIx;
    lastQix = qIx;
    }
}

void addPos(struct countMatrix *cm, struct axt *axt, int pos)
/* Add count from match at pos to matrix. */
{
int symIx;
int tPos = 0;

for (symIx = 0; symIx < axt->symCount; ++symIx)
    {
    if (axt->tSym[symIx] != '-')
	{
	if (tPos == pos)
	    {
	    char t = axt->tSym[symIx];
	    char q = axt->qSym[symIx];
	    int tIx = symToIx[(int)t];
	    int qIx = symToIx[(int)q];
	    if (tIx >= 0 && qIx >= 0)
		cm->counts[tIx][qIx] += 1;
	    break;
	    }
	++tPos;
	}
    }
}


void dumpCounts(FILE *f, struct countMatrix *cm, char *label)
/* Dump out position. */
{
int q, t;
int counts = countMatrixTotal(cm);

fprintf(f, "%s %d\n", label, counts);
for (t = 0; t < 7; ++t)
    {
    fprintf(f, "%c", ixToSym[t]);
    for (q = 0; q<7; ++q)
        fprintf(f, " %f", (double)cm->counts[t][q]/counts);
    fprintf(f, "\n");
    }
fprintf(f, "# ");
for (t = 0; t < 7; ++t)
    {
    int tot = 0;
    fprintf(f, "%c", ixToSym[t]);
    for (q = 0; q<7; ++q)
	tot += cm->counts[t][q];
    fprintf(f, " %f, ", (double)tot/counts);
    }
fprintf(f, "\n");
fprintf(f, "\n");
}

void dumpM1(FILE *f, struct c2Counts *m, char *label)
/* Dump first order markov matrix. */
{
int q, t, t1, t2;
int counts = c2CountsTotal(m);

fprintf(f, "%s %d\n", label, counts);
t = 0;
fprintf(f, "#   ");
for (t1=0; t1<7; ++t1)
   {
   for (t2=0; t2<7; ++t2)
       {
       fprintf(f, "   %c%c     ", ixToSym[t1], ixToSym[t2]);
       }
    }
fprintf(f,"\n");

for (t1=0; t1<7; ++t1)
    {
    for (t2=0; t2<7; ++t2)
	{
	fprintf(f, "%c%c", ixToSym[t1], ixToSym[t2]);
	for (q=0; q<7*7; ++q)
	   fprintf(f, " %8.7f", (double)m->counts[t][q]/counts);
	fprintf(f, "\n");
	++t;
	}
    }
fprintf(f, "\n");
}


void dumpCodon(FILE *f, struct codonCounts *cm, char *label)
/* Dump codon matrix */
{
int q, t, t1, t2, t3;
int counts = countCodonMatrixTotal(cm);

fprintf(f, "%s %d\n", label, counts);
t = 0;
fprintf(f, "#   ");
for (t1=0; t1<7; ++t1)
   {
   for (t2=0; t2<7; ++t2)
       {
       for (t3=0; t3<7; ++t3)
	   fprintf(f, "   %c%c%c    ", ixToSym[t1], ixToSym[t2], ixToSym[t3]);
       }
    }
fprintf(f,"\n");

for (t1=0; t1<7; ++t1)
   {
   for (t2=0; t2<7; ++t2)
       {
       for (t3=0; t3<7; ++t3)
           {
	   fprintf(f, "%c%c%c", ixToSym[t1], ixToSym[t2], ixToSym[t3]);
	   for (q=0; q<7*7*7; ++q)
	       fprintf(f, " %8.7f", (double)cm->counts[t][q]/counts);
	   fprintf(f, "\n");
	   ++t;
	   }
	}
    }
fprintf(f, "\n");
}

double scoreMotif(struct oddsMatrix *motif,  int motifSize, char *t, char *q)
/* Score motif at position. */
{
int i;
double score = 0;
for (i=0; i<motifSize; ++i)
    score += motif[i].odds[symToIx[(int)t[i]]][symToIx[(int)q[i]]];
return score;
}

void findBestHit(struct axt *axt, struct oddsMatrix motif[], int motifSize,
	int *retPos, double *retScore, int *retFirstPos, double *retFirstScore)
{
int pos, bestPos = -1;
double score, bestScore = -BIGNUM;
int endPos = axt->symCount - motifSize;
boolean gotFirst = FALSE;

*retFirstPos = -1;
*retFirstScore = 0;
for (pos=0; pos<endPos; ++pos)
    {
    score = scoreMotif(motif, motifSize, axt->tSym+pos, axt->qSym+pos);
    if (!gotFirst && score > threshold)
	{
	gotFirst = TRUE;
        *retFirstPos = pos;
	*retFirstScore = score;
	}
    if (score > bestScore)
        {
	bestScore = score;
	bestPos = pos;
	}
    }
assert(bestPos != -1);
*retPos = bestPos;
*retScore = bestScore;
}


void predict(struct countMatrix cKozak[10], struct countMatrix *cAll, 
	char *axtFile, char *outFile, struct hash *rsiHash)
/* Predict location of initial ATG */
{
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct oddsMatrix kozak[10];
int i;
int bestPos, firstPos, actualPos;
double bestScore, firstScore, actualScore;
struct axt *axt;

for (i=0; i<10; ++i)
    countToOdds(&cKozak[i], cAll, &kozak[i]);
while ((axt = axtRead(lf)) != NULL)
    {
    struct refSeqInfo *rsi = hashFindVal(rsiHash, axt->tName);
    if (rsi != NULL && rsi->cdsStart >= 5)
        {
	findBestHit(axt, kozak, 10, &bestPos, &bestScore, &firstPos, 
		&firstScore);
	actualPos = tIxToSymIx(axt, rsi->cdsStart - 5);
	actualScore = scoreMotif(kozak, 10, 
		axt->tSym+actualPos, axt->qSym + actualPos);
/* Score motif at position. */
	fprintf(f, "%s\t%d\t%f\t%d\t%f\t%d\t%f\n", axt->tName, 
		rsi->cdsStart, actualScore,
		tIxFromSymIx(axt, bestPos) + 5, bestScore, 
		tIxFromSymIx(axt, firstPos) + 5, firstScore);
	}
    axtFree(&axt);
    }
carefulClose(&f);
lineFileClose(&lf);
}

void twinOrfStats(char *axtFile, char *raFile, char *outFile)
/* twinOrfStats - Collect stats on refSeq cDNAs aligned to another species via axtForEst. */
{
struct hash *rsiHash = readRefRa(raFile);
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct axt *axt;
static struct countMatrix kozak[10], all, utr5, utr3, cds;
static struct c2Counts c2All, c2Utr5, c2Utr3, c2Cds;
char label[64];
char *predictFile = optionVal("predict", NULL);
int i;
struct codonCounts codons;

initCounts(&codons, 1);

threshold = optionFloat("threshold", threshold);
while ((axt = axtRead(lf)) != NULL)
    {
    struct refSeqInfo *rsi = hashFindVal(rsiHash, axt->tName);
    if (rsi != NULL && rsi->cdsStart >= 5)
        {
	if (checkAtg(axt, rsi->cdsStart))
	    {
	    for (i=0; i<10; ++i)
		addPos(&kozak[i], axt, rsi->cdsStart - 5 + i);
	    addRange(&all, &c2All, axt, 0, rsi->size);
	    addRange(&utr5, &c2Utr5, axt, 0, rsi->cdsStart);
	    addRange(&cds, &c2Cds, axt, rsi->cdsStart, rsi->cdsEnd);
	    addRange(&utr3, &c2Utr3, axt, rsi->cdsEnd, rsi->size);
	    addCodons(&codons, axt, rsi->cdsStart, rsi->cdsEnd-3);
	    }
	}
    axtFree(&axt);
    }
lineFileClose(&lf);
dumpCounts(f, &all, "all");
dumpCounts(f, &utr5, "utr5");
dumpCounts(f, &cds, "cds");
dumpCounts(f, &utr3, "utr3");
dumpM1(f, &c2All, "c2_all");
dumpM1(f, &c2Utr5, "c2_utr5");
dumpM1(f, &c2Cds, "c2_cds");
dumpM1(f, &c2Utr3, "c2_utr3");
for (i=0; i<10; ++i)
    {
    sprintf(label, "kozak[%d]", i-5);
    dumpCounts(f, &kozak[i], label);
    }
dumpCodon(f, &codons, "codon");
if (predictFile)
    {
    predict(kozak, &all, axtFile, predictFile, rsiHash);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
initSymToIx();
twinOrfStats(argv[1], argv[2], argv[3]);
return 0;
}
