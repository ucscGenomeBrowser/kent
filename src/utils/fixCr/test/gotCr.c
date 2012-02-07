#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"


boolean faReadNext(FILE *f, char *defaultName, boolean mustStartWithComment, 
    char **retCommentLine, struct dnaSeq **retSeq)
/* Read next sequence from .fa file. Return sequence in retSeq.  If retCommentLine is non-null
 * return the '>' line in retCommentLine.   The whole thing returns FALSE at end of file. */
{
char lineBuf[512];
int lineSize;
char *words[1];
int c;
long offset = ftell(f);
size_t dnaSize = 0;
DNA *dna, *sequence, b;
int bogusChars = 0;
char *name = defaultName;

if (name == NULL)
    name = "";
dnaUtilOpen();
if (retCommentLine != NULL)
    *retCommentLine = NULL;
*retSeq = NULL;

/* Skip first lines until it starts with '>' */
for (;;)
    {
    if(fgets(lineBuf, sizeof(lineBuf), f) == NULL)
        {
        return FALSE;
        }
    lineSize = strlen(lineBuf);
    if (lineBuf[0] == '>')
        {
	if (retCommentLine != NULL)
            *retCommentLine = cloneString(lineBuf);
        offset = ftell(f);
        chopByWhite(lineBuf, words, ArraySize(words));
        name = words[0]+1;
        break;
        }
    else if (!mustStartWithComment)
        {
        fseek(f, offset, SEEK_SET);
        break;
        }
    else
        offset += lineSize;
    }
/* Count up DNA. */
for (;;)
    {
    c = fgetc(f);
    if (c == EOF || c == '>')
        break;
    if (!isspace(c) && !isdigit(c))
        {
        ++dnaSize;
        }
    }

/* Allocate DNA and fill it up from file. */
dna = sequence = needLargeMem(dnaSize+1);
fseek(f, offset, SEEK_SET);
for (;;)
    {
    c = fgetc(f);
    if (c == EOF || c == '>')
        break;
    if (!isspace(c) && !isdigit(c))
        {
        if ((b = ntChars[c]) == 0)
            b = 'n';
        *dna++ = b;
        }
    }
if (c == '>')
    ungetc(c, f);
*dna = 0;

*retSeq = newDnaSeq(sequence, dnaSize, name);
return TRUE;
}


struct dnaSeq *faReadOneDnaSeq(FILE *f, char *defaultName, boolean mustStartWithComment)
/* Read sequence from FA file. Assumes positioned at or before
 * the '>' at start of sequence. */  
{
struct dnaSeq *seq;
if (!faReadNext(f, defaultName, mustStartWithComment, NULL, &seq))
    return NULL;
else
    return seq;
}


struct dnaSeq *faReadDna(char *fileName)
/* Open fa file and read a single sequence from it. */
{
FILE *f;
struct dnaSeq *seq;
f = mustOpen(fileName, "rb");
seq = faReadOneDnaSeq(f, fileName, FALSE);
fclose(f);
return seq;
}

struct dnaSeq *faReadAllDna(char *fileName)
/* Return list of all sequences in FA file. */
{
FILE *f;
struct dnaSeq *seqList = NULL, *seq;

f = mustOpen(fileName, "rb");
while ((seq = faReadOneDnaSeq(f, NULL, TRUE)) != NULL)
    slAddHead(&seqList, seq);
fclose(f);
slReverse(&seqList);
return seqList;
}

struct dnaSeq *faFromMemText(char *text)
/* Return a sequence from a .fa file that's been read into
 * a string in memory. This cannabalizes text, which should
 * be allocated with needMem.  This buffer becomes part of
 * the returned dnaSeq, which may be freed normally with
 * freeDnaSeq. */
{
char *name = "";
char *s, *d;
struct dnaSeq *seq;
int size = 0;
char c;

if (text[0] == '>')
    {
    char *end;
    s = strchr(text, '\n') + 1;
    name = text+1;
    end = skipToSpaces(name);
    if (end != NULL)
        *end = 0;
    }
else
    s = text;
AllocVar(seq);
seq->name = cloneString(name);
    
d = text;
while ((c = *s++) != 0)
    {
    if ((c = ntChars[c]) != 0) 
        {
        d[size++] = c;
        }
    }
d[size] = 0;

/* Put sequence into our little DNA structure. */
seq->dna = text;
seq->size = size;
return seq;
}


void faWriteNext(FILE *f, char *startLine, DNA *dna, int dnaSize)
/* Write next sequence to fa file. */
{
int dnaLeft = dnaSize;
int lineSize;
fprintf(f, ">%s\n", startLine);

while (dnaLeft > 0)
    {
    lineSize = dnaLeft;
    if (lineSize > 50)
        lineSize = 50;
    mustWrite(f, dna, lineSize);
    fputc('\n', f);
    dna += lineSize;
    dnaLeft -= lineSize;
    }
}

void faWrite(char *fileName, char *startLine, DNA *dna, int dnaSize)
/* Write out FA file or die trying. */
{
FILE *f = mustOpen(fileName, "w");
faWriteNext(f, startLine, dna, dnaSize);
fclose(f);
}