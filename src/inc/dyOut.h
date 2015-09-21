// dyOut - DYnamic string stack based OUTput
//
// CAUTION: Inclusion of this header will convert printfs to dyOutPrintfs!!!
//          This is an Experimental/Test strategy only.  When dyOut is adopted,
//          direct calls to dyOut functions should be used and printf macros removed.
//
// This module relies upon a dyString based stack of buffers which accumulate output
// for later printing.  As a stack, only the top buffer can be filled and flushed out
// (duOutFlush).  When flushing, the top buffer's content is appended to the next
// lower buffer, unless explicitly requested otherwise (dyOutDirectly).
// If the stack is empty, dyOutPrintf will be equivalent to printf.
//
// Output goes to STDOUT unless a single alternative out file is registered.

#ifdef DSPRINT_H      // NEVER INCLUDE TWICE.  ONLY INCLUDE DIRECTLY IN C FILES.
#error Including dyOut.h twice! Only include directly in C files as this redefines printf !!!
#else//ifndef DSPRINT_H
#define DSPRINT_H

#include "common.h"
#include "dystring.h"

int dyOutOpen(int initialBufSize);
// Allocate dynamic string and puts at top of stack
// returns token to be used for later stack accounting asserts

int dyOutPrintf(char *format, ...)
// Prints into end of the top ds buffer, and return resulting string length
// If there is no current buffer, this acts like a simple printf and returns -1
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;
#define dyOutPuts(str) dyOutPrintf("%s\n",str)
#define dyOutPutc(chr) dyOutPrintf("%c",chr)
#define SWAP_PRINTF
#ifdef SWAP_PRINTF
#define printf dyOutPrintf
#define puts dyOutPuts
#undef putc
#define putc dyOutPutc
#endif//def SWAP_PRINTF

int dyOutPrintDirectly(int token,FILE *file);
// Prints the contents of the top buffer directly to a file.
// Will leave the filled buffer at top of stack
// Returns the length printed.

int dyOutFlush(int token);
// Flushes the top buffer to the next lower one and empties top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.

int dyOutClose(int token);
// Abandons the top buffer and its content, freeing memory.
// Returns the length abandoned.
#define dyOutAbandon(token) dyOutClose(token)

int dyOutFlushAndClose(int token);
// Flushes the top buffer to the next lower one and frees the top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.

int dyOutFlushAndCloseAll();
// CAUTION: Bad practice to not Open/Close levels in turn!
// flushes, frees and closes all stacked buffers
// returns length flushed

char * dyOutContent(int token);
// returns the content of the current buffer as a string, but does not flush or free it
// NOTE: returned pointer is not cloned memory and will be changed by successive dyOut calls

int dyOutEmpty(int token);
// Empties the top buffer in stack (abandoning content), but leave buffer in place
// Returns the length abandoned.
#define dyOutAbandonContent(token) dyOutEmpty(token)

int dyOutSize(int token);
// Returns the current length of the buffer starting at the level corresponding to token.
// Unlike other cases, the token does not have to correspond to the top of the stack!
#define dyOutIsEmpty(token) (dyOutSize(token) == 0)

int dyOutSizeAll();
// returns combined current size of all buffers
#define dyOutIsAllEmpty(token) (dyOutSizeAll() == 0)

int dyOutStackDepth();
// Returns the current dyOut buffer stack depth
#define dyOutIsOpen() (dyOutStackDepth() > 0)

void dyOutRegisterAltStream(FILE *out);
// Registers an alternative to STDOUT.  After registering, all dyOut out goes to this file.
// NOTE: There is no stack. Register/Unregister serially.

void dyOutUnregisterAltStream(FILE *out);
// Unregisters the alternative to STDOUT.  After unregistering all dyOut out goes to STDOUT.
// NOTE: There is no stack. Register/Unregister serially.

char *dyOutCannibalizeAndClose(int token);
// Closes the top stack buffer returning content.  Returned string should be freed.

char *dyOutCannibalizeAndCloseAll();
// CAUTION: Bad practice to not Open/Close levels in turn!
// Closes all stack buffers and returns a single string that is the full content.  
// Returned string should be freed.

void *dyOutStackPop(int token);
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Returns a pointer which can be dyOutStackPush'd back into place.
// Pop/Push is useful for inserting print immediately before a dyOutOpen'd print buffer

void dyOutStackPush(int token,void *dyOutPointer);
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Push a previously dyOutStackPop'd print buffer back onto the stack
// Pop/Push is useful for inserting print immediately before a dyOutOpen'd print buffer


#ifdef NOT_NOW
// Requires regex function added to dystring.c which has been successfully tested.
boolean dyOutRegexReplace(int token, char *regExpression, char *replaceWith);
// CAUTION: Not for the faint of heart: print buffer surgery is no substitute for proper coding.
// Looks for and replaces a single instance of a wildcard match in the current dyOut print buffer
// regex follows normal EXTENDED (egrep) rules except:
// ^ - at beginning of the regExpression only and matches beginning of buffer (not of line)
// $ - at end of regExpression only and matches end of buffer (not end of line)
// \n - newlines in the body of the regExpression will match newlines in the current print buffer
// Returns TRUE on success
// CAUTION: Wildcards match largest sub-string [w.*d] matches all of "wild wild world"


//void dyOutTest();
// Tests of dyOut functions
#endif///def NOT_NOW

/* Next steps:
 * 1) DONE WITH MACROS: in cheapCgi.c replace all printfs with dyOutPrintf
 * 2) DONE WITH MACROS: in hui.c replace all printfs with dyOutPrintf
 * 3) DONE WITH MACROS: in hgTrackUi replace all printfs with dyOutPrintf
 * 4) DONE: After 1-3, add opens and closes to hgTrackUi
 * 5) DONE: Handle boxing cfg with dyOutPrintf (successful test)
 * 6) Handle isConfigurable tests with dyOutPrintf and throw away results
 *    This one will require making hgTrackUi Cfgs all lib code.
 */

#endif /* DSPRINT_H */
