// dsPrint - Dynamic string Stack based Printing.
//
// This module relies upon a dyString based stack of buffers which accumulate output
// for later printing.  As a stack, only the top buffer can be filled and flushed out
// (dsPrintFlush).  When flushing, the top buffer's content is appended to the next
// lower buffer, unless explicitly requested otherwise (dsPrintDirectly).
// If the stack is empty, dsPrintf will be equivalent to printf.
//
// Output goes to STDOUT unless a single alternative out file is registered.

#ifndef DSPRINT_H      /* Wrapper to avoid including this twice. */
#define DSPRINT_H

#include "common.h"
#include "dystring.h"

int dsPrintOpen(int initialBufSize);
// Allocate dynamic string and puts at top of stack
// returns token to be used for later stack accounting asserts

int dsPrintf(char *format, ...);
// Prints into end of the top ds buffer, and return resulting string length
// If there is no current buffer, this acts like a simple printf and returns -1
#define dsPuts(str) dsPrintf("%s\n",str)
#define dsPutc(chr) dsPrintf("%c",chr)
#define SWAP_PRINTF
#ifdef SWAP_PRINTF
#define printf dsPrintf
#define puts dsPuts
#undef putc
#define putc dsPutc
#endif//def SWAP_PRINTF

int dsPrintDirectly(int token,FILE *file);
// Prints the contents of the top buffer directly to a file.
// Will leave the filled buffer at top of stack
// Returns the length printed.

int dsPrintFlush(int token);
// Flushes the top buffer to the next lower one and empties top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.

int dsPrintClose(int token);
// Abandons the top buffer and its content, freeing memory.
// Returns the length abandoned.
#define dsPrintAbandon(token) dsPrintClose(token)

int dsPrintFlushAndClose(int token);
// Flushes the top buffer to the next lower one and frees the top buffer.
// If there is no lower buffer then the content is printed to STDOUT (or registered out).
// Returns the length flushed.

char * dsPrintContent(int token);
// returns the content of the current buffer as a string, but does not flush or free it
// NOTE: returned pointer is not cloned memory and will be changed by successive dsPrint calls

int dsPrintEmpty(int token);
// Empties the top buffer in stack (abandoning content), but leave buffer in place
// Returns the length abandoned.
#define dsPrintAbandonContent(token) dsPrintEmpty(token)

int dsPrintSize(int token);
// Returns the curent length of the buffer starting at the level corresponding to token.
// Unlike other cases, the token does not have to correspond to the top of the stack!
#define dsPrintIsEmpty(token) (dsPrintSize(token) == 0)

int dsPrintStackDepth();
// Returns the current dsPrint buffer stack depth
#define dsPrintIsOpen() (dsPrintStackDepth() > 0)

void dsPrintRegisterOut(FILE *out);
// Registers an alternative to STDOUT.  After registering, all dsPrint out goes to this file.
// NOTE: There is no stack. Register/Unregister serially.

void dsPrintUnregisterOut(FILE *out);
// Unregisters the alternative to STDOUT.  After unregistering all dsPrint out goes to STDOUT.
// NOTE: There is no stack. Register/Unregister serially.

int dsPrintSizeAll();
// returns combined current size of all buffers
#define dsPrintIsAllEmpty(token) (dsPrintSizeAll() == 0)

int dsPrintFlushAndCloseAll();
// flushes, frees and closes all stacked buffers
// returns length flushed

char *dsPrintCannibalizeAndClose(int token);
// Closes the top stack buffer returning content.  Returned string should be freed.

char *dsPrintCannibalizeAndCloseAll();
// Closes all stack buffers and returns a single string that is the full content.  
// Returned string should be freed.

//void dsPrintTest();
// Tests of dsPrint functions

/* Next steps:
 * 1) DONE WITH MACROS: in cheapCgi.c replace all printfs with dsPrintf
 * 2) DONE WITH MACROS: in hui.c replace all printfs with dsPrintf
 * 3) DONE WITH MACROS: in hgTrackUi replace all printfs with dsPrintf
 * 4) DONE: After 1-3, add opens and closes to hgTrackUi
 * 5) DONE: Handle boxing cfg with dsPrintf (note existing cfgs that support boxing still do it)
 * 6) Handle isConfigurable tests with dsPrintf and throw away results
 *    This one will require making hgTrackUi Cfgs all lib code.
 */

#endif /* DSPRINT_H */
