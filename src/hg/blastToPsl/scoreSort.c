#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "psl.h"
#include "options.h"

int minScore = 0;
int maxIns = 100000000;
boolean singleQ = FALSE;
boolean swapScore = FALSE;

struct score
{
    struct score *next;
    char *tName;
    char *qName;
    char  strand;
    int   tStart, tEnd;
    int   qStart, qEnd;
    int   score;
    double eValue;
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "scoreSort - sort psl file by associated score file.\n"
  "usage:\n"
  "   scoreSort in.psl in.score out.psl out.score\n"
  "options:\n"
  "   -singleQ      only one hit per query\n"
  "   -maxIns=num   maximum number of tInsert\n"
  "   -swapScore    swap query and target in input score file\n"
  "   -minScore=num minimum score to get printed\n"
  );
}
int sortByScore(const void *e1, const void *e2)
{
struct score *score1 = *(struct score **)e1;
struct score *score2 = *(struct score **)e2;

return score2->score - score1->score;
}

void sortScore(char *pslIn,char *scoresIn,char *pslOut,char *scoreOut)
{
struct hash *qHash = NULL;
struct hash *pslHash = newHash(8);
struct lineFile *lf = lineFileOpen(scoresIn, TRUE);
char *words[9];
int wordCount;
struct psl *pslList, *psl;
struct score *scoreList = NULL, *score;
struct dyString *ds = newDyString(1024);
FILE *pslOutFile = mustOpen(pslOut, "w");
FILE *scoreOutFile = mustOpen(scoreOut, "w");

pslList = pslLoadAll(pslIn);

for(psl = pslList; psl != NULL; psl = psl->next)
    {
    dyStringClear(ds);
    dyStringPrintf(ds, "%s%c%s-%d-%d-%d-%d",psl->tName, psl->strand[1],
	    psl->qName, psl->tStart, psl->tEnd, psl->qStart, psl->qEnd);
    // hashAddUnique(pslHash, ds->string, psl);
    hashAdd(pslHash, ds->string, psl);
    }

while (lineFileRow(lf, words))
    {
    AllocVar(score);
    score->tName = cloneString(words[4]);
    score->strand = words[0][1];
    score->tStart = atoi(words[5]);
    score->tEnd = atoi(words[6]);
    score->qName = cloneString(words[1]);
    score->qStart = atoi(words[2]);
    score->qEnd = atoi(words[3]);
    score->score = atoi(words[7]);
    score->eValue = atof(words[8]);
    slAddHead(&scoreList, score);
    }

slSort(&scoreList, sortByScore);
if (singleQ)
    qHash = newHash(8);

for(score = scoreList ; score != NULL ; score = score->next)
    {
    dyStringClear(ds);
    if (swapScore)
	dyStringPrintf(ds, "%s%c%s-%d-%d-%d-%d",score->qName, score->strand,
		score->tName, score->qStart, score->qEnd, score->tStart,score->tEnd);
    else
	dyStringPrintf(ds, "%s%c%s-%d-%d-%d-%d",score->tName, score->strand,
		score->qName, score->tStart, score->tEnd, score->qStart,score->qEnd);
    psl = hashFindVal(pslHash, ds->string);
    if (psl == NULL)
	; // printf("%s not found", ds->string);
    else if ((psl->tBaseInsert < maxIns) && (score->score > minScore))
	{
	if (!singleQ ||  (hashFindVal(qHash, psl->qName) == NULL) )
	    {
	    pslTabOut(psl, pslOutFile);
	    fprintf(scoreOutFile, "%s\t%s\t%d\t%d\t%s\t%d\t%d\t%d\t%g\n", 
		    psl->strand,score->qName, score->qStart, score->qEnd,
		    score->tName,  score->tStart, score->tEnd,score->score, score->eValue);
	    if (singleQ)
		hashAddUnique(qHash, psl->qName, psl);
	    }
	}
    }
carefulClose(&pslOutFile);
carefulClose(&scoreOutFile);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
minScore = optionInt("minScore", minScore);
maxIns = optionInt("maxIns", maxIns);
singleQ = optionExists("singleQ");
swapScore = optionExists("swapScore");
if (argc != 5)
    usage();
sortScore(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
