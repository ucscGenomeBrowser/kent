/* hgYeastRegCode - Load files from the regulatory code paper 
 * (large scale CHIP-CHIP on yeast) into database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "hgRelate.h"
#include "dnaMotif.h"

static char const rcsid[] = "$Id: hgYeastRegCode.c,v 1.2 2004/09/13 05:59:22 kent Exp $";

char *tmpDir = ".";
char *tableName = "transRegCode";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgYeastRegCode - Load files from the regulatory code paper (large scale \n"
  "CHIP-CHIP on yeast) into database\n"
  "usage:\n"
  "   hgYeastRegCode gffDir Final_InTableS2_v24.motifs output.bed output.motifs\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct yrc
/* Info about yeastRegulatoryCode record. */
    {
    struct yrc *next;
    int chromIx;		/* Which yeast chromosome. */
    int chromStart, chromEnd;	/* Bounds. */
    char *name;			/* Allocated elsewhere. */
    int pLevel;			/* Binding probability level. */
    int consLevel;		/* Conservation level. */
    };

struct hash *makeBed(char *gffDir, char *outBed)
/* Make bed file from GFFs.  Return hash of transcription factors. */
{
static char *consLevelPath[3] = {"3", "2", "0"};
static char *consLevelBed[3] = {"2", "1", "0"};
static char *pLevelPath[3] = {"p001b", "p005b", "nobind"};
static char *pLevelBed[3] = {"good", "weak", "none"};
static char *chromNames[16] = {"I", "II", "III", "IV", "V", "VI", 
	"VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XVI"};
int cIx, pIx;
FILE *f = mustOpen(outBed, "w");
struct hash *tfHash = newHash(0);
struct hash *yrcHash = newHash(18);
struct yrc *yrcList = NULL, *yrc;

for (cIx=0; cIx<3; ++cIx)
   {
   for (pIx=0; pIx<3; ++pIx)
       {
       struct lineFile *lf;
       char *line, *row[10];
       char fileName[PATH_LEN];
       int score = 1000 / (cIx + pIx + 1);
       char *pLevel = pLevelBed[pIx];
       char *consLevel = consLevelBed[cIx];
       char hashKey[256];

       safef(fileName, sizeof(fileName), "%s/IGR_v24.%s.%s.GFF",
       	   gffDir, consLevelPath[cIx], pLevelPath[pIx]);
       lf = lineFileOpen(fileName, TRUE);
       while (lineFileRow(lf, row))
            {
	    char *name = row[9];
	    char *e;
	    int chromIx, chromStart, chromEnd;
	    if (!sameWord(row[8], "Site"))
	        errAbort("Expecting 'Site' line %d of %s", lf->lineIx, lf->fileName);
	    e = strchr(name, ';');
	    if (e == NULL)
	        errAbort("Expecting semicolon line %d of %s", lf->lineIx, lf->fileName);
	    *e = 0;
	    chromIx = stringArrayIx(row[0], chromNames, ArraySize(chromNames));
	    if (chromIx < 0)
	        errAbort("Unrecognized chromosome line %d of %s", lf->lineIx, lf->fileName);
	    chromStart = lineFileNeedNum(lf, row, 3);
	    chromEnd = lineFileNeedNum(lf, row, 4);
	    safef(hashKey, sizeof(hashKey), "%s.%d.%d", name, chromIx, chromStart);
	    if ((yrc = hashFindVal(yrcHash, hashKey)) == NULL)
	        {
		AllocVar(yrc);
		yrc->chromIx= chromIx;
		yrc->chromStart = chromStart;
		yrc->chromEnd = chromEnd;
		yrc->name = hashStoreName(tfHash, name);
		yrc->pLevel = pIx;
		yrc->consLevel = cIx;
		hashAdd(yrcHash, hashKey, yrc);
		slAddHead(&yrcList, yrc);
		}
	    else
	        {
		if (pIx < yrc->pLevel)
		    yrc->pLevel = pIx;
		if (cIx < yrc->consLevel)
		    yrc->consLevel = cIx;
		}
	    }
       lineFileClose(&lf);
       }
   }
for (yrc = yrcList; yrc != NULL; yrc = yrc->next)
    {
    fprintf(f, "chr%d\t", yrc->chromIx+1);
    fprintf(f, "%d\t", yrc->chromStart);
    fprintf(f, "%d\t", yrc->chromEnd);
    fprintf(f, "%s\t", yrc->name);
    fprintf(f, "%d\t", (int)(1000/(yrc->pLevel + yrc->consLevel + 1)));
    fprintf(f, "%s\t", pLevelBed[yrc->pLevel]);
    fprintf(f, "%s\n", consLevelBed[yrc->consLevel]);
    }
carefulClose(&f);
hashFree(&yrcHash);
return tfHash;
}

boolean lineFileSkipTo(struct lineFile *lf, char *start)
/* Keep going until find a line that starts with start.
 * REturn FALSE at EOF. */
{
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith(start, line))
        return TRUE;
    }
return FALSE;
}

void badFormat(struct lineFile *lf)
/* Complain that format looks off. */
{
errAbort("Bad format line %d of %s", lf->lineIx, lf->fileName);
}

void readBaseProbs(struct lineFile *lf, char **words,
	char *firstWord, float **pArray, int colCount)
/* Allocate and read base probabilities. */
{
char *line;
int wordCount;
float *array;
int i;

lineFileNeedNext(lf, &line, NULL);
wordCount = chopByWhite(line, words, colCount+1);
lineFileExpectWords(lf, colCount+1, wordCount);
if (!sameString(words[0], firstWord))
    errAbort("Expecting %s, got %s line %d of %s", firstWord, words[0], 
    	lf->lineIx, lf->fileName);
AllocArray(array, colCount);
for (i=0; i<colCount; ++i)
    array[i] = atof(words[i+1]);
*pArray = array;
}

void makeMotifs(char *inFile, struct hash *tfHash, char *outFile)
/* Parse input motifs and save them to outFile in dnaMotif format. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
boolean done = FALSE;
FILE *f = mustOpen(outFile, "w");
struct hashEl *hel;

for (;;)
    {
    char *line;
    char *words[256], *word;
    int wordCount;
    struct dnaMotif *motif;
    if (!lineFileSkipTo(lf, "Probability matrix for"))
        break;
    lineFileNeedNext(lf, &line, NULL);
    wordCount = chopLine(line, words);
    if (wordCount >= ArraySize(words))
        errAbort("Line %d of %s is too long\n", lf->lineIx, lf->fileName);
    if (!sameString(words[0], "#"))
        badFormat(lf);
    AllocVar(motif);
    motif->columnCount = wordCount-1;
    readBaseProbs(lf, words, "#A", &motif->aProb, motif->columnCount);
    readBaseProbs(lf, words, "#C", &motif->cProb, motif->columnCount);
    readBaseProbs(lf, words, "#T", &motif->tProb, motif->columnCount);
    readBaseProbs(lf, words, "#G", &motif->gProb, motif->columnCount);

    if (!lineFileSkipTo(lf, "Source:"))
	lineFileUnexpectedEnd(lf);
    lineFileReuse(lf);
    lineFileNeedNext(lf, &line, NULL);
    word = nextWord(&line);
    word = nextWord(&line);
    if (word == NULL)
        errAbort("Short Source: line %d of %s", lf->lineIx, lf->fileName);
    motif->name = cloneString(word);
    
    hel = hashLookup(tfHash, motif->name);
    if (hel == NULL)
        errAbort("%s in %s but not GFFs", motif->name, lf->fileName);
    hel->val = motif;
    dnaMotifTabOut(motif, f);
    }
carefulClose(&f);
lineFileClose(&lf);
}

void hgYeastRegCode(char *gffDir, char *inMotifs, char *outBed, char *outMotifs)
/* hgYeastRegCode - Load files from the regulatory code paper 
 * (large scale CHIP-CHIP on yeast) into database. */
{
struct hash *tfHash = makeBed(gffDir, outBed);
makeMotifs(inMotifs, tfHash, outMotifs);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
hgYeastRegCode(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
