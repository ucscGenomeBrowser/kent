/* paraToken - reads a line file and produces paraTokens from
 * it. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "paraToken.h"

static char *pfTokTypeAsString(enum pfTokType type)
/* Return string corresponding to pfTokType */
{
static char buf[2];
switch (type)
    {
    case pftWhitespace: return "pftWhitespace";
    case pftComment: return "pftComment";
    case pftName: return "pftName";
    case pftString: return "pftString";
    case pftInt: return "pftInt";
    case pftFloat: return "pftFloat";
    case pftPlusPlus: return "pftPlusPlus";
    case pftMinusMinus: return "pftMinusMinus";
    case pftPlusEquals: return "pftPlusEquals";
    case pftMinusEquals: return "pftMinusEquals";
    case pftDivEquals: return "pftDivEquals";
    case pftMulEquals: return "pftMulEquals";
    case pftModEquals: return "pftModEquals";
    case pftEqualsEquals: return "pftEqualsEquals";
    default:
        buf[0] = type;
	return buf;
    }
}

static struct pfSource *pfSourceNew(char *fileName)
/* Create new pfSource based around file */
{
struct pfSource *source;
AllocVar(source);
source->name = cloneString(fileName);
return source;
}

struct pfTokenizer *pfTokenizerNew(char *fileName)
/* Create tokenizing structure on file. */
{
struct pfTokenizer *pfTkz;
AllocVar(pfTkz);
pfTkz->lm = lmInit(0);
pfTkz->source = pfSourceNew(fileName);
pfTkz->symbols = hashNew(16);
pfTkz->strings = hashNew(16);
pfTkz->lf = lineFileOpen(fileName, FALSE);
pfTkz->dy = dyStringNew(0);
return pfTkz;
}

static boolean getData(struct pfTokenizer *tkz)
/* Make sure that there is some data in line.  Return FALSE
 * if at end of file. */
{
if (tkz->posInLine >= tkz->lineSize)
    {
    if (!lineFileNext(tkz->lf, &tkz->line, &tkz->lineSize))
	return FALSE;
    tkz->posInLine = 0;
    }
return TRUE;
}

static void tokSingleChar(struct pfTokenizer *tkz, struct pfToken *tok,
	char c)
/* Create single character token where type is same as c */
{
tok->type = c;
tok->val.c = c;
tkz->posInLine += 1;
}

static void tokTwoChar(struct pfTokenizer *tkz, struct pfToken *tok,
     int type)
/* Create two character token of given type */
{
tok->type = type;
tkz->posInLine += 2;
}
  
static void tokWhiteSpace(struct pfTokenizer *tkz, struct pfToken *tok)
/* Create white space token encompassing up to next non-white space
 * or EOF */
{
int pos = tkz->posInLine+1;
for (;;)
    {
    if (pos >= tkz->lineSize)
        {
	if (!lineFileNext(tkz->lf, &tkz->line, &tkz->lineSize))
	    break;
	else
	    pos = 0;
	}
    if (!isspace(tkz->line[pos]))
        break;
    pos += 1;
    }
tok->type = pftWhitespace;
tkz->posInLine = pos;
}

static boolean allWhite(char *s, int count)
/* Return TRUE if next count chars in s are whitespace */
{
int i;
for (i=0; i<count; ++i)
    if (!isspace(s[i]))
        return FALSE;
return TRUE;
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

static void finishQuote(struct pfTokenizer *tkz, struct pfToken *tok)
/* Finish up a quoted string */
{
tok->type = pftString;
tok->val.s = hashStoreName(tkz->strings, tkz->dy->string);
}

static void tokMultiLineQuote(struct pfTokenizer *tkz, struct pfToken *tok,
	char *endSym)
/* Do multiple line quote.  There is no escaping in these. */
{
int endSymSize = strlen(endSym);
while (lineFileNext(tkz->lf, &tkz->line, &tkz->lineSize))
    {
    if (tkz->lineSize >= endSymSize && 
    	memcmp(tkz->line, endSym, endSymSize) == 0)
	{
	tkz->posInLine = endSymSize;
	finishQuote(tkz, tok);
	return;
	}
    dyStringAppendN(tkz->dy, tkz->line, tkz->lineSize);
    }
errAbort("Unterminated line quote starting line %d of %s",
	tok->startLine, tok->source->name);
}

static void tokString(struct pfTokenizer *tkz, struct pfToken *tok,
	char quoteC)
/* Create token up to end quote.  If quote is immediately
 * followed by a newline (or whitespace/newline) then
 * treat quote as line oriented. */
{
int pos = tkz->posInLine + 1;
dyStringClear(tkz->dy);
if (allWhite(tkz->line + pos, tkz->lineSize - pos))
    {
    char endSym[2];
    endSym[0] = quoteC;
    endSym[1] = 0;
    tkz->posInLine = pos;
    tokMultiLineQuote(tkz, tok, endSym);
    }
else
    {
    char c;
    for ( ;pos<tkz->lineSize; ++pos)
        {
	c = tkz->line[pos];
	if (c == quoteC)
	    {
	    tkz->posInLine = pos+1;
	    finishQuote(tkz, tok);
	    return;
	    }
	if (c == '\\')
	    {
	    char *s = tkz->line + pos + 1;
	    c = translateEscape(&s);
	    pos = s - tkz->line - 1;
	    }
	dyStringAppendC(tkz->dy, c);
	}
    errAbort("Unterminated quote line %d of %s", tkz->lf->lineIx,
    	tkz->lf->fileName);
    }
}

static void tokNameOrComplexQuote(struct pfTokenizer *tkz, 
	struct pfToken *tok)
/* Parse out name from c until first non-white-space.
 * If the name ends up being 'quote' then funnel it into
 * special delimited quotation handler. */
{
int pos;
char c;

dyStringClear(tkz->dy);
for (pos = tkz->posInLine; pos < tkz->lineSize; ++pos)
    {
    c = tkz->line[pos];
    if (c != '_' && !isalnum(c))
        break;
    dyStringAppendC(tkz->dy, c);
    }
if (sameString(tkz->dy->string, "quote"))
    {
    struct dyString *dy = dyStringNew(0);
    /* Skip spaces between "quote" and "to". */
    for (;pos < tkz->lineSize; ++pos)
        {
	if (!isspace(tkz->line[pos]))
	    break;
	}
    if (!(tkz->line[pos] == 't' && tkz->line[pos+1] == 'o' 
       && isspace(tkz->line[pos+2]) ))
        errAbort("Unrecognized quote type (missing to?) line %d of %s",
		tkz->lf->lineIx, tkz->lf->fileName);
    pos += 3;

    /* Skip spaces between "to" and marker. */
    for (;pos < tkz->lineSize; ++pos)
        {
	if (!isspace(tkz->line[pos]))
	    break;
	}

    dyStringClear(dy);
    for (;pos < tkz->lineSize; ++pos)
        {
	c = tkz->line[pos];
	if (isspace(c))
	    break;
	dyStringAppendC(dy, c);
	}
    if (dy->stringSize < 1)
        errAbort("quote without delimator string line %d of %s",
		tkz->lf->lineIx, tkz->lf->fileName);
    dyStringClear(tkz->dy);
    tokMultiLineQuote(tkz, tok, dy->string);
    dyStringFree(&dy);
    }
else
    {
    tok->type = pftName;
    tok->val.s = hashStoreName(tkz->symbols, tkz->dy->string);
    tkz->posInLine = pos;
    }
}

static void tokLineComment(struct pfTokenizer *tkz, struct pfToken *tok)
/* Create token for comment until end of line. */
{
tok->type = pftComment;
tkz->posInLine = tkz->lineSize;
}

static void finishFullComment(struct pfTokenizer *tkz, struct pfToken *tok,
	struct dyString *endPat)
/* Search for match to end pat, and close off comment there. */
{
for (;;)
    {
    char *end = memMatch(endPat->string, endPat->stringSize,
    	tkz->line + tkz->posInLine,  tkz->lineSize - tkz->posInLine);
    if (end != NULL)
        {
	tok->type = pftComment;
	tkz->posInLine = end + endPat->stringSize - tkz->line;
	return;
	}
    if (!lineFileNext(tkz->lf, &tkz->line, &tkz->lineSize))
        {
	errAbort("Unclosed comment starting line %d of %s",
		tok->startLine, tok->source->name);
	}
    tkz->posInLine = 0;
    }
}

static void tokFullComment(struct pfTokenizer *tkz, struct pfToken *tok)
/* Create token for comment that starts with '/' '*'. */
{
int posInLine = tkz->posInLine;
dyStringClear(tkz->dy);
if (tkz->posInLine + 2 < tkz->lineSize && tkz->line[posInLine+2] == '-')
    {
    int symStart = posInLine + 3;
    int pos;
    int symEnd = symStart;

    /* Find next white space. */
    for (pos = symStart; pos < tkz->lineSize; ++pos)
        {
	if (isspace(tkz->line[pos]))
	    {
	    symEnd = pos;
	    break;
	    }
	}
    dyStringAppendN(tkz->dy, tkz->line + symStart, symEnd - symStart);
    dyStringAppend(tkz->dy, "-*/");
    tkz->posInLine = pos + 1;
    }
else
    {
    dyStringAppend(tkz->dy, "*/");
    tkz->posInLine += 2;
    }
finishFullComment(tkz, tok, tkz->dy);
}

static void tokNumber(struct pfTokenizer *tkz, struct pfToken *tok, char c)
/* Suck up digits. */
{
int pos = tkz->posInLine;
boolean isFloat = FALSE;
while (isdigit(c))
    c = tkz->line[++pos];
if (c == '.')
    {
    isFloat = TRUE;
    for (;;)
        {
	c = tkz->line[++pos];
	if (!isdigit(c))
	    break;
	}
    }
if (c == 'e' || c == 'E')
    {
    isFloat = TRUE;
    for (;;)
        {
	c = tkz->line[++pos];
	if (!isdigit(c))
	    break;
	}
    }
if (isFloat)
    {
    tok->val.x = atof(tkz->line + tkz->posInLine);
    tok->type = pftFloat;
    }
else
    {
    tok->val.i = atoll(tkz->line + tkz->posInLine);
    tok->type = pftInt;
    }
tkz->posInLine = pos;
}

static void tokHexNumber(struct pfTokenizer *tkz, struct pfToken *tok)
/* Skip 0x, and suck up hex digits. */
{
int pos = tkz->posInLine+2;
unsigned long long l = 0;
int v;
char c;

for (;;)
   {
   c = tkz->line[pos];
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
	   tkz->posInLine = pos;
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
int pos;
char *s, c, c2;


/* Fetch next letter of input.  Return at EOF. */
if (!getData(tkz))
    return NULL;
pos = tkz->posInLine;
c = tkz->line[pos];
c2 = tkz->line[pos+1];

/* Allocate token structure. */
lmAllocVar(tkz->lm, tok);
tok->source = tkz->source;
tok->startLine = tkz->lf->lineIx;
tok->startCol = pos + 1;

/* Decide what to do based on first letter or two. */
switch (c)
    {
    case ' ':
    case '\t':
    case '\n':
	tokWhiteSpace(tkz,tok);
	break;
    case '\'':
    case '"':
        tokString(tkz, tok, c);
	break;
    case '/':
        if (c2 == '/')
	    tokLineComment(tkz, tok);
	else if (c2 == '*')
	    tokFullComment(tkz, tok);
	else if (c2 == '=')
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
    case '%':
        if (c2 == '=')
	    tokTwoChar(tkz, tok, pftModEquals);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '=':
        if (c2 == '=')
	    tokTwoChar(tkz, tok, pftEqualsEquals);
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
return tok;
}

void pfTokenDump(struct pfToken *tok, FILE *f)
/* Print info on token to file. */
{
if (tok == NULL)
    {
    fprintf(f, "EOF token\n");
    return;
    }
fprintf(f, "line %d, column %d, type %s(%d), value ",
    tok->startLine, tok->startCol, 
    pfTokTypeAsString(tok->type), tok->type);
switch (tok->type)
    {
    case pftName:
	fprintf(f, "%s", tok->val.s);
	break;
    case pftString:
	fprintf(f, "\"%s\"", tok->val.s);
	break;
    case pftInt:
	fprintf(f, "%lld", tok->val.i);
	break;
    case pftFloat:
	fprintf(f, "%f", tok->val.x);
	break;
    case pftWhitespace:
    case pftComment:
    case pftPlusPlus:
    case pftMinusMinus:
    case pftPlusEquals:
    case pftMinusEquals:
    case pftDivEquals:
    case pftMulEquals:
    case pftModEquals:
    case pftEqualsEquals:
	fprintf(f, "n/a");
        break;
    default:
	fprintf(f, "%c", tok->val.c);
	break;
    }
fprintf(f, "\n");
}

