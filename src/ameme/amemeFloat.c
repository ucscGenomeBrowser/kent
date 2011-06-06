/* ameme - The central part of AmphetaMeme, which finds patterns in DNA */
#include "common.h"
#include "memalloc.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "localmem.h"
#include "fa.h"
#include "portable.h"
#include "fuzzyFind.h"
#include "memgfx.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "sig.h"

boolean isFromWeb;

static void vaProgress(char *format, va_list args)
/* Print message to indicate progress - to web page if in
 * interactive mode, to console if not. */
{
FILE *f = (isFromWeb ? stdout : stderr);
if (format != NULL) {
    vfprintf(f, format, args);
    fflush(f);
    }
}

static void progress(char *format, ...)
/* Print message to indicate progress - to web page if in
 * interactive mode, to console if not. */
{
va_list args;
va_start(args, format);
vaProgress(format, args);
va_end(args);
}

enum nullModel
    {
    nmMark2,
    nmMark1,
    nmMark0,
    nmEven,
    };

struct cgiChoice nullModelChoices[] =
/* The different choices for the NULL model. */
    {
        {"Markov 2", nmMark2,},
        {"Markov 1", nmMark1,},
        {"Markov 0", nmMark0,},
        {"Even", nmEven,},
        {"m2", nmMark2,},
        {"m1", nmMark1,},
        {"m0", nmMark0,},
    };

/* Various parameters the user controls via CGI */
int nullModel = nmMark1;
char *nullModelCgiName = NULL;
int numMotifs = 2;
int defaultTileSize = 7;
boolean leftAlign = FALSE;
boolean useLocation = TRUE;
boolean kentishWeights = TRUE;
boolean maxOcc = 1;
int startScanLimit = 20;        /* # of sequences to scan for start tiles. */
double constrainer = 1.0;
char *bgSource = NULL;

/* Data about the sequence we're trying to find patterns in. (The foreground
 * or "good" sequence. */
char *goodName;
struct seqList *goodSeq = NULL;
int goodSeqElSize;
int goodSeqListSize;
int goodSeqNameSize;

double calcApproximateTime()
/* Get an estimated time in minutes. */
{
/* It takes about 40 s on 'ftp' to do  20 scan lines, 52 seqEl, 30 seq */
int startScanLines = startScanLimit;
double seqElSize = goodSeqElSize;
if (startScanLines > goodSeqListSize) startScanLines = goodSeqListSize;
return ((numMotifs * startScanLines * seqElSize * seqElSize * goodSeqListSize)/100000 + 10)/60;
}

/* Frequency tables for the null model (background). */

double freqBase[5] = {1.0, 0.25, 0.25, 0.25, 0.25};
/* The probability of finding a base in sequence. */
                    /* N     T     C     A     G */
double *freq = &freqBase[1];
/* Frequency table that allows -1 indices for N's */

double logFreqBase[5] = { 0.0, -0.60205999, -0.60205999, -0.60205999, -0.60205999};
double *logFreq = &logFreqBase[1];
/* Log of frequency. */

double mark1[5][5];
double logMark1[5][5];
/* First order Markov model - probability that one nucleotide will follow another. */
#define logMark1Prob(ntVal1, ntVal2) (logMark1[(ntVal1)+1][(ntVal2)+1])

double mark2[5][5][5];
double logMark2[5][5][5];
/* Second order Markove model - probability that a nucleotide follows two previous. */
#define logMark2Prob(v1,v2,v3) (logMark2[(v1)+1][(v2)+1][(v3)+1])



struct seqList
/* Records data for one sequence in list. */
    {
    struct seqList *next;
    char *comment;
    struct dnaSeq *seq;
    double *softMask;
    };

void freeSeqList(struct seqList **pList)
/* Free up a sequence list. */
{
struct seqList *seq, *next;
for (seq = *pList; seq != NULL; seq = next)
    {
    next = seq->next;
    freeDnaSeq(&seq->seq);
    freeMem(seq->comment);
    freeMem(seq->softMask);
    freeMem(seq);
    }
}

struct seqList *readSeqList(char *faFileName)
/* Read in an FA file into a sequence list. */
{
struct seqList *seqList = NULL, *seqEl;
FILE *f;
char *comment;
struct dnaSeq *seq;
int seqCount = 0;

f = mustOpen(faFileName, "r");
while (faReadNext(f, NULL, TRUE, &comment, &seq))
    {
    AllocVar(seqEl);
    seqEl->comment = comment;
    seqEl->seq = seq;
    slAddHead(&seqList, seqEl);
    }
slReverse(&seqList);
fclose(f);
return seqList;
}

int totalSeqSize(struct seqList *list)
/* How long is total sequence? */
{
struct seqList *el;
int acc = 0;
for (el = list; el != NULL; el = el->next)
    {
    acc += el->seq->size;
    }
return acc;
}

double averageSeqSize(struct seqList *list)
/* How long is average sequence. */
{
double tot = totalSeqSize(list);
return tot/slCount(list);
}

int maxSeqSize(struct seqList *list)
/* Figure out max size of sequences. */
{
struct seqList *el;
int seqSize = -1;

if (list == NULL)
    return 0;
for (el = list; el != NULL; el = el->next)
    {
    if (el->seq->size > seqSize)
        seqSize = el->seq->size;
    }
return seqSize;
}

int uniformSeqSize(struct seqList *list, boolean leftAlign)
/* Figure out max size of sequences and make all of them that size,
 * padding with -'s as necessary. */
{
int seqSize = maxSeqSize(list);
struct seqList *el;

for (el = list; el != NULL; el = el->next)
    {
    struct dnaSeq *seq = el->seq;
    int dif = seqSize - seq->size;
    if (dif != 0)
        {
        DNA *newDna = needMem(seqSize+1);
        assert(dif > 0);
        if (leftAlign)
            {
            memcpy(newDna, seq->dna, seq->size);
            memset(newDna + seq->size, '-', dif);
            }
        else
            {
            memset(newDna, '-', dif);
            memcpy(newDna+dif, seq->dna, seq->size);
            }
        newDna[seqSize] = 0;
        freeMem(seq->dna);
        seq->dna = newDna;
        seq->size = seqSize;
        }
    el->softMask = needMem(seqSize * sizeof(el->softMask[0]) );
    }
return seqSize;
}

/* Various constants used by statistical routines. */
double twoPi = 3.141592653589 * 2.0;
double oneOverSqrtTwoPi;
double logOneOverSqrtTwoPi;
double logOneFourth;
double logOneFifth;

double simpleGaussean(double x)
/* Gaussean distribution with standard deviation 1 and mean 0. */
{
return oneOverSqrtTwoPi * exp(-0.5*x*x );
}

double logSimpleGaussean(double x)
/* Log (base e) of simple Gaussean. */
{
return logOneOverSqrtTwoPi - 0.5 * x * x;
}

double gaussean(double x, double mean, double sd)
/* Gaussean distribution with mean and standard deviation at point x  */
{
x -= mean;
x /= sd;
return oneOverSqrtTwoPi * exp(-0.5*x*x) / sd;
}

double logGaussean(double x, double mean, double sd, double logSd)
/* Log of gaussian. */
{
x = (x-mean)/sd;
return logOneOverSqrtTwoPi - 0.5 * x * x - logSd;
}

void gaussLocale(double mean, double standardDeviation, int localeLen, double *locale)
/* Fill in locale with values of Gaussian distribution with given mean and standard
 * deviation at positions 0 ... localeLen-1.  Since this is going to clip off the
 * tails of the distribution, normalize things so that the sum over the entire locale
 * is equal to 1.0 */
{
double *d;
double *end = locale + localeLen;
double acc = 0;
double scale;
double gaussVal;
double x = -mean/standardDeviation;
double oneOverSd = 1.0/standardDeviation;

/* In first pass the scale is going to be off anyway, so don't bother
 * dividing by the square root of two pi, or the standard deviation
 * each time around. */
for(d = locale; d < end; d++)
    {
    gaussVal = *d = (x > 20 ? 0.1e-99 : exp(-0.5*x*x));
    acc += gaussVal;
    x += oneOverSd;
    }

/* Rescale things so everything totals to 1.0. */
scale = 1.0/acc;
for (d = locale; d < end; d++)
    *d *= scale;
}

/* Handle location probability for one sequence at a time with these. */
double *locProb = NULL;
int locProbAlloc = 0;

void makeLocProb(double mean, double sd, int size)
/* Make location probability table. */
{
if (!useLocation)
    return;
if (size > locProbAlloc)
    {
    freeMem(locProb);
    locProb = needMem(size * sizeof(*locProb));
    locProbAlloc = size;
    }
gaussLocale(mean, sd, size, locProb);
}

void statUtilOpen()
/* Initialize statistical utilities. */
{
static initted = FALSE;

if (!initted)
    {
    initted = TRUE;
    oneOverSqrtTwoPi = 1.0 / sqrt(twoPi);
    logOneOverSqrtTwoPi = log(oneOverSqrtTwoPi);
    logOneFourth = log(0.25);
    logOneFifth = log(0.20);
    }
}

struct profileColumn
/* A single column of a probability profile. */
    {
    struct profileColumn *next;
    double prob[4];
    };

struct profileLocale
/* Probability distribution of where profile likes to hit. */
    {
    double mean;
    double standardDeviation;
    };

struct profile
/* A profile - each column of the profile tells you the probability of finding
 * a particular nucleotide at that position. The locale tells you the probability
 * of finding the profile at a particular position. */
    {
    struct profile *next;
    struct profileColumn *columns;
    int columnCount;
    struct profileLocale locale;
    double score;
    double bestIndividualScore;
    };

struct lm *profileLm;
struct profileColumn *colMemCache;
struct profile *profMemCache;

void initProfileMemory()
/* Initialize local pool of memory for profiles and columns. */
{
profileLm = lmInit(0);
}

void freeProfileMemory()
{
lmCleanup(&profileLm);
}


struct profileColumn *allocColumn()
/* Allocate one column. */
{
struct profileColumn *column;
if ((column = colMemCache) != NULL)
    {
    colMemCache = colMemCache->next;
    zeroBytes(column, sizeof(*column));
    return column;
    }
else
    {
    return lmAlloc(profileLm, sizeof(*column));
    }
}

struct profile *allocProfile()
/* allocate one profile. */
{
struct profile *prof;
if ((prof = profMemCache) != NULL)
    {
    profMemCache = profMemCache->next;
    zeroBytes(prof, sizeof(*prof));
    return prof;
    }
else
    {
    return lmAlloc(profileLm, sizeof(*prof));
    }
}

void freeColumn(struct profileColumn *column)
/* Free one column. */
{
slAddHead(&colMemCache, column);
}

void freeColumns(struct profileColumn *list)
/* Free list of columns. */
{
struct profileColumn *next, *el;

for (el = list; el != NULL; el = next)
    {
    next = el->next;
    freeColumn(el);
    }
}

struct profile *blankProfile(int size)
/* Return a profile that's allocated, but has little filled in. */
{
struct profile *prof;
int i;

prof = allocProfile();
prof->columnCount = size;
for (i=0; i<size; ++i)
    {
    struct profileColumn *col;
    col = allocColumn();
    slAddHead(&prof->columns, col);
    }
return prof;
}

void removeFirstColumn(struct profile *prof)
/* Remove first column from profile. */
{
struct profileColumn *col = slPopHead(&prof->columns);
if (col)
    {
    freeColumn(col);
    prof->columnCount -= 1;
    }
}

void removeLastColumn(struct profile *prof)
/* Remove last column from profile. */
{
struct profileColumn *col = slPopTail(&prof->columns);
if (col)
    {
    freeColumn(col);
    prof->columnCount -= 1;
    }
}

struct profile *profileFromTile(DNA *tile, int tileSize, int location, int seqLen)
/* Make up a profile based on tile at location in sequence of a certain length. */
{
struct profile *prof;
struct profileColumn *col;
int baseVal;
int i;

/* First check to see if there are any non-nucleotides in tile.
 * Return NULL if so. */
for (i=0; i<tileSize; ++i)
    {
    if (ntVal[tile[i]] < 0)
	return NULL;
    }

prof = blankProfile(tileSize);
prof->locale.mean = location;
prof->locale.standardDeviation = seqLen/2;
for (col = prof->columns; col != NULL; col = col->next)
    {
    baseVal = ntVal[*tile++];
    if (baseVal < 0)
        col->prob[0] = col->prob[1] = col->prob[2] = col->prob[3] = -1.386294;   /* ln(1/4) */
    else
        {
        col->prob[0] = col->prob[1] = col->prob[2] = col->prob[3] = -2.302585;    /* ln(1/10) */
        col->prob[baseVal] = -0.356675;                                           /* ln(7/10) */
        }
    }
return prof;
}

void freeProfile(struct profile **pProfile)
/* Free up dynamically allocated profile. */
{
struct profile *prof;
if ((prof = *pProfile) != NULL)
    {
    freeColumns(prof->columns);
    slAddHead(&profMemCache, prof);
    pProfile = NULL;
    }
}

void freeProfileList(struct profile **pProfList)
/* Free list of profiles. */
{
struct profile *next, *el;

for (el = *pProfList; el != NULL; el = next)
    {
    next = el->next;
    freeProfile(&el);
    }
*pProfList = NULL;
}

int cmpProfiles(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending probe offset. */
{
const struct profile *a = *((struct profile **)va);
const struct profile *b = *((struct profile **)vb);
double diff;
diff = b->score - a->score;
if (diff < 0.0)
    return -1;
else if (diff > 0.0)
    return +1;
else
    return 0;
}

#define profFileSig 0x8AC3ADC7

FILE *createProfFile(char *fileName)
/* Create a profile file and save signature. */
{
bits32 sig = profFileSig;
FILE *f = mustOpen(fileName, "wb");
writeOne(f, sig);
return f;
}

void writeProfile(FILE *f, struct profile *prof)
/* Write one profile into file. */
{
struct profileColumn *col;

assert(prof->columnCount == slCount(prof->columns));
writeOne(f, prof->columnCount);
writeOne(f, prof->locale);
writeOne(f, prof->score);
writeOne(f, prof->bestIndividualScore);
for (col = prof->columns; col != NULL; col = col->next)
    mustWrite(f, col->prob, sizeof(col->prob) );
}

struct profile *loadProfiles(char *fileName)
/* Load up all profiles in file. */
{
FILE *f = mustOpen(fileName, "rb");
struct profile *prof, *profList = NULL;
struct profileColumn *col;
bits32 sig;
struct profile p;

mustReadOne(f, sig);
if (sig != profFileSig)
    errAbort("Bad signature on %s", fileName);
for (;;)
    {
    if (!readOne(f, p.columnCount))
        break;
    prof = blankProfile(p.columnCount);
    mustReadOne(f, prof->locale);
    mustReadOne(f, prof->score);
    mustReadOne(f, prof->bestIndividualScore);
    for (col = prof->columns; col != NULL; col = col->next)
        mustRead(f, col->prob, sizeof(col->prob) );
    slAddHead(&profList, prof);
    }
fclose(f);
slReverse(&profList);
return profList;
}

void makeFreqTable(struct seqList *seqList)
/* Figure out frequency of bases in input. */
{
struct dnaSeq *seq;
int histo[4];
int oneHisto[4];
double total;
int i;

zeroBytes(histo, sizeof(histo));
for (;seqList != NULL; seqList = seqList->next)
    {
    seq = seqList->seq;
    dnaBaseHistogram(seq->dna, seq->size, oneHisto);
    for (i=0; i<4; ++i)
        histo[i] += oneHisto[i];
    }
total = histo[0] + histo[1] + histo[2] + histo[3];
freq[-1] = 1.0;
for (i=0; i<4; ++i)
    freq[i] = (double)histo[i] / total;
logFreq[-1] = 0;
for (i=0; i<4; ++i)
    logFreq[i] = log(freq[i]);
}



void makeMark1(struct seqList *seqList, double mark1[5][5], double logMark1[5][5])
/* Make up 1st order Markov model - probability that one nucleotide
 * will follow another. */
{
struct dnaSeq *seq;
DNA *dna, *endDna;
int i,j;
int histo[5][5];
int hist1[5];

zeroBytes(histo, sizeof(histo));
zeroBytes(hist1, sizeof(hist1));
for (;seqList != NULL; seqList = seqList->next)
    {
    seq = seqList->seq;
    dna = seq->dna;
    endDna = dna + seq->size-1;
    for (;dna < endDna; ++dna)
        {
        i = ntVal[dna[0]];
        j = ntVal[dna[1]];
        hist1[i+1] += 1;
        histo[i+1][j+1] += 1;
        }
    }
for (i=0; i<5; ++i)
    {
    for (j=0; j<5; ++j)
        {
        double mark1Val;
        int matVal = histo[i][j] + 1;
        mark1Val = ((double)matVal)/(hist1[i]+5);
        mark1[i][j] = mark1Val;
        logMark1[i][j] = log(mark1Val);
        }
    }
for (i=0; i<5; ++i)
    {
    mark1[i][0] = freqBase[i];
    mark1[0][i] = 1;
    logMark1[i][0] = logFreqBase[i];
    logMark1[0][i] = 0;
    }
}

void makeMark2(struct seqList *seqList, double mark2[5][5][5], double logMark2[5][5][5])
/* Make up 1st order Markov model - probability that one nucleotide
 * will follow the previous two. */
{
struct dnaSeq *seq;
DNA *dna, *endDna;
int i,j,k;
int histo[5][5][5];
int hist2[5][5];
int total = 0;

zeroBytes(histo, sizeof(histo));
zeroBytes(hist2, sizeof(hist2));
for (;seqList != NULL; seqList = seqList->next)
    {
    seq = seqList->seq;
    dna = seq->dna;
    endDna = dna + seq->size-2;
    for (;dna < endDna; ++dna)
        {
        i = ntVal[dna[0]];
        j = ntVal[dna[1]];
        k = ntVal[dna[2]];
        hist2[i+1][j+1] += 1;
        histo[i+1][j+1][k+1] += 1;
        total += 1;
        }
    }
for (i=0; i<5; ++i)
    {
    for (j=0; j<5; ++j)
        {
        for (k=0; k<5; ++k)
            {
            double markVal;
            int matVal = histo[i][j][k]+1;
            if (i == 0 || j == 0 || k == 0)
                {
                if (k == 0)
                    {
                    mark2[i][j][k] = 1;
                    logMark2[i][j][k] = 0;
                    }
                else if (j == 0)
                    {
                    mark2[i][j][k] = freqBase[k];
                    logMark2[i][j][k] = logFreqBase[k];
                    }
                else if (i == 0)
                    {
                    mark2[i][j][k] = mark1[j][k];
                    logMark2[i][j][k] = logMark1[j][k];
                    }
                }
            else
                {
                markVal = ((double)matVal)/(hist2[i][j]+5);
                mark2[i][j][k] = markVal;
                logMark2[i][j][k] = log(markVal);
                }
            }
        }
    }
}

double mark0PatProb(DNA *pat, int patSize)
/* Return probability of pattern anywhere in sequence based
 * on frequency of each base. */
{
int i;
double logProb = 0;
int baseIx;

for (i=0; i<patSize; ++i)
    {
    baseIx = ntVal[pat[i]];
    logProb += logFreq[baseIx];
    }
return logProb;
}

double mark1PatProb(DNA *dna, int offset, int patSize)
/* Return probability of pattern anywhere in sequence based
 * on first degree Markov model (probability of base in context
 * of previous base. */
{
int i;
double logProb = 0;
int startIx = offset;
int endIx = offset+patSize;
if (startIx == 0)
    {
    logProb = logFreq[ntVal[dna[startIx]]];
    startIx = 1;
    }
for (i=startIx; i<endIx; ++i)
    logProb += logMark1Prob(ntVal[dna[i-1]], ntVal[dna[i]]);
return logProb;
}

double mark2PatProb(DNA *dna, int offset, int patSize)
/* Return probability of pattern anywhere in sequence based
 * on first degree Markov model (probability of base in context
 * of previous base. */
{
int i;
double logProb = 0;
int startIx = offset;
int endIx = offset+patSize;

if (startIx == 0)
    {
    logProb += logFreq[ntVal[dna[0]]];
    startIx = 1;
    }
if (startIx == 1)
    {
    logProb += logMark1Prob(ntVal[dna[0]], ntVal[dna[1]]);
    startIx = 2;
    }
for (i=startIx; i<endIx; ++i)
    logProb += logMark2Prob(ntVal[dna[i-2]], ntVal[dna[i-1]], ntVal[dna[i]]);
return logProb;
}

double nullPatProb(DNA *dna, int offset, int patSize)
/* Return probability of the pattern according to current model. */
{
switch (nullModel)
    {
    case nmMark2:
        return mark2PatProb(dna, offset, patSize);
        break;
    case nmMark1:
        return mark1PatProb(dna, offset, patSize);
        break;
    case nmMark0:
        return mark0PatProb(dna+offset, patSize);
        break;
    case nmEven:
        return logOneFourth*patSize;
        break;
    }
}

double profilePatProb(DNA *pat, double *softMask, struct profile *prof)
/* Return log probability of pattern following profile (without
 * yet considering locale.) */
{
double logProb = 0;
struct profileColumn *col;
int baseVal;

for (col = prof->columns; col != NULL; col = col->next)
    {
    if ((baseVal = ntVal[*pat++]) >= 0)
        logProb += col->prob[baseVal] + *softMask++;
    else
        logProb += logOneFourth;
    }
return logProb;
}

struct position
/* This records the position and score of a profile match. */
    {
    struct position *next;
    struct seqList *seqEl;
    int pos;
    double score;
    double weight;
    };

struct position *posCache = NULL;  /* Hang old positions here for reuse. */

struct position *allocPosition()
/* Allocate new position, off of posCache if possible. */
{
struct position *pos = posCache;
if (pos != NULL)
    {
    posCache = pos->next;
    return pos;
    }
return lmAlloc(profileLm, sizeof(*pos) );
}

void freePosition(struct position *pos)
/* Free up position. */
{
slAddHead(&posCache, pos);
}

void freePositionList(struct position **pPosList)
/* Free up list of positions. */
{
struct position *pos, *next;
for (pos = *pPosList; pos != NULL; pos = next)
    {
    next = pos->next;
    slAddHead(&posCache, pos);
    }
*pPosList = NULL;
}

int cmpPositions(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending probe offset. */
{
const struct position *a = *((struct position **)va);
const struct position *b = *((struct position **)vb);
double diff;
diff = b->score - a->score;
if (diff < 0.0)
    return -1;
else if (diff > 0.0)
    return +1;
else
    return 0;
}

struct position *bestMatchInSeq(struct profile *prof, struct seqList *seqEl,
    int seqSize, double *locProb, double threshold)
/* Return score and position of best match to profile in sequence. */
{
int profSize = prof->columnCount;
int endIx = seqSize - profSize + 1;
struct dnaSeq *dnaSeq = seqEl->seq;
DNA *seq = dnaSeq->dna;
double *softMask = seqEl->softMask;
DNA *dna;
double logInvPos = log(1.0/(endIx));
double bestScore = -9.9E99;
int bestPos = -1;
double score;
int i;
struct position *pos;

for (i=0; i< endIx; ++i)
    {
    dna = seq+i;
    score = profilePatProb(dna, softMask+i, prof) - nullPatProb(seq, i, profSize);
    if (useLocation)
        score += log(locProb[i]) - logInvPos;
    if (score > bestScore)
        {
        bestScore = score;
        bestPos = i;
        }
    }
if (bestScore < threshold)
    return NULL;
pos = allocPosition();
pos->seqEl = seqEl;
pos->pos = bestPos;
pos->score = bestScore;
pos->weight = 0.0;
pos->next = NULL;
return pos;
}

struct position *positionsInSeq(struct profile *prof, struct seqList *seqEl, int seqSize,
    double *locProb, double threshold)
/* Return a list of positions in sequence matching profile at better than threshold levels.
 * (Set threshold to 0.0 for "better than chance". ) */
{
int profSize = prof->columnCount;
int endIx = seqSize - profSize + 1;
struct dnaSeq *dnaSeq = seqEl->seq;
DNA *seq = dnaSeq->dna;
double *softMask = seqEl->softMask;
DNA *dna;
double logInvPos = log(1.0/(endIx));
double score;
int i;
struct position *pos, *posList = NULL;

for (i=0; i< endIx; ++i)
    {
    dna = seq+i;
    score = profilePatProb(dna, softMask+i, prof) - nullPatProb(seq, i, profSize);
    if (useLocation)
        score += log(locProb[i]) - logInvPos;
    if (score > threshold)
        {
        if (posList == NULL || posList->pos + profSize <= i )
        /* Easy, non-overlapping case. */
            {
            pos = allocPosition();
            pos->seqEl = seqEl;
            pos->pos = i;
            pos->score = score;
            pos->weight = 0;
            slAddHead(&posList, pos);
            }
        else
            {
            /* In overlapping case take the best overlapping score. */
            if (score > posList->score)
                {
                posList->pos = i;
                posList->score = score;
                }
            }
        }
    }
return posList;
}

struct position *getPositions(struct profile *prof, struct seqList *seqEl, int seqSize,
    double *locProb, double threshold)
/* Get the places the profile hits in the seqList. */
{
if (maxOcc == 1)
    return bestMatchInSeq(prof, seqEl, seqSize, locProb, threshold);
else
    {
    struct position *posList, *lastPos;
    int numToTake = maxOcc;
    int i;
    posList = positionsInSeq(prof, seqEl, seqSize, locProb, threshold);
    slSort(&posList, cmpPositions);
    lastPos = posList;
    for (i=1; i<numToTake; ++i)
        {
        if (lastPos == NULL)
            break;
        lastPos = lastPos->next;
        }
    if (lastPos != NULL)
        {
        freePositionList(&lastPos->next);
        lastPos->next = NULL;
        }
    return posList;
    }
}

double scoreAtPositions(struct profile *prof,
    struct position *posList, int posOff, int seqCount)
/* Score profile given list of positions. */
{
int profSize = prof->columnCount;
int endIx = goodSeqElSize - profSize + 1;
double logInvPos = log(1.0/(endIx));
struct position *position;
struct seqList *seqEl;
struct dnaSeq *seq;
DNA *dna;
int pos;
double profScore = 0;
double oneScore;

for (position = posList; position != NULL; position = position->next)
    {
    seqEl = position->seqEl;
    pos = position->pos + posOff;
    if (pos >= 0 && pos < endIx)
        {
        seq = seqEl->seq;
        dna = seq->dna + pos;
        oneScore = profilePatProb(dna, seqEl->softMask+pos, prof)
              - nullPatProb(seq->dna, pos, profSize);
        if (useLocation)
            oneScore +=  log(locProb[pos-posOff]) - logInvPos;
        }
    else
        oneScore = 0.0;
    oneScore = log(0.5 + 0.5*exp(oneScore));
    profScore += oneScore;
    }
return profScore/seqCount;
}

double calcWeightedVariance(
    double mean,     /* Weighted mean of positions. */
    struct position *posList)
/* Figure out varience of positions from mean weighted by weights. */
{
double dif;
double var = 0.0;
struct position *pos;
for (pos = posList; pos != NULL; pos = pos->next)
    {
    dif = mean - pos->pos;
    var += dif*dif*pos->weight;
    }
return var;
}

struct profileColumn *columnAtPositions(struct position *posList, int posOffset)
/* Create a profile column based on the frequency of the bases at posOff to positions
 * in the seqList. */
{
struct profileColumn *col;
int seqSize = goodSeqElSize;
int seqCount = 0;
struct seqList *seqEl;
struct dnaSeq *seq;
DNA *dna;
struct position *position;
int pos;
int i;
int hist[4];
int baseVal;
int histTotal=0;

/* Collect occurence histogram. */
zeroBytes(hist, sizeof(hist));
for (position = posList; position != NULL; position = position->next)
    {
    seqEl = position->seqEl;
    pos = position->pos + posOffset;
    seq = seqEl->seq;
    dna = seq->dna;
    if (pos >= 0 && pos < seqSize)
        {
        baseVal = ntVal[dna[pos]];
        if (baseVal >= 0)
            hist[baseVal] += 1;
        }
    ++seqCount;
    }

/* Figure out probabilities for each base in column. */
for (i=0; i<4; ++i)
    {
    hist[i] += 1;           /* Adjust for sample being incomplete */
    histTotal += hist[i];
    }

/* If most of column positions are out of sequence, return FALSE. */
if (histTotal * 2 < seqCount)
    return NULL;

col = allocColumn();
for (i=0; i<4; ++i)
    {
    col->prob[i] = log( (double)hist[i]/histTotal);
    }
return col;
}

double iterateProfile(struct profile *prof, struct seqList *goodSeq,
    struct profile **retProfile, boolean erase)
{
double profScore = 0;
double oneScore;
int onePos;
int seqCount = slCount(goodSeq);
int seqSize = goodSeqElSize;
struct seqList *seqEl;
double accWeight = 0;
double weight;
double maxWeight = -9.9E99;
double mean = 0;
boolean saveWeight = (retProfile != NULL || erase);
struct position *posList = NULL, *pos, *newPosList;


/* Go through sequence list and collect best hit on each sequence.
 * and score it. */
makeLocProb(prof->locale.mean, prof->locale.standardDeviation, seqSize);
for (seqEl = goodSeq; seqEl != NULL; seqEl = seqEl->next)
    {
    newPosList = getPositions(prof, seqEl, seqSize, locProb, 0.0);
    for (pos = newPosList; pos != NULL; pos = pos->next)
        {
        oneScore = pos->score;
        //    oneScore = log(0.5 + 0.5*exp(oneScore));
        profScore += oneScore;
        if (saveWeight)
            {
            if (kentishWeights)
                {
                weight = oneScore;
                if (weight < 0)
                    weight = 0;
                }
            else
                weight = 1.0/(1.0 + exp(-oneScore));
            pos->weight = weight;
            accWeight += weight;
            if (maxWeight < weight)
                maxWeight = weight;
            }
        }
    posList = slCat(newPosList, posList);
    }
profScore /= seqCount;

if (erase)
/* Probabalistically erase hits. */
    {
    int startPos, endPos;
    double *softMask;
    for (pos = posList; pos != NULL; pos = pos->next)
        {
        seqEl = pos->seqEl;
        startPos = pos->pos;
        weight = 1.0 - pos->weight/maxWeight;
        if (weight <= 0.0000001)
            weight = 0.000001;
        weight = log(weight);
        softMask = seqEl->softMask;
        endPos = startPos + prof->columnCount;
        if (endPos > seqSize)
            endPos = seqSize;
        if (startPos < 0)
            startPos = 0;
        for (; startPos < endPos; ++startPos)
            {
            softMask[startPos] += weight;
            }
        }
    }
else if (retProfile != NULL)
/* Generate next generation profile if that's what they want. */
    {
    int profSize = prof->columnCount;
    struct profileColumn *col;
    struct profile *newProf = blankProfile(profSize);
    struct dnaSeq *seq;
    DNA *dna;
    int hitIx = 0;
    double minSd = 1.0;
    double sd;
    int shiftLeftEnd = 0, shiftRightEnd = 0;
    double newProfScore;
    struct position *pos;
    double expandConstrainFactor = constrainer/seqCount;    /* Reduce urge to expand. */

    /* Go through hits and use them to generate next generation
     * of profile. */
    for (pos = posList; pos != NULL; pos = pos->next)
        {
        onePos = pos->pos;
        seqEl = pos->seqEl;
        weight = (pos->weight /= accWeight);
        seq = seqEl->seq;
        dna = seq->dna + onePos;
        for (col = newProf->columns; col != NULL; col = col->next)
            {
            int baseVal = ntVal[*dna++];
            if (baseVal < 0)    /* Matching an N - distribute weight evenly. */
                {
                int i;
                double w4 = weight/4;
                for (i=0; i<4; ++i)
                    col->prob[i] += w4;
                }
            else                /* Concentrate weight on specific match. */
                {
                double w99 = weight * 0.99;
                double wLeft = (weight - w99)/3;
                int i;
                for (i=0; i<4; ++i)
                    col->prob[i] += (i == baseVal ? w99 : wLeft);
                }
            }
        mean += weight * onePos;
        }

    /* Convert profile columns to log form. */
    for (col = newProf->columns; col != NULL; col = col->next)
        {
        int i;
        for (i=0; i<4; ++i)
            {
            double val = col->prob[i];
            col->prob[i] = log(val);
            }
        }

    newProf->locale.mean = mean;
    sd = sqrt(calcWeightedVariance(mean, posList));
    if (sd< minSd)
        sd= minSd;
    newProf->locale.standardDeviation = sd;

    /* Get current score of new profile. */
    profScore = scoreAtPositions(newProf, posList, 0, seqCount);

    /* See if profile improves by subtracting column from left */
    if ((col = slPopHead(&newProf->columns)) != NULL)
        {
        newProf->columnCount -= 1;
        newProfScore = scoreAtPositions(newProf, posList, 1, seqCount);
        if (newProfScore > profScore)
            {
            newProf->locale.mean += 1.0;
            shiftLeftEnd = 1;
            profScore = newProfScore;
            freeColumn(col);
            }
        else
            {
            slAddHead(&newProf->columns, col);
            newProf->columnCount += 1;
            }
        }

    /* See if profile improves by subtracting column to right */
    if ((col = slPopTail(&newProf->columns)) != NULL)
        {
        newProf->columnCount -= 1;
        newProfScore = scoreAtPositions(newProf, posList, shiftLeftEnd, seqCount);
        if (newProfScore > profScore)
            {
            shiftRightEnd = -1;
            profScore = newProfScore;
            freeColumn(col);
            }
        else
            {
            slAddTail(&newProf->columns, col);
            newProf->columnCount += 1;
            }
        }

    /* See if profile improves by adding a column to the left */
    if (!shiftLeftEnd)
        {
        col = columnAtPositions(posList, -1);
        if (col != NULL)
            {
            slAddHead(&newProf->columns, col);
            newProf->columnCount += 1;
            newProfScore = scoreAtPositions(newProf, posList, -1, seqCount);
            if (newProfScore > profScore + expandConstrainFactor)
                {
                newProf->locale.mean -= 1.0;
                shiftLeftEnd = -1;
                profScore = newProfScore;
                }
            else
                {
                removeFirstColumn(newProf);
                }
            }
        }
    /* See if profile improves by adding a column to the right */
    if (!shiftRightEnd)
        {
        col = columnAtPositions(posList, profSize);
        if (col != NULL)
            {
            slAddTail(&newProf->columns, col);
            newProf->columnCount += 1;
            newProfScore = scoreAtPositions(newProf, posList, shiftLeftEnd, seqCount);
            if (newProfScore > profScore + expandConstrainFactor)
                {
                shiftRightEnd = +1;
                profScore = newProfScore;
                }
            else
                {
                removeLastColumn(newProf);
                }
            }
        }
    newProf->score = profScore;
    *retProfile = newProf;
    }
freePositionList(&posList);
return profScore;
}

void maskProfileFromSeqList(struct profile *profile, struct seqList *seqList)
/* Skew the soft mask of the seqList so that things the profile matches will
 * become unlikely to be matched by something else. */
{
iterateProfile(profile, seqList,  NULL, TRUE);
}

struct profile *findStartProfiles(struct seqList *goodSeq, int scanLimit)
/* Find the most promising profiles by considering ones made up of all starting
 * positions run for one EM iteration. */
{
struct profile *profileList = NULL, *profile;
struct seqList *seqEl;
struct dnaSeq *seq;
DNA *start, *dna, *end;
int tileSize = defaultTileSize;
int seqSize;
int progTime = 0;

progress("Searching for starting profiles");
for (seqEl = goodSeq; seqEl != NULL; seqEl = seqEl->next)
    {
    if (--scanLimit < 0)
        break;
    seq = seqEl->seq;
    seqSize = seq->size;
    start = seq->dna;
    end = start + seq->size - tileSize;
    for (dna = start; dna <= end; dna++)
        {
        if ((profile = profileFromTile(dna, tileSize, dna-start, seqSize)) != NULL)
	    {
	    profile->score = iterateProfile(profile, goodSeq, NULL, FALSE);
	    slAddHead(&profileList, profile);
	    if (++progTime >= 50)
		{
		progress(".");
		progTime = 0;
		}
	    }
        }
    }
progress("\n");
htmlHorizontalLine();
return profileList;
}


char *consensusSeq(struct profile *prof)
/* Returns a consensus sequence made up of the most popular nucleotide
 * in each position of the profile. */
{
static char buf[100];
struct profileColumn *col;
int consIx = 0;

assert(prof->columnCount < sizeof(buf));
for (col = prof->columns; col != NULL; col = col->next)
    {
    int i;
    int bestVal = -1;
    double bestProb = -99E99;
    for (i=0; i<4; ++i)
        {
        if (col->prob[i] > bestProb)
            {
            bestProb = col->prob[i];
            bestVal = i;
            }
        }
    buf[consIx++] = valToNt[bestVal];
    }
buf[consIx] = 0;
return buf;
}

void printProfile(FILE *f, struct profile *prof)
/* Display salient facts about a profile. */
{
struct profileColumn *col;
int i;
char *consSeq = consensusSeq(prof);

fprintf(f, "%5.4f @ %4.2f sd %4.2f ", prof->score, prof->locale.mean, prof->locale.standardDeviation);
touppers(consSeq);
fprintf(f, "%s\n", consSeq);
for (i = 0; i<4; ++i)
    {
    fprintf(f, "\t%c  ", valToNt[i]);
    for (col = prof->columns; col != NULL; col = col->next)
        {
        fprintf(f, "%4.3f ", exp(col->prob[i]) );
        }
    fprintf(f, "\n");
    }
}

void readPremadeBg(char *backgroundName)
/* Read in a premade background.  If a precalculated copy exists
 * use it, otherwise do calculations and save them for next time. */
{
FILE *f;
bits32 sig;
char bgName[512];
struct seqList *bgSeq;
int suffixPos = strlen(backgroundName);
strcpy(bgName, backgroundName);
strcpy(bgName+suffixPos, ".lm2");
if ((f = fopen(bgName, "rb")) == NULL)
    {
    strcpy(bgName+suffixPos, ".fa");
    bgSeq = readSeqList(bgName);
    makeFreqTable(bgSeq);
    makeMark1(bgSeq, mark1, logMark1);
    makeMark2(bgSeq, mark2, logMark2);
    strcpy(bgName+suffixPos, ".mk2");
    f = mustOpen(bgName, "wb");
    sig = lm2Signature;
    writeOne(f, sig);
    mustWrite(f, freqBase, sizeof(freqBase));
    mustWrite(f, logFreqBase, sizeof(logFreqBase));
    mustWrite(f, mark1, sizeof(mark1));
    mustWrite(f, logMark1, sizeof(logMark1));
    mustWrite(f, mark2, sizeof(mark2));
    mustWrite(f, logMark2, sizeof(logMark2));
    }
else
    {
    mustReadOne(f, sig);
    if (sig != lm2Signature)
        errAbort("Bad signature on %s", bgName);
    mustRead(f, freqBase, sizeof(freqBase));
    mustRead(f, logFreqBase, sizeof(logFreqBase));
    mustRead(f, mark1, sizeof(mark1));
    mustRead(f, logMark1, sizeof(logMark1));
    mustRead(f, mark2, sizeof(mark2));
    mustRead(f, logMark2, sizeof(logMark2));
    }
fclose(f);
}

void getNullModel(char *goodName, char *badName, boolean premadeBg)
/* Make null (background) model from bad if it exists otherwise from good. */
{
if (premadeBg)
    {
    readPremadeBg(badName);
    }
else
    {
    struct seqList *bgSeq;
    char *seqName = badName;
    if (seqName == NULL)
        seqName = goodName;
    bgSeq = readSeqList(seqName);
    makeFreqTable(bgSeq);
    makeMark1(bgSeq, mark1, logMark1);
    if (nullModel == nmMark2)
        makeMark2(bgSeq, mark2, logMark2);
    freeSeqList(&bgSeq);
    }
}


void showProfHits(struct profile *prof, struct seqList *seqList, double *retBestScore)
/* Display profile in context of goodSeq. While we're scanning through also
 * return best score of profile on any sequence. */
{
struct seqList *el;
struct dnaSeq *seq;
double bestScore = 0.0;
double score;
double totalScore;
int seqSize = seqList->seq->size;
DNA *uppered = needMem(seqSize + 1);
struct position *pos, *posList;

makeLocProb(prof->locale.mean, prof->locale.standardDeviation, goodSeqElSize);
for (el = seqList; el != NULL; el = el->next)
    {
    seq = el->seq;
    memcpy(uppered, seq->dna, seqSize);
    uppered[seqSize] = 0;
    posList = getPositions(prof, el, seq->size, locProb, 0.0);
    totalScore = 0;
    for (pos = posList; pos != NULL; pos = pos->next)
        {
        score = pos->score;
        if (score > 0)
            toUpperN(uppered + pos->pos, prof->columnCount);
        if (score < 0) score = 0;
        totalScore += score;
        if (score > bestScore)
            bestScore = score;
        }
    freePositionList(&posList);
    printf("%5.2f %-*s %s\n", totalScore, goodSeqNameSize, seq->name, uppered);
    }
freeMem(uppered);
*retBestScore = bestScore;
}

long hashProfile(struct profile *prof)
/* Return a double that's close to unique for the profile. */
{
double acc[4];
struct profileColumn *col;
double retDoub = 0;
int i;
for (i=0; i<4; ++i)
    acc[i] = 0;
for (col = prof->columns; col != NULL; col = col->next)
    {
    for (i=0; i<4; ++i)
        {
        acc[i] *= 1.1;
        acc[i] += col->prob[i];
        }
    }
for (i=0; i<4; ++i)
    {
    retDoub *= 1.3;
    retDoub += acc[i];
    }
return ((long)(retDoub*100  + prof->locale.mean + prof->locale.standardDeviation ));
}

struct profile *convergeProfile(struct profile *initProf)
/* Iterate profile until it converges up to 30 times. */
{
int i;
int maxIterations = 35;
long profHash = 0, lastHash = 0;
double score = -99e99, lastScore;
struct profile *newProf;
struct profile *prof = initProf;

for (i=0; i<maxIterations; ++i)
    {
    lastScore = score;
    lastHash = profHash;
    score = iterateProfile(prof, goodSeq,  &newProf, FALSE);
    if (prof != initProf)
        freeProfile(&prof);
    prof = newProf;
    profHash = hashProfile(prof);
    if (fabs(score-lastScore) < 0.00001 && fabs(profHash - lastHash) < 0.1)
        break;
    }
return prof;
}

void doTopTiles(int repCount, char *profFileName, FILE *logFile)
/* Get top looking tile, converge it, save it, print it, erase it, and repeat. */
{
int repIx;
FILE *f = createProfFile(profFileName);
struct profile *bestProf;

for (repIx = 0; repIx < repCount; ++repIx)
    {
    struct profile *profList;
    struct profile *newList = NULL;
    struct profile *prof, *newProf;
    int maxCount = 35;

    progress("Looking for motif #%d\n", repIx+1);
    profList = findStartProfiles(goodSeq, startScanLimit);
    slSort(&profList, cmpProfiles);
    progress("Converging");
    for (prof = profList; prof != NULL; prof = prof->next)
        {
        newProf = convergeProfile(prof);
        slAddHead(&newList, newProf);
        if (--maxCount <= 0)
            break;
	progress(".");
        }
    progress("\n");
    slSort(&newList, cmpProfiles);
    bestProf = newList;
    printProfile(stdout,bestProf);
    if (logFile)
        printProfile(logFile, bestProf);
    htmlHorizontalLine();
    showProfHits(bestProf, goodSeq, &bestProf->bestIndividualScore);
    writeProfile(f, bestProf);
    htmlHorizontalLine();
    maskProfileFromSeqList(bestProf, goodSeq);
    freeProfileList(&profList);
    freeProfileList(&newList);
    }
fclose(f);
}

void pasteToFa(char *varName, char **retFileName, int *retSeqCount, int *retLongestLen)
/* Returns a (temporary) fa file made from the contents of the CGI variable
 * varName. */
{
static struct tempName tn;
FILE *f;
char *lines[1024];
int lineCount;
char *rawGood;
char *cleanSeq;
int i;
int lineIx = 0;
char *goodName = NULL;
int seqCount = 0;
int longestLen = 0;
int oneLen;

*retFileName = NULL;
*retSeqCount = 0;
rawGood = cgiString(varName);
makeTempName(&tn, "imp", ".fa");
goodName = cloneString(tn.forCgi);
lineCount = chopString(rawGood, "\r\n", lines, ArraySize(lines));
if (lineCount <= 0)
    return;
f = mustOpen(goodName, "w");
if (lines[0][0] == '>') /* Looks like an FA file - just copy it to file. */
    {
    for (i=0; i<lineCount; ++i)
        {
        if (lines[i][0] == '>')
            {
            ++seqCount;
            oneLen = 0;
            }
        else
            {
            oneLen += strlen(lines[i])-1;
            if (oneLen > longestLen)
                longestLen = oneLen;
            }
        fprintf(f, "%s\n", lines[i]);
        }
    }
else
    {
    for (i=0; i<lineCount; ++i)
        {
        cleanSeq = lines[i];
        dnaFilter(cleanSeq, cleanSeq);
        if (cleanSeq[0] == 0)
            continue;
        ++lineIx;
        ++seqCount;
        oneLen = strlen(lines[i]);
        if (oneLen > longestLen)
            longestLen = oneLen;
        fprintf(f, ">%d\n%s\n", lineIx, cleanSeq);
        }
    }
*retFileName = goodName;
*retSeqCount = seqCount;
*retLongestLen = longestLen;
fclose(f);
}

struct rgbColor distinctColors[] = {
/* A bunch of colors that should appear relatively distinct from each other. */
    { 0, 0, 0 },
    { 0, 0, 255},
    { 255, 0, 0},
    { 0, 200, 0},
    { 128, 0, 255},
    { 255, 128, 0},
    { 0, 128, 255},
    { 255, 0, 255},
    { 0, 255, 255},
    { 255, 255, 0},
};

struct rgbColor textBgColor = {226, 226, 226};
struct rgbColor gfxBgColor = {255, 255, 255};
struct rgbColor gfxLineColor = {190, 190, 190};

void blendColors(struct rgbColor a, struct rgbColor b, double mix, struct rgbColor *retBlend)
/* Return a blend of a and b. Mix should be between 0 and 1. Mix of 0 means blend is pure
 * color a. */
{
double invMix;

invMix = 1.0 - mix;
retBlend->r = round( a.r*invMix + b.r*mix);
retBlend->g = round( a.g*invMix + b.g*mix);
retBlend->b = round( a.b*invMix + b.b*mix);
}


void setFontColor(struct rgbColor color)
/* Set the font color for html. */
{
static struct rgbColor lastColor = {0,0,0};
static boolean firstTextColorSwitch = TRUE;


if (color.r != lastColor.r || color.b != lastColor.b || color.g != lastColor.g)
    {
    if (firstTextColorSwitch)
        firstTextColorSwitch = FALSE;
    else
        printf("</span>");
    printf("<span style='color:#%02X%02X%02X;'>",color.r, color.g, color.b);
    lastColor = color;
    }
}

void blackText()
/* Set the font color for html to black. */
{
static struct rgbColor black = {0,0,0};
setFontColor(black);
}

void colorTextOut(char c, struct rgbColor color)
/* Print out one character in color to html file. */
{
setFontColor(color);
fputc(c, stdout);
}

void colorProfile(char *profFileName, struct seqList *seqList, int seqSize)
/* Display profile in color on sequences. */
{
struct profile *profList = loadProfiles(profFileName);
int profCount = slCount(profList);
struct rgbColor *colors = needMem(seqSize * sizeof(colors[0]));
int colorIx = 0;
int colCount;
struct seqList *seqEl;
struct dnaSeq *seq;
struct profile *prof;
DNA *dna;
double *softMask;
struct rgbColor blend;
int i,j;
int start, end;
double mix;
int seqCount = slCount(seqList);
int border = 1;
int barHeight = 5;
int barMidOff = barHeight/2;
struct memGfx *mg;
int pixWidth = 600;
int insideWidth = pixWidth-2*border;
int xoff = border;
int yoff = border;
int pixHeight = seqCount * (barHeight+border) + border;
int colorsPer = 16;
int coff;
Color mappedCol;
int x1, x2;
struct tempName gifTn;

printf("<H3>Color Coding for Profiles</H3>\n");
for (colorIx = 0, prof = profList; prof != NULL; prof = prof->next)
    {
    setFontColor(distinctColors[colorIx]);
    printProfile(stdout, prof);
    colorIx = (colorIx+1)%ArraySize(distinctColors);
    };
blackText();
htmlHorizontalLine();

printf("<H3>Colored Text View of Profiles</H3>\n");
printf("Different colors represent different profiles. Darker colors\n"
       "represent stronger matches to profile.\n\n");

/* Want to draw most significant last. */
slReverse(&profList);

/* Allocate something to draw on and set the color map so have shades of
 * each color. */
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);
coff = mg->colorsUsed;
colCount = slCount(profList);
if (colCount > ArraySize(distinctColors)) colCount = ArraySize(distinctColors);
for (i=0; i<colCount; ++i)
    {
    for (j=0; j<colorsPer; ++j)
        {
        blendColors(gfxBgColor, distinctColors[i], (double)j/(colorsPer-1), &blend);
        mg->colorMap[mg->colorsUsed++] = blend;
        }
    }

for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    mgDrawBox(mg, xoff, yoff+barMidOff, insideWidth, 1, MG_GRAY);
    softMask = seqEl->softMask;
    for (i=0; i<seqSize; ++i)
        softMask[i] = 0.0;

    for (i=0; i<seqSize; ++i)
        colors[i] = textBgColor;
    seq = seqEl->seq;
    dna = seq->dna;
    colorIx = (profCount-1)%ArraySize(distinctColors);
    for (prof = profList; prof != NULL; prof = prof->next)
        {
        if (prof->bestIndividualScore > 0)
            {
            struct position *pos, *posList = NULL;
            makeLocProb(prof->locale.mean, prof->locale.standardDeviation, seqSize);
            posList = getPositions(prof, seqEl, seqSize, locProb, 0.0);
            for (pos = posList; pos != NULL; pos = pos->next)
                {
                /* Colored text. */
                start = pos->pos;
                end = start + prof->columnCount;
                if (end > seqSize)
                    end = seqSize;
                mix = pos->score/prof->bestIndividualScore;
                if (mix > 1.0) mix = 1.0;
                else if (mix < 0.0) mix = 0.0;
                blendColors(textBgColor, distinctColors[colorIx], mix, &blend);
                for (i=start; i < end; ++i)
                    colors[i] = blend;

                /* Make colored pic. */
                x1 = xoff + round((double)start*insideWidth/seqSize);
                x2 = xoff + round((double)end*insideWidth/seqSize);
                mappedCol = coff + colorsPer*colorIx + round(mix*(colorsPer-1));
                mgDrawBox(mg, x1, yoff, x2-x1, barHeight, mappedCol);
                }
            freePositionList(&posList);
            if (--colorIx < 0)
                colorIx = ArraySize(distinctColors)-1;
            }
        }
    blackText();
    printf("%-*s ", goodSeqNameSize, seq->name);
    for (i=0; i<seqSize; ++i)
        {
        colorTextOut(dna[i], colors[i]);
        }
    yoff += barHeight + border;
    printf("\n");
    }

/* Tell .html where the gif is. */
htmlHorizontalLine();
blackText();
printf("<H3>Graphical Summary of Profile Hits</H3>\n");
printf("Colors represent different profiles. Darker colors represent\n"
       "stronger matches to profile.\n");
makeTempName(&gifTn, "imp", ".gif");
mgSaveGif(mg, gifTn.forCgi, FALSE);
mgFree(&mg);
printf("<IMG SRC=\"%s\" WIDTH=%d HEIGHT=%d BORDER=0>\n",
    gifTn.forCgi, pixWidth, pixHeight);

freeMem(colors);
freeProfileList(&profList);
}

int maxSeqNameSize(struct seqList *seqList)
/* Return length of longest name in sequence. */
{
struct seqList *seqEl;
struct dnaSeq *seq;
int bestLen = 0;
int len;

for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    seq = seqEl->seq;
    len = strlen(seq->name);
    if (len > bestLen)
        bestLen = len;
    }
return bestLen;
}

void makePremadeBgPathName(char *fileName, char *retPathName, int retPathSize)
/* Make path name for background file out of just file name. */
{
char *jkwebDir;
if ((jkwebDir = getenv("JKWEB")) == NULL)
    jkwebDir = "";
sprintf(retPathName, "%sameme.dir", jkwebDir);
firstWordInFile(retPathName, retPathName, retPathSize);
strcat(retPathName, fileName);
}

static double randScale;    /* Scales rand() result to 0.0 to 1.0 */

double normRand()
/* Return a random number between 0 and 1 */
{
return rand() * randScale;
}

void initRandom()
/* Initialize random number generator */
{
/* Set up random number generator. */
srand( (unsigned)time( NULL ) );
randScale = 1.0/RAND_MAX;
}

int randomNucleotide(double *probTable)
/* Generate a random nucleotide based on a log-probability table. */
{
double r = normRand();
int i;
double acc = 0;
for (i=1; i<5; ++i)
    {
    acc += probTable[i];
    if (r <= acc)
        {
        return i-1;
        }
    }
return 0;
}

DNA ntFromVal(int val)
/* Return nucleotide corresponding to val. */
{
if (val < 0)
    return 'n';
else
    return valToNt[val];
}

void generateOneSeq(FILE *f, int num, int seqLen, double even[5], double mark0[5], double mark1[5][5], double mark2[5][5][5])
/* Generate one sequence into FA file. */
{
int i;

fprintf(f, ">%d\n", num);
switch (nullModel)
    {
    case nmMark2:
        {
        int dnaVal, lastDnaVal, lastLastDnaVal;
        if (seqLen > 0)
            {
            lastDnaVal = randomNucleotide(mark0);
            fputc(ntFromVal(lastDnaVal), f);
            }
        if (seqLen > 1)
            {
            dnaVal = randomNucleotide(&(mark1[lastDnaVal+1][0]) );
            fputc(ntFromVal(dnaVal), f);
            }
        for (i=2; i<seqLen; ++i)
            {
            lastLastDnaVal = lastDnaVal;
            lastDnaVal = dnaVal;
            dnaVal = randomNucleotide(&(mark2[lastLastDnaVal+1][lastDnaVal+1][0]) );
            fputc(ntFromVal(dnaVal), f);
            }
        fputc('\n', f);
        }
        break;
    case nmMark1:
        {
        int dnaVal, lastDnaVal;
        dnaVal = randomNucleotide(mark0);
        if (seqLen > 0)
            fputc(ntFromVal(dnaVal), f);
        for (i=1; i<seqLen; ++i)
            {
            lastDnaVal = dnaVal;
            dnaVal = randomNucleotide(&(mark1[lastDnaVal+1][0]) );
            fputc(ntFromVal(dnaVal), f);
            }
        fputc('\n', f);
        }
        break;
    case nmMark0:
    case nmEven:
        {
        double *mod = (nullModel == nmEven ? even : mark0 );
        for (i=0; i<seqLen; ++i)
            fputc(ntFromVal(randomNucleotide(mod)), f);
        }
        break;
    }
}

static double evenProb[5] = {1.0, 0.25, 0.25, 0.25, 0.25};

void generate(char *faFileName, int numSeq, int lenSeq)
/* Generate a fa file containing numSeq sequences of lenSeq bases */
{
FILE *f = mustOpen(faFileName, "w");
int i;

for (i=0; i<numSeq; ++i)
    {
    generateOneSeq(f, i, lenSeq, evenProb, freqBase, mark1, mark2);
    }
fclose(f);
}

char *randomSpoof(char *realName)
/* Make an FA file that contains sequences identical in length and number
 * to those in the FA file realName, but whose bases are randomly generate
 * according to the null model. Returns name of spoof file. */
{
struct seqList *seqList, *seqEl;
static struct tempName tn;
FILE *f;
int num = 0;

seqList = readSeqList(realName);
makeTempName(&tn, "con", ".fa");
f = mustOpen(tn.forCgi, "w");
for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    generateOneSeq(f, ++num, seqEl->seq->size, evenProb, freqBase, mark1, mark2);
    }
fclose(f);
freeSeqList(&seqList);
return tn.forCgi;
}

void oneSearchSet(char *badName, boolean premade, FILE *logFile)
/* Do one set of motif searches */
{
struct tempName tn;
char *backgroundName;

goodSeq = readSeqList(goodName);
goodSeqListSize = slCount(goodSeq);
goodSeqElSize = uniformSeqSize(goodSeq, leftAlign);
goodSeqNameSize = maxSeqNameSize(goodSeq);

makeTempName(&tn, "imp", ".pfl");

printf("<P>Looking for %d motifs in %d sequences. Longest sequence is %d bases.</P>\n",
    numMotifs, goodSeqListSize, goodSeqElSize);
printf("<P>Settings are %s location; %d occurrences per sequence; %s align; ",
    (useLocation ? "use" : "ignore"), maxOcc,
    (leftAlign ? "left" : "right") );
printf("training weights %s; initial motif size %d; ",
    (kentishWeights ? "Kentish" : "classical"), defaultTileSize);
printf("restrain expansionist tendencies %f;  number of sequences in initial scan %d; ",
    constrainer, startScanLimit);
backgroundName = bgSource;
if (backgroundName == NULL)
    backgroundName = badName;
if (backgroundName == NULL)
    backgroundName = "same as foreground";
printf("background model %s; background data %s;</P>",
    (nullModelCgiName == NULL ? "Markov 0" : nullModelCgiName),
    backgroundName);

printf("<TT><PRE>\n");
progress("This run would take about %2.1f minutes on a lightly loaded UCSC CSE web server.",
    calcApproximateTime());
htmlHorizontalLine();

doTopTiles(numMotifs, tn.forCgi, logFile);

colorProfile(tn.forCgi, goodSeq, goodSeqElSize);
printf("</TT></PRE>\n");

freeSeqList(&goodSeq);
}

void doRandomTest(char *badName, boolean premade)
/* Generate tables for scores on random sequences. */
{
int seqLen, seqCount;
struct tempName randTn, profTn;
FILE *logFile = mustOpen("\\temp\\random.txt", "w");
int reps = 2;
int i;

makeTempName(&randTn, "rand", ".fa");
makeTempName(&profTn, "rand", ".pfl");

printf("<TT><PRE>\n");
for (seqLen = 100; seqLen <= 500; seqLen += 100)
    for (seqCount = 100; seqCount <= 100; seqCount += 10)
        {
        goodName = randTn.forCgi;
        for (i=1; i<=reps; ++i)
            {
            fprintf(logFile, "----------------- %d Random sequence of %d nucleotides take %d----------------\n",
                seqCount, seqLen, i);
            generate(goodName, seqCount, seqLen);
            oneSearchSet(badName, premade, logFile);
            }
        }
fclose(logFile);
}


void doMiddle()
/* Generate middle part of html file. In this case just read all the cgi variables
 * and then call routine to actually process. */
{
char *nullModelCgi = "background";
char *badName = NULL;
char badNameBuf[512];
boolean premade = FALSE;
boolean isRandomTest = cgiBoolean("randomTest");
boolean isControlRun = cgiBoolean("controlRun");
long startTime = clock1000(), endTime;

printf("<H2>Improbizer Results</H2>\n");
leftAlign = cgiBoolean("leftAlign");
if (cgiVarExists(nullModelCgi))
    {
    nullModelCgiName = cgiEncode(cgiString(nullModelCgi));
    nullModel = cgiOneChoice(nullModelCgi, nullModelChoices, ArraySize(nullModelChoices));
    }
if (cgiVarExists("maxOcc"))
    maxOcc = cgiInt("maxOcc");
if (cgiVarExists("tileSize"))
    defaultTileSize = cgiInt("tileSize");
if (cgiVarExists("startScanLimit"))
    startScanLimit = cgiInt("startScanLimit");
if (cgiVarExists("numMotifs"))
    numMotifs = cgiInt("numMotifs");
if (cgiVarExists("constrainer"))
    constrainer = cgiDouble("constrainer");
if (cgiVarExists("trainingWeights"))
    {
    char *tw = cgiString("trainingWeights");
    if (sameWord(tw, "Kentish"))
        kentishWeights = TRUE;
    else if (sameWord(tw, "classical"))
        kentishWeights = FALSE;
    else
        errAbort("Unknown trainingWeights %s", tw);
    }
useLocation = !cgiBoolean("ignoreLocation");
if (!isRandomTest)
    {
    if (cgiVarExists("goodText"))
        {
        pasteToFa("goodText", &goodName, &goodSeqListSize, &goodSeqElSize);
        if (goodSeqListSize <= 0)
            errAbort("You need to paste in something. Go back and try again!");
        }
    if (goodName == NULL)
        goodName = cgiString("good");
    }
if (cgiVarExists("badText"))
    {
    int numSeq, elSize;
    pasteToFa("badText", &badName, &numSeq, &elSize);
    if (numSeq <= 0)
        badName = NULL;
    }
if (badName == NULL)
    badName = cgiOptionalString("bad");

/* If they selected a premade background, figure out file that goes with it,
 * then look up directory to find file in. */
if ((bgSource = cgiOptionalString("backgroundDataSource")) != NULL)
    {
    char *premadeBg = NULL;

    if (sameString(bgSource, "Worm Intron 3'"))
        premadeBg = "wormInt3";
    else if (sameString(bgSource, "Worm Intron 5'"))
        premadeBg = "wormInt5";
    else if (sameString(bgSource, "Yeast Promoter"))
        premadeBg = "yeastPromo";
    else if (sameString(bgSource, "Same as Foreground"))
        ;
    else if (sameString(bgSource, "From Data Pasted Below"))
        ;
    else
       errAbort("Unknown backgroundDataSource");

    if (premadeBg != NULL)
        {
        makePremadeBgPathName(premadeBg, badNameBuf, sizeof(badNameBuf));
        badName = badNameBuf;
        premade = TRUE;
        }
    }
getNullModel(goodName, badName, premade);
if (isControlRun)
    goodName = randomSpoof(goodName);
if (isRandomTest )
    {
    puts("<P>Random test mode - this will take a good long time.  Be sure to kill "
         "the process if you get impatient.</P>\n");
    doRandomTest(badName, premade);
    }
else
    {
    if (isFromWeb && calcApproximateTime() > 5.0)
        {
        errAbort("Sorry, this job is too big for our web server - it would use about "
               "%2.1f minutes of CPU time. Out of fairness to the other users of this "
               "machine we limit jobs to 5.0 minutes of CPU time or less.  Please reduce "
               "the size of your data (now %d sequences of %d bases each), "
               "the number of motifs you're looking "
               "for (now %d), or the number of sequences in the initial scan (now %d). "
               "The most important influence on run time is the maximum size of an individual sequence. "
               " If you really need "
               "to run the program on a data set this large contact Jim Kent (kent@biology.ucsc.edu) "
               "to get a batch version of this program to run on your own machine.", calcApproximateTime(),
               goodSeqListSize, goodSeqElSize,
               numMotifs, startScanLimit
               );
        }
    puts("<P>Improbizer will display the results in parts.  First it will "
         "display the profiles (consensus sequences with the probability of "
         "each base at each position) individually as they are calculated. The "
         "position of a profile in a sequence is indicated by upper case. The "
         "strength of the profile match is indicated by the score on the left. "
         "There will be a delay during this phase as each profile is calculated. "
         "Second Improbizer will "
         "display all profiles at one over each sequence. Each profile "
         "has it's own color and the stronger the profile matches the darker "
         "it will appear in the sequence. Finally there will be a graphic "
         "summary of all the profiles at the end, using the same color "
         "conventions.</P>");
    oneSearchSet(badName, premade, NULL);
    endTime = clock1000();
    htmlHorizontalLine();
    printf("Calculations took %4.3f minutes\n", (endTime-startTime)*0.001/60);
    }
}

int main(int argc, char *argv[])
{
// pushCarefulMemHandler();
initProfileMemory();
dnaUtilOpen();
statUtilOpen();
initRandom();

isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    {
    errAbort("ameme - find common patterns in DNA\n"
             "usage\n"
             "    ameme good=goodIn.fa [bad=badIn.fa] [numMotifs=2] [background=m1] [maxOcc=2]\n"
             "where goodIn.fa is a multi-sequence fa file containing instances\n"
	     "of the motif you want to find, badIn.fa is a file containing similar\n"
	     "sequences but lacking the motif, numMotifs is the number of motifs\n"
	     "to scan for, background is m0,m1, or m2 for various levels of Markov\n"
	     "models, and maxOcc is the maximum occurrences of the motif you \n"
	     "expect to find in a single sequence\n");
    }

/* Print out html header.  Make background color brilliant white. */
puts("Content-Type:text/html\n");
printf("<HEAD>\n<TITLE>%s</TITLE>\n</HEAD>\n\n", "Improbizer Results");
puts("<BODY BGCOLOR='#FFFFFF'>\n");

/* Wrap error handling et. around doMiddle. */
htmEmptyShell(doMiddle, NULL);

// carefulCheckHeap();

/* Write end of html. */
htmlEnd();
return 0;
}
