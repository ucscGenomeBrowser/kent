/* hgYeastRegCode - Load files from the regulatory code paper 
 * (large scale CHIP-CHIP on yeast) into database. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "hgRelate.h"
#include "obscure.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgYeastRegCode - Load files from the regulatory code paper (large scale \n"
  "CHIP-CHIP on yeast) into database\n"
  "usage:\n"
  "   hgYeastRegCode motifGffDir Final_InTableS2_v24.motifs probe.gff Conditions_Summary.txt outputMotif.bed output.motifs outputProbe.bed outputConditions.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int romanToArabicChrom(char *roman, struct lineFile *lf)
/* Convert chromosome from roman numeral to a regular number. */
{
static char *chromNames[16] = {"I", "II", "III", "IV", "V", "VI", 
	"VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XVI"};
int chromIx = stringArrayIx(roman, chromNames, ArraySize(chromNames));
if (chromIx < 0)
    errAbort("Unrecognized chromosome line %d of %s", lf->lineIx, lf->fileName);
return chromIx;
}

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

struct hash *makeMotifBed(char *gffDir, char *outBed)
/* Make bed file from GFFs.  Return hash of transcription factors. */
{
static char *consLevelPath[3] = {"3", "2", "0"};
static char *consLevelBed[3] = {"2", "1", "0"};
static char *pLevelPath[3] = {"p001b", "p005b", "nobind"};
static char *pLevelBed[3] = {"good", "weak", "none"};
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
       char *row[10];
       char fileName[PATH_LEN];
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
	    chromIx = romanToArabicChrom(row[0], lf);
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

void chopOff(char *s, char c)
/* Chop string at last occurence of char c. */
{
s = strrchr(s, c);
if (s != NULL)
    *s = 0;
}

struct tfBinding
/* A transcription factor and it's binding probability. */
    {
    struct tfBinding *next;
    char *tf;	/* Transcription factor. */
    double binding;	/* Binding val. */
    };

struct hash *makeProbeBed(char *inGff, char *outBed)
/* Convert probe location GFF file to BED. */
{
struct lineFile *lf = lineFileOpen(inGff, TRUE);
char *row[9];
struct hash *hash = newHash(16);
FILE *f = mustOpen(outBed, "w");
while (lineFileNextRowTab(lf, row, ArraySize(row)))
    {
    int chromIx = romanToArabicChrom(row[0], lf);
    int start = lineFileNeedNum(lf, row, 3) - 1;
    int end = lineFileNeedNum(lf, row, 4);
    char *s = row[8];
    char *orf, *note; 
    char *boundAt = "Bound at ";
    struct tfBinding *tfbList = NULL, *tfb;
    if (!startsWith("Probe ", s))
        errAbort("Expecting 9th column to start with 'Probe ' line %d of %s",
		lf->lineIx, lf->fileName);
    nextWord(&s); // skip Probe
    orf = nextWord(&s);
    chopOff(orf, ';');
    note = nextWord(&s);
    if (!sameWord("Note", note))
        errAbort("Expecting 'note' in 9th column line %d of %s", 
		lf->lineIx, lf->fileName);
    s = skipLeadingSpaces(s);
    if (!parseQuotedString(s, s, NULL))
        errAbort("Expecting quoted string in 9th column line %d of %s",
		lf->lineIx, lf->fileName);
    if (startsWith("Bad Probe", s))
        continue;
    else if (startsWith("Not bound", s))
        {
	/* Ok, we do nothing. */
	}
    else if (startsWith(boundAt, s))
	{
	while (s != NULL && startsWith(boundAt, s))
	    {
	    char *word, *by;
	    double binding;
	    s += strlen(boundAt);
	    word = nextWord(&s);
	    binding = atof(word);
	    by = nextWord(&s);
	    if (!sameString("by:", by))
	        errAbort("Expecting by: line %d of %s", lf->lineIx, lf->fileName);
	    while ((word = nextWord(&s)) != NULL)
		{
		char lastChar = 0, *e;
		e = word + strlen(word) - 1;
		lastChar = *e;
		if (lastChar == ';' || lastChar == ',')
		     *e = 0;
		AllocVar(tfb);
		tfb->binding = binding;
		tfb->tf = cloneString(word);
		slAddHead(&tfbList, tfb);
		if (lastChar == ';')
		     break;
		}
	    s = skipLeadingSpaces(s);
	    }
	slReverse(&tfbList);
	}
    else
        {
	errAbort("Expecting %s in note line %d of %s", boundAt, 
		lf->lineIx, lf->fileName);
	}
    fprintf(f, "chr%d\t%d\t%d\t", chromIx+1, start, end);
    fprintf(f, "%s\t%d\t", orf, slCount(tfbList));
    for (tfb = tfbList; tfb != NULL; tfb = tfb->next)
	fprintf(f, "%s,", tfb->tf);
    fprintf(f, "\t");
    for (tfb = tfbList; tfb != NULL; tfb = tfb->next)
        fprintf(f, "%4.3f,", tfb->binding);
    fprintf(f, "\n");
    hashAdd(hash, orf, NULL);
    }
lineFileClose(&lf);
carefulClose(&f);
return hash;
}

void makeConditions(char *input, char *output)
/* Parse input in form:
 *    transcriptionFactor   list, of, conditions
 * into
 *    transcriptionFactor<tab>list
 *    transcriptionFactor<tab>of
 *    transcriptionFactor<tab>conditions
 */    
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *tf, *cond;
    tf = nextWord(&line);
    while ((cond = nextWord(&line)) != NULL)
        {
	stripChar(cond, ',');
	fprintf(f, "%s\t%s\n", tf, cond);
	}
    }
carefulClose(&f);
lineFileClose(&lf);
}

void hgYeastRegCode(
	char *motifGffDir, char *inMotifs, char *probeGff, char *inConditions,
	char *outMotifBed, char *outMotifs, char *outProbe, char *outConditions)
/* hgYeastRegCode - Load files from the regulatory code paper 
 * (large scale CHIP-CHIP on yeast) into database. */
{
struct hash *tfHash = makeMotifBed(motifGffDir, outMotifBed);
makeMotifs(inMotifs, tfHash, outMotifs);
makeConditions(inConditions, outConditions);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 9)
    usage();
hgYeastRegCode(argv[1], argv[2], argv[3], argv[4], 
	argv[5], argv[6], argv[7], argv[8]);
return 0;
}
