/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "linefile.h"

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
	    {
            b = 'n';
	    }
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

dnaUtilOpen();
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
    if (isspace(c) || isdigit(c))
	continue;
    if ((c = ntChars[c]) != 0) 
        d[size++] = c;
    else
	d[size++] = 'n';
    }
d[size] = 0;

/* Put sequence into our little DNA structure. */
seq->dna = text;
seq->size = size;
return seq;
}

struct dnaSeq *faReadDna(char *fileName)
/* Open fa file and read a single sequence from it. */
{
int maxSize = fileSize(fileName);
int size = 0;
int fd;
DNA *s;
struct dnaSeq *seq;

if (maxSize < 0)
    errAbort("can't open %s", fileName);
s = needLargeMem(maxSize+1);
fd = open(fileName, O_RDONLY);
read(fd, s, maxSize);
close(fd);
s[maxSize] = 0;
return faFromMemText(s);
}

static int faFastBufSize = 0;
static DNA *faFastBuf;

static void expandFaFastBuf(int bufPos)
/* Make faFastBuf bigger. */
{
if (faFastBufSize == 0)
    {
    faFastBufSize = 64 * 1024;
    faFastBuf = needLargeMem(faFastBufSize);
    }
else
    {
    DNA *newBuf;
    int newBufSize = faFastBufSize + faFastBufSize;
    newBuf = needLargeMem(newBufSize);
    memcpy(newBuf, faFastBuf, bufPos);
    freeMem(faFastBuf);
    faFastBuf = newBuf;
    faFastBufSize = newBufSize;
    }
}

void faFreeFastBuf()
/* Free up buffers used in fa fast and speedreading. */
{
freez(&faFastBuf);
faFastBufSize = 0;
}

boolean faFastReadNext(FILE *f, DNA **retDna, int *retSize, char **retName)
/* Read in next FA entry as fast as we can. Return FALSE at EOF. 
 * The returned DNA and name will be overwritten by the next call
 * to this function. */
{
int c;
int bufIx = 0;
static char name[256];
int nameIx = 0;
boolean gotSpace = FALSE;

/* Seek to next '\n' and save first word as name. */
dnaUtilOpen();
name[0] = 0;
for (;;)
    {
    if ((c = fgetc(f)) == EOF)
        {
        *retDna = NULL;
        *retSize = 0;
        *retName = NULL;
        return FALSE;
        }
    if (!gotSpace && nameIx < ArraySize(name)-1)
        {
        if (isspace(c))
            gotSpace = TRUE;
        else if (c != '>')
            {
            name[nameIx++] = c;
            }
        }
    if (c == '\n')
        break;
    }
name[nameIx] = 0;
/* Read until next '>' */
for (;;)
    {
    c = fgetc(f);
    if (isspace(c) || isdigit(c))
        continue;
    if (c == EOF || c == '>')
        c = 0;
    else
	{
	c = ntChars[c];
	if (c == 0) c = 'n';
	}
    if (bufIx >= faFastBufSize)
	expandFaFastBuf(bufIx);
    faFastBuf[bufIx++] = c;
    if (c == 0)
        {
        *retDna = faFastBuf;
        *retSize = bufIx-1;
        *retName = name;
        return TRUE;
        }
    }
}


void faWriteNext(FILE *f, char *startLine, DNA *dna, int dnaSize)
/* Write next sequence to fa file. */
{
int dnaLeft = dnaSize;
int lineSize;
if (startLine != NULL)
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

boolean faSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName)
/* Read in next FA entry as fast as we can. Faster than that old,
 * pokey faFastReadNext. Return FALSE at EOF. 
 * The returned DNA and name will be overwritten by the next call
 * to this function. */
{
int c;
int bufIx = 0;
static char name[256];
int nameIx = 0;
boolean gotSpace = FALSE;
int lineSize, i;
char *line;

dnaUtilOpen();

/* Read first line, make sure it starts wiht '>', and read first word
 * as name of sequence. */
name[0] = 0;
if (!lineFileNext(lf, &line, &lineSize))
    {
    *retDna = NULL;
    *retSize = 0;
    return FALSE;
    }
if (line[0] == '>')
    {
    line = firstWordInLine(skipLeadingSpaces(line+1));
    if (line == NULL)
        errAbort("Expecting sequence name after '>' line %d of %s", lf->lineIx, lf->fileName);
    strncpy(name, line, sizeof(name));
    }
else
    {
    errAbort("Expecting '>' line %d of %s", lf->lineIx, lf->fileName);
    }
/* Read until next '>' */
for (;;)
    {
    if (!lineFileNext(lf, &line, &lineSize))
        break;
    if (line[0] == '>')
        {
	lineFileReuse(lf);
	break;
	}
    if (bufIx + lineSize >= faFastBufSize)
	expandFaFastBuf(bufIx);
    for (i=0; i<lineSize; ++i)
        {
	c = line[i];
	if (isalpha(c))
	    {
	    c = ntChars[c];
	    if (c == 0) c = 'n';
	    faFastBuf[bufIx++] = c;
	    }
	}
    }
if (bufIx >= faFastBufSize)
    expandFaFastBuf(bufIx);
faFastBuf[bufIx] = 0;
*retDna = faFastBuf;
*retSize = bufIx;
*retName = name;
return TRUE;
}

struct dnaSeq *faReadAllDna(char *fileName)
/* Return list of all sequences in FA file. */
{
struct lineFile *lf = lineFileOpen(fileName, FALSE);
struct dnaSeq *seqList = NULL, *seq;
DNA *dna;
char *name;
int size;

while (faSpeedReadNext(lf, &dna, &size, &name))
    {
    AllocVar(seq);
    seq->name = cloneString(name);
    seq->size = size;
    seq->dna = cloneMem(dna, size+1);
    slAddHead(&seqList, seq);
    }
lineFileClose(&lf);
slReverse(&seqList);
faFreeFastBuf();
return seqList;
}

