/* motifSig - Combine info from multiple control runs and main improbizer run. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "improbRunInfo.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "motifSig - Combine info from multiple control runs and main improbizer run\n"
  "usage:\n"
  "   motifSig output.tab seqDir motifDir controlDir(s)\n"
  );
}

void countSeq(char *fileName, int *retSeqCount, int *retBaseCount)
/* Count bases and sequences in fa file. */
{
int seqCount = 0, baseCount = 0, oneSize;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
DNA *dna;
char *name;

while (faSpeedReadNext(lf, &dna, &oneSize, &name))
    {
    seqCount += 1;
    baseCount += oneSize;
    }

lineFileClose(&lf);
*retSeqCount = seqCount;
*retBaseCount = baseCount;
}

#define maxMotifSize 256

struct motif
/* Info on one motif. */
    {
    char consensus[maxMotifSize+1];  /* Consensus sequence. */
    float score;	/* Motif Score */
    float pos;          /* Average position. */
    float posSd;        /* Standard deviation of positions. */
    int size;	/* Number of columns in motif - up to maxMotifSize. */
    float profile[4][maxMotifSize];  /* Profile a,c,g,t probabilities. */
    };

boolean readMotif(struct lineFile *lf, struct motif *m)
/* Read five lines of motif info. */
{
char *line;
char *words[maxMotifSize+1];
int wordCount;
int i,j;
int colCount = 0;

/* Get first line and parse it. */
ZeroVar(m);
if (!lineFileNext(lf, &line, NULL))
    return FALSE;
wordCount = chopLine(line, words);
if (wordCount < 6 || !sameString(words[1], "@"))
    errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
m->score = atof(words[0]);
m->pos = atof(words[2]);
m->posSd = atof(words[4]);
strncpy(m->consensus, words[5], sizeof(m->consensus));

/* Get next lines with columns. */
for (i=0; i<4; ++i)
    {
    if (!lineFileNext(lf, &line, NULL))
        errAbort("Unexpected end of file in %s", lf->fileName);
    wordCount = chopLine(line, words);
    if (i == 0)
        m->size = colCount = wordCount - 1;
    else
        lineFileExpectWords(lf, colCount+1, wordCount);
    for (j=0; j<colCount; ++j)
        m->profile[i][j] = atof(words[j+1]);
    }
return TRUE;
}

struct improbRunInfo * analyseOneMotifRun(char *runName, char *seqDir,
    char *motifDir, int controlCount, char *controls[])
/* Bundle up data on one improbizer run and associated control runs. */
{
char fileName[512];
char motifName[256];
int seqCount, baseCount;
struct improbRunInfo *iriList = NULL, *iri;
struct lineFile *lf = NULL;
struct motif motif;
int motifIx = 0;
int i;
float acc, best, mean, x;

printf("%s\n", runName);

/* Count bases in sequences - this will be used in each iri. */
sprintf(fileName, "%s/%s.fa", seqDir, runName);
countSeq(fileName, &seqCount, &baseCount);

/* Allocate iri and read the main run. */
sprintf(fileName, "%s/%s", motifDir, runName);
lf = lineFileOpen(fileName, TRUE);
while (readMotif(lf, &motif))
    {
    AllocVar(iri);
    slAddTail(&iriList, iri);
    ++motifIx;
    snprintf(motifName, sizeof(motifName), "%s.%d", runName, motifIx);
    iri->name = cloneString(motifName);
    iri->seqCount = seqCount;
    iri->runScore = motif.score;
    iri->runPos = motif.pos;
    iri->runPosSd = motif.posSd;
    iri->columnCount = motif.size;
    iri->consensus = cloneString(motif.consensus);
    iri->aProb = CloneArray(motif.profile[0], motif.size);
    iri->cProb = CloneArray(motif.profile[1], motif.size);
    iri->gProb = CloneArray(motif.profile[2], motif.size);
    iri->tProb = CloneArray(motif.profile[3], motif.size);
    iri->controlCount = controlCount;
    AllocArray(iri->controlScores, controlCount);
    }
lineFileClose(&lf);

/* Read the control runs. */
for (i=0; i<controlCount; ++i)
    {
    sprintf(fileName, "%s/%s", controls[i], runName);
    lf = lineFileOpen(fileName, TRUE);
    for (iri = iriList; iri != NULL; iri = iri->next)
        {
	if (!readMotif(lf, &motif))
	    errAbort("%s doesn't contain the expected number of motifs", lf->fileName);
	iri->controlScores[i] = motif.score;
	}
    lineFileClose(&lf);
    }

/* Calculate best and mean on control runs. */
for (iri = iriList; iri != NULL; iri = iri->next)
    {
    acc = best = 0;
    for (i=0; i<controlCount; ++i)
        {
	x = iri->controlScores[i];
	acc += x;
	if (x > best)
	    best = x;
	}
    iri->bestControlScore = best;
    iri->meanControlScore = acc/controlCount;
    }

/* Calculate standard deviation of control runs. */
for (iri = iriList; iri != NULL; iri = iri->next)
    {
    acc = 0;
    mean = iri->meanControlScore;
    for (i=0; i<controlCount; ++i)
        {
	x = iri->controlScores[i] - mean;
	acc += x*x;
	}
    if (controlCount > 1)
        acc /= controlCount;
    iri->sdControlScore = sqrt(acc);
    }

return iriList;
}

void motifSig(char *outName, char *seqDir, char *motifDir, 
	int controlCount, char *controls[])
/* motifSig - Combine info from multiple control runs and main improbizer run. */
{
FILE *f = mustOpen(outName, "w");
struct slName *mfList = listDir(motifDir, "*"), *mf;
struct improbRunInfo *iriList = NULL,  *iriSmallList = NULL, *iri;

for (mf = mfList; mf != NULL; mf = mf->next)
    {
    iriSmallList = analyseOneMotifRun(mf->name, seqDir, motifDir, controlCount, controls);
    iriList = slCat(iriList, iriSmallList);
    }
for (iri = iriList; iri != NULL; iri = iri->next)
    improbRunInfoTabOut(iri, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 5)
    usage();
motifSig(argv[1], argv[2], argv[3], argc-4, argv+4);
return 0;
}
