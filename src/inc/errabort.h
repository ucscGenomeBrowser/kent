/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ErrAbort.h - our error handler. 
 *
 * This maintains two stacks - a warning message printer
 * stack, and a "abort handler" stack.
 *
 * By default the warnings will go to stderr, and
 * aborts will exit the program.  You can push a
 * function on to the appropriate stack to change
 * this behavior.  The top function on the stack
 * gets called.
 *
 * Most functions in this library will call errAbort()
 * if they run out of memory.  
 */


void errAbort(char *format, ...);
/* Abort function, with optional (printf formatted) error message. */

void vaErrAbort(char *format, va_list args);
/* Abort function, with optional (vprintf formatted) error message. */

void errnoAbort(char *format, ...);
/* Prints error message from UNIX errno first, then does errAbort. */

typedef void (*AbortHandler)();
/* Function that can abort. */

void pushAbortHandler(AbortHandler handler);
/* Set abort handler */

void popAbortHandler();
/* Revert to old abort handler. */

void noWarnAbort();
/* Abort without message. */

void pushDebugAbort();
/* Push abort handler that will invoke debugger. */

void vaWarn(char *format, va_list args);
/* Call top of warning stack to issue warning. */

void warn(char *format, ...);
/* Issue a warning message. */

typedef void (*WarnHandler)(char *format, va_list args);
/* Function that can warn. */

void pushWarnHandler(WarnHandler handler);
/* Set warning handler */

void popWarnHandler();
/* Revert to old warn handler. */

