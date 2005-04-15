/* Built in string handling. */
#include "common.h"
#include "runType.h"
#include "../compiler/pfPreamble.h"
#include "object.h"
#include "string.h"


static void _pf_string_cleanup(struct _pf_string *string, int typeId)
/* Clean up string->s and string itself. */
{
// uglyf("_pf_string_cleanup %s\n", string->s);
verbose(2, "_pf_string_cleanup %s\n", string->s);
if (string->allocated != 0)
    freeMem(string->s);
freeMem(string);
}

static struct _pf_string *_pf_string_new(char *s, int size)
/* Wrap string buffer of given size. */
{
struct _pf_string *string;
AllocVar(string);
string->_pf_refCount = 1;
string->_pf_cleanup = _pf_string_cleanup;
string->s = cloneStringZ(s, size);
string->size = string->allocated = size;
return string;
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
    total += stack[i].String->size;
string->s = s = needLargeMem(total+1);
for (i=0; i<count; ++i)
    {
    _pf_String ss = stack[i].String;
    memcpy(s, ss->s, ss->size);
    s += ss->size;
    if (--ss->_pf_refCount <= 0)
        ss->_pf_cleanup(ss, 0);
    }
*s = 0;
string->size = string->allocated = total;
return string;
}

void _pf_cm_string_upper(_pf_Stack *stack)
/* Uppercase existing string */
{
_pf_String string = stack[0].String;
toUpperN(string->s, string->size);
}

void _pf_cm_string_lower(_pf_Stack *stack)
/* Lowercase existing string */
{
_pf_String string = stack[0].String;
toLowerN(string->s, string->size);
}

void _pf_cm_string_dupe(_pf_Stack *stack)
/* Return duplicate of string */
{
_pf_String string = stack[0].String;
stack[0].String = _pf_string_new(string->s, string->size);
}

void _pf_cm_string_start(_pf_Stack *stack)
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

void _pf_cm_string_end(_pf_Stack *stack)
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


void _pf_cm_string_append(_pf_Stack *stack)
/* Put something on end of string. */
{
_pf_String start = stack[0].String;
_pf_String end = stack[1].String;
int newSize = start->size + end->size;
if (newSize > start->allocated)
    {
    char *s;
    if (newSize <= 64*1024)
	start->allocated = 2*newSize;
    else
        start->allocated = newSize + 64*1024;
    start->s = needLargeMemResize(start->s, start->allocated);
    }
memcpy(start->s + start->size, end->s, end->size);
start->s[newSize] = 0;
start->size = newSize;

/* Clean up references on stack.  (Not first param since it's a method). */
if (--end->_pf_refCount <= 0)
    end->_pf_cleanup(end, 0);
}

void _pf_cm_string_find(_pf_Stack *stack)
/* Find occurence of substring within string.  Return -1 if
 * not found, otherwise start location within string. */
{
_pf_String string = stack[0].String;
_pf_String sub = stack[1].String;
char *loc = stringIn(sub->s, string->s);
if (loc == NULL)
    stack[0].Int = -1;
else
    stack[0].Int = loc - string->s;

/* Clean up references on stack.  (Not first param since it's a method). */
if (--sub->_pf_refCount <= 0)
    sub->_pf_cleanup(sub, 0);
}
