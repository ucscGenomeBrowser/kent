/* pfToken - reads a file and produces paraTokens from
 * it. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "errabort.h"
#include "obscure.h"
#include "pfScope.h"
#include "pfToken.h"

void pfSourcePos(struct pfSource *source, char *pos, 
    char **retFileName, int *retLine, int *retCol)
/* Return file name, line, and column (zero based) of pos. */
{
int lineIx = 0, colIx = 0;
char *s = source->contents, *e;
char *endContents = s + source->contentSize;

if (pos < s || pos >= endContents)
    internalErr();
*retFileName = source->name;

for (;;)
    {
    e = strchr(s, '\n');
    if (e == NULL)
        e = endContents;
    else
        e += 1;
    if (pos >= s && pos < e)
        {
	*retLine = lineIx;
	*retCol = pos - s;
	return;
	}
    lineIx += 1;
    s = e;
    }
}

static void pfTokenFileLineCol(struct pfToken *tok, 
    char **retFileName, int *retLine, int *retCol)
/* Return file name, line, and column (zero based) of token start. */
{
pfSourcePos(tok->source, tok->text, retFileName, retLine, retCol);
}

static void errAtPos(struct pfTokenizer *tkz, char *format, ...)
/* Warn about error at given tkz->pos. */
{
int line, col;
char *fileName;
va_list args;
va_start(args, format);
pfSourcePos(tkz->source, tkz->pos, &fileName, &line, &col);
warn("Line %d col %d of %s", line+1, col+1, fileName);
vaErrAbort(format, args);
noWarnAbort();
va_end(args);
}

char *pfTokenAsString(struct pfToken *tok)
/* Return string representation of token.  FreeMem the string
 * when done. */
{
if (tok->type == pftEnd)
    return cloneString("end of file");
else
    return cloneStringZ(tok->text, tok->textSize);
}

void errAt(struct pfToken *tok, char *format, ...)
/* Warn about error at given token. */
{
int line, col;
char *fileName;
va_list args;
va_start(args, format);
pfTokenFileLineCol(tok, &fileName, &line, &col);
warn("Line %d col %d of %s near '%s'", line+1, col+1, fileName, pfTokenAsString(tok));
vaErrAbort(format, args);
noWarnAbort();
va_end(args);
}


void expectingGot(char *expecting, struct pfToken *got)
/* Complain about unexpected stuff and quit. */
{
errAt(got, "Expecting %s got %s", expecting, pfTokenAsString(got));
}


char *pfTokTypeAsString(enum pfTokType type)
/* Return string corresponding to pfTokType */
{
static char buf[16];
switch (type)
    {
    case pftWhitespace: return "pftWhitespace";
    case pftComment: return "pftComment";
    case pftName: return "pftName";
    case pftString: return "pftString";
    case pftSubstitute: return "pftSubstitute";
    case pftInt: return "pftInt";
    case pftFloat: return "pftFloat";
    case pftDotDotDot:    return "...";
    case pftPlusPlus:        return "++";
    case pftMinusMinus:      return "--";
    case pftPlusEquals:      return "+=";
    case pftMinusEquals:     return "-=";
    case pftDivEquals:       return "/=";
    case pftMulEquals:       return "*=";
    case pftEqualsEquals:    return "==";
    case pftNotEquals:       return "!=";
    case pftGreaterOrEquals: return ">=";
    case pftLessOrEquals:    return "<=";
    case pftLogAnd:          return "&&";
    case pftLogOr:           return "||";
    case pftShiftLeft:       return "<<";
    case pftShiftRight:      return ">>";
    case pftBreak: 	     return "pftBreak";
    case pftClass: 	     return "pftClass";
    case pftCase:	     return "pftCase";
    case pftCatch:	     return "pftCatch";
    case pftConst:	     return "pftConst";
    case pftContinue: 	     return "pftContinue";
    case pftElse: 	     return "pftElse";
    case pftExtends: 	     return "pftExtends";
    case pftFor: 	     return "pftFor";
    case pftFlow: 	     return "pftFlow";
    case pftGlobal:	     return "pftGlobal";
    case pftIf: 	     return "pftIf";
    case pftImport:	     return "pftImport";
    case pftIn:	             return "pftIn";
    case pftInclude:         return "pftInclude";
    case pftInto: 	     return "pftInto";
    case pftLocal:	     return "pftLocal";
    case pftNew:	     return "pftNew";
    case pftNil:             return "pftNil";
    case pftOf: 	     return "pftOf";
    case pftOperator:	     return "pftOperator";
    case pftPara: 	     return "pftPara";
    case pftPolymorphic:     return "pftPolymorphic";
    case pftPrivate:	     return "pftPrivate";
    case pftProtected:	     return "pftProtected";
    case pftReadable:        return "pftReadable";
    case pftReturn: 	     return "pftReturn";
    case pftStatic:          return "pftStatic";
    case pftTil: 	     return "pftTil";
    case pftTo: 	     return "pftTo";
    case pftTry:             return "pftTry";
    case pftWhile: 	     return "pftWhile";
    case pftWritable:        return "pftWritable";
    default:
	if (type < 256)
	    {
	    buf[0] = type;
	    buf[1] = 0;
	    return buf;
	    }
	else
	    {
	    safef(buf, sizeof(buf), "tok#%d", type);
	    return buf;
	    }
	break;
    }
}

struct pfSource *pfSourceNew(char *name, char *contents)
/* Create new pfSource. */
{
struct pfSource *source;
AllocVar(source);
source->name = cloneString(name);
source->contents = contents;
source->contentSize = strlen(contents);
return source;
}

struct pfSource *pfSourceOnFile(char *fileName)
/* Create new pfSource based around file */
{
char *contents;
readInGulp(fileName, &contents, NULL);
return pfSourceNew(fileName, contents);
}

struct pfTokenizer *pfTokenizerNew(struct hash *reservedWords)
/* Create tokenizer with given reserved words.  You'll need to
 * do a pfTokenizerNewSource() before using it much. */
{
struct pfTokenizer *tkz;
AllocVar(tkz);
tkz->lm = lmInit(0);
tkz->symbols = hashNew(16);
tkz->strings = hashNew(16);
tkz->dy = dyStringNew(0);
if (reservedWords == NULL)
    reservedWords = hashNew(1);
tkz->reserved = reservedWords;
return tkz;
}


void pfTokenizerNewSource(struct pfTokenizer *tkz, struct pfSource *source)
/*  Attatch new source to tokenizer. */
{
tkz->source = source;
tkz->pos = source->contents;
tkz->endPos = tkz->pos + source->contentSize;
}

static void tokSingleChar(struct pfTokenizer *tkz, struct pfToken *tok,
	char c)
/* Create single character token where type is same as c */
{
tok->type = c;
tok->val.c = c;
tok->textSize = 1;
tkz->pos += 1;
}

static void tokTwoChar(struct pfTokenizer *tkz, struct pfToken *tok,
     int type)
/* Create two character token of given type */
{
tok->type = type;
tok->textSize = 2;
tkz->pos += 2;
}

static void tokThreeChar(struct pfTokenizer *tkz, struct pfToken *tok,
     int type)
/* Create three character token of given type */
{
tok->type = type;
tok->textSize = 3;
tkz->pos += 3;
}
  
static void skipWhiteSpace(struct pfTokenizer *tkz)
/* Skip over white space */
{
char *pos = tkz->pos;
while (isspace(pos[0]))
    ++pos;
tkz->pos = pos;
}

static char *allWhiteToEol(char *s)
/* If rest of line is white space, return start of next
 * line, otherwise return NULL. */
{
char c;
for (;;)
    {
    c = *s++;
    if (!isspace(c))
        return NULL;
    if (c == '\n')
        return s;
    }
}

static boolean isoctal(char c)
/* Return TRUE if it's an octal digit. */
{
switch (c)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        return TRUE;
    default:
        return FALSE;
    }
}

static char translateEscape(char **s)
/*****************************************************************************
 * translate the character(s) after a '\' into a single character.
 ****************************************************************************/
{
char    *in_str;
short    counter;
char    inchar;
char    outchar;

in_str = *s;
switch (inchar = *in_str++)
    {
    case 'a':   outchar = '\a';  break;
    case 'b':   outchar = '\b';  break;
    case 'f':   outchar = '\f';  break;
    case 'n':   outchar = '\n';  break;
    case 'r':   outchar = '\r';  break;
    case 't':   outchar = '\t';  break;
    case 'v':   outchar = '\v';  break;
    case '?':   outchar = '\?';  break;
    case '\\':  outchar = '\\';  break;
    case '\'':  outchar = '\'';  break;
    case '\"':  outchar = '\"';  break;
    case 'x':
	outchar = 0;
	inchar = *in_str;
	while (isxdigit(inchar))
	    {
	    if (inchar <= '9')
		inchar -= '0';
	    else if (inchar <= 'F')
		inchar -= 'A' - 10;
	    else
		inchar -= 'a' - 10;
	    outchar = (outchar << 4) | inchar;
	    inchar = *++in_str;
	    }
	break;
    default:
	if (isdigit(inchar) && inchar != '8' && inchar != '9')
	    {
	    counter = 4;
	    outchar = 0;
	    while (--counter)
		{
		outchar = (outchar << 3) | (inchar & 0x07);
		inchar = *in_str;
		if (isoctal(inchar))
		    ++in_str;
		else
		    break;
		}
	    }
	else
	    outchar = inchar;
	break;
    }

*s = in_str;
return outchar;
}

static void finishQuote(struct pfTokenizer *tkz, struct pfToken *tok,
	boolean isSubstitute)
/* Finish up a quoted string */
{
if (!isSubstitute || strchr(tkz->dy->string, '$') == NULL)
    tok->type = pftString;
else
    tok->type = pftSubstitute;
tok->val.s = hashStoreName(tkz->strings, tkz->dy->string);
tok->textSize = tkz->pos - tok->text;
}

static void tokMultiLineQuote(struct pfTokenizer *tkz, struct pfToken *tok,
	char *endSym, boolean addLf, boolean isSubstitute)
/* Do multiple line quote.  There is no escaping in these. */
{
int endSymSize = strlen(endSym);
char *pos = stringIn(endSym, tkz->pos);
char *e;

if (pos != NULL)
    {
    dyStringAppendN(tkz->dy, tkz->pos,  pos - tkz->pos);
    if (addLf)
        dyStringAppendC(tkz->dy, '\n');
    tkz->pos = pos + endSymSize;
    finishQuote(tkz, tok, isSubstitute);
    }
else
    errAt(tok, "Unterminated line quote.");
}

static void tokString(struct pfTokenizer *tkz, struct pfToken *tok,
	char quoteC, boolean isSubstitute)
/* Create token up to end quote.  If quote is immediately
 * followed by a newline (or whitespace/newline) then
 * treat quote as line oriented. */
{
char *pos = tkz->pos+1;
char *nextLine = allWhiteToEol(pos);
dyStringClear(tkz->dy);
if (nextLine != NULL)
    {
    char endSym[3];
    endSym[0] = '\n';
    endSym[1] = quoteC;
    endSym[2] = 0;
    tkz->pos = nextLine;
    tokMultiLineQuote(tkz, tok, endSym, TRUE, isSubstitute);
    }
else
    {
    char c;
    for (;;)
        {
	c = *pos++;
	if (c == 0 || c == '\n')
	    break;
	if (c == quoteC)
	    {
	    tkz->pos = pos;
	    finishQuote(tkz, tok, isSubstitute);
	    return;
	    }
	if (c == '\\')
	    {
	    c = translateEscape(&pos);
	    }
	dyStringAppendC(tkz->dy, c);
	}
    errAt(tok, "Unterminated quote.");
    }
}

static void tokLiteralChar(struct pfTokenizer *tkz, struct pfToken *tok,
	char quoteC)
/* Tokenize single-quote bounded character into a small number. */
{
char *pos = tkz->pos+1;
char c;
c = *pos++;
if (c == '\\')
    c = translateEscape(&pos);
tok->val.i = c;
tok->type = pftInt;
c = *pos++;
if (c != quoteC)
    errAt(tok, "Missing closing %c", c);
tok->textSize = pos - tkz->pos;
tkz->pos = pos;
}

static void tokNameOrComplexQuote(struct pfTokenizer *tkz, 
	struct pfToken *tok)
/* Parse out name from c until first non-white-space.
 * If the name ends up being 'quote' then funnel it into
 * special delimited quotation handler. */
{
char *pos = tkz->pos;
struct dyString *dy = tkz->dy;
char c;

dyStringClear(dy);
for (;;)
    {
    c = *pos;
    if (c != '_' && !isalnum(c))
        break;
    dyStringAppendC(dy, c);
    pos += 1;
    }
tkz->pos = pos;

if (sameString(dy->string, "quote") || sameString(dy->string, "quot"))
    {
    boolean isSubstitute = sameString(dy->string, "quote");
    struct dyString *q = dyStringNew(0);
    skipWhiteSpace(tkz);  /* Skip spaces between "quote" and "to". */
    pos = tkz->pos;
    if (!(pos[0] == 't' && pos[1] == 'o' 
       && isspace(pos[2]) ))
        errAt(tok, "Unrecognized quote type (missing to?)");
    tkz->pos = pos + 2;
    skipWhiteSpace(tkz); /* Skip spaces between "to" and marker. */
    pos = tkz->pos;
    dyStringClear(q);
    for (;;)
        {
	c = *pos++;
	if (isspace(c))
	    break;
	dyStringAppendC(q, c);
	}
    tkz->pos = pos;
    if (q->stringSize < 1)
	errAt(tok, "quote without delimator string");
    dyStringClear(dy);
    tokMultiLineQuote(tkz, tok, q->string, FALSE, isSubstitute);
    dyStringFree(&q);
    }
else
    {
    tok->type = pftName;
    tok->val.s = hashStoreName(tkz->symbols, dy->string);
    }
tok->textSize = tkz->pos - tok->text;
}

static void skipLine(struct pfTokenizer *tkz)
/* Move tkz->pos to start of next line. */
{
char *e = strchr(tkz->pos, '\n');
if (e == NULL)
    e = tkz->endPos;
else
    e += 1;
tkz->pos = e;
}

static void finishFullComment(struct pfTokenizer *tkz, char *pos,
	struct dyString *endPat)
/* Search for match to end pat, and close off comment there. */
{
char *end = memMatch(endPat->string, endPat->stringSize, pos,
    tkz->endPos - tkz->pos)	;
if (end == NULL)
    errAtPos(tkz, "Unclosed comment.");
else
    {
    end += endPat->stringSize;
    tkz->pos = end;
    }
}

static void skipFullComment(struct pfTokenizer *tkz)
/* Set tkz->pos to past comment. */
{
char *pos = tkz->pos;
dyStringClear(tkz->dy);
if (pos[2] == '-')
    {
    pos += 3;	/* Skip over  "/*-" */
    /* Save to next white space */
    for (; pos<tkz->endPos; ++pos)
        {
	char c = *pos;
	if (isspace(c))
	   break;
	dyStringAppendC(tkz->dy, c);
	}
    dyStringAppend(tkz->dy, "-*/");
    }
else
    {
    pos += 2;
    dyStringAppend(tkz->dy, "*/");
    }
finishFullComment(tkz, pos, tkz->dy);
}


static void tokNumber(struct pfTokenizer *tkz, struct pfToken *tok, char c)
/* Suck up digits. */
{
char *pos = tkz->pos;
boolean isFloat = FALSE;
while (isdigit(c))
    c = *(++pos);
if (c == '.')
    {
    isFloat = TRUE;
    for (;;)
        {
	c = *(++pos);
	if (!isdigit(c))
	    break;
	}
    }
if (c == 'e' || c == 'E')
    {
    isFloat = TRUE;
    for (;;)
        {
	c = *(++pos);
	if (!isdigit(c))
	    break;
	}
    }
if (isFloat)
    {
    tok->val.x = atof(tkz->pos);
    tok->type = pftFloat;
    }
else
    {
    if (c == 'L')
	{
	tok->val.l = atoll(tkz->pos);
	tok->type = pftLong;
	pos += 1;
	}
    else
	{
	tok->val.i = atoi(tkz->pos);
	tok->type = pftInt;
	}
    }
tok->textSize = pos - tkz->pos;
tkz->pos = pos;
}

static void tokBinaryNumber(struct pfTokenizer *tkz, struct pfToken *tok)
/* Skip 0b, and suck up binary digits. */
{
char *pos = tkz->pos + 2;
unsigned long long l = 0;
int v;
char c;

for (;;)
   {
   c = *pos;
   switch (c)
       {
       case '0':
           v = 0; break;
       case '1':
           v = 1; break;
       default:
	   tok->val.i = l;
	   tok->type = pftInt;
	   tkz->pos = pos;
	   return;
       }
    l <<= 1;
    l += v;
    pos += 1;
    }
}

static void tokHexNumber(struct pfTokenizer *tkz, struct pfToken *tok)
/* Skip 0x, and suck up hex digits. */
{
char *pos = tkz->pos + 2;
unsigned long long l = 0;
int v;
char c;

for (;;)
   {
   c = *pos;
   switch (c)
       {
       case '0':
           v = 0; break;
       case '1':
           v = 1; break;
       case '2':
           v = 2; break;
       case '3':
           v = 3; break;
       case '4':
           v = 4; break;
       case '5':
           v = 5; break;
       case '6':
           v = 6; break;
       case '7':
           v = 7; break;
       case '8':
           v = 8; break;
       case '9':
           v = 9; break;
       case 'a':
       case 'A':
           v = 10; break;
       case 'b':
       case 'B':
           v = 11; break;
       case 'c':
       case 'C':
           v = 12; break;
       case 'd':
       case 'D':
           v = 13; break;
       case 'e':
       case 'E':
           v = 14; break;
       case 'f':
       case 'F':
           v = 15; break;
       default:
	   tok->val.i = l;
	   tok->type = pftInt;
	   tkz->pos = pos;
	   return;
       }
    l <<= 4;
    l += v;
    pos += 1;
    }
}




struct pfToken *pfTokenizerNext(struct pfTokenizer *tkz)
/* Puts next token in token.  Returns token type. For single
 * character tokens, token type is just the char value,
 * other token types start at 256. */
{
struct pfToken *tok;
char *pos;
char *s, c, c2;


/* Skip over white space and comments. */
for (;;)
    {
    skipWhiteSpace(tkz);
    if (tkz->pos >= tkz->endPos)
	return NULL;
    pos = tkz->pos;
    c = pos[0];
    c2 = pos[1];
    if (c == '/')
	{
	if (c2 == '/')
	    {
	    skipLine(tkz);
	    continue;
	    }
	else if (c2 == '*')
	    {
	    skipFullComment(tkz);
	    continue;
	    }
	}
    break;
    }


/* Allocate token structure. */
lmAllocVar(tkz->lm, tok);
tok->source = tkz->source;
tok->text = tkz->pos;

/* Decide what to do based on first letter or two. */
switch (c)
    {
    case '\'':
        tokString(tkz, tok, c, FALSE);
	break;
    case '"':
	tokString(tkz, tok, c, TRUE);
	break;
    case '/':
	if (c2 == '=')
	    tokTwoChar(tkz, tok, pftDivEquals);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '+':
	if (c2 == '+')
	    tokTwoChar(tkz, tok, pftPlusPlus);
	else if (c2 == '=')
	    tokTwoChar(tkz, tok, pftPlusEquals);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '.':
        if (c2 == '.' && pos[2] == '.')
	    tokThreeChar(tkz, tok, pftDotDotDot);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '-':
	if (c2 == '-')
	    tokTwoChar(tkz, tok, pftMinusMinus);
	else if (c2 == '=')
	    tokTwoChar(tkz, tok, pftMinusEquals);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '*':
	if (c2 == '=')
	    tokTwoChar(tkz, tok, pftMulEquals);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '=':
	if (c2 == '=')
	    tokTwoChar(tkz, tok, pftEqualsEquals);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '!':
        if (c2 == '=')
	    tokTwoChar(tkz, tok, pftNotEquals);
	else
	    tokSingleChar(tkz, tok, c);
        break;
    case '>':
        if (c2 == '=')
	    tokTwoChar(tkz, tok, pftGreaterOrEquals);
	else if (c2 == '>')
	    tokTwoChar(tkz, tok, pftShiftRight);
	else 
	    tokSingleChar(tkz, tok, c);
        break;
    case '<':
        if (c2 == '=')
	    tokTwoChar(tkz, tok, pftLessOrEquals);
	else if (c2 == '<')
	    tokTwoChar(tkz, tok, pftShiftLeft);
	else
	    tokSingleChar(tkz, tok, c);
        break;
    case '&':
        if (c2 == '&')
	    tokTwoChar(tkz, tok, pftLogAnd);
	else
	    tokSingleChar(tkz, tok, c);
        break;
    case '|':
        if (c2 == '|')
	    tokTwoChar(tkz, tok, pftLogOr);
	else
	    tokSingleChar(tkz, tok, c);
        break;

    case '_':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
	tokNameOrComplexQuote(tkz, tok);
	break;
    case '0':
	if (c2 == 'x')
	    tokHexNumber(tkz, tok);
	else if (c2 == 'b')
	    tokBinaryNumber(tkz, tok);
	else
	    tokNumber(tkz, tok, c);
	break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	tokNumber(tkz, tok, c);
	break;
    default:
	tokSingleChar(tkz, tok, c);
	break;
    }
if (tok->type == pftName)
    {
    int x = hashIntValDefault(tkz->reserved, tok->val.s, 0);
    if (x != 0)
        tok->type = x;
    }
tkz->tokenCount += 1;
// pfTokenDump(tok, uglyOut, TRUE);
return tok;
}


void pfTokenDump(struct pfToken *tok, FILE *f, boolean doLong)
/* Print info on token to file. */
{
int line, col;
char *fileName;
if (tok == NULL)
    {
    fprintf(f, "NULL token\n");
    return;
    }
else if (tok->type == pftEnd)
    {
    fprintf(f, "end of file\n");
    return;
    }
pfTokenFileLineCol(tok, &fileName, &line, &col);
if (doLong)
    fprintf(f, "file %s line %d, column %d, ", fileName, line, col);
fprintf(f, "type %s(%d), value ", pfTokTypeAsString(tok->type), tok->type);
switch (tok->type)
    {
    case pftName:
	fprintf(f, "%s", tok->val.s);
	break;
    case pftString:
    case pftSubstitute:
	fprintf(f, "\"%s\"", tok->val.s);
	break;
    case pftLong:
	fprintf(f, "%lld", tok->val.l);
	break;
    case pftInt:
	fprintf(f, "%d", tok->val.i);
	break;
    case pftFloat:
	fprintf(f, "%f", tok->val.x);
	break;
    default:
	mustWrite(f, tok->text, tok->textSize);
	break;
    }
fprintf(f, "\n");
}

struct pfToken *pfTokenGetClosingTok(struct pfToken *openTok,
	enum pfTokType openType, enum pfTokType closeType)
/* Given a token that represents an open glyph such as (, [ or {,
 * find token that represents the closing glyph ) ] or } .
 * This does handle nesting. */
{
struct pfToken *tok;
int depth = 0;
assert(openTok->type == openType);
for (tok = openTok; tok != NULL; tok = tok->next)
    {
    if (tok->type == openType)
        ++depth;
    else if (tok->type == closeType)
        {
	--depth;
	if (depth == 0)
	    return tok;
	}
    }
errAt(openTok, "Couldn't find closing %c", closeType);
return NULL;
}

struct pfToken *pfTokenFirstBetween(struct pfToken *start,
	struct pfToken *end, enum pfTokType type)
/* Return first token of given type between start and end.
 * End is not inclusive. */
{
struct pfToken *tok;
for (tok = start; tok != end; tok = tok->next)
    if (tok->type == type)
        return tok;
return NULL;
}
