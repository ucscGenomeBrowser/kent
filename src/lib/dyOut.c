// dyOut - DYnamic string stack based OUTput
//
// This module relies upon a dyString based stack of buffers which accumulate output
// for later printing.  As a stack, only the top buffer can be filled and flushed out
// (duOutFlush).  When flushing, the top buffer's content is appended to the next
// lower buffer, unless explicitly requested otherwise (dyOutDirectly).
// If the stack is empty, dyOutPrintf will be equivalent to printf.
//
// Output goes to STDOUT unless a single alternative out file is registered.

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dyOut.h"

// The stack of dyString buffers is only accessible locally
static struct dyString *dyOutStack = NULL;
#define dyOutAssertToken(token) assert(dyOutPointerToToken(dyOutStack) == token)

// This module defaults to STDOUT but an alternative can be registered
static FILE *dyOutStream = NULL;
#define dyOutStreamAssertUnregistered() assert(dyOutStream == stdout || dyOutStream == NULL)
#define dyOutStreamAssertRegistered(out) assert(dyOutStream == out)


INLINE void dyOutStreamInitialize(void)
// Must initialize the static dyOutStream before use.
{
if (dyOutStream == NULL) 
    dyOutStream = stdout;
}


INLINE int dyOutPointerToToken(struct dyString *pointer)
// It is desirable to return a unique token but NOT give access to the dyString it is based on
{
return (int)((size_t)(pointer) & 0xffffffff);
}


static struct dyString *dyOutPointerFromToken(int token)
// Return the stack member corresponding to the token.
// This ix can be used to walk up the stack from a given point
{
struct dyString *ds = dyOutStack;
int ix = 0;

for (; ds != NULL; ds = ds->next, ++ix)
    {
    if (dyOutPointerToToken(ds) == token)
        return ds;
    }
return NULL;
}


int dyOutOpen(int initialBufSize)
// Allocate dynamic string and puts at top of stack
// returns token to be used for later stack accounting asserts
{
if (initialBufSize <= 0)
    initialBufSize = 2048; // presume large
struct dyString *ds = dyStringNew(initialBufSize);
slAddHead(&dyOutStack,ds);
return dyOutPointerToToken(ds);
}


int dyOutPrintf(char *format, ...)
// Prints into end of the top ds buffer, and return resulting string length
// If there is no current buffer, this acts like a simple printf and returns -1
{
int len = -1; // caller could assert returned length > 0 to ensure dyOut is open!
va_list args;
va_start(args, format);
if (dyOutStack != NULL)
    {
    dyStringVaPrintf(dyOutStack, format, args);
    len = dyStringLen(dyOutStack);
    }
else
    {
    dyOutStreamInitialize();
    vfprintf(dyOutStream,format, args);
    }
va_end(args);
return len;
}


int dyOutPrintDirectly(int token,FILE *file)
// Prints the contents of the top buffer directly to a file.
// Will leave the filled buffer at top of stack
// Returns the length printed.
{
dyOutAssertToken(token);

int len = dyStringLen(dyOutStack);
if (len != 0)
    {
    fprintf(file,"%s",dyStringContents(dyOutStack));
    fflush(file);
    }
return len;
}


int dyOutFlush(int token)
// Flushes the top buffer to the next lower one and empties top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.
{
dyOutAssertToken(token);

int len = dyStringLen(dyOutStack);
if (len != 0)
    {
    if (dyOutStack->next == NULL)
        {
        dyOutStreamInitialize();
        fprintf(dyOutStream,"%s",dyStringContents(dyOutStack));
        fflush(dyOutStream);
        }
    else
        dyStringAppend(dyOutStack->next,dyStringContents(dyOutStack));
    dyStringClear(dyOutStack);
    }
return len;
}


int dyOutClose(int token)
// Abandons the top buffer and its content, freeing memory.
// Returns the length abandoned.
{
dyOutAssertToken(token);
int len = dyStringLen(dyOutStack);
struct dyString *ds = slPopHead(&dyOutStack);
dyStringFree(&ds);
return len;
}

int dyOutFlushAndClose(int token)
// Flushes the top buffer to the next lower one and frees the top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.
{
int len = dyOutFlush(token);
dyOutClose(token);
return len;
}


int dyOutFlushAndCloseAll()
// CAUTION: Bad practice to not Open/Close levels in turn!
// flushes, frees and closes all stacked buffers
// returns length flushed
{
int len = 0;

// more efficient method than repeated dyOutFlushAndClose calls
if (dyOutStack != NULL)
    {
    dyOutStreamInitialize();
    slReverse(&dyOutStack);  // Oldest prints first.
    while (dyOutStack != NULL)
        {
        struct dyString *ds = slPopHead(&dyOutStack);
        char *content = dyStringCannibalize(&ds);
        len += strlen(content);
        fprintf(dyOutStream,"%s",content);
        freeMem(content);
        }
    fflush(dyOutStream);
    }
return len;
}


char * dyOutContent(int token)
// returns the content of the current buffer as a string, but does not flush or free it
// NOTE: returned pointer is not cloned memory and will be changed by successive dyOut calls
{
dyOutAssertToken(token);
return dyStringContents(dyOutStack);
}


int dyOutEmpty(int token)
// Empties the top buffer in stack (abandoning content), but leave buffer in place
// Returns the length abandoned.
{
dyOutAssertToken(token);
int len = dyStringLen(dyOutStack);
dyStringClear(dyOutStack);
return len;
}


int dyOutSize(int token)
// Returns the current length of the buffer starting at the level corresponding to token.
// Unlike other cases, the token does not have to correspond to the top of the stack!
{
struct dyString *dsTarget = dyOutPointerFromToken(token);
assert(dsTarget != NULL);

int len = dyStringLen(dsTarget);
struct dyString *ds = dyOutStack;
for (; ds != dsTarget && ds != NULL; ds = ds->next) // assertable != NULL, but still
    len += dyStringLen(ds);
return len;
}


int dyOutSizeAll()
// returns combined current size of all buffers
{
int len = 0;
if (dyOutStack != NULL)
    {
    struct dyString *ds = dyOutStack;
    for (;ds != NULL; ds = ds->next)
        len += dyStringLen(ds);
    }
return len;
}


int dyOutStackDepth()
// Returns the current dyOut buffer stack depth
{
return slCount(dyOutStack);
}


void dyOutRegisterAltStream(FILE *out)
// Registers an alternative to STDOUT.  After registering, all dyOut out goes to this file.
// NOTE: There is no stack. Register/Unregister serially.
{
dyOutStreamAssertUnregistered();
dyOutStream = out;
}


void dyOutUnregisterAltStream(FILE *out)
// Unregisters the alternative to STDOUT.  After unregistering all dyOut out goes to STDOUT.
// NOTE: There is no stack. Register/Unregister serially.
{
dyOutStreamAssertRegistered(out);
dyOutStream = stdout;
}


char *dyOutCannibalizeAndClose(int token)
// Closes the top stack buffer returning content.  Returned string should be freed.
{
dyOutAssertToken(token);
struct dyString *ds = slPopHead(&dyOutStack);
return dyStringCannibalize(&ds);
}


char *dyOutCannibalizeAndCloseAll()
// CAUTION: Bad practice to not Open/Close levels in turn!
// Closes all stack buffers and returns a single string that is the full content.  
// Returned string should be freed.
{
// Using repeated dyOutFlushAndClose calls will build the contents into a single string.
while (dyOutStack != NULL && dyOutStack->next != NULL) // Leaves only one on the stack
    {
    int token = dyOutPointerToToken(dyOutStack);
    dyOutFlushAndClose(token); // Flushes each to the next lower buffer
    }
if (dyOutStack == NULL)
    return NULL;
else
    return dyStringCannibalize(&dyOutStack);
}


void *dyOutStackPop(int token)
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Returns a pointer which can be dyOutStackPush'd back into place.
// Pop/Push is useful for inserting print immediately before a dyOutOpen'd print buffer
{
// Note: while this returns a struct dyString *, the caller is kept in the dark.
//       This is because only this module should manipulate these buffers directly.
dyOutAssertToken(token);
return slPopHead(&dyOutStack);
}


void dyOutStackPush(int token,void *dyOutPointer)
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Push a previously dyOutStackPop'd print buffer back onto the stack
// Pop/Push is useful for inserting print immediately before a dyOutOpen'd print buffer
{
assert(dyOutPointerToToken(dyOutPointer) == token);
slAddHead(&dyOutStack,dyOutPointer);
}


#ifdef NOT_NOW
// Requires regex function added to dystring.c which has been successfully tested.
boolean dyOutRegexReplace(int token, char *regExpression, char *replaceWith)
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Looks for and replaces a single instance of a wildcard match in the current dyOut print buffer
// regex follows normal EXTENDED (egrep) rules except:
// ^ - at beginning of the regExpression only and matches beginning of buffer (not of line)
// $ - at end of regExpression only and matches end of buffer (not end of line)
// \n - newlines in the body of the regExpression will match newlines in the current print buffer
// Returns TRUE on success
// CAUTION: Wildcards match largest sub-string [w.*d] matches all of "wild wild world"
{
dyOutAssertToken(token);
return dyStringRegexReplace(dyOutStack,regExpression,replaceWith);
}


void dyOutTest()
// Tests of dyOut functions
{
printf("Plain printf\n");
dyOutPrintf("1 dyOutPrintf unopened\n");

int token1 = dyOutOpen(0);
dyOutPrintf("2 dyOutOpen:1\n");
dyOutFlush(token1);
dyOutPrintf("3^10   1:%d after flush\n",dyOutStackDepth());
int token2 = dyOutOpen(256);
dyOutPrintf("4 dsOpen:2 len1:%d  len2:%d  lenAll:%d\n",
         dyOutSize(token1),dyOutSize(token2),dyOutSizeAll());
dyOutRegisterAltStream(stderr);
dyOutFlush(token2);
dyOutUnregisterAltStream(stderr);
dyOutPrintf("5      2:%d After flush  len1:%d  len2:%d  lenAll:%d\n",
         dyOutStackDepth(),dyOutSize(token1),dyOutSize(token2),dyOutSizeAll());
dyOutFlushAndClose(token2);
dyOutPrintf("6      1:%d After flushAndClose:2  len1:%d\n",dyOutStackDepth(),dyOutSize(token1));
token2 = dyOutOpen(256);
dyOutPrintf("7 dyOutOpen:2 reopen:2  WILL ABANDON CONTENT\n");
char *content = cloneString(dyOutContent(token2));
dyOutAbandonContent(token2);
dyOutPrintf("8x7    2:%d len1:%d len2:%d lenAll:%d isEmpty2:%s\n",
         dyOutStackDepth(),dyOutSize(token1),dyOutSize(token2),dyOutSizeAll(),
         (dyOutIsEmpty(token2) ? "EMPTY":"NOT_EMPTY"));
strSwapChar(content,'\n','~');  // Replace newline before printing.
dyOutPrintf("9      2:%d No flush yet   len1:%d  len2:%d  lenAll:%d  prev7:[%s]\n",
         dyOutStackDepth(),dyOutSize(token1),dyOutSize(token2),dyOutSizeAll(),content);
freez(&content);
int token3 = dyOutOpen(256);
dyOutPuts("10  Open:3 Line doubled and out of turn due to dyOutPrintDirectly()");
dyOutPrintDirectly(token3,stdout);
dyOutPrintf("11     3:%d Just prior to closing all.  len1:%d  len2:%d  len3:%d  lenAll:%d",
         dyOutStackDepth(),
         dyOutSize(token1),dyOutSize(token2),dyOutSize(token3),dyOutSizeAll());
int token4 = dyOutOpen(256);
dyOutPrintf("12  Open:4 Opened  stack:%d  WILL ABANDON\n",dyOutStackDepth());
dyOutAbandon(token4);
dyOutPutc('\n');
dyOutPuts("13  Puts:  Added last '\\n' with dsPutc(), this with dsPuts().");
dyOutPrintf("14     3:%d Last line!  Expect 7 & 12 missing, 10 dupped.  lenAll:%d  Bye.\n",
         dyOutStackDepth(),dyOutSizeAll());
dyOutFlushAndCloseAll();
assert(dyOutStack == NULL);
int ix = 0;
for (;ix<20;ix++) // tested to 1000
    {
    dyOutOpen(32); // Don't even care about tokens!
    if (ix == 0)
        dyOutPrintf("CannibalizeAndCloseAll():");
    dyOutPrintf(" %d",dyOutStackDepth());
    }
content = dyOutCannibalizeAndCloseAll();
printf("\n[%s]\n",content);
freez(&content);
}
#endif///def NOT_NOW


