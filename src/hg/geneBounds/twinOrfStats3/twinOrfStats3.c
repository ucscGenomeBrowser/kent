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
  "twinOrfStats3 - Collect stats on refSeq cDNAs aligned to another \n"
  "species via axtForEst\n"
  "usage:\n"
  "   twinOrfStats3 refSeqAli.axt refSeq.ra twinOrf.stats\n"
  "options:\n"
  "   -earlyAaSize=N - default 20 - number of amino acids in N terminal model\n"
  "   -predict=predict.out - Predict start codon position on same set\n"
  "   -threshold=N.N - default 4 - used with predict only\n"
  );
}

double threshold = 4.0;
int earlyAaSize = 20;	/* Size of first bits. */

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

#define ix2(t,q)  ((t)*7 + (q))
#define symIx2(t,q)  ix2(symToIx[(int)t],symToIx[(int)q])
/* Convert symbol pair to single symbol in larger alphabet. */

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

struct c1Counts
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

struct c3Counts
/* A matrix for codon counts */
   {
   int counts[7*7][7*7][7*7];
   };



void initC3Counts(struct c3Counts *cm, int count)
/* Initialize count matrix with values */
{
int i, j, k;
for (i=0; i<7*7; ++i)
    for (j=0; j<7*7; ++j)
	for (k=0; k<7*7; ++k)
	    cm->counts[i][j][k] = count;
}

int c1CountsTotal(struct c1Counts *cm)
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

int c3CountsTotal(struct c3Counts *cm)
/* Count total number in matrix. */
{
int i, j, k;
int count = 0;
for (i=0; i<7*7; ++i)
    for (j=0; j<7*7; ++j)
	for (k=0; k<7*7; ++k)
	    count += cm->counts[i][j][k];
return count;
}

void countToOdds(struct c1Counts *fore, struct c1Counts *back, 
	struct oddsMatrix *om)
/* Make matrix that is log odds base on foreground and background counts. */
{
double foreTotal = c1CountsTotal(fore);
double backTotal = c1CountsTotal(back);
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

void addCodons(struct c3Counts *cm, struct axt *axt, int startT, int endT)
/* Add range of values to matrix. */
{
int tPos = startT;
char t,q;
int ix[3];
static char tCod[4], qCod[4];
int codIx = 0, i;
int symIx;

if (startT >= endT)
    return;
symIx = tIxToSymIx(axt, startT);
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
	  for (i=0; i<3; ++i)
	      ix[i] = symIx2(tCod[i], qCod[i]);
	  cm->counts[ix[0]][ix[1]][ix[2]] += 1;
	  codIx = 0;
	  }
       ++tPos;
       }
    }
}

void addRange(struct c1Counts *c1, struct c2Counts *c2, struct c3Counts *c3,
	struct axt *axt, int startT, int endT)
/* Add range of values to matrix. */
{
int symIx = tIxToSymIx(axt, startT);
int tPos = startT;
char t,q;
int tIx[3], qIx[3];
int i;

for (i=0; i<3; ++i)
    tIx[i] = qIx[i] = -1;

for (;symIx < axt->symCount; ++symIx)
    {
    if (tPos >= endT)
        break;
    t = axt->tSym[symIx];
    q = axt->qSym[symIx];
    tIx[2] = symToIx[(int)t];
    qIx[2] = symToIx[(int)q];
    if (tIx[2] >= 0 && qIx[2] >= 0)
	{
	c1->counts[tIx[2]][qIx[2]] += 1;
	if (tIx[1] >= 0 && qIx[1] >= 0)
	    {
	    c2->counts[ix2(tIx[1],qIx[1])][ix2(tIx[2],qIx[2])] += 1;
	    if (tIx[0] >= 0 && qIx[0] >= 0)
	        {
		c3->counts[ix2(tIx[0],qIx[0])][ix2(tIx[1],qIx[1])][ix2(tIx[2],qIx[2])] += 1;
		}
	    }
	}
    if (t != '-' && t != '.')
       ++tPos;
    for (i=0; i<2; ++i)
        {
	tIx[i] = tIx[i+1];
	qIx[i] = qIx[i+1];
	}
    }
}

void addPos(struct c1Counts *c1, struct c2Counts *c2, struct axt *axt, int pos)
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
	    int tLastIx = symToIx[(int)(axt->tSym[symIx-1])];
	    int qLastIx = symToIx[(int)(axt->qSym[symIx-1])];
	    if (tIx >= 0 && qIx >= 0)
		{
		c1->counts[tIx][qIx] += 1;
		if (tLastIx >= 0 && qLastIx >= 0)
		    {
		    c2->counts[ix2(tLastIx,qLastIx)][ix2(tIx,qIx)] += 1;
		    }
		}
	    break;
	    }
	++tPos;
	}
    }
}


void dumpC1(FILE *f, struct c1Counts *cm, char *label)
/* Dump out position. */
{
int q, t;
int counts = c1CountsTotal(cm);

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

void dumpC2(FILE *f, struct c2Counts *m, char *label)
/* Dump 2 char pair matrix. */
{
int q1, q2, t1, t2, i1, i2;
int counts = c2CountsTotal(m);

fprintf(f, "%s %d\n", label, counts);
fprintf(f, "#    ");
for (t1=0; t1<7; ++t1)
   {
   for (q1=0; q1<7; ++q1)
       {
       fprintf(f, "   %c/%c    ", ixToSym[t1], ixToSym[q1]);
       }
    }
fprintf(f,"\n");

for (t1=0; t1<7; ++t1)
    {
    for (q1=0; q1<7; ++q1)
	{
	i1 = ix2(t1,q1);
	fprintf(f, "%c/%c", ixToSym[t1], ixToSym[q1]);
	for (t2=0; t2<7; ++t2)
	    {
	    for (q2=0; q2<7; ++q2)
		{
		i2 = ix2(t2,q2);
	        fprintf(f, " %8.7f", (double)m->counts[i1][i2]/counts);
		}
	    }
	fprintf(f, "\n");
	}
    }
fprintf(f, "\n");
}


void dumpC3(FILE *f, struct c3Counts *cm, char *label)
/* Dump 3 char pair matrix */
{
int i1, i2, i3, q1, q2, q3, t1, t2, t3;
int counts = c3CountsTotal(cm);

fprintf(f, "%s %d\n", label, counts);
fprintf(f, "#       ");
for (t1=0; t1<7; ++t1)
   {
   for (q1=0; q1<7; ++q1)
       {
       fprintf(f, "   %c/%c    ", ixToSym[t1], ixToSym[q1]);
       }
    }
fprintf(f,"\n");

for (t1=0; t1<7; ++t1)
    {
    for (q1=0; q1<7; ++q1)
        {
	i1 = ix2(t1,q1);
	for (t2 = 0; t2<7; ++t2)
	    {
	    for (q2=0; q2<7; ++q2)
	        {
		i2 = ix2(t2,q2);
		fprintf(f, "%c%c/%c%c ", ixToSym[t1], ixToSym[t2], ixToSym[q1], ixToSym[q2]);
		for (t3=0; t3<7; ++t3)
		    {
		    for (q3=0; q3<7; ++q3)
		        {
			i3 = ix2(t3,q3);
			fprintf(f, " %8.7f", (double)cm->counts[i1][i2][i3]/counts);
			}
		    }
		fprintf(f, "\n");
		}
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


void predict(struct c1Counts cKozak[10], struct c1Counts *cAll, 
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
static struct c1Counts c1Kozak[10], c1all, c1utr5, c1utr3, c1cds;
static struct c2Counts c2Kozak[10], c2All, c2Utr5, c2Utr3, c2Cds;
static struct c3Counts c3All, c3Utr5, c3Utr3, c3Cds;
char label[64];
char *predictFile = optionVal("predict", NULL);
int i;
static struct c3Counts cod1, cod2, cod3, stop, earlyCod1, earlyCod2, earlyCod3;
int earlySize;

initC3Counts(&cod1, 0);
initC3Counts(&cod2, 0);
initC3Counts(&cod3, 0);
initC3Counts(&earlyCod1, 0);
initC3Counts(&earlyCod2, 0);
initC3Counts(&earlyCod3, 0);
initC3Counts(&c3Utr3, 0);
initC3Counts(&c3Utr5, 0);
initC3Counts(&stop, 0);

threshold = optionFloat("threshold", threshold);
earlyAaSize = optionInt("earlyAaSize", earlyAaSize);
earlySize = 3*earlyAaSize;
while ((axt = axtRead(lf)) != NULL)
    {
    struct refSeqInfo *rsi = hashFindVal(rsiHash, axt->tName);
    if (rsi != NULL && rsi->cdsStart >= 6)
        {
	if (checkAtg(axt, rsi->cdsStart))
	    {
	    for (i=0; i<10; ++i)
		addPos(&c1Kozak[i], &c2Kozak[i], axt, rsi->cdsStart - 5 + i);
	    addRange(&c1all, &c2All, &c3All, axt, 0, rsi->size);
	    addRange(&c1utr5, &c2Utr5, &c3Utr5, axt, 0, rsi->cdsStart);
	    addRange(&c1cds, &c2Cds, &c3Cds, axt, rsi->cdsStart, rsi->cdsEnd);
	    addRange(&c1utr3, &c2Utr3, &c3Utr3, axt, rsi->cdsEnd, rsi->size);

	    /* The +3+1 in the expression below breaks down as so:  the
	     * +3 is to move past the first 'ATG' codon, which is part of
	     * the Kozak consensus model, not the coding model.  The +1
	     * is so that we look at the 2nd and 3rd bases of the previous
	     * codon, and the first base of the current codon.   */
	    addCodons(&earlyCod1, axt, rsi->cdsStart+3+1, rsi->cdsStart+1+earlySize);
	    addCodons(&earlyCod2, axt, rsi->cdsStart+3+2, rsi->cdsStart+2+earlySize);
	    addCodons(&earlyCod3, axt, rsi->cdsStart+3+3, rsi->cdsStart+3+earlySize);
	    addCodons(&cod1, axt, rsi->cdsStart+3+1+earlySize, rsi->cdsEnd-5);
	    addCodons(&cod2, axt, rsi->cdsStart+3+2+earlySize, rsi->cdsEnd-4);
	    addCodons(&cod3, axt, rsi->cdsStart+3+3+earlySize, rsi->cdsEnd-3);
	    addCodons(&stop, axt, rsi->cdsEnd-3, rsi->cdsEnd);
	    }
	}
    axtFree(&axt);
    }
lineFileClose(&lf);

dumpC1(f, &c1all, "c1_all");
dumpC2(f, &c2All, "c2_all");
dumpC3(f, &c3All, "c3_all");

dumpC1(f, &c1utr5, "c1_utr5");
dumpC2(f, &c2Utr5, "c2_utr5");
dumpC3(f, &c3Utr5, "c3_utr5");

dumpC1(f, &c1cds, "c1_cds");
dumpC2(f, &c2Cds, "c2_cds");
dumpC3(f, &c3Cds, "c3_cds");

dumpC1(f, &c1utr3, "c1_utr3");
dumpC2(f, &c2Utr3, "c2_utr3");
dumpC3(f, &c3Utr3, "c3_utr3");

for (i=0; i<10; ++i)
    {
    sprintf(label, "c1_kozak[%d]", i-5);
    dumpC1(f, &c1Kozak[i], label);
    sprintf(label, "c2_kozak[%d]", i-5);
    dumpC2(f, &c2Kozak[i], label);
    }
dumpC3(f, &earlyCod1, "earlyCod1");
dumpC3(f, &earlyCod2, "earlyCod2");
dumpC3(f, &earlyCod3, "earlyCod3");
dumpC3(f, &cod1, "cod1");
dumpC3(f, &cod2, "cod2");
dumpC3(f, &cod3, "cod3");
dumpC3(f, &stop, "stop");

if (predictFile)
    {
    predict(c1Kozak, &c1all, axtFile, predictFile, rsiHash);
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

