/* Built in string handling. */
#include "common.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "pfString.h"
#include "sqlNum.h"
#include "cheapcgi.h"


static void _pf_string_cleanup(struct _pf_string *string, int typeId)
/* Clean up string->s and string itself. */
{
verbose(2, "_pf_string_cleanup %s\n", string->s);
if (string->allocated != 0)
    freeMem(string->s);
freeMem(string);
}

struct _pf_string *_pf_string_new_empty(int bufSize)
/* Create string with given bufSize, but no actual
 * contents. */
{
struct _pf_string *string;
AllocVar(string);
string->_pf_refCount = 1;
string->_pf_cleanup = _pf_string_cleanup;
string->s = needMem(bufSize);
string->allocated = bufSize;
return string;
}

struct _pf_string *_pf_string_new(char *s, int size)
/* Wrap string buffer of given size. */
{
struct _pf_string *string = _pf_string_new_empty(size+1);
memcpy(string->s, s, size);
string->size = size;
return string;
}

struct _pf_string *_pf_string_on_existing_c(char *cString)
/* Wrap string around existing c-style string that is in
 * dynamic memory. */
{
int len = strlen(cString);
_pf_String string;
AllocVar(string);
string->_pf_refCount = 1;
string->_pf_cleanup = _pf_string_cleanup;
string->s = cString;
string->allocated = len+1;
string->size = len;
return string;
}

struct _pf_string *_pf_string_dupe(char *s, int size)
/* Clone string of given size and wrap string around it. */
{
return _pf_string_new(s, size);
}

_pf_Bit _pf_stringSame(_pf_String a, _pf_String b)
/* Comparison between two strings not on the stack. */
{
if (a == b)
    return TRUE;
if (a == NULL || b == NULL)
    return FALSE;
return (strcmp(a->s, b->s) == 0);
}

int _pf_strcmp(_pf_Stack *stack)
/* Return comparison between strings.  Cleans them off
 * of stack.  Does not put result on stack because
 * the code generator may use it in different ways. */
{
_pf_String a = stack[0].String;
_pf_String b = stack[1].String;
int ret = 0;

if (a != b)
    {
    if (a == NULL)
        ret = -1;
    else if (b == NULL)
        ret = 1;
    else
        ret = strcmp(a->s, b->s);
    }

if (NULL != a && --a->_pf_refCount <= 0)
    a->_pf_cleanup(a, 0);
if (NULL != b && --b->_pf_refCount <= 0)
    b->_pf_cleanup(b, 0);
return ret;
}

static struct _pf_string *_pf_string_init(char *s)
/* Wrap string around constant. */
{
return _pf_string_new(s, strlen(s));
}

struct _pf_string *_pf_string_from_const(char *s)
/* Wrap string around constant. */
{
struct _pf_string *string = _pf_string_init(s);
return string;
}

struct _pf_string *_pf_string_from_long(_pf_Long ll)
/* Wrap string around Long. */
{
char buf[32];
safef(buf, sizeof(buf), "%lld", ll);
return _pf_string_init(buf);
}

struct _pf_string *_pf_string_from_double(_pf_Double d)
/* Wrap string around Double. */
{
char buf[32];
safef(buf, sizeof(buf), "%0.2f", d);
return _pf_string_init(buf);
}


struct _pf_string *_pf_string_from_Int(_pf_Int i)
/* Wrap string around Int. */
{
return _pf_string_from_long(i);
}

struct _pf_string *_pf_string_from_Float(_pf_Stack *stack)
/* Wrap string around Float. */
{
return _pf_string_from_long(stack->Float);
}


struct _pf_string *_pf_string_cat(_pf_Stack *stack, int count)
/* Create new string that's a concatenation of the above strings. */
{
int i;
size_t total=0;
size_t pos = 0;
_pf_String string;
char *s;
AllocVar(string);
string->_pf_refCount = 1;
string->_pf_cleanup = _pf_string_cleanup;
for (i=0; i<count; ++i)
    {
    _pf_String ss = stack[i].String;
    if (ss)
	total += ss->size;
    }
string->s = s = needLargeMem(total+1);
for (i=0; i<count; ++i)
    {
    _pf_String ss = stack[i].String;
    if (ss)
	{
	memcpy(s, ss->s, ss->size);
	s += ss->size;
	if (--ss->_pf_refCount <= 0)
	    ss->_pf_cleanup(ss, 0);
	}
    }
*s = 0;
string->size = string->allocated = total;
return string;
}

void _pf_string_make_independent_copy(_pf_Stack *stack)
/* Make it so that string on stack is an indepent copy of
 * what is already on stack.  As an optimization this routine
 * can do nothing if the refCount is exactly 1. */
{
_pf_String string = stack[0].String;
if (string != NULL && string->_pf_refCount != 1)
    {
    stack[0].String = _pf_string_new(string->s, string->size);
    string->_pf_refCount -= 1;
    }
}

void _pf_cm_string_upper(_pf_Stack *stack)
/* to string.upper() into (string s) */
{
_pf_String string = stack[0].String;
_pf_nil_check(string);
string = _pf_string_new(string->s, string->size);
toUpperN(string->s, string->size);
stack[0].String = string;
}

void _pf_cm_string_lower(_pf_Stack *stack)
/* to string.lower() into (string s) */
{
_pf_String string = stack[0].String;
_pf_nil_check(string);
string = _pf_string_new(string->s, string->size);
toLowerN(string->s, string->size);
stack[0].String = string;
}

void _pf_cm_string_dupe(_pf_Stack *stack)
/* Return duplicate of string */
{
_pf_String string = stack[0].String;
if (string)
    stack[0].String = _pf_string_new(string->s, string->size);
}

void _pf_cm_string_same(_pf_Stack *stack)
/* Find occurence of substring within string.  Return -1 if
 * not found, otherwise start location within string. */
{
_pf_String string = stack[0].String;
_pf_String other = stack[1].String;
if (!other)
    stack[0].Bit = FALSE;
else
    {
    stack[0].Bit = sameWord(string->s, other->s);
    if (--other->_pf_refCount <= 0)
	other->_pf_cleanup(other, 0);
    }
}


void _pf_cm_string_first(_pf_Stack *stack)
/* Return start of string */
{
_pf_String string = stack[0].String;
int size = stack[1].Int;
if (size < 0)
    size = 0;
if (size > string->size)
    size = string->size;
stack[0].String = _pf_string_new(string->s, size);
}

void _pf_cm_string_last(_pf_Stack *stack)
/* Return end of string */
{
_pf_String string = stack[0].String;
int size = stack[1].Int;
if (size < 0)
    size = 0;
if (size > string->size)
    size = string->size;
stack[0].String = _pf_string_new(string->s + string->size - size, size);
}

void _pf_cm_string_middle(_pf_Stack *stack)
/* Return middle of string */
{
_pf_String string = stack[0].String;
int start = stack[1].Int;
int size = stack[2].Int;
int end = start + size;
if (start < 0) start = 0;
if (end > string->size) end = string->size;
if (size < 0)
    errAbort("Negative size (%d) in string.middle", size);
stack[0].String = _pf_string_new(string->s + start, end - start);
}

void _pf_cm_string_rest(_pf_Stack *stack)
/* Return rest of string (skipping up to start) */
{
_pf_String string = stack[0].String;
int start = stack[1].Int;
if (start < 0) start = 0;
if (start > string->size) start = string->size;
stack[0].String = _pf_string_new(string->s + start, string->size - start);
}

void _pf_strcat(_pf_String a, char *b, int bSize)
/* Concatenate b onto the end of a. */
{
if (b != NULL)
    {
    int newSize = a->size + bSize;
    if (newSize >= a->allocated)
	{
	char *s;
	if (newSize <= 64*1024)
	    a->allocated = 2*newSize;
	else
	    a->allocated = newSize + 64*1024;
	a->s = needLargeMemResize(a->s, a->allocated);
	}
    memcpy(a->s + a->size, b, bSize);
    a->s[newSize] = 0;
    a->size = newSize;
    }
}

void _pf_cm_string_append(_pf_Stack *stack)
/* Put something on end of string. */
{
_pf_String start = stack[0].String;
_pf_String end = stack[1].String;
if (end != NULL)
    {
    _pf_strcat(start, end->s, end->size);
    /* Clean up references on stack.  (Not first param since it's a method). */
    if (--end->_pf_refCount <= 0)
	end->_pf_cleanup(end, 0);
    }
}

void _pf_cm_string_find(_pf_Stack *stack)
/* Find occurence of substring within string.  Return -1 if
 * not found, otherwise start location within string. */
{
_pf_String string = stack[0].String;
_pf_String sub = stack[1].String;
char *loc;
_pf_nil_check(sub);
loc = stringIn(sub->s, string->s);
if (loc == NULL)
    stack[0].Int = -1;
else
    stack[0].Int = loc - string->s;

/* Clean up references on stack.  (Not first param since it's a method). */
if (--sub->_pf_refCount <= 0)
    sub->_pf_cleanup(sub, 0);
}

void _pf_cm_string_findNext(_pf_Stack *stack)
/* Find occurence of substring within string.  Return -1 if
 * not found, otherwise start location within string. */
{
/* Get input off of stack. */
_pf_String string = stack[0].String;
_pf_String sub = stack[1].String;
_pf_Int start = stack[2].Int;

/* Calculate where match is.  Don't bother looking is
 * start coordinate outside of string. */
char *loc = NULL;
if (start >= 0 && start < string->size)
    loc = stringIn(sub->s, string->s + start);
if (loc == NULL)
    stack[0].Int = -1;
else
    stack[0].Int = loc - string->s;

/* Clean up references on stack.  (Not first param since it's a method). */
if (--sub->_pf_refCount <= 0)
    sub->_pf_cleanup(sub, 0);
}

static int findLast(_pf_String pattern, _pf_String string)
/* Find last occurence of pattern in string */
{
int found = -1;
if (pattern->size <= string->size && pattern->size > 0)
    {
    int pos = string->size - pattern->size;
    char *s = string->s;
    char c = pattern->s[0];
    int patSizeMinusOne = pattern->size-1;
    while (pos >= 0)
        {
	if (s[pos] == c)
	    {
	    if (patSizeMinusOne==0)
	        {
		found = pos;
		break;
		}
	    else
	        {
		if (memcmp(s+1, pattern->s+1, patSizeMinusOne) == 0)
		    {
		    found = pos;
		    break;
		    }
		}
	    }
	pos -= 1;
	}
    }
return found;
}

void _pf_cm_string_findLast(_pf_Stack *stack)
/* Find last occurence of substring within string.  Return -1 if
 * not found, otherwise start location within string. */
{
_pf_String string = stack[0].String;
_pf_String sub = stack[1].String;
_pf_nil_check(sub);

stack[0].Int = findLast(sub, string);

/* Clean up references on stack.  (Not first param since it's a method). */
if (--sub->_pf_refCount <= 0)
    sub->_pf_cleanup(sub, 0);
}


static void nextBetween(_pf_String string, _pf_String start, _pf_String end, 
    int pos, _pf_String *retBetween, _pf_Int *retNextPos)
/* Find start and then end in string, and if find both,
 * return string between them. */
{
char *s=NULL, *e = NULL;
_pf_nil_check(start);
_pf_nil_check(end);
if (pos >= 0 && pos + start->size + end->size <= string->size)
    {
    s = stringIn(start->s, string->s + pos);
    if (s != NULL && (s + start->size + end->size - string->s) <= string->size)
	{
	s += start->size;
	e = stringIn(end->s, s);
	}
    }
if (e != NULL)
    {
    *retBetween = _pf_string_new(s, e-s);
    *retNextPos = e - string->s;
    }
else
    {
    *retBetween = NULL;
    *retNextPos = -1;
    }
}

void _pf_cm_string_between(_pf_Stack *stack)
/* Find occurence of substring within string.  Return -1 if
 * not found, otherwise start location within string. */
{
_pf_String string = stack[0].String;
_pf_String start = stack[1].String;
_pf_String end = stack[2].String;
_pf_Int nextPos;

nextBetween(string, start, end, 0, &stack[0].String, &nextPos);

/* Clean up references on stack.  (Not first param since it's a method). */
if (--start->_pf_refCount <= 0)
    start->_pf_cleanup(start, 0);
if (--end->_pf_refCount <= 0)
    end->_pf_cleanup(end, 0);
}

void _pf_cm_string_nextBetween(_pf_Stack *stack)
/* Find occurence of substring within string.  Return -1 if
 * not found, otherwise start location within string. */
{
_pf_String string = stack[0].String;
_pf_String start = stack[1].String;
_pf_String end = stack[2].String;
int pos = stack[3].Int;
int nextPos;

nextBetween(string, start, end, pos, &stack[0].String, &stack[1].Int);

/* Clean up references on stack.  (Not first param since it's a method). */
if (--start->_pf_refCount <= 0)
    start->_pf_cleanup(start, 0);
if (--end->_pf_refCount <= 0)
    end->_pf_cleanup(end, 0);
}



static int tokenSpan(char *s)
/* Count up number of characters in word starting at s. */
{
int count = 0;
if (s != NULL && s[0] != 0)
    {
    char *e = s;
    char c = *e++;
    if (isalnum(c) || c == '_')
        {
	for (;;)
	    {
	    c = *e;
	    if (!isalnum(c) && c != '_')
		break;
	    e += 1;
	    }
	}
    count = e - s;
    }
return count;
}

static int wordSpan(char *s)
/* Count up number of characters in space-delimited word starting at s. */
{
char *e = skipToSpaces(s);
if (e == NULL)
    return strlen(s);
else
    return e - s;
}

static void nextSkippingWhite(_pf_Stack *stack, int (*calcSpan)(char *s))
{
_pf_String string = stack[0].String;
int pos = stack[1].Int;
char *s, c;

if (string == NULL || pos < 0)
    {
    stack[0].String = NULL;
    stack[1].Int = -1;
    return;
    }
s = skipLeadingSpaces(string->s + pos);
c = s[0];
if (c == 0)
    {
    stack[0].String = NULL;
    stack[1].Int = string->size;
    }
else
    {
    int span = calcSpan(s);
    stack[0].String = _pf_string_new(s, span);
    stack[1].Int = s + span - string->s;
    }
}

void _pf_cm_string_nextToken(_pf_Stack *stack)
/* Return next white space or punctuation separated word. */
{
nextSkippingWhite(stack, tokenSpan);
}

void _pf_cm_string_nextWord(_pf_Stack *stack)
/* Return next white-space separated word. */
{
nextSkippingWhite(stack, wordSpan);
}

void _pf_cm_string_nextLine(_pf_Stack *stack)
/*   flow string.nextLine(int pos) into (string line, int nextPos) */
{
_pf_String string = stack[0].String;
int pos = stack[1].Int;
char *s, *e, c;

if (string == NULL || pos < 0 || pos > string->size)
    {
    stack[0].String = NULL;
    stack[1].Int = -1;
    return;
    }
s = string->s + pos;
e = strchr(s, '\n');
if (e == NULL)
    {
    e = string->s + string->size;
    stack[1].Int = string->size;
    }
else
    stack[1].Int = e - string->s + 1;
stack[0].String = _pf_string_new(s, e-s);
}

void _pf_cm_string_betweenQuotes(_pf_Stack *stack)
/* flow string.betweenQuotes(int pos) into (string line, int nextPos) 
 * The initial string position is at the opening quote.  This
 * will find the closing quote of the same type, put what is between
 * the quotes into line, and put nextPos to just past the closing quote. */
{
_pf_String string = stack[0].String, result;
int pos = stack[1].Int, size = 0;
char *startQuote, *endQuote = NULL, *endString, quoteC, c, lastC, *s, *d;

_pf_nil_check(string);
startQuote = string->s + pos;
endString = string->s + string->size;
lastC = quoteC = *startQuote;
startQuote += 1;
if (string->size < 1)
    errAbort("empty string passed to string.betweenQuotes");

/* Find end quote and count how big we need to be. */
for (s = startQuote; s<endString; ++s)
    {
    c = *s;
    if (c == quoteC)
        {
	if (lastC == '\\')
	    --size;
	else
	    {
	    endQuote = s;
	    break;
	    }
	}
    lastC = c;
    size += 1;
    }
if (!endQuote)
    errAbort("Unterminated %c quote", quoteC);

/* Create string for result and fill it */
result = _pf_string_new_empty(size+1);
d = result->s;
for (s = startQuote; s<endQuote; ++s)
    {
    c = *s;
    if (c == quoteC)  /* we know previous char was escape */
       d[-1] = c;
    else
       *d++ = c;
    }
result->size = size;

stack[0].String = result;
stack[1].Int = (pos + (endQuote-startQuote) + 2);
}


static int countWordsSkippingWhite(char *s, int (*calcSpan)(char *s))
/* Count words - not in the space-delimited sense, but in the
 * paraFlow word since - where punctuation also separates words,
 * and punctuation marks are returned as a separate word. */
{
int count = 0, span;
char c;
for (;;)
    {
    s = skipLeadingSpaces(s);
    span = calcSpan(s);
    if (span == 0)
        break;
    count += 1;
    s += span;
    }
return count;
}

static void stringSplitSkippingWhite(_pf_Stack *stack, int (*calcSpan)(char *s))
/* Implements flow string.words() into array of string words */
{
_pf_String string = stack[0].String, *strings;
_pf_Array words;
int i, wordCount, span;
char *s;

_pf_nil_check(string);
s = string->s;
wordCount = countWordsSkippingWhite(s, calcSpan);
words = _pf_dim_array(wordCount, _pf_find_string_type_id());
strings =  (_pf_String *)words->elements;
for (i=0; i<wordCount; ++i)
    {
    s = skipLeadingSpaces(s);
    span = calcSpan(s);
    strings[i] = _pf_string_new(s, span);
    s += span;
    }
stack[0].Array = words;
}

void _pf_cm_string_tokens(_pf_Stack *stack)
/* Implements flow string.tokens() into array of string words */
{
stringSplitSkippingWhite(stack, tokenSpan);
}

void _pf_cm_string_words(_pf_Stack *stack)
/* Implements flow string.words() into array of string words */
{
stringSplitSkippingWhite(stack, wordSpan);
}

static void stringSplitOnChar(_pf_Stack *stack, char c, 
	boolean includeTrailingEmpty)
/* Used by flow string.lines() into array of string lines
 * and     flow string.split() into arrray of string parts */
{
_pf_String string = stack[0].String, *strings;
_pf_Array words;
int i, charCount, lastSize;
char *s;

_pf_nil_check(string);
s = string->s;
charCount = countChars(s, c);
words = _pf_dim_array(charCount+1, _pf_find_string_type_id());
strings =  (_pf_String *)words->elements;
for (i=0; i<charCount; ++i)
    {
    char *e = strchr(s, c);
    strings[i] = _pf_string_new(s, e-s);
    s = e+1;
    }
lastSize = string->size - (s - string->s);
if (includeTrailingEmpty || lastSize > 0)
    strings[charCount] = _pf_string_new(s, lastSize);
else
    words->size -= 1;
stack[0].Array = words;
}

void _pf_cm_string_lines(_pf_Stack *stack)
/* flow line() into (array of string lines) 
 * Creates an array of lines from string */
{
stringSplitOnChar(stack, '\n', FALSE);
}

static boolean charInSplitter(char c, _pf_String splitter)
/* Return TRUE if c is in splitter. */
{
int i;
char *s = splitter->s;
for (i = splitter->size-1; i>=0; i-=1)
    if (c == s[i])
        return TRUE;
return FALSE;
}

static void stringSplitOnMultiChars(_pf_Stack *stack, _pf_String splitter)
/* Split when see any of the strings in splitter. */
{
_pf_String string = stack[0].String, *strings;
_pf_Array words;
int i, divCount=0, size;
char *s;

_pf_nil_check(string);
s = string->s;
size = string->size;
for (i=0; i<size; ++i)
    if (charInSplitter(s[i], splitter))
        ++divCount;
words = _pf_dim_array(divCount+1, _pf_find_string_type_id());
strings =  (_pf_String *)words->elements;
for (i=0; i<divCount; ++i)
    {
    char *e = s;
    while (!charInSplitter(*e, splitter))
        e += 1;
    strings[i] = _pf_string_new(s, e-s);
    s = e+1;
    }
strings[divCount] = _pf_string_new(s, string->size - (s - string->s));
stack[0].Array = words;
}

void _pf_cm_string_split(_pf_Stack *stack)
/* flow split(string splitter) into (array of string parts)  */
{
_pf_String splitter = stack[1].String;
_pf_nil_check(splitter);
if (splitter->size == 1)
    stringSplitOnChar(stack, splitter->s[0], TRUE);
else
    stringSplitOnMultiChars(stack, splitter);

/* Clean up references on stack.  (Not first param since it's a method). */
if (--splitter->_pf_refCount <= 0)
    splitter->_pf_cleanup(splitter, 0);
}
 

void _pf_cm_string_trim(_pf_Stack *stack)
/* Trim away leading and trailing spaces. */
{
int size, startReal, endReal, realSize;
char *s;
_pf_String string = stack[0].String;
_pf_nil_check(string);
size = string->size;
s = string->s;
for (startReal = 0; startReal < size; ++startReal)
    {
    if (!isspace(s[startReal]))
	break;
    }
if (startReal == size)
    {
    endReal = size;
    realSize = 0;
    }
else
    {
    for (endReal = size-1; ; --endReal)  /* we know we'll hit something. */
	{
	if (!isspace(s[endReal]))
	    break;
	}
    realSize = endReal - startReal + 1;
    }
stack[0].String = _pf_string_new(s + startReal, realSize);
}


static void fitString(_pf_Stack *stack, boolean right)
/* Fit string on either side. */
{
_pf_String string = stack[0].String;
_pf_Int size = stack[1].Int;
_pf_String out;
AllocVar(out);
out->_pf_refCount = 1;
out->_pf_cleanup = _pf_string_cleanup;
out->s = needMem(size+1);
out->size = string->allocated = size;
if (string->size >= size)
     memcpy(out->s, string->s, size);
else
     {
     if (right)
         {
	 int diff = size - string->size;
	 memset(out->s, ' ', diff);
	 memcpy(out->s + diff, string->s, string->size);
	 }
     else
	 {
	 memcpy(out->s, string->s, string->size);
	 memset(out->s + string->size, ' ', size - string->size);
	 }
     }
stack[0].String = out;
}

void _pf_cm_string_fitLeft(_pf_Stack *stack)
/* Expand string to size, keeping string on left, and adding spaces as
 * needed. 
 *     string.fitLeft(size) into (string new)*/
{
fitString(stack, FALSE);
}

void _pf_cm_string_fitRight(_pf_Stack *stack)
/* Expand string to size, keeping string on right, and adding spaces as
 * needed. 
 *     string.fitRight(size) into (string new)*/
{
fitString(stack, TRUE);
}

void pf_floatString(_pf_Stack *stack)
/* floatString(double f, int digitsBeforeDecimal, int digitsAfterDecimal, 
 *         bit forceScientific) into string s
 * This returns a floating point number formatted just how you like it. */
{
_pf_Double f = stack[0].Double;
_pf_Int digitsBeforeDecimal = stack[1].Int;
_pf_Int digitsAfterDecimal = stack[2].Int;
_pf_Bit forceScientific = stack[3].Bit;
char buf[32];
char format[16];
char *sci = (forceScientific ? "e" : "f");

safef(format, sizeof(format), "%%%d.%d%s", 
	digitsBeforeDecimal+digitsAfterDecimal+1,
	digitsAfterDecimal, sci);
safef(buf, sizeof(buf), format, f);
stack[0].String = _pf_string_new(buf, strlen(buf));
}

void pf_intString(_pf_Stack *stack)
/* Format an integer just how you like it. *?
 * intString(long l, int minWidth, bit zeroPad, bit commas) into String s
 */
{
_pf_Long l = stack[0].Long;
_pf_Int minWidth = stack[1].Int;
_pf_Bit zeroPad = stack[2].Bit;
_pf_Bit commas = stack[3].Bit;
char buf[32];
char format[16];
safef(format, sizeof(format), "%%%s%dlld", 
	(zeroPad ? "0" : ""), minWidth);
safef(buf, sizeof(buf), format, l);
if (commas)
    {
    int len = strlen(buf);
    char commaBuf[48];
    int sourcePos = 0, destPos = 0;
    int size = len%3;
    if (size == 0) size = 3;
    for (;;)
        {
	memcpy(commaBuf+destPos, buf+sourcePos, size);
	destPos += size;
	sourcePos += size;
	if (sourcePos >= len)
	    break;
	if (commaBuf[destPos-1] != ' ')
	    {
	    commaBuf[destPos] = ',';
	    destPos += 1;
	    }
	size = 3;
	}
    stack[0].String = _pf_string_new(commaBuf, destPos);
    }
else
    stack[0].String = _pf_string_new(buf, strlen(buf));
}

void _pf_string_substitute(_pf_Stack *stack)
/* Create new string on top of stack based on all of the
 * substitution parameters already on the stack.  The
 * first param is the string with the $ signs. The rest
 * of the parameters are strings  to substitute in, in the
 * same order as the $'s in the first param.  */
{
_pf_String sourceString = stack[0].String;
_pf_String destString = NULL;
_pf_String varString;
int varCount = 0, varIx = 0;
int estimatedSize = 0;  /*  Estimate will be a little high. */
int destSize = 0;
char *destBuf;
char *s, *e, *d, *sEnd;
char *nil = "(nil)";
int nilSize = 5;

/* Count up number of substitutions, and estimate the resulting
 * string size.  Make a buffer big enough to hold substituted result. */
s = sourceString->s;
e = s + sourceString->size;
while (s < e)
    {
    if (s[0] == '$')
        {
	if (s[1] == '$')
	    s += 1;
	else
	    {
	    varCount += 1;
	    varString = stack[varCount].String;
	    if (varString)
		estimatedSize += varString->size;
	    else
	        estimatedSize += nilSize;
	    }
	}
    s += 1;
    }
estimatedSize += sourceString->size;
destBuf = needMem(estimatedSize+1);

/* Loop through copying source with substitutions to dest. */
s = sourceString->s;
sEnd = s + sourceString->size;
d = destBuf;
while (s < sEnd)
    {
    char c = *s;
    if (c == '$')
        {
	s += 1;
	c = *s;
	if (c == '$')
	    {
	    *d++ = '$';
	    s += 1;
	    }
	else 
	    {
	    if (c == '(')
		{
		/* Skip to closing paren in s */
		s = strchr(s, ')');
		assert(s != NULL);
		s += 1;
		}
	    else
	        {
		/* Skip to character after var name. */
		while ((isalnum(c) || c == '_') && s < sEnd)
		    {
		    c = *(++s);
		    }
		}
	    ++varIx;
	    varString = stack[varIx].String;
	    if (varString)
		{
		memcpy(d, varString->s, varString->size);
		d += varString->size;
		}
	    else
	        {
		memcpy(d, nil, nilSize);
		d += nilSize;
		}
	    }
	}
    else
        {
	*d++ = c;
	s += 1;
	}
    }
assert(varIx == varCount);
destSize = d - destBuf;

/* Clean up all the input references on stack. */
for (varIx=0; varIx <= varCount; ++varIx)  /* <= intentional here */
    {
    struct _pf_string *s = stack[varIx].String;
    if (s != NULL && --s->_pf_refCount <= 0)
	s->_pf_cleanup(s, 0);
    }

/* Create a string object to return and put it on stack. */
stack[0].String = _pf_string_new(destBuf, destSize);
freeMem(destBuf);
}

void _pf_cm_string_startsWith(_pf_Stack *stack)
/*  string.startsWith(string prefix) into bit b */
{
_pf_String string = stack[0].String;
_pf_String prefix = stack[1].String;
_pf_nil_check(prefix);
stack[0].Bit = startsWith(prefix->s, string->s);

/* Clean up references on stack.  (Not first param since it's a method). */
if (--prefix->_pf_refCount <= 0)
    prefix->_pf_cleanup(prefix, 0);
}


void _pf_cm_string_endsWith(_pf_Stack *stack)
/*  string.endsWith(string prefix) into bit b */
{
_pf_String string = stack[0].String;
_pf_String suffix = stack[1].String;
_pf_nil_check(suffix);
stack[0].Bit = endsWith(string->s, suffix->s);

/* Clean up references on stack.  (Not first param since it's a method). */
if (--suffix->_pf_refCount <= 0)
    suffix->_pf_cleanup(suffix, 0);
}

void _pf_cm_string_asInt(_pf_Stack *stack)
/* Convert string to int. */
{
_pf_String string = stack[0].String;
stack[0].Int = sqlSigned(string->s);
}

void _pf_cm_string_asLong(_pf_Stack *stack)
/* Convert string to long. */
{
_pf_String string = stack[0].String;
stack[0].Long = sqlLongLong(string->s);
}

void _pf_cm_string_asDouble(_pf_Stack *stack)
/* Convert string to double. */
{
_pf_String string = stack[0].String;
stack[0].Double = sqlDouble(string->s);
}

void pf_isSpace(_pf_Stack *stack)
/* Return TRUE if it's a space character. */
{
_pf_Char c = stack[0].Char;
stack[0].Bit = (isspace(c) != 0);
}

void _pf_cm_string_leadingSpaces(_pf_Stack *stack)
/* Return number of leading spaces. */
{
int count = 0;
char *s;
_pf_String string = stack[0].String;
int pos = stack[1].Int;
_pf_nil_check(string);
s = string->s+pos;
while (isspace(*s))
    {
    count++;
    s++;
    }
stack[0].Int = count;
}


void _pf_cm_string_cgiEncode(_pf_Stack *stack)
/* Return cgi-encoded version of string */
{
_pf_String string = stack[0].String;
if (string)
    {
    char *encoded = cgiEncode(string->s);
    stack[0].String = _pf_string_on_existing_c(encoded);
    }
}

void _pf_cm_string_cgiDecode(_pf_Stack *stack)
/* Return cgi-decoded version of string */
{
_pf_String string = stack[0].String;
if (string)
    {
    _pf_String d = _pf_string_new_empty(string->size);
    cgiDecode(string->s, d->s, string->size);
    d->size = strlen(d->s);
    stack[0].String = d;
    }
}

