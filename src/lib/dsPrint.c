// dsPrint - Dynamic string Stack based Printing.
//
// This module relies upon a dyString based stack of buffers which accumulate output
// for later printing.  As a stack, only the top buffer can be filled and flushed out
// (dsPrintFlush).  When flushing, the top buffer's content is appended to the next
// lower buffer, unless explicitly requested otherwise (dsPrintDirectly).
// If the stack is empty, dsPrintf will be equivalent to printf.
//
// Output goes to STDOUT unless a single alternative out file is registered.

#include "common.h"
#include "dsPrint.h"

// The stack of dyString buffers is only accessible locally
static struct dyString *dsStack = NULL;
// It is desirable to return a unique token but NOT give access to the dyString it is based on
#define dsPointerToToken(pointer) (int)((long)(pointer) & 0xffffffff)
#define dsAssertToken(token) assert(dsPointerToToken(dsStack) == token)

// This module defaults to STDOUT but an alternative can be registered
static FILE *dsOut = NULL;
#define dsInitializeOut() {if (dsOut == NULL) dsOut = stdout;}
#define dsAssertUnregisteredOut() assert(dsOut == stdout || dsOut == NULL)
#define dsAssertRegisteredOut(out) assert(dsOut == out)


int dsPrintOpen(int initialBufSize)
// Allocate dynamic string and puts at top of stack
// returns token to be used for later stack accounting asserts
{
if (initialBufSize <= 0)
    initialBufSize = 2048; // presume large
struct dyString *ds = dyStringNew(initialBufSize);
slAddHead(&dsStack,ds);
return dsPointerToToken(ds);
}


int dsPrintf(char *format, ...)
// Prints into end of the top ds buffer, and return resulting string length
// If there is no current buffer, this acts like a simple printf and returns -1
{
int len = -1; // caller could assert returned length > 0 to ensure dsPrint is open!
va_list args;
va_start(args, format);
if (dsStack != NULL)
    {
    dyStringVaPrintf(dsStack, format, args);
    len = dyStringLen(dsStack);
    }
else
    {
    dsInitializeOut()
    vfprintf(dsOut,format, args);
    }
va_end(args);
return len;
}


int dsPrintDirectly(int token,FILE *file)
// Prints the contents of the top buffer directly to a file.
// Will leave the filled buffer at top of stack
// Returns the length printed.
{
dsAssertToken(token);

int len = dyStringLen(dsStack);
if (len != 0)
    fprintf(file,"%s",dyStringContents(dsStack));
    fflush(file);
return len;
}


int dsPrintFlush(int token)
// Flushes the top buffer to the next lower one and empties top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.
{
dsAssertToken(token);

int len = dyStringLen(dsStack);
if (len != 0)
    {
    if (dsStack->next == NULL)
        {
        dsInitializeOut();
        fprintf(dsOut,"%s",dyStringContents(dsStack));
        fflush(dsOut);
        }
    else
        dyStringAppend(dsStack->next,dyStringContents(dsStack));
    dyStringClear(dsStack);
    }
return len;
}


int dsPrintClose(int token)
// Abandons the top buffer and its content, freeing memory.
// Returns the length abandoned.
{
dsAssertToken(token);
int len = dyStringLen(dsStack);
struct dyString *ds = slPopHead(&dsStack);
dyStringFree(&ds);
return len;
}

int dsPrintFlushAndClose(int token)
// Flushes the top buffer to the next lower one and frees the top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.
{
int len = dsPrintFlush(token);
dsPrintClose(token);
return len;
}


char * dsPrintContent(int token)
// returns the content of the current buffer as a string, but does not flush or free it
// NOTE: returned pointer is not cloned memory and will be changed by successive dsPrint calls
{
dsAssertToken(token);
return dyStringContents(dsStack);
}


int dsPrintEmpty(int token)
// Empties the top buffer in stack (abandoning content), but leave buffer in place
// Returns the length abandoned.
{
dsAssertToken(token);
int len = dyStringLen(dsStack);
dyStringClear(dsStack);
return len;
}


static int dsPrintStackIxFromToken(int token)
// Return the stack member corresponding to the token.
// This ix can be used to walk up the stack from a given point
{
struct dyString *ds = dsStack; 
int ix = 0;

for (; ds != NULL; ds = ds->next, ++ix)
    {
    if (dsPointerToToken(ds) == token)
        return ix;
    }
return -1;
}


static struct dyString *dsPrintStackMemberFromIx(int ix)
// Return the stack member corresponding to the ix.
// By walking stack ix's, you can walk up or down the stack
{
if (ix < 0)
    return NULL;
return slElementFromIx(dsStack,ix);
}


int dsPrintSize(int token)
// Returns the curent length of the buffer starting at the level corresponding to token.
// Unlike other cases, the token does not have to correspond to the top of the stack!
{
//dsAssertToken(token);
//return dyStringLen(dsStack);
int ix = dsPrintStackIxFromToken(token);
assert(ix >= 0);

int len = 0;
struct dyString *ds = NULL;
for (;ix>0;ix--)
    {
    ds = dsPrintStackMemberFromIx(ix);
    assert(ds != NULL);
    len += dyStringLen(ds);
    }
len += dyStringLen(dsStack);
return len;
}


int dsPrintStackDepth()
// Returns the current dsPrint buffer stack depth
{
return slCount(dsStack);
}


void dsPrintRegisterOut(FILE *out)
// Registers an alternative to STDOUT.  After registering, all dsPrint out goes to this file.
// NOTE: There is no stack. Register/Unregister serially.
{
dsAssertUnregisteredOut();
dsOut = out;
}


void dsPrintUnregisterOut(FILE *out)
// Unregisters the alternative to STDOUT.  After unregistering all dsPrint out goes to STDOUT.
// NOTE: There is no stack. Register/Unregister serially.
{
dsAssertRegisteredOut(out);
dsOut = stdout;
}


int dsPrintSizeAll()
// returns combined current size of all buffers
{
int len = 0;
if (dsStack != NULL)
    {
    struct dyString *ds = dsStack;
    for (;ds != NULL; ds = ds->next)
        len += dyStringLen(ds);
    }
return len;
}


int dsPrintFlushAndCloseAll()
// flushes, frees and closes all stacked buffers
// returns length flushed
{
int len = 0;
if (dsStack != NULL)
    {
    while (dsStack != NULL)
        {
        int token = dsPointerToToken(dsStack);
        len = dsPrintFlushAndClose(token); // Only the last length is needed!
        }
    }
return len;
}


char *dsPrintCannibalizeAndClose(int token)
// Closes the top stack buffer returning content.  Returned string should be freed.
{
dsAssertToken(token);
struct dyString *ds = slPopHead(&dsStack);
return dyStringCannibalize(&ds);
}


char *dsPrintCannibalizeAndCloseAll()
// Closes all stack buffers and returns a single string that is the full content.  
// Returned string should be freed.
{
while (dsStack != NULL && dsStack->next != NULL)
    {
    int token = dsPointerToToken(dsStack);
    dsPrintFlushAndClose(token); // Flushes each to the next lower buffer
    }
if (dsStack == NULL)
    return NULL;
else
    return dyStringCannibalize(&dsStack);
}

/*
void dsPrintTest()
// Tests of dsPrint functions
{
printf("Plain printf\n");
dsPrintf("1 dsPrintf unopened\n");

int token1 = dsPrintOpen(0);
dsPrintf("2 dsOpen:1\n");
dsPrintFlush(token1);
dsPrintf("3^10   1:%d after flush\n",dsPrintStackDepth());
int token2 = dsPrintOpen(256);
dsPrintf("4 dsOpen:2 len1:%d  len2:%d  lenAll:%d\n",
         dsPrintSize(token1),dsPrintSize(token2),dsPrintSizeAll());
dsPrintRegisterOut(stderr);
dsPrintFlush(token2);
dsPrintUnregisterOut(stderr);
dsPrintf("5      2:%d After flush  len1:%d  len2:%d  lenAll:%d\n",
         dsPrintStackDepth(),dsPrintSize(token1),dsPrintSize(token2),dsPrintSizeAll());
dsPrintFlushAndClose(token2);
dsPrintf("6      1:%d After flushAndClose:2  len1:%d\n",dsPrintStackDepth(),dsPrintSize(token1));
token2 = dsPrintOpen(256);
dsPrintf("7 dsOpen:2 reopen:2  WILL ABANDON CONTENT\n");
char *content = cloneString(dsPrintContent(token2));
dsPrintAbandonContent(token2);
dsPrintf("8x7    2:%d len1:%d len2:%d lenAll:%d isEmpty2:%s\n",
         dsPrintStackDepth(),dsPrintSize(token1),dsPrintSize(token2),dsPrintSizeAll(),
         (dsPrintIsEmpty(token2) ? "EMPTY":"NOT_EMPTY"));
strSwapChar(content,'\n','~');  // Replace newline before printing.
dsPrintf("9      2:%d No flush yet   len1:%d  len2:%d  lenAll:%d  prev7:[%s]\n",
         dsPrintStackDepth(),dsPrintSize(token1),dsPrintSize(token2),dsPrintSizeAll(),content);
freez(&content);
int token3 = dsPrintOpen(256);
dsPuts("10  Open:3 Line doubled and out of turn due to dsPrintDirectly()");
dsPrintDirectly(token3,stdout);
dsPrintf("11     3:%d Just prior to closing all.  len1:%d  len2:%d  len3:%d  lenAll:%d",
         dsPrintStackDepth(),
         dsPrintSize(token1),dsPrintSize(token2),dsPrintSize(token3),dsPrintSizeAll());
int token4 = dsPrintOpen(256);
dsPrintf("12  Open:4 Opened  stack:%d  WILL ABANDON\n",dsPrintStackDepth());
dsPrintAbandon(token4);
dsPutc('\n');
dsPuts("13  Puts:  Added last '\\n' with dsPutc(), this with dsPuts().");
dsPrintf("14     3:%d Last line!  Expect 7 & 12 missing, 10 dupped.  lenAll:%d  Bye.\n",
         dsPrintStackDepth(),dsPrintSizeAll());
dsPrintFlushAndCloseAll();
assert(dsStack == NULL);
int ix = 0;
for (;ix<20;ix++) // tested to 1000
    {
    dsPrintOpen(32); // Don't even care about tokens!
    if (ix == 0)
        dsPrintf("CannibalizeAndCloseAll():");
    dsPrintf(" %d",dsPrintStackDepth());
    }
content = dsPrintCannibalizeAndCloseAll();
printf("\n[%s]\n",content);
freez(&content);
}
*/

