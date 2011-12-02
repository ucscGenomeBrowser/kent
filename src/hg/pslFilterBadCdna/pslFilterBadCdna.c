/* pslFilterBadCdna - remove mrna records with suspcious polyA tails at 3' end. Due to incomplete digestion of DNA during rna library prep. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "psl.h"
#include "cheapcgi.h"
#include "portable.h"
#include "nib.h"
#include "hdb.h"


int reward = 1;
int cost = 1;
int gapOpenCost = 4;
double gapSizeLogMod = 1.0;
int minScore = 15;
int minMatch = 30;
int minUniqueMatch = 20;
int maxBadPpt = 700;
int minAli = 600;
int minAliT = 0;

#define GOOD 0
#define BAD 1
#define GPRIME 2
#define POLYAREGION 40

void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "pslFilterBadCdna - filter out psl file\n"
    "    pslFilterBadCdna nibdir in.psl goodOut.psl gprimeOut.psl \n"
    "options\n"
    "    -dir  Input files are directories rather than single files\n"
    "    -reward=N (default %d) Bonus to score for match\n"
    "    -cost=N (default %d) Penalty to score for mismatch\n"
    "    -gapOpenCost=N (default %d) Penalty for gap opening\n"
    "    -gapSizeLogMod=N (default %4.2f) Penalty for gap sizes\n"
    "    -minScore=N (default %d) Minimum score to pass filter\n"
    "    -minMatch=N (default %d) Min match (including repeats to pass)\n"
    "    -minUniqueMatch (default %d) Min non-repeats to pass)\n"
    "    -maxBadPpt (default %d) Maximum divergence in parts per thousand\n"
    "    -minAli (default %d) Minimum ratio query in alignment in ppt\n"
    "    -minAliT (default %d) Like minAli for target\n",
    reward, cost, gapOpenCost, gapSizeLogMod, minScore, minMatch,
    minUniqueMatch, maxBadPpt, minAli, minAliT);
}

int scoreWindow(char c, char *s, int size, int *score, int *start, int *end)
/* calculate score array with score at each position in s, match to char c adds 1 to score, mismatch adds -1 */
/* index of max score is returned , size is size of s */
{
int i=0, j=0, max=0, count = 0; 

*end = 0;

for (i=0 ; i<size ; i++)
    {
    int prevScore = (i > 0) ? score[i-1] : 0;

    if (toupper(s[i]) == toupper(c) )
        score[i] = prevScore+1;
    else
        score[i] = prevScore-2;
    if (score[i] >= max)
        {
        max = score[i];
        *end = i;
        for (j=i ; j>=0 ; j--)
            if (score[j] == 0)
                {
                *start = j+1;
                break;
                }
        }
    if (score[i] < 0) 
        score[i] = 0;
    }
assert (*end < size);
/* traceback to find start */
for (i=*end ; i>=0 ; i--)
    if (score[i] == 0)
        {
        *start = i+1;
        break;
        }

for (i=*start ; i<=*end ; i++)
    {
    assert (i < size);
    if (toupper(s[i]) == toupper(c) )
        count++;
    }
return count;
}

int polyACalc(int start, int end, char *strand, char *nibDir, char *chrom, int region, int *polyAstart, int *polyAend)
/* get size of polyA tail in genomic dna , count bases in a window */
{
char nibFile[256];
FILE *f;
int seqSize;
struct dnaSeq *seq = NULL;
int count = 0;
int seqStart = strand[0] == '+' ? end : start-region;
int score[POLYAREGION+1], pStart = 0, pEnd = 0; 
int cSize = hChromSize(chrom);

*polyAstart = 0 , *polyAend = 0;
safef(nibFile, sizeof(nibFile), "%s/%s.nib", nibDir, chrom);
nibOpenVerify(nibFile, &f, &seqSize);
if (seqStart < 0) seqStart = 0;
if (seqStart + region > seqSize) region = seqSize - seqStart;
if (region == 0)
    {
    fclose(f);
    return 0;
    }
assert(seqSize <= cSize);
assert(seqStart+region <= cSize);
seq = nibLdPartMasked(NIB_MASK_MIXED, nibFile, f, seqSize, seqStart, region);
if (strand[0] == '+')
    {
    assert (seq->size <= POLYAREGION);
//printf("\n + range=%d %d size %d %s \n",seqStart, seqStart+region, region, seq->dna );
    count = scoreWindow('A',seq->dna,seq->size, score, polyAstart, polyAend);
    }
else
    {
    assert (seq->size <= POLYAREGION);
//printf("\n - range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('T',seq->dna,seq->size, score, polyAend, polyAstart);
    }
pStart += seqStart;
*polyAstart += seqStart;
*polyAend += seqStart;
freeDnaSeq(&seq);
fclose(f);
return count;
}

int filterOk(struct psl *psl, char *nibDir)
/* Return GOOD if psl passes filter. */
/* Return BAD if psl fails filter. */
/* Return GPRIME if psl looks like genomic primed dna . */
{
int score;
int totalAli = psl->match + psl->repMatch + psl->misMatch;
double totAli1000 = totalAli * 1000;
int polyAstart = 0;
int polyAend = 0;

int polyA = polyACalc(psl->tStart, psl->tEnd, psl->strand, nibDir, psl->tName, 
                        POLYAREGION, &polyAstart, &polyAend);
int diff = psl->strand[0] == '+' ? polyAstart-(psl->tEnd) : polyAstart -(psl->tStart);
if (psl->match + psl->repMatch < minMatch)
    return BAD;
if (psl->match < minUniqueMatch)
    return BAD;
if (totAli1000 / psl->qSize < minAli)
    return BAD;
if (totAli1000 / psl->tSize < minAliT)
    return BAD;
if (pslCalcMilliBad(psl, FALSE) > maxBadPpt)
    return BAD;
score = (psl->match + psl->repMatch)*reward - psl->misMatch*cost 
        - (psl->qNumInsert + psl->tNumInsert + 1) * gapOpenCost
	- log(psl->qBaseInsert + psl->tBaseInsert + 1) * gapSizeLogMod;
if (score < minScore)
    return BAD;
if ( polyA >= 15 && psl->blockCount <= 1 && psl->misMatch < 5 && abs(diff) < 3 && (psl->qSize - psl->match) < 30)
    {
    printf("%s polyA %d start %d strand %c blocks %d %s:%d-%d\n",psl->qName, polyA, 
            diff, psl->strand[0], psl->blockCount, psl->tName, psl->tStart, psl->tEnd);
    return GPRIME;
    }
return GOOD;
}

void pslFilterBadCdna(char *nibDir, char *inName, char *outName, char *badName)
/* Filter inName into outName. */
{
struct lineFile *in = pslFileOpen(inName);
FILE *out = mustOpen(outName, "w");
FILE *gprime = mustOpen(badName, "w");
struct psl *psl;
int passCount = 0;
int gprimeCount = 0;
int totalCount = 0;
int status = -1;

printf("Filtering %s to %s\n", inName, outName);
pslWriteHead(out);
while ((psl = pslNext(in)) != NULL)
    {
    ++totalCount;
    status = filterOk(psl, nibDir) ;
    if (status == GOOD)
	{
	++passCount;
	pslTabOut(psl, out);
	}
    else if (status == GPRIME)
        {
	++gprimeCount;
	pslTabOut(psl, gprime);
        }
    pslFree(&psl);
    }
lineFileClose(&in);
fclose(out);
fclose(gprime);
printf("%d of %d passed filter %d of %d look like gprimed bad mrna\n", 
        passCount, totalCount, gprimeCount, totalCount);
}

void pslFilterDir(char *nibDir, char *inDir, char *outDir, char *badOut)
/* Filter all .psl and .pslx files in directory. */
{
struct slName *inList = listDir(inDir, "*.psl*");
struct slName *inEl;
int inCount = slCount(inList), inIx = 0;
char inName[512], outName[512];
for (inEl = inList; inEl != NULL; inEl = inEl->next)
    {
    printf("%d of %d ", ++inIx, inCount);
    fflush(stdout);
    sprintf(inName, "%s/%s", inDir, inEl->name);
    sprintf(outName, "%s/%s", outDir, inEl->name);
    pslFilterBadCdna(nibDir, inName, outName, badOut);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 6)
    usage();
hSetDb(argv[1]);
reward = cgiUsualInt("reward", reward);
cost = cgiUsualInt("cost", cost);
gapOpenCost = cgiUsualInt("gapOpenCost", gapOpenCost);
gapSizeLogMod = cgiUsualDouble("gapSizeLogMod", gapSizeLogMod);
minScore = cgiUsualInt("minScore", minScore);
minMatch = cgiUsualInt("minMatch", minMatch);
minUniqueMatch = cgiUsualInt("minUniqueMatch", minUniqueMatch);
maxBadPpt = cgiUsualInt("maxBadPpt", maxBadPpt);
minAli = cgiUsualInt("minAli", minAli);
if (cgiBoolean("dir") || cgiBoolean("dirs"))
    {
    pslFilterDir(argv[2], argv[3], argv[4], argv[5]);
    }
else
    {
    pslFilterBadCdna(argv[2], argv[3], argv[4], argv[5]);
    }
return 0;
}
