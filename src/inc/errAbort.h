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
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef ERRABORT_H
#define ERRABORT_H

boolean isErrAbortInProgress();  
/* Flag to indicate that an error abort is in progress.
 * Needed so that a warn handler can tell if it's really
 * being called because of a warning or an error. */

void errAbortSetDoContentType(boolean value);
/* change the setting of doContentType, ie. if errorAbort should print a 
 * http Content type line. */

void errAbort(char *format, ...)
/* Abort function, with optional (printf formatted) error message. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void vaErrAbort(char *format, va_list args);
/* Abort function, with optional (vprintf formatted) error message. */

void errnoAbort(char *format, ...)
/* Prints error message from UNIX errno first, then does errAbort. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

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

void warn(char *format, ...)
/* Issue a warning message. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void errnoWarn(char *format, ...)
/* Prints error message from UNIX errno first, then does rest of warning. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

typedef void (*WarnHandler)(char *format, va_list args);
/* Function that can warn. */

void pushWarnHandler(WarnHandler handler);
/* Set warning handler */

void popWarnHandler();
/* Revert to old warn handler. */

void pushWarnAbort();
/* Push handler that will abort on warnings. */

void pushSilentWarnHandler();
/* Set warning handler to be quiet.  Do a popWarnHandler to restore. */

void errAbortDebugnPushPopErr();
/*  generate stack dump if there is a error in the push/pop functions */

#endif /* ERRABORT_H */
