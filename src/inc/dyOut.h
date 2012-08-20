// dsPrint - Dynamic string Stack based Printing.  
//
// CAUTION: Inclusion of this header will convert printfs to dspPrintfs!!!
//
// This module relies upon a dyString based stack of buffers which accumulate output
// for later printing.  As a stack, only the top buffer can be filled and flushed out
// (dspFlush).  When flushing, the top buffer's content is appended to the next
// lower buffer, unless explicitly requested otherwise (dspPrintDirectly).
// If the stack is empty, dsPrintf will be equivalent to printf.
//
// NOTE: module prefix is 'dsp' not 'dsPrint' so that "print" is always used as a verb.
//
// Output goes to STDOUT unless a single alternative out file is registered.

#ifdef DSPRINT_H      // NEVER INCLUDE TWICE.  ONLY INCLUDE DIRECTLY IN C FILES.
#error Including dsPrint.h twice! Only include directly in C files as this redefines printf !!!
#else//ifndef DSPRINT_H
#define DSPRINT_H

#include "common.h"
#include "dystring.h"

int dspOpen(int initialBufSize);
// Allocate dynamic string and puts at top of stack
// returns token to be used for later stack accounting asserts

int dspPrintf(char *format, ...);
// Prints into end of the top ds buffer, and return resulting string length
// If there is no current buffer, this acts like a simple printf and returns -1
#define dspPuts(str) dspPrintf("%s\n",str)
#define dspPutc(chr) dspPrintf("%c",chr)
#define SWAP_PRINTF
#ifdef SWAP_PRINTF
#define printf dspPrintf
#define puts dspPuts
#undef putc
#define putc dspPutc
#endif//def SWAP_PRINTF

int dspPrintDirectly(int token,FILE *file);
// Prints the contents of the top buffer directly to a file.
// Will leave the filled buffer at top of stack
// Returns the length printed.

int dspFlush(int token);
// Flushes the top buffer to the next lower one and empties top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.

int dspClose(int token);
// Abandons the top buffer and its content, freeing memory.
// Returns the length abandoned.
#define dspAbandon(token) dspClose(token)

int dspFlushAndClose(int token);
// Flushes the top buffer to the next lower one and frees the top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.

int dspFlushAndCloseAll();
// CAUTION: Bad practice to not Open/Close levels in turn!
// flushes, frees and closes all stacked buffers
// returns length flushed

char * dspContent(int token);
// returns the content of the current buffer as a string, but does not flush or free it
// NOTE: returned pointer is not cloned memory and will be changed by successive dsPrint calls

int dspEmpty(int token);
// Empties the top buffer in stack (abandoning content), but leave buffer in place
// Returns the length abandoned.
#define dspAbandonContent(token) dspEmpty(token)

int dspSize(int token);
// Returns the current length of the buffer starting at the level corresponding to token.
// Unlike other cases, the token does not have to correspond to the top of the stack!
#define dspIsEmpty(token) (dspSize(token) == 0)

int dspSizeAll();
// returns combined current size of all buffers
#define dspIsAllEmpty(token) (dspSizeAll() == 0)

int dspStackDepth();
// Returns the current dsPrint buffer stack depth
#define dspIsOpen() (dspStackDepth() > 0)

void dspRegisterOutStream(FILE *out);
// Registers an alternative to STDOUT.  After registering, all dsPrint out goes to this file.
// NOTE: There is no stack. Register/Unregister serially.

void dspUnregisterOutStream(FILE *out);
// Unregisters the alternative to STDOUT.  After unregistering all dsPrint out goes to STDOUT.
// NOTE: There is no stack. Register/Unregister serially.

char *dspCannibalizeAndClose(int token);
// Closes the top stack buffer returning content.  Returned string should be freed.

char *dspCannibalizeAndCloseAll();
// CAUTION: Bad practice to not Open/Close levels in turn!
// Closes all stack buffers and returns a single string that is the full content.  
// Returned string should be freed.

void *dspStackPop(int token);
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Returns a pointer which can be dspStackPush'd back into place.
// Pop/Push is useful for inserting print immediately before a dspOpen'd print buffer

void dspStackPush(int token,void *dspPointer);
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Push a previously dspStackPop'd print buffer back onto the stack
// Pop/Push is useful for inserting print immediately before a dspOpen'd print buffer


#ifdef NOT_NOW
// Requires regex function added to dystring.c which has been successfully tested.
boolean dspRegexReplace(int token, char *regExpression, char *replaceWith);
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Looks for and replaces a single instance of a wildcard match in the current dsp print buffer
// regex follows normal EXTENDED (egrep) rules except:
// ^ - at beginning of the regExpression only and matches beginning of buffer (not of line)
// $ - at end of regExpression only and matches end of buffer (not end of line)
// \n - newlines in the body of the regExpression will match newlines in the current print buffer
// Returns TRUE on success
// CAUTION: Wildcards match largest sub-string [w.*d] matches all of "wild wild world"


//void dsPrintTest();
// Tests of dsPrint functions
#endif///def NOT_NOW

/* Next steps:
 * 1) DONE WITH MACROS: in cheapCgi.c replace all printfs with dsPrintf
 * 2) DONE WITH MACROS: in hui.c replace all printfs with dsPrintf
 * 3) DONE WITH MACROS: in hgTrackUi replace all printfs with dsPrintf
 * 4) DONE: After 1-3, add opens and closes to hgTrackUi
 * 5) DONE: Handle boxing cfg with dsPrintf (successful test)
 * 6) Handle isConfigurable tests with dsPrintf and throw away results
 *    This one will require making hgTrackUi Cfgs all lib code.
 */

#endif /* DSPRINT_H */
