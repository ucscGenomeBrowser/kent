/* ameme - The central part of AmphetaMeme, which finds patterns in DNA */
#include "common.h"
#include "memalloc.h"
#include "errabort.h"
#include "obscure.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "localmem.h"
#include "sig.h"
#include "fa.h"
#include "portable.h"
#include "codebias.h"
#include "memgfx.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "slog.h"
#include "dnaMarkov.h"
#include "ameme.h"
#include "fragFind.h"
#include "linefile.h"
#include "dystring.h"
#include "dnaMotif.h"

boolean isFromWeb;          /* True if run as CGI. */
boolean isMotifMatcher;     /* True if run from motifMatcher.html. */
boolean outputLogo;	    /* True if want sequence logo output. */
FILE *htmlOut;		    /* Where to send output. */




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

void horizontalLine()
/* Make horizontal line across html. */
{
fprintf(htmlOut, "<P><HR ALIGN=\"CENTER\"></P>");
}

char *amemeDir()
/* Return directory name ameme files are in. */
{
static char dir[512];
static boolean initted = FALSE;
if (!initted)
    {
    char *jkwebDir;
    initted = TRUE;
    if ((jkwebDir = getenv("JKWEB")) == NULL)
        jkwebDir = "";
    snprintf(dir, sizeof(dir), "%sameme.dir", jkwebDir);
    firstWordInFile(dir, dir, sizeof(dir));
    }
return dir;
}


enum nullModel
    {
    nmMark2,
    nmMark1,
    nmMark0,
    nmCoding,
    nmEven,
    };

struct cgiChoice nullModelChoices[] =
/* The different choices for the NULL model. */
    {
        {"Markov 2", nmMark2,},
        {"Markov 1", nmMark1,},
        {"Markov 0", nmMark0,},
        {"Coding", nmCoding,},
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
int maxOcc = 1;
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

/* Data about the background sequence. */
char *badName = NULL;
char badNameBuf[512];

int fragMismatchTable[11] = {0, 0, 0, 1, 1, 2, 2, 2, 3, 3, 3};

double factorial(int x)
/* Return x factorial. */
{
double acc = 1;
for (; x > 1; x -= 1)
    acc *= x;
return acc;
}

double partialFactorial(int x, int y)
/* Return x!/y! */
{
double acc = 1;
assert(x >= y);
for (;x > y; x -= 1)
    acc *= x;
return acc;
}

double countWaysToChoose(int n, int setSize)
/* Count different ways to choose n things from a set of setSize */
{
return partialFactorial(setSize, n)/factorial(n);
}

double convergeTime(boolean considerRc)
/* Return approximate converge time. */
{
double rcFactor = (considerRc ? 2.0 : 1.0);
double totalBases = goodSeqElSize * goodSeqListSize;
double acc = rcFactor * numMotifs * maxOcc * totalBases / (30000.0 * machineSpeed() * 60.0);
return acc;
}

double findSeedsFragTime(boolean considerRc, int patSize)
/* Return approximate time to find seeds doing frag time. */
{
double reallyBig = 10e20;
double acc;
int mismatchCount = fragMismatchTable[patSize];
int power = ((patSize+mismatchCount)<<1);
    return reallyBig;
acc = (1<<power);
acc *= countWaysToChoose(mismatchCount, patSize);
if (considerRc)
    acc *= 2;
acc /= (400000000.0 * machineSpeed() * 60);
if (badName != NULL)
    acc += 0.05;
acc += convergeTime(considerRc);
return reallyBig;
//return acc;
}

double findSeedsScanTime(boolean considerRc)
/* Return approximate time to find seeds doing one EM iteration. */
{
int startScanLines = startScanLimit;
double seqElSize = goodSeqElSize;
double rcFactor = (considerRc ? 2.0 : 1.0);
double acc;

if (startScanLines > goodSeqListSize) startScanLines = goodSeqListSize;
acc = ((rcFactor * numMotifs * startScanLines * maxOcc * seqElSize * seqElSize * goodSeqListSize)
    /1000000 + 3)/(machineSpeed() * 60);
acc += 30*convergeTime(considerRc);
return acc;
}

double calcApproximateTime(boolean considerRc)
/* Get an estimated time in minutes. */
{
double time, t;
double convMult = 0.33;	/* Adjust for server speed here. */
time = findSeedsFragTime(considerRc, defaultTileSize);
t = findSeedsScanTime(considerRc);
if (t < time)
    time = t;
return time*convMult;
}

/* Frequency tables for the null model (background). */

double mark0[5] = {1.0, 0.25, 0.25, 0.25, 0.25};
/* The probability of finding a base in sequence. */
                    /* N     T     C     A     G */
double *freq = &mark0[1];
/* Frequency table that allows -1 indices for N's */

int slogMark0[5];
int *slogFreq = &slogMark0[1];
/* Log of frequency. */

double mark1[5][5];
int slogMark1[5][5];
/* First order Markov model - probability that one nucleotide will follow another. */
#define slogMark1Prob(ntVal1, ntVal2) (slogMark1[(ntVal1)+1][(ntVal2)+1])

double mark2[5][5][5];
int slogMark2[5][5][5];
/* Second order Markov model - probability that a nucleotide follows two previous. */
#define slogMark2Prob(v1,v2,v3) (slogMark2[(v1)+1][(v2)+1][(v3)+1])

double codingMark2[3][5][5][5];
int slogCodingMark2[3][5][5][5];
#define slogCodingProb(frame, v1, v2, v3) (slogCodingMark2[frame][(v1)+1][(v2)+1][(v3)+1])

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

struct dnaSeq *seqsInList(struct seqList *seqList)
/* Return dnaSeq's in seqList threaded into a singly
 * linked list. */
{
struct seqList *seqEl;
struct dnaSeq *list = NULL, *el;
for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    el = seqEl->seq;
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

struct seqList *readSeqMaybeMakeFrame(char *faFileName, int nullModel)
/* Load in an FA file.  If nullModel is coding figure out frame for each
 * sequence. */
{
struct seqList *seqList, *seqEl;
seqList = readSeqList(faFileName);
if (seqList == NULL)
    errAbort("No sequences in %s\n", faFileName);
if (nullModel == nmCoding)
    {
    char codFileName[512];
    struct codonBias *cb;
    snprintf(codFileName, sizeof(codFileName), "%s%s", amemeDir(), "ce.cod");
    cb = codonLoadBias(codFileName);
    for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
        {
        struct dnaSeq *seq = seqEl->seq;
        seqEl->frame = codonFindFrame(seq->dna, seq->size, cb);
        }
    freeMem(cb);
    }
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

void uglyfCheckGoodSeqSize()
{
struct seqList *el;
struct dnaSeq *seq;
for (el = goodSeq; el != NULL; el = el->next)
    {
    seq = el->seq;
    if (goodSeqElSize != seq->size)
        errAbort("bad good 1");
    if (goodSeqElSize != (int)strlen(seq->dna))
        errAbort("bad good 2");
    }
}

int uniformSeqSize(struct seqList *list, boolean leftAlign, int nullModel)
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
            if (nullModel == nmCoding)
                el->frame = (el->frame + dif) % 3;
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

void gaussLocale(double mean, double standardDeviation, int localeLen,
    double *locale, int *slogLocale)
/* Fill in locale with values of Gaussian distribution with given mean and standard
 * deviation at positions 0 ... localeLen-1.  Since this is going to clip off the
 * tails of the distribution, normalize things so that the sum over the entire locale
 * is equal to 1.0 */
{
double acc = 0;
double scale;
double gaussVal;
double x = -mean/standardDeviation;
double oneOverSd = 1.0/standardDeviation;
int i;

/* In first pass the scale is going to be off anyway, so don't bother
 * dividing by the square root of two pi, or the standard deviation
 * each time around. */
for(i=0; i<localeLen; ++i)
    {
    gaussVal = locale[i] = (x > 18 ? 0.1e-90 : exp(-0.5*x*x));
    acc += gaussVal;
    x += oneOverSd;
    }

/* Rescale things so everything totals to 1.0. */
scale = 1.0/acc;
for (i=0; i<localeLen; ++i)
    {
    gaussVal = (locale[i] *= scale);
    if (gaussVal < 0.0000001)
        gaussVal = 0.0000001;
    slogLocale[i] = slog(gaussVal);
    }
}

/* Handle location probability for one sequence at a time with these. */
double *locProb = NULL;
int *slogLocProb = NULL;
int locProbAlloc = 0;

void makeLocProb(double mean, double sd, int size)
/* Make location probability table. */
{
if (!useLocation)
    return;
if (size > locProbAlloc)
    {
    freeMem(locProb);
    freeMem(slogLocProb);
    locProb = needMem(size * sizeof(*locProb));
    slogLocProb = needMem(size * sizeof(*slogLocProb));
    locProbAlloc = size;
    }
gaussLocale(mean, sd, size, locProb, slogLocProb);
}

static double *tempColProbs[4];
static int tempColProbSize = 0;

double **getTempColProbs(int colCount)
/* Get 4 arrays of doubles for temporary storage of column
 * probabilities. Make sure they're initialized to zero. */
{
int byteSize = colCount * sizeof(double);
int i;

if (colCount > tempColProbSize)
    {
    double *tb;
    /* Ask for more than we need this time to avoid frequent
     * reallocation. */
    tempColProbSize = 2*colCount;
    freeMem(tempColProbs[0]);
    tb = needMem(4*tempColProbSize*sizeof(double));
    for (i=0; i<4; ++i)
        {
        tempColProbs[i] = tb;
        tb += tempColProbSize;
        }
    }
for (i=0; i<4; ++i)
    memset(tempColProbs[i], 0, byteSize);
return tempColProbs;
}

/* Various constants used by statistical routines. */
int slogOneFourth;
int slog70Percent;
int slog10Percent;

void statUtilOpen()
/* Initialize statistical utilities. */
{
static boolean initted = FALSE;

if (!initted)
    {
    initted = TRUE;
    slogOneFourth = slog(0.25);
    slog70Percent = slog(0.70);
    slog10Percent = slog(0.10);
    }
}

struct profileColumn
/* A single column of a probability profile. */
    {
    struct profileColumn *next;
    int slogProb[4];
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
    int score;
    int bestIndividualScore;
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
    if (ntVal[(int)tile[i]] < 0)
	return NULL;
    }

prof = blankProfile(tileSize);
prof->locale.mean = location;
prof->locale.standardDeviation = seqLen/2;
for (col = prof->columns; col != NULL; col = col->next)
    {
    baseVal = ntVal[(int)(*tile++)];
    if (baseVal < 0)
        col->slogProb[0] = col->slogProb[1] = col->slogProb[2] = col->slogProb[3] = slogOneFourth;
    else
        {
        col->slogProb[0] = col->slogProb[1] = col->slogProb[2] = col->slogProb[3] = slog10Percent;
        col->slogProb[baseVal] = slog70Percent;
        }
    }
return prof;
}

struct profile *rcProfileCopy(struct profile *forward)
/* Return reverse complemented version of "forward" profile. */
{
struct profile *rc;
struct profileColumn *fCol, *rCol;

/* Allocate profile and complement columns. */
rc = blankProfile(forward->columnCount);
for (fCol = forward->columns, rCol = rc->columns; fCol != NULL; fCol = fCol->next, rCol = rCol->next)
    {
    rCol->slogProb[A_BASE_VAL] = fCol->slogProb[T_BASE_VAL];
    rCol->slogProb[C_BASE_VAL] = fCol->slogProb[G_BASE_VAL];
    rCol->slogProb[G_BASE_VAL] = fCol->slogProb[C_BASE_VAL];
    rCol->slogProb[T_BASE_VAL] = fCol->slogProb[A_BASE_VAL];
    }

/* Copy non-column variables. */
rc->locale = forward->locale;
rc->score = forward->score;
rc->bestIndividualScore = forward->bestIndividualScore;

/* Reverse columns and return. */
slReverse(&rc->columns);
return rc;
}

void rcProfileInPlace(struct profile *profile)
/* Return reverse complemented version of "forward" profile. */
{
struct profileColumn *col;
struct profileColumn temp;

/* Complement it. */
for (col = profile->columns; col != NULL; col = col->next)
    {
    temp = *col;
    col->slogProb[A_BASE_VAL] = temp.slogProb[T_BASE_VAL];
    col->slogProb[C_BASE_VAL] = temp.slogProb[G_BASE_VAL];
    col->slogProb[G_BASE_VAL] = temp.slogProb[C_BASE_VAL];
    col->slogProb[T_BASE_VAL] = temp.slogProb[A_BASE_VAL];
    }
/* Reverse it. */
slReverse(&profile->columns);
}

struct profileColumn *rcColumnCopy(struct profileColumn *col)
/* Return a reverse complemented copy of column. */
{
struct profileColumn *rcCol = allocColumn();

rcCol->slogProb[A_BASE_VAL] = col->slogProb[T_BASE_VAL];
rcCol->slogProb[C_BASE_VAL] = col->slogProb[G_BASE_VAL];
rcCol->slogProb[G_BASE_VAL] = col->slogProb[C_BASE_VAL];
rcCol->slogProb[T_BASE_VAL] = col->slogProb[A_BASE_VAL];
return rcCol;
}


int cmpProfiles(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending probe offset. */
{
const struct profile *a = *((struct profile **)va);
const struct profile *b = *((struct profile **)vb);
int diff;
diff = b->score - a->score;
return diff;
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
    mustWrite(f, col->slogProb, sizeof(col->slogProb) );
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
        mustRead(f, col->slogProb, sizeof(col->slogProb) );
    slAddHead(&profList, prof);
    }
fclose(f);
slReverse(&profList);
return profList;
}

void makeFreqTable(struct seqList *seqList)
/* Figure out frequency of bases in input. */
{
struct dnaSeq *list = seqsInList(seqList);
dnaMark0(list, mark0, slogMark0);
}

void makeMark1(struct seqList *seqList, double mark0[5], int slogMark0[5],
	double mark1[5][5], int slogMark1[5][5])
/* Make up 1st order Markov model - probability that one nucleotide
 * will follow another. */
{
struct dnaSeq *list = seqsInList(seqList);
dnaMark1(list, mark0, slogMark0, mark1, slogMark1);
}

void makeTripleTable(struct seqList *seqList,
	double mark0[5], int slogMark0[5],
	double mark1[5][5], int slogMark1[5][5],
	double mark2[5][5][5], int slogMark2[5][5][5],
	int offset, int advance, int earlyEnd)
/* Convert seqList to dnaSeqs and call the real routine. */
{
struct dnaSeq *list = seqsInList(seqList);
dnaMarkTriple(list, mark0, slogMark0, mark1, slogMark1,
	mark2, slogMark2, offset, advance, earlyEnd);
}

void makeMark2(struct seqList *seqList,
	double mark0[5], int slogMark0[5],
	double mark1[5][5], int slogMark1[5][5],
	double mark2[5][5][5], int slogMark2[5][5][5])
/* Make up 1st order Markov model - probability that one nucleotide
 * will follow the previous two. */
{
makeTripleTable(seqList, mark0, slogMark0, mark1, slogMark1,
	mark2, slogMark2, 0, 1, 0);
}

int mark0PatSlogProb(DNA *pat, int patSize)
/* Return probability of pattern anywhere in sequence based
 * on frequency of each base. */
{
int i;
int slogProb = 0;
int baseIx;

for (i=0; i<patSize; ++i)
    {
    baseIx = ntVal[(int)pat[i]];
    slogProb += slogFreq[baseIx];
    }
return slogProb;
}

int mark1PatSlogProb(DNA *dna, int offset, int patSize)
/* Return probability of pattern anywhere in sequence based
 * on first degree Markov model (probability of base in context
 * of previous base. */
{
int i;
int slogProb = 0;
int startIx = offset;
int endIx = offset+patSize;
if (startIx == 0)
    {
    slogProb = slogFreq[ntVal[(int)dna[startIx]]];
    startIx = 1;
    }
for (i=startIx; i<endIx; ++i)
    slogProb += slogMark1Prob(ntVal[(int)dna[i-1]], ntVal[(int)dna[i]]);
return slogProb;
}

int mark2PatSlogProb(DNA *dna, int offset, int patSize)
/* Return probability of pattern anywhere in sequence based
 * on second degree Markov model (probability of base in context
 * of previous two bases.) */
{
int i;
int slogProb = 0;
int startIx = offset;
int endIx = offset+patSize;

if (startIx == 0)
    {
    slogProb += slogFreq[ntVal[(int)dna[0]]];
    startIx = 1;
    }
if (startIx == 1)
    {
    slogProb += slogMark1Prob(ntVal[(int)dna[0]], ntVal[(int)dna[1]]);
    startIx = 2;
    }
for (i=startIx; i<endIx; ++i)
    slogProb += slogMark2Prob(ntVal[(int)dna[i-2]], ntVal[(int)dna[i-1]], ntVal[(int)dna[i]]);
return slogProb;
}

int codingPatSlogProb(DNA *dna, int offset, int patSize, int frame)
/* Return probability of pattern anywhere in sequence based
 * on previous two bases and frame. */
{
int i;
int slogProb = 0;
int startIx = offset;
int endIx = offset+patSize;
int patUsed = 0;

if (startIx == 0)
    {
    slogProb += slogFreq[ntVal[(int)dna[0]]];
    startIx = 1;
    ++patUsed;
    if (++frame >= 3)
        frame = 0;
    }
if (startIx == 1)
    {
    slogProb += slogMark1Prob(ntVal[(int)dna[0]], ntVal[(int)dna[1]]);
    startIx = 2;
    ++patUsed;
    if (++frame >= 3)
        frame = 0;
    }
for (i=startIx; i<endIx; ++i)
    {
    slogProb += slogCodingProb(frame, ntVal[(int)dna[i-2]], ntVal[(int)dna[i-1]], ntVal[(int)dna[i]]);
    ++patUsed;
    if (++frame >= 3)
        frame = 0;
    }
assert(patUsed == patSize);
return slogProb;
}

int nullPatSlogProb(DNA *dna, int offset, int patSize, int frame)
/* Return probability of the pattern according to current model. */
{
switch (nullModel)
    {
    case nmMark2:
        return mark2PatSlogProb(dna, offset, patSize);
        break;
    case nmMark1:
        return mark1PatSlogProb(dna, offset, patSize);
        break;
    case nmMark0:
        return mark0PatSlogProb(dna+offset, patSize);
        break;
    case nmCoding:
        return codingPatSlogProb(dna, offset, patSize, (offset+frame)%3);
        break;
    case nmEven:
        return slogOneFourth*patSize;
        break;
    default:
        errAbort("Unknown null model");
	return 0;
    }
}

#ifdef OLD
int profilePatSlogProb(DNA *pat, int *softMask, struct profile *prof)
/* Return log probability of pattern following profile (without
 * yet considering locale.) */
{
int slogProb = 0;
struct profileColumn *col;
int baseVal;

for (col = prof->columns; col != NULL; col = col->next)
    {
    if ((baseVal = ntVal[(int)(*pat++)]) >= 0)
        slogProb += col->slogProb[baseVal] + *softMask++;
    else
        slogProb += slogOneFourth;
    }
return slogProb;
}
#endif

int profilePatSlogProb(DNA *pat, int *softMask, struct profileColumn *col, int colCount)
/* Return log probability of pattern following profile (without
 * yet considering locale.) */
{
int slogProb = 0;
int baseVal;

while (--colCount >= 0)
    {
    if ((baseVal = ntVal[(int)(*pat++)]) >= 0)
        slogProb += col->slogProb[baseVal] + *softMask++;
    else
        slogProb += slogOneFourth;
    col = col->next;
    }
return slogProb;
}

struct position
/* This records the position and score of a profile match. */
    {
    struct position *next;
    struct seqList *seqEl;
    int pos;
    int score;
    double weight;
    boolean isRc;
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
int diff;
diff = b->score - a->score;
return diff;
}

void matchAtBaseIx(struct seqList *seqEl, struct profileColumn *col,
    struct profileColumn *rcCol,
    int columnCount, int baseIx, int slogInvPos,
    int *retScore, boolean *retIsRc)
/* Return the score and reverse complement status of profile and rcProfile at position.
 * The rcProf may be null. */
{
struct dnaSeq *dnaSeq = seqEl->seq;
DNA *seq = dnaSeq->dna;
int *softMask = seqEl->softMask;
DNA *dna = seq+baseIx;
boolean isRc = FALSE;
int patScore, rcPatScore, nonPatScore, score;

nonPatScore = -nullPatSlogProb(seq, baseIx, columnCount, seqEl->frame);
if (useLocation)
    nonPatScore += slogLocProb[baseIx] - slogInvPos;
patScore = profilePatSlogProb(dna, softMask+baseIx, col, columnCount) + nonPatScore;
if (rcCol)
    {
    int rcNonPatScore;
    reverseComplement(seq+baseIx, columnCount);
    	// Todo - adjust frame for minus strand here.
    rcNonPatScore = -nullPatSlogProb(seq, baseIx, columnCount, seqEl->frame);
    if (useLocation)
	rcNonPatScore += slogLocProb[baseIx] - slogInvPos;
    reverseComplement(seq+baseIx, columnCount);

    rcPatScore = profilePatSlogProb(dna, softMask+baseIx, rcCol, columnCount)
       + rcNonPatScore;
    if (rcPatScore > patScore)
        {
        patScore = rcPatScore;
        isRc = TRUE;
        }
    }
score = patScore;
*retScore = score;
*retIsRc = isRc;
}

struct position *bestMatchInSeq(struct profile *prof, struct profile *rcProf, struct seqList *seqEl,
    int seqSize, int *slogLocProb, int threshold)
/* Return score and position of best match to profile in sequence. */
{
int profSize = prof->columnCount;
int endIx = seqSize - profSize + 1;
int slogInvPos = slog(1.0/(endIx));
int bestScore = -0x3fffffff;
int bestPos = -1;
boolean isRc, bestRc = FALSE;
int score;
int i;
struct profileColumn *col, *rcCol = NULL;
struct position *pos;

col = prof->columns;
if (rcProf != NULL)
    rcCol = rcProf->columns;
for (i=0; i< endIx; ++i)
    {
    matchAtBaseIx(seqEl, col, rcCol, profSize, i, slogInvPos, &score, &isRc);
    if (score > bestScore)
        {
        bestScore = score;
        bestPos = i;
        bestRc = isRc;
        }
    }
if (bestScore < threshold)
    return NULL;
pos = allocPosition();
pos->seqEl = seqEl;
pos->pos = bestPos;
pos->score = bestScore;
pos->weight = 0.0;
pos->isRc = bestRc;
pos->next = NULL;
return pos;
}

struct position *positionsInSeq(struct profile *prof, struct profile *rcProf, struct seqList *seqEl, int seqSize,
    int *slogLocProb, int threshold)
/* Return a list of positions in sequence matching profile at better than threshold levels.
 * (Set threshold to 0 for "better than chance". ) */
{
int profSize = prof->columnCount;
int endIx = seqSize - profSize + 1;
int slogInvPos = slog(1.0/(endIx));
boolean isRc;
int score;
int i;
struct position *pos, *posList = NULL;
struct profileColumn *col, *rcCol = NULL;

col = prof->columns;
if (rcProf != NULL)
    rcCol = rcProf->columns;
for (i=0; i< endIx; ++i)
    {
    matchAtBaseIx(seqEl, col, rcCol, profSize, i, slogInvPos, &score, &isRc);
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
            pos->isRc = isRc;
            slAddHead(&posList, pos);
            }
        else
            {
            /* In overlapping case take the best overlapping score. */
            if (score > posList->score)
                {
                posList->pos = i;
                posList->score = score;
                posList->isRc = isRc;
                }
            }
        }
    }
return posList;
}

struct position *getPositions(struct profile *prof, struct profile *rcProf, struct seqList *seqEl, int seqSize,
    int *slogLocProb, int threshold)
/* Get the places the profile hits in the seqList. */
{
if (maxOcc == 1)
    return bestMatchInSeq(prof, rcProf, seqEl, seqSize, slogLocProb, threshold);
else
    {
    struct position *posList, *lastPos;
    int numToTake = maxOcc;
    int i;
    posList = positionsInSeq(prof, rcProf, seqEl, seqSize, slogLocProb, threshold);
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

int scoreAtPositions(struct profileColumn *col, struct profileColumn *rcCol, int colCount,
    struct position *posList, int posOff, int rcPosOff, int seqCount)
/* Score profile given list of positions. */
{
int endIx = goodSeqElSize - colCount + 1;
int slogInvPos = slog(1.0/(endIx));
struct position *position;
struct seqList *seqEl;
int pos;
int profScore = 0;
int oneScore;
boolean isRc;

for (position = posList; position != NULL; position = position->next)
    {
    seqEl = position->seqEl;
    pos = position->pos + (position->isRc ? rcPosOff : posOff);
    if (pos >= 0 && pos < endIx)
        {
        matchAtBaseIx(seqEl, col, rcCol, colCount, pos, slogInvPos, &oneScore, &isRc);
        }
    else
        oneScore = 0;
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

struct profileColumn *columnAtPositions(struct position *posList, int posOffset, int profSize)
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
    seq = seqEl->seq;
    dna = seq->dna;
    if (position->isRc)
        {
        pos = position->pos +  profSize - 1 - posOffset;
        if (pos >= 0 && pos < seqSize)
            {
            baseVal = ntVal[(int)ntCompTable[(int)dna[pos]]];
            if (baseVal >= 0)
                hist[baseVal] += 1;
            }
        }
    else
        {
        pos = position->pos + posOffset;
        if (pos >= 0 && pos < seqSize)
            {
            baseVal = ntVal[(int)dna[pos]];
            if (baseVal >= 0)
                hist[baseVal] += 1;
            }
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
    col->slogProb[i] = slog( (double)hist[i]/histTotal);
    }
return col;
}

void erasePos(struct position *pos, struct profile *prof, int seqElSize, double maxWeight)
/* Probabalistically erase one hit */
{
int startPos, endPos;
int *softMask;
int slogWeight;
struct seqList *seqEl;
double weight;

seqEl = pos->seqEl;
startPos = pos->pos;
weight = 1.0 - (pos->weight/maxWeight);
if (weight <= 0.0000001)
    weight = 0.000001;
slogWeight = slog(weight);
softMask = seqEl->softMask;
endPos = startPos + prof->columnCount;
if (endPos > seqElSize)
    endPos = seqElSize;
if (startPos < 0)
    startPos = 0;
for (; startPos < endPos; ++startPos)
    {
    softMask[startPos] += slogWeight;
    }
}

double calcWeights(struct position *posList)
/* Fills in weights based on position scores.  Returns maximum weight
 * of any single position. */
{
double accWeight = 0.0;
int oneScore;
double invAcc;
double maxWeight = -1.0;
double weight;
struct position *pos;

/* Calculate raw weights from scores and add them up. */
for (pos = posList; pos != NULL; pos = pos->next)
    {
    oneScore = pos->score;
    weight = 1.0/(1.0 + invSlog(-oneScore));
    pos->weight = weight;
    accWeight += weight;
    if (maxWeight < weight)
        maxWeight = weight;
    }
invAcc = 1.0/accWeight;
/* Normalize weights so they add up to 1.0 */
for (pos = posList; pos != NULL; pos = pos->next)
    {
    pos->weight *= invAcc;
    }
maxWeight *= invAcc;
return maxWeight;
}

int iterateProfile(struct profile *prof, struct profile *rcProf,
    struct seqList *seqList, int seqElSize,
    struct profile **retProfile, struct profile **retRcProfile, boolean erase,
    FILE *hitOut, int profileId, boolean hitTrombaFormat)
/* Run a profile over sequence list and return the score.  Possibly update
 * the profile or erase where the profile hits from the sequences.
 * If hitOut is non-NULL store info on hit there.*/
{
int profScore = 0;
int onePos;
int seqCount = slCount(seqList);
struct seqList *seqEl;
double weight;
double maxWeight = 0;
double mean = 0;
boolean saveWeight = (retProfile != NULL || erase);
struct position *posList = NULL, *pos, *newPosList;
int seqIx = 0;


/* Go through sequence list and collect best hit on each sequence.
 * and score it. */
makeLocProb(prof->locale.mean, prof->locale.standardDeviation, seqElSize);
for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    newPosList = getPositions(prof, rcProf, seqEl, seqElSize, slogLocProb, 0);
    for (pos = newPosList; pos != NULL; pos = pos->next)
        {
        profScore += pos->score;
	if (hitOut != NULL && pos->score > 0)
	    {
	    if (hitTrombaFormat)
		{
	        fprintf(hitOut, "%d,%d,", seqIx, seqEl->seq->size - pos->pos);
		mustWrite(hitOut, seqEl->seq->dna + pos->pos, prof->columnCount);
		fprintf(hitOut, "\n");
		}
	    else
		fprintf(hitOut, "%d\t%2.3f\t%s\t%d\n",
		    profileId, invSlogScale*pos->score,
		    seqEl->seq->name, pos->pos);
	    }
        }
    posList = slCat(newPosList, posList);
    ++seqIx;
    }
profScore /= seqCount;

if (saveWeight)
    maxWeight = calcWeights(posList);
if (erase)
/* Probabalistically erase hits. */
    {
    for (pos = posList; pos != NULL; pos = pos->next)
        erasePos(pos, prof, seqElSize, maxWeight);
    }
else if (retProfile != NULL)
/* Generate next generation profile if that's what they want. */
    {
    int profSize = prof->columnCount;
    struct profileColumn *col;
    struct profileColumn *column, *rcColumn = NULL;
    struct profile *newProf = blankProfile(profSize);
    struct profile *rcNewProf = NULL;
    struct dnaSeq *seq;
    DNA *dna;
    int colIx;
    double minSd = 1.0;
    double sd;
    int shiftLeftEnd = 0, shiftRightEnd = 0;
    int rcShiftLeftEnd = 0, rcShiftRightEnd = 0;
    int newProfScore;
    struct position *pos;
    boolean doResize = (constrainer < 900.0);
    double **tempColProbs = getTempColProbs(profSize);

    /* Go through hits and use them to generate next generation
     * of profile. */
    for (pos = posList; pos != NULL; pos = pos->next)
        {
        onePos = pos->pos;
        seqEl = pos->seqEl;
        weight = pos->weight;
        seq = seqEl->seq;
        dna = seq->dna + onePos;
        if (pos->isRc)
            reverseComplement(dna, profSize);
        for (colIx = 0; colIx < profSize; ++colIx)
            {
            int baseVal = ntVal[(int)dna[colIx]];
            if (baseVal < 0)    /* Matching an N - distribute weight evenly. */
                {
                int i;
                double w4 = weight/4;
                for (i=0; i<4; ++i)
                    tempColProbs[i][colIx] += w4;
                }
            else                /* Concentrate weight on specific match. */
                {
                double w99 = weight * 0.99;
                double wLeft = (weight - w99)/3;
                int i;
                for (i=0; i<4; ++i)
                    tempColProbs[i][colIx] += (i == baseVal ? w99 : wLeft);
                }
            }
        if (pos->isRc)
            reverseComplement(dna, profSize);
        mean += weight * onePos;
        }

    /* Convert profile columns to log form. */
    for (col = newProf->columns, colIx = 0; col != NULL; col = col->next, colIx += 1)
        {
        int i;
        for (i=0; i<4; ++i)
            {
            double val = tempColProbs[i][colIx];
            col->slogProb[i] = slog(val);
            }
        }
    newProf->locale.mean = mean;
    sd = sqrt(calcWeightedVariance(mean, posList));
    if (sd< minSd)
        sd= minSd;
    newProf->locale.standardDeviation = sd;

    column = newProf->columns;
    if (rcProf != NULL)
        {
        rcNewProf = rcProfileCopy(newProf);
        rcColumn = rcNewProf->columns;
        }
    profScore = scoreAtPositions(column, rcColumn, profSize, posList, 0, 0, seqCount);

    /* Add and subtract columns at end to see if this improves score. */
    if (doResize)
        {
        int expandConstrainFactor = round(fSlogScale*constrainer/seqCount);    /* Reduce urge to expand. */

        /* See if profile improves by subtracting column from left */
        if (newProf->columnCount > 1)
            {
            int newCount = newProf->columnCount-1;
            column = newProf->columns->next;
            if (rcNewProf)
                rcColumn = rcNewProf->columns;
            newProfScore = scoreAtPositions(column, rcColumn, newCount,
                posList, 1, 0, seqCount);
            if (newProfScore > profScore)
                {
                shiftLeftEnd = 1;
                profScore = newProfScore;
                removeFirstColumn(newProf);
                newProf->locale.mean += 1.0;
                if (rcNewProf)
                    {
                    removeLastColumn(rcNewProf);
                    rcShiftRightEnd = -1;
                    }
                }
            }

        /* See if profile improves by subtracting column to right */
        if (newProf->columnCount > 1)
            {
            int newCount = newProf->columnCount-1;
            column = newProf->columns;
            if (rcNewProf)
                rcColumn = rcNewProf->columns->next;
            newProfScore = scoreAtPositions(column, rcColumn, newCount,
                posList, shiftLeftEnd, 1, seqCount);
            if (newProfScore > profScore)
                {
                shiftRightEnd = -1;
                profScore = newProfScore;
                removeLastColumn(newProf);
                if (rcNewProf)
                    {
                    removeFirstColumn(rcNewProf);
                    rcNewProf->locale.mean += 1.0;
                    rcShiftLeftEnd = 1;
                    }
                }
            }

        /* See if profile improves by adding a column to the left */
        if (!shiftLeftEnd)
            {
            col = columnAtPositions(posList, -1, profSize);
            if (col != NULL)
                {
                int newCount = newProf->columnCount+1;
                slAddHead(&newProf->columns, col);
                newProf->columnCount = newCount;
                column = newProf->columns;
                if (rcNewProf)
                    {
                    struct profileColumn *rcCol = rcColumnCopy(col);
                    slAddTail(&rcNewProf->columns, rcCol);
                    rcNewProf->columnCount = newCount;
                    rcColumn = rcNewProf->columns;
                    }
                newProfScore = scoreAtPositions(column, rcColumn, newCount,
                    posList, -1, rcShiftLeftEnd, seqCount);
                if (newProfScore > profScore + expandConstrainFactor)
                    {
                    newProf->locale.mean -= 1.0;
                    shiftLeftEnd = -1;
                    profScore = newProfScore;
                    if (rcNewProf)
                        {
                        rcShiftRightEnd = 1;
                        }
                    }
                else
                    {
                    removeFirstColumn(newProf);
                    if (rcNewProf)
                        removeLastColumn(rcNewProf);
                    }
                }
            }

        /* See if profile improves by adding a column to the right */
        if (!shiftRightEnd)
            {
            col = columnAtPositions(posList, profSize, profSize);
            if (col != NULL)
                {
                int newCount = newProf->columnCount+1;
                slAddTail(&newProf->columns, col);
                newProf->columnCount = newCount;
                column = newProf->columns;
                if (rcNewProf)
                    {
                    struct profileColumn *rcCol = rcColumnCopy(col);
                    slAddHead(&rcNewProf->columns, rcCol);
                    rcNewProf->columnCount = newCount;
                    rcColumn = rcNewProf->columns;
                    }
                newProfScore = scoreAtPositions(column, rcColumn, newCount,
                    posList, shiftLeftEnd, -1, seqCount);
                if (newProfScore > profScore + expandConstrainFactor)
                    {
                    shiftRightEnd = +1;
                    profScore = newProfScore;
                    if (rcNewProf)
                        {
                        rcShiftLeftEnd = -1;
                        rcNewProf->locale.mean += 1.0;
                        }
                    }
                else
                    {
                    removeLastColumn(newProf);
                    if (rcNewProf)
                        removeFirstColumn(rcNewProf);
                    }
                }
            }
        }
    newProf->score = profScore;
    *retProfile = newProf;
    *retRcProfile = rcNewProf;
    }
freePositionList(&posList);
return profScore;
}

void maskProfileFromSeqList(struct profile *profile, struct profile *rcProfile, struct seqList *seqList, int seqElSize)
/* Skew the soft mask of the seqList so that things the profile matches will
 * become unlikely to be matched by something else. */
{
iterateProfile(profile, rcProfile, seqList, seqElSize, NULL, NULL, TRUE, NULL, 0, FALSE);
}

struct profile *scanStartProfiles(struct seqList *goodSeq, int scanLimit, boolean considerRc)
/* Find the most promising profiles by considering ones made up of all starting
 * positions run for one EM iteration. */
{
struct profile *profileList = NULL, *profile, *rcProfile = NULL;
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
            if (considerRc)
                rcProfile =  rcProfileCopy(profile);
	    profile->score = iterateProfile(profile, rcProfile, goodSeq,
	    	goodSeqElSize, NULL, NULL, FALSE, NULL, 0, FALSE);
	    freeProfile(&rcProfile);
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
slSort(&profileList, cmpProfiles);
horizontalLine();
return profileList;
}

struct profile *fragStartProfiles(char *goodName, char *badName, boolean considerRc)
/* Call fragFinder to get the best starting profile. */
{
double fprof[16][4];
struct profile *prof;
struct profileColumn *col;
int i,j;
double reallySmall = 0.000000001;
int fragSize = defaultTileSize;
double x;

if (fragSize > goodSeqElSize)
    fragSize = goodSeqElSize;
fragFind(goodSeq, badName, fragSize, fragMismatchTable[fragSize], considerRc, fprof);
prof = blankProfile(fragSize);
for (i=0, col = prof->columns; i<fragSize; ++i, col = col->next)
    {
    for (j=0; j<4; ++j)
        {
        x = fprof[i][j];
        if (x <= reallySmall)
            x = reallySmall;
        col->slogProb[j] = slog(x);
        }
    }
/* Make location distribution very close to uniform. */
prof->locale.mean = goodSeqElSize/2;
prof->locale.standardDeviation = 10*goodSeqElSize;
return prof;
}

struct profile *findStartProfiles(struct seqList *goodSeq, boolean considerRc)
/* Find the most promising starting profiles using either EM or fragFind whichever
 * looks to be faster. */
{
struct profile *profList;
profList = scanStartProfiles(goodSeq, startScanLimit, considerRc);
return profList;
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
    int bestProb = -0x7fffffff;
    for (i=0; i<4; ++i)
        {
        if (col->slogProb[i] > bestProb)
            {
            bestProb = col->slogProb[i];
            bestVal = i;
            }
        }
    buf[consIx++] = valToNt[bestVal];
    }
buf[consIx] = 0;
return buf;
}

static void motifHitSection(struct dnaSeq *seq, struct dnaMotif *motif, FILE *f)
/* Print out section about motif. */
{
#ifdef GS_PATH
char *gs = GS_PATH;
#else
char *gs = NULL;
#endif

if (motif != NULL)
    {
    struct tempName pngTn;
    fprintf(f,"<PRE>");
    dnaMotifMakeProbabalistic(motif);
    makeTempName(&pngTn, "logo", ".png");
    dnaMotifToLogoPng(motif, 47, 140, gs, "../trash", pngTn.forCgi);
    fprintf(f,"   ");
    fprintf(f,"<IMG SRC=\"%s\" BORDER=1>", pngTn.forHtml);
    fprintf(f,"\n");
    fprintf(f,"motif consensus\n");
    dnaMotifPrintProb(motif, f);
    fprintf(f,"</PRE>");
    }
}

void printProfile(FILE *f, struct profile *prof)
/* Display salient facts about a profile. */
{
struct profileColumn *col;
int i;
char *consSeq = consensusSeq(prof);
int orderTranslater[4] = {A_BASE_VAL, C_BASE_VAL, G_BASE_VAL, T_BASE_VAL};
int baseVal;
struct dnaMotif *motif;

fprintf(f, "%5.4f @ %4.2f sd %4.2f ", invSlogScale*prof->score, prof->locale.mean, prof->locale.standardDeviation);
touppers(consSeq);
AllocVar(motif);
motif->name = NULL;
motif->columnCount = 0;
fprintf(f, "%s\n", consSeq);
for (col = prof->columns; col != NULL; col = col->next)
    motif->columnCount++;
AllocArray(motif->aProb, motif->columnCount);
AllocArray(motif->cProb, motif->columnCount);
AllocArray(motif->gProb, motif->columnCount);
AllocArray(motif->tProb, motif->columnCount);
for (i = 0; i<4; ++i)
    {
    int j = 0;
    baseVal = orderTranslater[i];
    fprintf(f, "\t%c  ", valToNt[baseVal]);
    for (col = prof->columns; col != NULL; col = col->next)
        {
        fprintf(f, "%4.3f ", invSlog(col->slogProb[baseVal]) );
        switch (valToNt[baseVal])
            {
            case 'a':
                motif->aProb[j] = (float)invSlog(col->slogProb[baseVal]);
                break;
            case 'c':
                motif->cProb[j] = (float)invSlog(col->slogProb[baseVal]);
                break;
            case 'g':
                motif->gProb[j] = (float)invSlog(col->slogProb[baseVal]);
                break;
            case 't':
                motif->tProb[j] = (float)invSlog(col->slogProb[baseVal]);
                break;
            }
        j++;
        }
    fprintf(f, "\n");
    }
if (f == htmlOut && outputLogo)
    motifHitSection(NULL, motif, f);
}

void makeNullModels(struct seqList *bgSeq)
/* Create null models from background sequence. */
{
int frame;
static DNA *stopCodons[3]  = {"tag", "tga", "taa"};
int i;

makeFreqTable(bgSeq);
makeMark1(bgSeq, mark0, slogMark0, mark1, slogMark1);
makeMark2(bgSeq, mark0, slogMark0, mark1, slogMark1, mark2, slogMark2);
for (frame=0; frame<3; frame += 1)
    {
    int readFrame = (frame+2)%3;
    makeTripleTable(bgSeq, mark0, slogMark0, mark1, slogMark1,
    	codingMark2[readFrame], slogCodingMark2[readFrame], frame, 3, 3);
    }
/* Usually there's a little noise in our data that makes stop codons appear at a small
 * frequency in coding regions.  We won't eliminate this completely, but reduce it
 * by a factor of 100.... */
for (i=0; i<ArraySize(stopCodons); ++i)
    {
    int i0 = ntVal[(int)stopCodons[i][0]]+1;
    int i1 = ntVal[(int)stopCodons[i][1]]+1;
    int i2 = ntVal[(int)stopCodons[i][2]]+1;
    double logVal;
    logVal = (codingMark2[2][i0][i1][i2] *= 0.01);
    slogCodingMark2[2][i0][i1][i2] = carefulSlog(logVal);
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
strcpy(bgName+suffixPos, ".mk2");
if ((f = fopen(bgName, "rb")) == NULL)
    {
    strcpy(bgName+suffixPos, ".fa");
    bgSeq = readSeqList(bgName);
    makeNullModels(bgSeq);
    strcpy(bgName+suffixPos, ".mk2");
    f = mustOpen(bgName, "wb");
    sig = lm2Signature;
    writeOne(f, sig);
    mustWrite(f, mark0, sizeof(mark0));
    mustWrite(f, slogMark0, sizeof(slogMark0));
    mustWrite(f, mark1, sizeof(mark1));
    mustWrite(f, slogMark1, sizeof(slogMark1));
    mustWrite(f, mark2, sizeof(mark2));
    mustWrite(f, slogMark2, sizeof(slogMark2));
    mustWrite(f, codingMark2, sizeof(codingMark2));
    mustWrite(f, slogCodingMark2, sizeof(slogCodingMark2));
    }
else
    {
    mustReadOne(f, sig);
    if (sig != lm2Signature)
        errAbort("Bad signature on %s", bgName);
    mustRead(f, mark0, sizeof(mark0));
    mustRead(f, slogMark0, sizeof(slogMark0));
    mustRead(f, mark1, sizeof(mark1));
    mustRead(f, slogMark1, sizeof(slogMark1));
    mustRead(f, mark2, sizeof(mark2));
    mustRead(f, slogMark2, sizeof(slogMark2));
    mustRead(f, codingMark2, sizeof(codingMark2));
    mustRead(f, slogCodingMark2, sizeof(slogCodingMark2));
    }
fclose(f);
}


#ifdef JUSTDEBUG
void dumpMark2Table(double mark2Table[5][5][5], int slogMark2Table[5][5][5], char *name)
/* Print out mark2 table. */
{
int i,j,k;

fprintf(htmlOut, "<PRE><TT>\n%s\n", name);
for (i=1; i<5; ++i)
    {
    for (j=1; j<5; ++j)
        {
        for (k=1; k<5; ++k)
            {
            fprintf(htmlOut, "%1.3f ", mark2Table[i][j][k]);
            }
        fprintf(htmlOut, "   ");
        }
    fprintf(htmlOut, "\n");
    }
fprintf(htmlOut, "\n");
for (i=1; i<5; ++i)
    {
    for (j=1; j<5; ++j)
        {
        for (k=1; k<5; ++k)
            {
            fprintf(htmlOut, "%6d ", slogMark2Table[i][j][k]);
            }
        fprintf(htmlOut, "   ");
        }
    fprintf(htmlOut, "\n");
    }
fprintf(htmlOut, "\n");
}
#endif /* JUSTDEBUG */

void getNullModel(char *goodName, char *badName, boolean premadeBg)
/* Make null (background) model from bad if it exists otherwise from good. */
{
if (premadeBg)
    {
    readPremadeBg(badName);
    strcat(badName, ".fa");
    }
else
    {
    struct seqList *bgSeq;
    char *seqName = badName;
    if (seqName == NULL)
        seqName = goodName;
    bgSeq = readSeqList(seqName);
    makeNullModels(bgSeq);
    freeSeqList(&bgSeq);
    }
}

int bestPosScore(struct position *posList)
/* Return best score in posList. */
{
struct position *pos;
int bestScore = 0;
int score;

for (pos = posList; pos != NULL; pos = pos->next)
    {
    score = pos->score;
    if (score > bestScore)
        bestScore = score;
    }
return bestScore;
}

void showProfHits(struct profile *prof, struct profile *rcProf, struct seqList *seqList, int seqElSize,
    int nameSize, int *retBestScore)
/* Display profile in context of goodSeq. While we're scanning through also
 * return best score of profile on any sequence. */
{
struct seqList *el;
struct dnaSeq *seq;
int score;
int bestScore = 0;
int totalScore;
int seqSize = seqList->seq->size;
DNA *uppered = needMem(seqSize + 1);
struct position *pos, *posList;

makeLocProb(prof->locale.mean, prof->locale.standardDeviation, seqElSize);
for (el = seqList; el != NULL; el = el->next)
    {
    seq = el->seq;
    memcpy(uppered, seq->dna, seqSize);
    uppered[seqSize] = 0;
    posList = getPositions(prof, rcProf, el, seqElSize, slogLocProb, 0);
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
    fprintf(htmlOut, "%5.2f %-*s %s\n", invSlogScale*totalScore, nameSize, seq->name, uppered);
    }
freeMem(uppered);
*retBestScore = bestScore;
}



long hashProfile(struct profile *prof)
/* Return a double that's close to unique for the profile. */
{
long acc[4];
struct profileColumn *col;
long ret = 0;
int i;
for (i=0; i<4; ++i)
    acc[i] = 0;
for (col = prof->columns; col != NULL; col = col->next)
    {
    for (i=0; i<4; ++i)
        {
        acc[i] += col->slogProb[i];
        }
    }
for (i=0; i<4; ++i)
    {
    ret ^= acc[i];
    }
return (ret + round(prof->locale.mean*10 + prof->locale.standardDeviation*10) );
}

struct profile *convergeProfile(struct profile *initProf, boolean considerRc)
/* Iterate profile until it converges up to 30 times. */
{
int i;
int maxIterations = 35;
long profHash = 0, lastHash = 0;
int score = -0x7fffffff, lastScore;
struct profile *newProf, *rcNewProf = NULL;
struct profile *prof = initProf, *rcProf = NULL;

if (considerRc)
    rcProf = rcProfileCopy(prof);
for (i=0; i<maxIterations; ++i)
    {
    lastScore = score;
    lastHash = profHash;
    score = iterateProfile(prof, rcProf, goodSeq,  goodSeqElSize,
    	&newProf, &rcNewProf, FALSE, NULL, 0, FALSE);
    if (prof != initProf)
        freeProfile(&prof);
    freeProfile(&rcProf);
    prof = newProf;
    rcProf = rcNewProf;
    profHash = hashProfile(prof);
    if (score == lastScore && profHash == lastHash)
        break;
    }
freeProfile(&rcProf);
return prof;
}

void doTopTiles(int repCount, char *profFileName, FILE *logFile, boolean considerRc)
/* Get top looking tile, converge it, save it, print it, erase it, and repeat. */
{
int repIx;
FILE *f = createProfFile(profFileName);
struct profile *bestProf;

for (repIx = 0; repIx < repCount; ++repIx)
    {
    struct profile *profList;
    struct profile *newList = NULL;
    struct profile *prof, *rcProf = NULL, *newProf;
    int maxCount = 35;

    progress("Looking for motif #%d\n", repIx+1);
    profList = findStartProfiles(goodSeq, considerRc);
    progress("Converging");
    for (prof = profList; prof != NULL; prof = prof->next)
        {
        newProf = convergeProfile(prof, considerRc);
        slAddHead(&newList, newProf);
        if (--maxCount <= 0)
            break;
	progress(".");
        }
    progress("\n");
    if (newList != NULL)
	{
	slSort(&newList, cmpProfiles);
	bestProf = newList;
	printProfile(htmlOut,bestProf);
	if (logFile)
	    printProfile(logFile, bestProf);
	horizontalLine();
	if (considerRc)
	    rcProf = rcProfileCopy(bestProf);
	showProfHits(bestProf, rcProf, goodSeq, goodSeqElSize, goodSeqNameSize, &bestProf->bestIndividualScore);
	writeProfile(f, bestProf);
	horizontalLine();
	maskProfileFromSeqList(bestProf, rcProf, goodSeq, goodSeqElSize);
	}
    freeProfile(&rcProf);
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
int oneLen = 0;

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
        fprintf(htmlOut, "</span>");
    fprintf(htmlOut, "<span style='color:#%02X%02X%02X;'>",color.r, color.g, color.b);
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
fputc(c, htmlOut);
}

void colorProfiles(struct profile *profList, struct seqList *seqList,
	int seqSize, boolean considerRc)
/* Display profiles in color on sequences. */
{
int profCount = slCount(profList);
struct rgbColor *colors = needMem(seqSize * sizeof(colors[0]));
int colorIx = 0;
int colCount;
struct seqList *seqEl;
struct dnaSeq *seq;
struct profile *prof;
DNA *dna;
struct rgbColor blend;
int i,j,k;
int start, end;
double mix;
int maxScore = -1;
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
char *gifName = cgiOptionalString("gif");
struct position ***posMatrix;
struct position *pos, *posList;
int nameSize = maxSeqNameSize(seqList);

/* Allocate 2-D array for storing positions. */
posMatrix = needMem(profCount * sizeof(posMatrix[0]));
for (i=0; i<profCount; ++i)
    posMatrix[i] = needMem(seqCount * sizeof(posMatrix[i][0]) );

/* Clear soft masks. */
for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    zeroBytes(seqEl->softMask, seqSize * sizeof(seqEl->softMask[0]) );

/* Fill in array with position info, figuring out max single
 * score as you go. */
for (i=0,prof=profList; prof != NULL; ++i,prof=prof->next)
    {
    struct profile *rcProf = NULL;
    if (considerRc)
        rcProf = rcProfileCopy(prof);
    makeLocProb(prof->locale.mean, prof->locale.standardDeviation, seqSize);
    for (j=0,seqEl=seqList; seqEl != NULL; ++j,seqEl=seqEl->next)
        {
        posMatrix[i][j] = posList = getPositions(prof, rcProf, seqEl, seqSize, slogLocProb, 0);
        for (pos = posList; pos != NULL; pos = pos->next)
            {
            if (pos->score > maxScore)
                maxScore = pos->score;
            }
        }
    maskProfileFromSeqList(prof, rcProf, seqList, seqSize);
    freeProfile(&rcProf);
    }

fprintf(htmlOut, "<H3>Color Coding for Profiles</H3>\n");
for (colorIx = 0, prof = profList; prof != NULL; prof = prof->next)
    {
    setFontColor(distinctColors[colorIx]);
    printProfile(htmlOut, prof);
    colorIx = (colorIx+1)%ArraySize(distinctColors);
    };
blackText();
horizontalLine();

fprintf(htmlOut, "<H3>Colored Text View of Profiles</H3>\n");
fprintf(htmlOut, "Different colors represent different profiles. Darker colors\n"
       "represent stronger matches to profile.\n\n");

/* Want to draw most significant last. */
//slReverse(&profList);

/* Allocate something to draw on and set the color map so have shades of
 * each color. */
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);
coff = mg->colorsUsed;
colCount = profCount;
if (colCount > ArraySize(distinctColors)) colCount = ArraySize(distinctColors);
for (i=0; i<colCount; ++i)
    {
    for (j=0; j<colorsPer; ++j)
        {
        blendColors(gfxBgColor, distinctColors[i], (double)j/(colorsPer-1), &blend);
        mg->colorMap[mg->colorsUsed++] = blend;
        }
    }

for (seqEl=seqList,i=0; seqEl != NULL; seqEl=seqEl->next,i+=1)
    {
    mgDrawBox(mg, xoff, yoff+barMidOff, insideWidth, 1, MG_GRAY);
    for (j=0; j<seqSize; ++j)
        colors[j] = textBgColor;
    seq = seqEl->seq;
    dna = seq->dna;
    colorIx = 0;
    for (prof=profList,j=0; prof != NULL; prof=prof->next,j+=1)
        {
        if (prof->bestIndividualScore > 0)
            {
            for (pos = posMatrix[j][i]; pos != NULL; pos = pos->next)
                {
                /* Figure out color to draw in. */
                start = pos->pos;
                end = start + prof->columnCount;
                if (end > seqSize)
                    end = seqSize;
                mix = (double)pos->score/maxScore;
                if (mix > 1.0) mix = 1.0;
                else if (mix < 0.0) mix = 0.0;
                blendColors(textBgColor, distinctColors[colorIx], mix, &blend);

                /* Store text colors. */
                for (k=start; k < end; ++k)
                    colors[k] = blend;

                /* Add colored box to summary pic. */
                x1 = xoff + round((double)start*insideWidth/seqSize);
                x2 = xoff + round((double)end*insideWidth/seqSize);
                mappedCol = coff + colorsPer*colorIx + round(mix*(colorsPer-1));
                mgDrawBox(mg, x1, yoff, x2-x1, barHeight, mappedCol);
                }
            if (++colorIx >= ArraySize(distinctColors))
                colorIx = 0;
            }
        }
    blackText();
    fprintf(htmlOut, "%-*s ", nameSize, seq->name);
    for (j=0; j<seqSize; ++j)
        {
        DNA base = dna[j];
        if (base == 0)
            break;
        colorTextOut(base, colors[j]);
        }
    yoff += barHeight + border;
    fprintf(htmlOut, "\n");
    }

/* Tell .html where the gif is. */
horizontalLine();
blackText();
fprintf(htmlOut, "<H3>Graphical Summary of Profile Hits</H3>\n");
fprintf(htmlOut, "Colors represent different profiles. Darker colors represent\n"
       "stronger matches to profile.\n");
if (gifName == NULL)
    {
    makeTempName(&gifTn, "imp", ".png");
    gifName = gifTn.forCgi;
    }
mgSavePng(mg, gifName, FALSE);
chmod(gifName, 0666);
mgFree(&mg);
fprintf(htmlOut, "<IMG SRC=\"%s\" WIDTH=%d HEIGHT=%d BORDER=0>\n",
    gifName, pixWidth, pixHeight);

freeMem(colors);

/* Free 2-D array for storing positions. */
for (i=0; i<profCount; ++i)
    {
    for (j=0; j<seqCount; ++j)
        freePositionList(&posMatrix[i][j]);
    freeMem(posMatrix[i]);
    }
freeMem(posMatrix);
}

void loadAndColorProfiles(char *profFileName, struct seqList *seqList, int seqSize, boolean considerRc)
/* Display profile in color on sequences. */
{
struct profile *profList = loadProfiles(profFileName);
if (profList != NULL)
    colorProfiles(profList, seqList, seqSize, considerRc);
freeProfileList(&profList);
}

void makePremadeBgPathName(char *fileName, char *retPathName, int retPathSize)
/* Make path name for background file out of just file name. */
{
char *dir = amemeDir();
sprintf(retPathName, "%s%s", dir, fileName);
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
/* Set up random number generator with seed depending on time and host. */
unsigned seed = (unsigned)time(NULL);
char *host = getenv("HOST");
if (host == NULL)
    host = getenv("JOB_ID");
if (host != NULL)
    seed += hashCrc(host);
srand(seed);
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

void generateOneSeq(FILE *f, int num, int seqLen,
    double even[5], double mark0[5], double mark1[5][5], double mark2[5][5][5],
    double codingMark2[3][5][5][5], int frame, int nullModel)
/* Generate one sequence into FA file. */
{
int i;

fprintf(f, ">%d\n", num);
switch (nullModel)
    {
    case nmCoding:
        {
        int dnaVal = -1, lastDnaVal = -1, lastLastDnaVal = -1;
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
            dnaVal = randomNucleotide(&(codingMark2[frame][lastLastDnaVal+1][lastDnaVal+1][0]) );
            if (++frame >= 3)
                frame = 0;
            fputc(ntFromVal(dnaVal), f);
            }
        fputc('\n', f);
        }
    case nmMark2:
        {
        int dnaVal = -1, lastDnaVal = -1, lastLastDnaVal = -1;
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
    generateOneSeq(f, i, lenSeq, evenProb, mark0, mark1, mark2, codingMark2, rand()%3, nullModel);
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

seqList = readSeqMaybeMakeFrame(realName, nullModel);
makeTempName(&tn, "con", ".fa");
f = mustOpen(tn.forCgi, "w");
for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    generateOneSeq(f, ++num, seqEl->seq->size,
        evenProb, mark0, mark1, mark2, codingMark2, seqEl->frame, nullModel);
    }
fclose(f);
freeSeqList(&seqList);
return tn.forCgi;
}

void oneSearchSet(boolean premade, FILE *logFile, boolean considerRc)
/* Do one set of motif searches */
{
struct tempName tn;
char *backgroundName;
double approxTime;

goodSeq = readSeqMaybeMakeFrame(goodName, nullModel);
goodSeqListSize = slCount(goodSeq);
goodSeqElSize = uniformSeqSize(goodSeq, leftAlign, nullModel);
goodSeqNameSize = maxSeqNameSize(goodSeq);

makeTempName(&tn, "imp", ".pfl");

fprintf(htmlOut, "<P>Looking for %d motifs in %d sequences. Longest sequence is %d bases.</P>\n",
    numMotifs, goodSeqListSize, goodSeqElSize);
fprintf(htmlOut, "<P>Settings are %s location; %sinclude reverse complement; %d occurrences per sequence; %s align; ",
    (useLocation ? "use" : "ignore"),
    (considerRc ? "" : "don't "), maxOcc,
    (leftAlign ? "left" : "right") );
fprintf(htmlOut, "restrain expansionist tendencies %f;  number of sequences in initial scan %d; ",
    constrainer, startScanLimit);
backgroundName = bgSource;
if (backgroundName == NULL)
    backgroundName = badName;
if (backgroundName == NULL)
    backgroundName = "same as foreground";
fprintf(htmlOut, "background model %s; background data %s;</P>",
    (nullModelCgiName == NULL ? "Markov 0" : nullModelCgiName),
    backgroundName);

approxTime = calcApproximateTime(considerRc);
progress("This run would take about %2.2f minutes on a lightly loaded vintage 2003 web server.",
    approxTime);
fprintf(htmlOut, "<TT><PRE>\n");
horizontalLine();

doTopTiles(numMotifs, tn.forCgi, logFile, considerRc);

loadAndColorProfiles(tn.forCgi, goodSeq, goodSeqElSize, considerRc);
remove(tn.forCgi);
fprintf(htmlOut, "</TT></PRE>\n");

freeSeqList(&goodSeq);
}

void doRandomTest(boolean premade, boolean considerRc)
/* Generate tables for scores on random sequences. */
{
int seqLen, seqCount;
struct tempName randTn;
FILE *logFile = mustOpen("\\temp\\random.txt", "w");
int reps = 2;
int i;

makeTempName(&randTn, "rand", ".fa");

fprintf(htmlOut, "<TT><PRE>\n");
for (seqLen = 100; seqLen <= 500; seqLen += 100)
    for (seqCount = 100; seqCount <= 100; seqCount += 10)
        {
        goodName = randTn.forCgi;
        for (i=1; i<=reps; ++i)
            {
            fprintf(logFile, "----------------- %d Random sequence of %d nucleotides take %d----------------\n",
                seqCount, seqLen, i);
            generate(goodName, seqCount, seqLen);
            oneSearchSet(premade, logFile, considerRc);
            }
        }
chmod(randTn.forCgi, 0666);
fclose(logFile);
}


void impDoMiddle()
/* Do main work of Improbizer.  Find motifs and display them. */
{
char *nullModelCgi = "background";
boolean premade = FALSE;
boolean isRandomTest = cgiBoolean("randomTest");
boolean isControlRun = cgiBoolean("controlRun");
boolean considerRc = cgiBoolean("rcToo");
long startTime, endTime;
boolean codingWeirdness;  /* True when we need to worry about coding background. */
FILE *motifOutFile = NULL;
char *motifOutName;

startTime = clock1000();
fprintf(htmlOut, "<H2>Improbizer Results</H2>\n");
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
if ((motifOutName = cgiOptionalString("motifOutput")) != NULL)
    motifOutFile = mustOpen(motifOutName, "w");
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
codingWeirdness =  (nullModel == nmCoding);
if (badName == NULL)
    {
    badName = cgiOptionalString("bad");
    if (badName != NULL)
        codingWeirdness = FALSE;
    }
/* If they selected a premade background, figure out file that goes with it,
 * then look up directory to find file in. */
if ((bgSource = cgiOptionalString("backgroundDataSource")) != NULL)
    {
    char *premadeBg = NULL;

    if (sameString(bgSource, "Worm Intron 3'"))
        premadeBg = "wormInt3";
    else if (sameString(bgSource, "Worm Intron 5'"))
        premadeBg = "wormInt5";
    else if (sameString(bgSource, "Worm Coding"))
        {
        premadeBg = "cecoding";
        codingWeirdness = FALSE;
        }
    else if (sameString(bgSource, "Yeast Promoter"))
        premadeBg = "yeastPromo";
    else if (sameString(bgSource, "Same as Foreground"))
        ;
    else if (sameString(bgSource, "From Data Pasted Below"))
        codingWeirdness = FALSE;
    else
       errAbort("Unknown backgroundDataSource");

    if (premadeBg != NULL)
        {
        makePremadeBgPathName(premadeBg, badNameBuf, sizeof(badNameBuf));
        badName = badNameBuf;
        premade = TRUE;
        }
    }
if (codingWeirdness)
    {
    errAbort("For coding background you must either paste in-frame coding regions\n"
             "into the background, from the command line include a 'bad' file,\n"
             "or select the premade background 'Worm Coding'\n");
    }
getNullModel(goodName, badName, premade);
if (isControlRun)
    goodName = randomSpoof(goodName);
if (isRandomTest )
    {
    fprintf(htmlOut, "<P>Random test mode - this will take a good long time.  Be sure to kill "
         "the process if you get impatient.</P>\n\n");
    doRandomTest(premade, considerRc);
    }
else
    {
    double approxTime = calcApproximateTime(considerRc);
    double maxTime = 5.0;

    if (isFromWeb && approxTime > maxTime)
        {
        errAbort("Sorry, this job is too big for our web server - it would use about "
               "%2.1f minutes of CPU time. Out of fairness to the other users of this "
               "machine we limit jobs to %2.1f minutes of CPU time or less.  Please reduce "
               "the size of your data (now %d sequences of %d bases each), "
               "the number of motifs you're looking "
               "for (now %d), or the number of sequences in the initial scan (now %d). "
               "The most important influence on run time is the maximum size of an individual sequence. "
               " If you really need "
               "to run the program on a data set this large contact Jim Kent (kent@biology.ucsc.edu) "
               "to get a batch version of this program to run on your own machine.",
               approxTime, maxTime,
               goodSeqListSize, goodSeqElSize,
               numMotifs, startScanLimit
               );
        }
    fprintf(htmlOut, "<P>Improbizer will display the results in parts.  First it will "
         "display the profiles (consensus sequences with the probability of "
         "each base at each position) individually as they are calculated. The "
         "position of a profile in a sequence is indicated by upper case. The "
         "strength of the profile match is indicated by the score on the left. "
         "There will be a delay during this phase as each profile is calculated. "
         "Second Improbizer will "
         "display all profiles at once over each sequence. Each profile "
         "has it's own color and the stronger the profile matches the darker "
         "it will appear in the sequence. Finally there will be a graphic "
         "summary of all the profiles at the end, using the same color "
         "conventions.</P>\n");
    oneSearchSet(premade, motifOutFile, considerRc);
    endTime = clock1000();
    horizontalLine();
    fprintf(htmlOut, "Calculation time was %4.3f minutes\n", 0.001*(endTime-startTime)/60);
    }
if (isControlRun)
    remove(goodName);
}

void explainMotif()
/* Explain to user what a motif should look like and exit. */
{
errAbort("<TT><PRE>"
    "Not all of the motifs are correct.  A motif should look something like:\n"
    "   a  0.208 0.239 0.018 0.990 0.003 0.003 0.990 0.990 0.003 0.650 0.506\n"
    "   c  0.068 0.044 0.003 0.003 0.990 0.003 0.003 0.003 0.990 0.064 0.042\n"
    "   g  0.074 0.061 0.070 0.003 0.003 0.003 0.003 0.003 0.003 0.181 0.050\n"
    "   t  0.650 0.656 0.908 0.003 0.003 0.990 0.003 0.003 0.003 0.104 0.401\n"
    "Each row needs to start out with a nucleotide, and be followed with numbers\n"
    "which represent the probability of the nucleotide occurring at that point\n"
    "in the sequence.  You can also submit motifs like:\n"
    "   a  3 1 4 3 3 2\n"
    "   c  2 5 2 2 1 0\n"
    "   g  4 2 4 3 5 7\n"
    "   t  1 2 0 2 1 1\n"
    "where the numbers represent counts rather than probabilities\n"
    "If you are including location information in the motif please add an\n"
    "additional line so that your motif looks something like:\n"
    "   3.1654 @ 18.40 sd 7.95 GGGAAC\n"
    "       t  0.058 0.101 0.003 0.003 0.216 0.269 \n"
    "       c  0.024 0.054 0.019 0.314 0.071 0.369 \n"
    "       a  0.308 0.378 0.003 0.679 0.709 0.215 \n"
    "       g  0.611 0.467 0.974 0.003 0.003 0.148 \n"
    "Where the top line is of the format:\n"
    "   score @ mean sd variance consensus\n"
    "You can leave out the score and consensus, they are ignored (but\n"
    "if your motif comes from Improbizer it's easier to leave them in)."
    );
}

boolean parseLocation(char *words[], int wordCount, double *retMean, double *retVariance)
/* Get mean and variance out of what's left of line after the @ in a line like:
 *   7.5603 @ 17.15 sd 10.41 TTTACTAACAAT
 */
{
if (wordCount < 3 || !isdigit(words[0][0]) || !isdigit(words[2][0]))
    explainMotif();
*retMean = atof(words[0]);
*retVariance = atof(words[2]);
return TRUE;
}

boolean andFour(boolean b[4])
/* Return TRUE if all four b[i] are TRUE. */
{
return (b[0] && b[1] && b[2] && b[3]);
}

struct profile *parseMotifs(char *text, boolean useLocation)
/* Convert motif from text description (something like the following)
    7.5603 @ 17.15 sd 10.41 TTTACTAACAAT
        t  0.650 0.656 0.908 0.003 0.003 0.990 0.003 0.003 0.003 0.104 0.401
        c  0.068 0.044 0.003 0.003 0.990 0.003 0.003 0.003 0.990 0.064 0.042
        a  0.208 0.239 0.018 0.990 0.003 0.003 0.990 0.990 0.003 0.650 0.506
        g  0.074 0.061 0.070 0.003 0.003 0.003 0.003 0.003 0.003 0.181 0.050
 * to a profile structure. */
{
#define maxLen 100
struct profile *profList = NULL;
char *lines[128];
int lineCount;
int lineIx = 0;
lineCount = chopString(text, "\n", lines, ArraySize(lines));


for (;;)
    {
    char *words[maxLen+1];
    double probs[4][maxLen];
    double mean = 0.0, variance = 0.0;
    double oneProb;
    char *word;
    int baseVal;
    boolean gotLocation = FALSE;
    boolean gotNt[4];
    int wordCount;
    int profLen = 0;
    int i=0,j=0;
    struct profile *prof;
    struct profileColumn *col;


    /* Skip over any blank lines or lines that start with #*/
    for (;lineIx < lineCount; ++lineIx)
        {
        char *s = skipLeadingSpaces(lines[lineIx]);
        if (s != 0 && s[0] != '#')
            break;
        }

    zeroBytes(gotNt, sizeof(gotNt));
    if (lineIx >= lineCount)
        break;    /* Empty motif is not an error. */

    if (lineCount < 4)
        explainMotif();

    while (lineIx<lineCount)
        {
        wordCount = chopLine(lines[lineIx], words);
        if (wordCount > maxLen+1)
            errAbort("Sorry, this program can only handle motifs with up to %d bases", maxLen);
        if (wordCount > 0)  /* Tolerate blank lines. */
            {
            /* First word should be the name of a base.  Check this,
             * make sure each base only used once, and that all
             * lines of motif have the same number of elements.
             * Along with all this checking, store the probability
             * values in each row. */
            if (sameString(words[1], "@") )
                gotLocation = parseLocation(words+2, wordCount-2, &mean, &variance);
            else if (sameString(words[0], "@") )
                gotLocation = parseLocation(words+1, wordCount-1, &mean, &variance);
            else    /* Parse nucleotide probability line. */
                {
                word = words[0];
                if (strlen(word) != 1)
                    explainMotif();
                if ((baseVal = ntVal[(int)word[0]]) < 0)
                    explainMotif();
                if (gotNt[baseVal])
                    errAbort("Got more than one row for %s", word);
                gotNt[baseVal] = TRUE;
                if (profLen > 0 && profLen != wordCount-1)
                    errAbort("Not all lines in motif have the same number of numbers. Aborting.");
                profLen = wordCount-1;
                for (j=0; j<profLen; ++j)
                    {
                    word = words[j+1];
                    if (!isdigit(word[0]))
                        explainMotif();
                    oneProb = atof(word);
                    probs[baseVal][j] = oneProb;
                    }
                }
            }
        ++lineIx;
        if (gotNt[0] && gotNt[1] && gotNt[2] && gotNt[3])
            break;
        }

    /* Check that they have filled in values for all four nucleotides
     * and if necessary location. */
    if (!(gotNt[0] && gotNt[1] && gotNt[2] && gotNt[3]))
        errAbort("No data for nucleotide %c", valToNt[i]);
    if (useLocation && !gotLocation)
        errAbort("You need to include location information, or select the Ignore Location check box\n");

    /* Convert probability array into a profile. */
    prof = blankProfile(profLen);
    for (i=0, col = prof->columns; i<profLen; ++i, col = col->next)
        {
        double total = 0;
        double x;
        for (j=0; j<4; ++j)
            total += probs[j][i];
        if (total <= 0.00001)
            errAbort("Column too close to zero");
        for (j=0; j<4; ++j)
            {
            x = probs[j][i]/total;
            if (x <= 0.000000001)
                x = 0.000000001;
            col->slogProb[j] = slog(x);
            }
        }
    prof->locale.mean = mean;
    prof->locale.standardDeviation = variance;
    slAddTail(&profList, prof);

    }
return profList;
#undef maxLen
}

void mmDoMiddle()
/* Do main work of Motif Matcher - read in motifs user has supplied and
 * display them. */
{
char motifVarName[24];
char *rawMotifs[6];
char *rm;
int motifCount = 0;
int i;
struct profile *profList = NULL, *prof;
char *faName;
int seqCount;
int seqElSize;
struct seqList *seqList;
int nameSize;
boolean considerRc = cgiBoolean("rcToo");
char *motifOutName;
FILE *motifOutFile = NULL;
char *motifInName;
char *seqFileName, *backFileName;
char *hitFileName = cgiOptionalString("hits");
FILE *hitFile = NULL;
boolean hitTrombaFormat = cgiBoolean("hitTrombaFormat");
int profIx = 0;

if ((motifOutName = cgiOptionalString("motifOutput")) != NULL)
    motifOutFile = mustOpen(motifOutName, "w");
fprintf(htmlOut, "<TT><PRE>");

/* Get some CGI variables. */
if (cgiVarExists("maxOcc"))
    maxOcc = cgiInt("maxOcc");
useLocation = !cgiBoolean("ignoreLocation");


/* Get input sequence from file.  If necessary copy pasted
 * in sequence to file first. */
seqFileName = cgiOptionalString("seqFile");
if (seqFileName == NULL)
    seqFileName = cgiOptionalString("good");
if (seqFileName == NULL)
    {
    pasteToFa("seq", &faName, &seqCount, &seqElSize);
    seqFileName = faName;
    }
seqList = readSeqList(seqFileName);
nameSize = maxSeqNameSize(seqList);

/* Set up background model. */
backFileName = cgiOptionalString("bad");
if (backFileName == NULL)
    backFileName = seqFileName;
nullModel = nmMark0;
if (cgiVarExists("background"))
    nullModel = cgiOneChoice("background", nullModelChoices, ArraySize(nullModelChoices));
getNullModel(seqFileName, backFileName, FALSE);

/* Make all sequences the same size and fill in
 * soft mask table. */
seqElSize = uniformSeqSize(seqList, TRUE, nullModel);

/* Read in motifs. */
if ((motifInName = cgiOptionalString("motifs")) != NULL)
    {
    char *buf;
    size_t size;
    readInGulp(motifInName, &buf, &size);
    if ((prof = parseMotifs(buf, useLocation)) != NULL)
        profList = slCat(profList, prof);
    freeMem(buf);
    }
else
    {
    /* Collect the motifs that are actually present into an array. */
    for (i=1; i<=ArraySize(rawMotifs); ++i)
        {
        snprintf(motifVarName, sizeof(motifVarName), "motif%d", i);
        rawMotifs[motifCount] = rm = cgiString(motifVarName);
        if ((prof = parseMotifs(rm, useLocation)) != NULL)
            {
            ++motifCount;
            profList = slCat(profList, prof);
            }
        }
    }
motifCount = slCount(profList);
if (motifCount == 0)
    errAbort("No motifs entered.");

/* Possibly open hit file. */
if (hitFileName != NULL)
    hitFile = mustOpen(hitFileName, "w");

/* Run the motifs once to figure out where they land and
 * what the score is. */
for (prof = profList; prof != NULL; prof = prof->next)
    {
    struct profile *rcProf = NULL;
    if (considerRc)
        rcProf = rcProfileCopy(prof);
    prof->score = iterateProfile(prof, rcProf, seqList, seqElSize,
    	NULL, NULL, FALSE, hitFile, ++profIx, hitTrombaFormat);
    horizontalLine();
    printProfile(htmlOut, prof);
    if (motifOutFile)
        printProfile(motifOutFile, prof);
    showProfHits(prof, rcProf, seqList, seqElSize, nameSize, &prof->bestIndividualScore);
    freeProfile(&rcProf);
    }
horizontalLine();
colorProfiles(profList, seqList, seqElSize, considerRc);
}

void doMiddle()
/* Print out body of HTML.  Decide whether to
 * run as improbizer or as motif matcher. */
{
if (isMotifMatcher)
    mmDoMiddle();
else
    impDoMiddle();
}

int main(int argc, char *argv[])
{
char *programName;

//pushCarefulMemHandler();


initProfileMemory();
dnaUtilOpen();
statUtilOpen();

isFromWeb = cgiIsOnWeb();

if (!isFromWeb && !cgiSpoof(&argc, argv))
    {
    errAbort("ameme - find common patterns in DNA\n"
             "usage\n"
             "    ameme good=goodIn.fa [bad=badIn.fa] [numMotifs=2] [background=m1] [maxOcc=2] [motifOutput=fileName] [html=output.html] [gif=output.gif] [rcToo=on] [controlRun=on] [startScanLimit=20] [outputLogo] [constrainer=1]\n"
             "where goodIn.fa is a multi-sequence fa file containing instances\n"
	     "of the motif you want to find, badIn.fa is a file containing similar\n"
	     "sequences but lacking the motif, numMotifs is the number of motifs\n"
	     "to scan for, background is m0,m1, or m2 for various levels of Markov\n"
	     "models, maxOcc is the maximum occurrences of the motif you \n"
	     "expect to find in a single sequence and motifOutput is the name \n"
             "of a file to store just the motifs in. rcToo=on searches both strands.\n"
             "If you include controlRun=on in the command line, a random set of \n"
             "sequences will be generated that match your foreground data set in size, \n"
             "and your background data set in nucleotide probabilities. The program \n"
             "will then look for motifs in this random set. If the scores you get in a \n"
             "real run are about the same as those you get in a control run, then the motifs\n"
             "Improbizer has found are probably not significant.\n");
    }

outputLogo = cgiVarExists("outputLogo");
/* Figure out where to put html output. */
if (cgiVarExists("html"))
    {
    char *fileName = cgiString("html");
    htmlOut = mustOpen(fileName, "w");
    }
else
    htmlOut = stdout;

initRandom(); /* This one needs to be after htmlOut set up. */

/* Figure out if we're going to find a pattern in sequences, or just display
 * where a pre-computed pattern occurs in sequences. */
isMotifMatcher = cgiVarExists("motifMatcher");
if (isMotifMatcher)
    programName = "Motif Matcher";
else
    programName = "Improbizer";

/* Print out html header.  Make background color brilliant white. */
if (isFromWeb)
    puts("Content-Type:text/html\n");
fprintf(htmlOut, "<HEAD>\n<TITLE>%s Results</TITLE>\n</HEAD>\n\n", programName);
fprintf(htmlOut, "<BODY BGCOLOR='#FFFFFF'>\n\n");

/* Wrap error handling et. around doMiddle. */
if (isFromWeb)
    htmEmptyShell(doMiddle, NULL);
else
   doMiddle();

//carefulCheckHeap();

/* Write end of html. */
htmEnd(htmlOut);
return 0;
}
