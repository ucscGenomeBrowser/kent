/* pfToken - reads a file and produces paraTokens from
 * it. */

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

void pfTokenFileLineCol(struct pfToken *tok, 
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

void errAt(struct pfToken *tok, char *format, ...)
/* Warn about error at given token. */
{
int line, col;
char *fileName;
va_list args;
va_start(args, format);
pfTokenFileLineCol(tok, &fileName, &line, &col);
warn("Line %d col %d of %s", line+1, col+1, fileName);
vaErrAbort(format, args);
noWarnAbort();
va_end(args);
}


void expectingGot(char *expecting, struct pfToken *got)
/* Complain about unexpected stuff and quit. */
{
char *s = cloneStringZ(got->text, got->textSize);
errAt(got, "Expecting %s got %s", expecting, s);
}


char *pfTokTypeAsString(enum pfTokType type)
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
    case pftPlusPlus:        return "++";
    case pftMinusMinus:      return "--";
    case pftPlusEquals:      return "+=";
    case pftMinusEquals:     return "-=";
    case pftDivEquals:       return "/=";
    case pftMulEquals:       return "*=";
    case pftModEquals:       return "%=";
    case pftEqualsEquals:    return "==";
    case pftNotEquals:       return "!=";
    case pftGreaterOrEquals: return ">=";
    case pftLessOrEquals:    return "<=";
    case pftLogAnd:          return "&&";
    case pftLogOr:           return "||";
    case pftShiftLeft:       return "<<";
    case pftShiftRight:      return ">>";
    case pftRoot:            return "/.";
    case pftParent:          return "<.";
    case pftSys:             return "%.";
    case pftUser:            return "+.";
    case pftSysOrUser:       return "*.";
    default:
        buf[0] = type;
	return buf;
    }
}

struct pfSource *pfSourceNew(char *fileName)
/* Create new pfSource based around file */
{
struct pfSource *source;
AllocVar(source);
source->name = cloneString(fileName);
readInGulp(fileName, &source->contents, &source->contentSize);
return source;
}

static void addBuiltInTypes(struct pfScope *scope)
/* Add built in types . */
{
// static char *basic[] = { "var" , "string" , "bit" , "byte" , "short" , "int" , "long", "float"};
// static char *collections[] = { "array", "list", "tree", "dir" };

struct pfBaseType *varBase = pfScopeAddType(scope, "var", FALSE, NULL);
struct pfBaseType *streamBase = pfScopeAddType(scope, "$", FALSE, varBase);
struct pfBaseType *numBase = pfScopeAddType(scope, "#", FALSE, varBase);
struct pfBaseType *colBase = pfScopeAddType(scope, "[]", TRUE, varBase);

pfScopeAddType(scope, "()", TRUE, colBase);
pfScopeAddType(scope, "class", TRUE, colBase);

pfScopeAddType(scope, "string", FALSE, streamBase);

pfScopeAddType(scope, "bit", FALSE, numBase);
pfScopeAddType(scope, "byte", FALSE, numBase);
pfScopeAddType(scope, "short", FALSE, numBase);
pfScopeAddType(scope, "int", FALSE, numBase);
pfScopeAddType(scope, "long", FALSE, numBase);
pfScopeAddType(scope, "float", FALSE, numBase);
pfScopeAddType(scope, "double", FALSE, numBase);

pfScopeAddType(scope, "array", TRUE, colBase);
pfScopeAddType(scope, "list", TRUE, colBase);
pfScopeAddType(scope, "tree", TRUE, colBase);
pfScopeAddType(scope, "dir", TRUE, colBase);
}

struct pfTokenizer *pfTokenizerNew(char *fileName, struct hash *reservedWords)
/* Create tokenizing structure on file.  Reserved words is an int valued hash
 * that may be NULL. */
{
struct pfTokenizer *tkz;
AllocVar(tkz);
tkz->lm = lmInit(0);
tkz->source = pfSourceNew(fileName);
tkz->pos = tkz->source->contents;
tkz->endPos = tkz->pos + tkz->source->contentSize;
tkz->symbols = hashNew(16);
tkz->strings = hashNew(16);
tkz->modules = hashNew(0);
tkz->dy = dyStringNew(0);
if (reservedWords == NULL)
    reservedWords = hashNew(1);
tkz->reserved = reservedWords;
tkz->scope = pfScopeNew(NULL, 8);
addBuiltInTypes(tkz->scope);
return tkz;
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
  
static void skipWhiteSpace(struct pfTokenizer *tkz)
/* Skip over white space */
{
char *pos = tkz->pos;
while (isspace(pos[0]))
    ++pos;
tkz->pos = pos;
}

static boolean allWhiteToEol(char *s)
/* Return TRUE if next count chars in s are whitespace */
{
char c;
for (;;)
    {
    c = *s++;
    if (!isspace(s[0]))
        return FALSE;
    if (c == '\n')
        return TRUE;
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

static void finishQuote(struct pfTokenizer *tkz, struct pfToken *tok)
/* Finish up a quoted string */
{
tok->type = pftString;
tok->val.s = hashStoreName(tkz->strings, tkz->dy->string);
tok->textSize = tkz->pos - tok->text;
}

static void tokMultiLineQuote(struct pfTokenizer *tkz, struct pfToken *tok,
	char *endSym)
/* Do multiple line quote.  There is no escaping in these. */
{
int endSymSize = strlen(endSym);
char *pos = strchr(tkz->pos, '\n');
char *e;

if (pos != NULL)
    {
    pos += 1;
    while ((e = strchr(pos, '\n')) != NULL)
	{
	e += 1;
	if (memcmp(e, endSym, endSymSize) == 0)
	    {
	    tkz->pos = e + endSymSize;
	    finishQuote(tkz, tok);
	    return;
	    }
	dyStringAppendN(tkz->dy, pos, e - pos);
	pos = e;
	}
    }
errAt(tok, "Unterminated line quote.");
}

static void tokString(struct pfTokenizer *tkz, struct pfToken *tok,
	char quoteC)
/* Create token up to end quote.  If quote is immediately
 * followed by a newline (or whitespace/newline) then
 * treat quote as line oriented. */
{
char *pos = tkz->pos+1;
dyStringClear(tkz->dy);
if (allWhiteToEol(pos))
    {
    char endSym[2];
    endSym[0] = quoteC;
    endSym[1] = 0;
    tkz->pos = pos;
    tokMultiLineQuote(tkz, tok, endSym);
    }
else
    {
    char c;
    for (;;)
        {
	c = *pos++;
	if (c == quoteC)
	    {
	    tkz->pos = pos;
	    finishQuote(tkz, tok);
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

static void tokNameOrComplexQuote(struct pfTokenizer *tkz, 
	struct pfToken *tok)
/* Parse out name from c until first non-white-space.
 * If the name ends up being 'quote' then funnel it into
 * special delimited quotation handler. */
{
char *pos = tkz->pos;
char c;

dyStringClear(tkz->dy);
for (;;)
    {
    c = *pos;
    if (c != '_' && !isalnum(c))
        break;
    dyStringAppendC(tkz->dy, c);
    pos += 1;
    }
tkz->pos = pos;

if (sameString(tkz->dy->string, "quote"))
    {
    struct dyString *dy = dyStringNew(0);
    skipWhiteSpace(tkz);  /* Skip spaces between "quote" and "to". */
    pos = tkz->pos;
    if (!(pos[0] == 't' && pos[1] == 'o' 
       && isspace(pos[2]) ))
        errAt(tok, "Unrecognized quote type (missing to?)");
    tkz->pos = pos + 2;
    skipWhiteSpace(tkz); /* Skip spaces between "to" and marker. */
    pos = tkz->pos;
    dyStringClear(dy);
    for (;;)
        {
	c = *pos;
	if (isspace(c))
	    break;
	dyStringAppendC(dy, c);
	pos += 1;
	}
    tkz->pos = pos;
    if (dy->stringSize < 1)
	errAt(tok, "quote without delimator string");
    dyStringClear(tkz->dy);
    tokMultiLineQuote(tkz, tok, dy->string);
    dyStringFree(&dy);
    }
else
    {
    tok->type = pftName;
    tok->val.s = hashStoreName(tkz->symbols, tkz->dy->string);
    }
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
    tok->val.i = atoll(tkz->pos);
    tok->type = pftInt;
    }
tok->textSize = pos - tkz->pos;
tkz->pos = pos;
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
    case '"':
	tokString(tkz, tok, c);
	break;
    case '/':
	if (c2 == '=')
	    tokTwoChar(tkz, tok, pftDivEquals);
	else if (c2 == '.')
	    tokTwoChar(tkz, tok, pftRoot);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '+':
	if (c2 == '+')
	    tokTwoChar(tkz, tok, pftPlusPlus);
	else if (c2 == '=')
	    tokTwoChar(tkz, tok, pftPlusEquals);
	else if (c2 == '.')
	    tokTwoChar(tkz, tok, pftUser);
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
	else if (c2 == '.')
	    tokTwoChar(tkz, tok, pftSysOrUser);
	else
	    tokSingleChar(tkz, tok, c);
	break;
    case '%':
	if (c2 == '=')
	    tokTwoChar(tkz, tok, pftModEquals);
	else if (c2 == '.')
	    tokTwoChar(tkz, tok, pftSys);
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
	else if (c2 == '.')
	    tokTwoChar(tkz, tok, pftParent);
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
return tok;
}


void pfTokenDump(struct pfToken *tok, FILE *f, boolean doLong)
/* Print info on token to file. */
{
int line, col;
char *fileName;
if (tok == NULL)
    {
    fprintf(f, "EOF token\n");
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
	fprintf(f, "\"%s\"", tok->val.s);
	break;
    case pftInt:
	fprintf(f, "%lld", tok->val.i);
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

