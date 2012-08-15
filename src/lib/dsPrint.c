// dsPrint - Dynamic string Stack based Printing.
//
// This module relies upon a dyString based stack of buffers which accumulate output
// for later printing.  As a stack, only the top buffer can be filled and flushed out
// (dspFlush).  When flushing, the top buffer's content is appended to the next
// lower buffer, unless explicitly requested otherwise (dspDirectly).
// If the stack is empty, dspPrintf will be equivalent to printf.
//
// NOTE: module prefix is 'dsp' not 'dsPrint' so that "print" is always used as a verb.
//
// Output goes to STDOUT unless a single alternative out file is registered.

#include "common.h"
#include "dsPrint.h"

// The stack of dyString buffers is only accessible locally
static struct dyString *dspStack = NULL;
// It is desirable to return a unique token but NOT give access to the dyString it is based on
#define dspPointerToToken(pointer) (int)((long)(pointer) & 0xffffffff)
#define dspAssertToken(token) assert(dspPointerToToken(dspStack) == token)

// This module defaults to STDOUT but an alternative can be registered
static FILE *dspOut = NULL;
#define dspInitializeOut() {if (dspOut == NULL) dspOut = stdout;}
#define dspAssertUnregisteredOut() assert(dspOut == stdout || dspOut == NULL)
#define dspAssertRegisteredOut(out) assert(dspOut == out)


int dspOpen(int initialBufSize)
// Allocate dynamic string and puts at top of stack
// returns token to be used for later stack accounting asserts
{
if (initialBufSize <= 0)
    initialBufSize = 2048; // presume large
struct dyString *ds = dyStringNew(initialBufSize);
slAddHead(&dspStack,ds);
return dspPointerToToken(ds);
}


int dspPrintf(char *format, ...)
// Prints into end of the top ds buffer, and return resulting string length
// If there is no current buffer, this acts like a simple printf and returns -1
{
int len = -1; // caller could assert returned length > 0 to ensure dsPrint is open!
va_list args;
va_start(args, format);
if (dspStack != NULL)
    {
    dyStringVaPrintf(dspStack, format, args);
    len = dyStringLen(dspStack);
    }
else
    {
    dspInitializeOut()
    vfprintf(dspOut,format, args);
    }
va_end(args);
return len;
}


int dspPrintDirectly(int token,FILE *file)
// Prints the contents of the top buffer directly to a file.
// Will leave the filled buffer at top of stack
// Returns the length printed.
{
dspAssertToken(token);

int len = dyStringLen(dspStack);
if (len != 0)
    fprintf(file,"%s",dyStringContents(dspStack));
    fflush(file);
return len;
}


int dspFlush(int token)
// Flushes the top buffer to the next lower one and empties top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.
{
dspAssertToken(token);

int len = dyStringLen(dspStack);
if (len != 0)
    {
    if (dspStack->next == NULL)
        {
        dspInitializeOut();
        fprintf(dspOut,"%s",dyStringContents(dspStack));
        fflush(dspOut);
        }
    else
        dyStringAppend(dspStack->next,dyStringContents(dspStack));
    dyStringClear(dspStack);
    }
return len;
}


int dspClose(int token)
// Abandons the top buffer and its content, freeing memory.
// Returns the length abandoned.
{
dspAssertToken(token);
int len = dyStringLen(dspStack);
struct dyString *ds = slPopHead(&dspStack);
dyStringFree(&ds);
return len;
}

int dspFlushAndClose(int token)
// Flushes the top buffer to the next lower one and frees the top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.
{
int len = dspFlush(token);
dspClose(token);
return len;
}


int dspFlushAndCloseAll()
// CAUTION: Bad practice to not Open/Close levels in turn!
// flushes, frees and closes all stacked buffers
// returns length flushed
{
int len = 0;

// more efficient method than repeated dspFlushAndClose calls
if (dspStack != NULL)
    {
    dspInitializeOut();
    slReverse(&dspStack);  // Oldest prints first.
    while (dspStack != NULL)
        {
        struct dyString *ds = slPopHead(&dspStack);
        char *content = dyStringCannibalize(&ds);
        len += strlen(content);
        fprintf(dspOut,"%s",content);
        freeMem(content);
        }
    fflush(dspOut);
    }
return len;
}


char * dspContent(int token)
// returns the content of the current buffer as a string, but does not flush or free it
// NOTE: returned pointer is not cloned memory and will be changed by successive dsPrint calls
{
dspAssertToken(token);
return dyStringContents(dspStack);
}


int dspEmpty(int token)
// Empties the top buffer in stack (abandoning content), but leave buffer in place
// Returns the length abandoned.
{
dspAssertToken(token);
int len = dyStringLen(dspStack);
dyStringClear(dspStack);
return len;
}


static struct dyString *dspPointerFromToken(int token)
// Return the stack member corresponding to the token.
// This ix can be used to walk up the stack from a given point
{
struct dyString *ds = dspStack;
int ix = 0;

for (; ds != NULL; ds = ds->next, ++ix)
    {
    if (dspPointerToToken(ds) == token)
        return ds;
    }
return NULL;
}


int dspSize(int token)
// Returns the current length of the buffer starting at the level corresponding to token.
// Unlike other cases, the token does not have to correspond to the top of the stack!
{
struct dyString *dsTarget = dspPointerFromToken(token);
assert(dsTarget != NULL);

int len = dyStringLen(dsTarget);
struct dyString *ds = dspStack;
for (; ds != dsTarget && ds != NULL; ds = ds->next) // assertable != NULL, but still
    len += dyStringLen(ds);
return len;
}


int dspSizeAll()
// returns combined current size of all buffers
{
int len = 0;
if (dspStack != NULL)
    {
    struct dyString *ds = dspStack;
    for (;ds != NULL; ds = ds->next)
        len += dyStringLen(ds);
    }
return len;
}


int dspStackDepth()
// Returns the current dsPrint buffer stack depth
{
return slCount(dspStack);
}


void dspRegisterOutStream(FILE *out)
// Registers an alternative to STDOUT.  After registering, all dsPrint out goes to this file.
// NOTE: There is no stack. Register/Unregister serially.
{
dspAssertUnregisteredOut();
dspOut = out;
}


void dspUnregisterOutStream(FILE *out)
// Unregisters the alternative to STDOUT.  After unregistering all dsPrint out goes to STDOUT.
// NOTE: There is no stack. Register/Unregister serially.
{
dspAssertRegisteredOut(out);
dspOut = stdout;
}


char *dspCannibalizeAndClose(int token)
// Closes the top stack buffer returning content.  Returned string should be freed.
{
dspAssertToken(token);
struct dyString *ds = slPopHead(&dspStack);
return dyStringCannibalize(&ds);
}


char *dspCannibalizeAndCloseAll()
// CAUTION: Bad practice to not Open/Close levels in turn!
// Closes all stack buffers and returns a single string that is the full content.  
// Returned string should be freed.
{
// Using repeated dspFlushAndClose calls will build the contents into a single string.
while (dspStack != NULL && dspStack->next != NULL) // Leaves only one on the stack
    {
    int token = dspPointerToToken(dspStack);
    dspFlushAndClose(token); // Flushes each to the next lower buffer
    }
if (dspStack == NULL)
    return NULL;
else
    return dyStringCannibalize(&dspStack);
}


void *dspStackPop(int token)
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Returns a pointer which can be dspStackPush'd back into place.
// Pop/Push is useful for inserting print immediately before a dspOpen'd print buffer
{
// Note: while this returns a struct dyString *, the caller is kept in the dark.
//       This is because only this module should manipulate these buffers directly.
dspAssertToken(token);
return slPopHead(&dspStack);
}


void dspStackPush(int token,void *dspPointer)
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Push a previously dspStackPop'd print buffer back onto the stack
// Pop/Push is useful for inserting print immediately before a dspOpen'd print buffer
{
assert(dspPointerToToken(dspPointer) == token);
slAddHead(&dspStack,dspPointer);
}


#ifdef NOT_NOW
// Requires regex function added to dystring.c which has been successfully tested.
boolean dspRegexReplace(int token, char *regExpression, char *replaceWith)
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Looks for and replaces a single instance of a wildcard match in the current dsp print buffer
// regex follows normal EXTENDED (egrep) rules except:
// ^ - at beginning of the regExpression only and matches beginning of buffer (not of line)
// $ - at end of regExpression only and matches end of buffer (not end of line)
// \n - newlines in the body of the regExpression will match newlines in the current print buffer
// Returns TRUE on success
// CAUTION: Wildcards match largest sub-string [w.*d] matches all of "wild wild world"
{
dspAssertToken(token);
return dyStringRegexReplace(dspStack,regExpression,replaceWith);
}


void dsPrintTest()
// Tests of dsPrint functions
{
printf("Plain printf\n");
dspPrintf("1 dspPrintf unopened\n");

int token1 = dspOpen(0);
dspPrintf("2 dsOpen:1\n");
dspFlush(token1);
dspPrintf("3^10   1:%d after flush\n",dspStackDepth());
int token2 = dspOpen(256);
dspPrintf("4 dsOpen:2 len1:%d  len2:%d  lenAll:%d\n",
         dspSize(token1),dspSize(token2),dspSizeAll());
dspRegisterOutStream(stderr);
dspFlush(token2);
dspUnregisterOutStream(stderr);
dspPrintf("5      2:%d After flush  len1:%d  len2:%d  lenAll:%d\n",
         dspStackDepth(),dspSize(token1),dspSize(token2),dspSizeAll());
dspFlushAndClose(token2);
dspPrintf("6      1:%d After flushAndClose:2  len1:%d\n",dspStackDepth(),dspSize(token1));
token2 = dspOpen(256);
dspPrintf("7 dsOpen:2 reopen:2  WILL ABANDON CONTENT\n");
char *content = cloneString(dspContent(token2));
dspAbandonContent(token2);
dspPrintf("8x7    2:%d len1:%d len2:%d lenAll:%d isEmpty2:%s\n",
         dspStackDepth(),dspSize(token1),dspSize(token2),dspSizeAll(),
         (dspIsEmpty(token2) ? "EMPTY":"NOT_EMPTY"));
strSwapChar(content,'\n','~');  // Replace newline before printing.
dspPrintf("9      2:%d No flush yet   len1:%d  len2:%d  lenAll:%d  prev7:[%s]\n",
         dspStackDepth(),dspSize(token1),dspSize(token2),dspSizeAll(),content);
freez(&content);
int token3 = dspOpen(256);
dspPuts("10  Open:3 Line doubled and out of turn due to dspPrintDirectly()");
dspPrintDirectly(token3,stdout);
dspPrintf("11     3:%d Just prior to closing all.  len1:%d  len2:%d  len3:%d  lenAll:%d",
         dspStackDepth(),
         dspSize(token1),dspSize(token2),dspSize(token3),dspSizeAll());
int token4 = dspOpen(256);
dspPrintf("12  Open:4 Opened  stack:%d  WILL ABANDON\n",dspStackDepth());
dspAbandon(token4);
dspPutc('\n');
dspPuts("13  Puts:  Added last '\\n' with dsPutc(), this with dsPuts().");
dspPrintf("14     3:%d Last line!  Expect 7 & 12 missing, 10 dupped.  lenAll:%d  Bye.\n",
         dspStackDepth(),dspSizeAll());
dspFlushAndCloseAll();
assert(dspStack == NULL);
int ix = 0;
for (;ix<20;ix++) // tested to 1000
    {
    dspOpen(32); // Don't even care about tokens!
    if (ix == 0)
        dspPrintf("CannibalizeAndCloseAll():");
    dspPrintf(" %d",dspStackDepth());
    }
content = dspCannibalizeAndCloseAll();
printf("\n[%s]\n",content);
freez(&content);
}
#endif///def NOT_NOW


