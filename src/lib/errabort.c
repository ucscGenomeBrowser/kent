/* ErrAbort.c - our error handler. 
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
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "errabort.h"

static char const rcsid[] = "$Id: errabort.c,v 1.13 2004/11/10 00:10:50 markd Exp $";

static void defaultVaWarn(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) {
    fflush(stdout);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    }
}

#define maxWarnHandlers 20
static WarnHandler warnArray[maxWarnHandlers] = {defaultVaWarn,};
static int warnIx = 0;

void vaWarn(char *format, va_list args)
/* Call top of warning stack to issue warning. */
{
warnArray[warnIx](format, args);
}

void warn(char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);
}

void errnoWarn(char *format, ...)
/* Prints error message from UNIX errno first, then does rest of warning. */
{
char fbuf[512];
va_list args;
va_start(args, format);
sprintf(fbuf, "%s\n%s", strerror(errno), format);
vaWarn(fbuf, args);
va_end(args);
}


void pushWarnHandler(WarnHandler handler)
/* Set abort handler */
{
if (warnIx >= maxWarnHandlers-1)
    errAbort("Too many pushWarnHandlers, can only handle %d\n", maxWarnHandlers-1);
warnArray[++warnIx] = handler;
}

void popWarnHandler()
/* Revert to old warn handler. */
{
if (warnIx <= 0)
    errAbort("Too many popWarnHandlers\n");
--warnIx;
}

static void defaultAbort()
/* Default error handler exits program. */
{
if ((getenv("ERRASSERT") != NULL) || (getenv("ERRABORT") != NULL))
    abort();
else
    exit(-1);
}

#define maxAbortHandlers 12
static AbortHandler abortArray[maxAbortHandlers] = {defaultAbort,};
static int abortIx = 0;

void noWarnAbort()
/* Abort without message. */
{
abortArray[abortIx]();
exit(-1);		/* This is just to make compiler happy. 
                         * We have already exited or longjmped by now. */
}

void vaErrAbort(char *format, va_list args)
/* Abort function, with optional (vprintf formatted) error message. */
{
vaWarn(format, args);
noWarnAbort();
}

void errAbort(char *format, ...)
/* Abort function, with optional (printf formatted) error message. */
{
va_list args;
va_start(args, format);
vaErrAbort(format, args);
va_end(args);
}

void errnoAbort(char *format, ...)
/* Prints error message from UNIX errno first, then does errAbort. */
{
char fbuf[512];
va_list args;
va_start(args, format);
sprintf(fbuf, "%s\n%s", strerror(errno), format);
vaErrAbort(fbuf, args);
va_end(args);
}

void pushAbortHandler(AbortHandler handler)
/* Set abort handler */
{
if (abortIx >= maxAbortHandlers-1)
    errAbort("Too many pushAbortHandlers, can only handle %d\n", maxAbortHandlers-1);
abortArray[++abortIx] = handler;
}

void popAbortHandler()
/* Revert to old abort handler. */
{
if (abortIx <= 0)
    errAbort("Too many popAbortHandlers\n");
--abortIx;
}

static void debugAbort()
/* Call the debugger. */
{
fflush(stdout);
assert(FALSE);
defaultAbort();
}

void pushDebugAbort()
/* Push abort handler that will invoke debugger. */
{
pushAbortHandler(debugAbort);
}

static void warnAbortHandler(char *format, va_list args)
/* warn handler that also aborts. */
{
defaultVaWarn(format, args);
noWarnAbort();
}

void pushWarnAbort()
/* Push handler that will abort on warnings. */
{
pushWarnHandler(warnAbortHandler);
}
