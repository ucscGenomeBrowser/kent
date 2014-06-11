/* tokenizer - A tokenizer structure that will chop up file into
 * tokens.  It is aware of quoted strings and otherwise tends to return
 * white-space or punctuated-separated words, with punctuation in
 * a separate token.  This is used by autoSql. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errAbort.h"
#include "linefile.h"
#include "tokenizer.h"


struct tokenizer *tokenizerOnLineFile(struct lineFile *lf)
/* Create a new tokenizer on open lineFile. */
{
struct tokenizer *tkz;
AllocVar(tkz);
tkz->sAlloc = 128;
tkz->string = needMem(tkz->sAlloc);
tkz->lf = lf;
tkz->curLine = tkz->linePt = "";
return tkz;
}

struct tokenizer *tokenizerNew(char *fileName)
/* Return a new tokenizer. */
{
return tokenizerOnLineFile(lineFileOpen(fileName, TRUE));
}

void tokenizerFree(struct tokenizer **pTkz)
/* Tear down a tokenizer. */
{
struct tokenizer *tkz;
if ((tkz = *pTkz) != NULL)
    {
    freeMem(tkz->string);
    lineFileClose(&tkz->lf);
    freez(pTkz);
    }
}

void tokenizerReuse(struct tokenizer *tkz)
/* Reuse token. */
{
if (!tkz->eof)
    tkz->reuse = TRUE;
}

int tokenizerLineCount(struct tokenizer *tkz)
/* Return line of current token. */
{
return tkz->lf->lineIx;
}

char *tokenizerFileName(struct tokenizer *tkz)
/* Return name of file. */
{
return tkz->lf->fileName;
}

char *tokenizerNext(struct tokenizer *tkz)
/* Return token's next string (also available as tkz->string) or
 * NULL at EOF. */
{
char *start, *end;
char c, *s;
int size;
if (tkz->reuse)
    {
    tkz->reuse = FALSE;
    return tkz->string;
    }
tkz->leadingSpaces = 0;
for (;;)	/* Skip over white space and comments. */
    {
    int lineSize;
    s = start = skipLeadingSpaces(tkz->linePt);
    tkz->leadingSpaces += s - tkz->linePt;
    if ((c = start[0]) != 0)
	{
	if (tkz->uncommentC && c == '/')
	     {
	     if (start[1] == '/')
		 ;  /* Keep going in loop effectively ignoring rest of line. */
	     else if (start[1] == '*')
		 {
		 start += 2;
		 for (;;)
		     {
		     char *end = stringIn("*/", start);
		     if (end != NULL)
			  {
			  tkz->linePt = end+2;
			  break;
			  }
		     if (!lineFileNext(tkz->lf, &tkz->curLine, &lineSize))
			  errAbort("End of file (%s) in comment", tokenizerFileName(tkz));
		     start = tkz->curLine;
		     }
		 continue;
		 }
	     else
		 break;
	     }
	else if (tkz->uncommentShell && c == '#')
	     ;  /* Keep going in loop effectively ignoring rest of line. */
	else
	    break;	/* Got something real. */
	}
    if (!lineFileNext(tkz->lf, &tkz->curLine, &lineSize))
	{
	tkz->eof = TRUE;
	return NULL;
	}
    tkz->leadingSpaces += 1;
    tkz->linePt = tkz->curLine;
    }
if (isalnum(c) || (c == '_'))
    {
    for (;;)
	{
        s++;
	if (!(isalnum(*s) || (*s == '_')))
	    break;
	}
    end = s;
    }
else if (c == '"' || c == '\'')
    {
    char quot = c;
    if (tkz->leaveQuotes)
	start = s++;
    else
	start = ++s;
    for (;;)
	{
	c = *s;
	if (c == quot)
	    {
	    if (s[-1] == '\\')
		{
		if (s >= start+2 && s[-2] == '\\')
		    break;
		}
	    else
		break;
	    }
	else if (c == 0)
	    {
	    break;
	    }
	++s;
	}
    end = s;
    if (c != 0)
	++s;
    if (tkz->leaveQuotes)
	end += 1;
    }
else
    {
    end = ++s;
    }
tkz->linePt = s;
size = end - start;
if (size >= tkz->sAlloc)
    {
    tkz->sAlloc = size+128;
    tkz->string = needMoreMem(tkz->string, 0, tkz->sAlloc);
    }
memcpy(tkz->string, start, size);
tkz->string[size] = 0;
return tkz->string;
}


void tokenizerErrAbort(struct tokenizer *tkz, char *format, ...)
/* Print error message followed by file and line number and
 * abort. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
errAbort("line %d of %s:\n%s", 
	tokenizerLineCount(tkz), tokenizerFileName(tkz), tkz->curLine);
}

void tokenizerNotEnd(struct tokenizer *tkz)
/* Squawk if at end. */
{
if (tkz->eof)
    errAbort("Unexpected end of file");
}

char *tokenizerMustHaveNext(struct tokenizer *tkz)
/* Get next token, which must be there. */
{
char *s = tokenizerNext(tkz);
if (s == NULL)
    errAbort("Unexpected end of file");
return s;
}

void tokenizerMustMatch(struct tokenizer *tkz, char *string)
/* Require next token to match string.  Return next token
 * if it does, otherwise abort. */
{
if (sameWord(tkz->string, string))
    tokenizerMustHaveNext(tkz);
else
    tokenizerErrAbort(tkz, "Expecting %s got %s", string, tkz->string);
}

