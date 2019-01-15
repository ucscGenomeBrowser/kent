/* lav.c - common lav file reading routines */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "lav.h"

static void unexpectedEof(struct lineFile *lf)
/* Squawk about unexpected end of file. */
{
errAbort("Unexpected end of file in %s", lf->fileName);
}

static void seekEndOfStanza(struct lineFile *lf)
/* find end of stanza */
{
char *line;
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        unexpectedEof(lf);
    if (line[0] == '}')
        break;
    }
}

static char *needNextWord(struct lineFile *lf, char **pLine)
/* Get next word from line or die trying. */
{
char *word = nextWord(pLine);
if (word == NULL)
   errAbort("Short line %d of %s\n", lf->lineIx, lf->fileName);
return word;
}

static char *justChrom(char *s)
/* Simplify mongo nib file thing in axt. */
{
char *e = stringIn(".nib:", s);
if (e == NULL)
    return s;
*e = 0;
e = strrchr(s, '/');
if (e == NULL)
    return s;
else
    return e+1;
}

void parseS(struct lineFile *lf, int *tSize, int *qSize)
/* Parse s stanza and return tSize and qSize */
{
char *words[3];
if (!lineFileRow(lf, words))
    unexpectedEof(lf);
*tSize = lineFileNeedNum(lf, words, 2);
if (!lineFileRow(lf, words))
    unexpectedEof(lf);
*qSize = lineFileNeedNum(lf, words, 2);
seekEndOfStanza(lf);
}

void parseH(struct lineFile *lf,  char **tName, char **qName, boolean *isRc)
/* Parse out H stanza */
{
char *line, *word, *e;
int i;


/* Set return variables to default values. */
freez(qName);
freez(tName);
*isRc = FALSE;

for (i=0; ; ++i)
    {
    if (!lineFileNext(lf, &line, NULL))
       unexpectedEof(lf);
    if (line[0] == '#')
       continue;
    if (line[0] == '}')
       {
       if (i < 2)
	   errAbort("Short H stanza line %d of %s", lf->lineIx, lf->fileName);
       break;
       }
    word = needNextWord(lf, &line);
    word++;  /* Skip over `"' and optional `>' */
    if (*word == '>')
	word++;
    e = strchr(word, '"');
    if (e != NULL) 
        {
	*e = 0;
	if (line != NULL)
	    ++line;
	}
    if (i == 0)
        *tName = cloneString(justChrom(word));
    else if (i == 1)
        *qName = cloneString(justChrom(word));
    if ((line != NULL) && (stringIn("(reverse", line) != NULL))
        *isRc = TRUE;
    }
}

void parseD(struct lineFile *lf, char **matrix, char **command, FILE *f)
/* Parse d stanza and return matrix and blastz command line */
{
char *line, *words[64];
int i, size, wordCount = 0;
struct axtScoreScheme *ss = NULL;
freez(matrix);
freez(command);
if (!lineFileNext(lf, &line, &size))
   unexpectedEof(lf);
if (stringIn("lastz",line))
    {
    stripChar(line,'"');
    wordCount = chopLine(line, words);
    fprintf(f, "##aligner=%s",words[0]);
    for (i = 3 ; i <wordCount ; i++)
        fprintf(f, " %s ",words[i]);
    fprintf(f,"\n");
    char aligner[strlen(words[0])+1];
    safecpy(aligner, sizeof aligner, words[0]);
    ss = axtScoreSchemeReadLf(lf);
    axtScoreSchemeDnaWrite(ss, f, aligner);
    }
seekEndOfStanza(lf);
}

struct block *removeFrayedEnds(struct block *blockList)
/* Remove zero length blocks at start and/or end to 
 * work around blastz bug. */
{
struct block *block = blockList;
struct block *lastBlock = NULL;
if (block != NULL && block->qStart == block->qEnd)
    {
verbose(2,"removeFrayedEnds: first block: %d == %d\n",
  block->qStart, block->qEnd);
    blockList = blockList->next;
    freeMem(block);
    }
for (block = blockList; block != NULL; block = block->next)
    {
    if (block->next == NULL)  /* Last block in list */
        {
	if (block->qStart == block->qEnd)
	    {
verbose(2,"removeFrayedEnds:\n");
	    if (lastBlock == NULL)  /* Only block on list. */
		blockList = NULL;
	    else
	        lastBlock->next = NULL;
	    }
	}
    lastBlock = block;
    }
if (lastBlock != NULL && lastBlock->qStart == lastBlock->qEnd)
    freeMem(lastBlock);
return blockList;
}
