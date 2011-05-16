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

// developer: this include is for an occasionally useful means of getting stack info without crashing
// however, it is not supported on cygwin.  Conditionally compile this in when desired.
//#define BACKTRACE_EXISTS
#ifdef BACKTRACE_EXISTS
#include <execinfo.h>
#endif///def BACKTRACE_EXISTS
#include "common.h"
#include "dystring.h"
#include "errabort.h"

static char const rcsid[] = "$Id: errabort.c,v 1.16 2010/01/12 18:16:27 markd Exp $";

static boolean debugPushPopErr = FALSE; // generate stack dump on push/pop error
boolean errAbortInProgress = FALSE;  /* Flag to indicate that an error abort is in progress.
                                      * Needed so that a warn handler can tell if it's really
                                      * being called because of a warning or an error. */

static void defaultVaWarn(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) {
    fflush(stdout);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    }
}

static void silentVaWarn(char *format, va_list args)
/* Warning handler that just hides it.  Useful sometimes when high level code
 * expects low level code may fail (as in finding a file on the net) but doesn't
 * want user to be bothered about it. */
{
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

void warnWithBackTrace(char *format, ...)
/* Issue a warning message and append backtrace. */
{
va_list args;
va_start(args, format);
struct dyString *dy = newDyString(255);
dyStringAppend(dy, format);

#define STACK_LIMIT 20
char **strings = NULL;
int count = 0;

// developer: this is an occasionally useful means of getting stack info without crashing
// however, it is not supported on cygwin.  Conditionally compile this in when desired.
// The define is at top to include execinfo.h
#ifdef BACKTRACE_EXISTS
void *buffer[STACK_LIMIT];
count = backtrace(buffer, STACK_LIMIT);
strings = backtrace_symbols(buffer, count);
#endif///def BACKTRACE_EXISTS

if (strings == NULL)
    dyStringAppend(dy,"\nno backtrace_symbols available in errabort::warnWithBackTrace().");
else
    {
    dyStringAppend(dy,"\nBACKTRACE [can use 'addr2line -Cfise {exe} addr addr ...']:");
    int ix;
    for (ix = 1; ix < count && strings[ix] != NULL; ix++)
        dyStringPrintf(dy,"\n%s", strings[ix]);

    free(strings);
    }
vaWarn(dyStringCannibalize(&dy), args);
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
    {
    if (debugPushPopErr)
        dumpStack("pushWarnHandler overflow");
    errAbort("Too many pushWarnHandlers, can only handle %d\n", maxWarnHandlers-1);
    }
warnArray[++warnIx] = handler;
}

void popWarnHandler()
/* Revert to old warn handler. */
{
if (warnIx <= 0)
    {
    if (debugPushPopErr)
        dumpStack("popWarnHandler underflow");
    errAbort("Too many popWarnHandlers");
    }
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
/* flag is needed because both errAbort and warn generate message
 * using the warn handler, however sometimes one needed to know
 * (like when logging), if it's an error or a warning.  This is far from
 * perfect, as this isn't cleared if the error handler continues,
 * as with an exception mechanism. */
errAbortInProgress = TRUE;
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
    {
    if (debugPushPopErr)
        dumpStack("pushAbortHandler overflow");
    errAbort("Too many pushAbortHandlers, can only handle %d", maxAbortHandlers-1);
    }
abortArray[++abortIx] = handler;
}

void popAbortHandler()
/* Revert to old abort handler. */
{
if (abortIx <= 0)
    {
    if (debugPushPopErr)
        dumpStack("popAbortHandler underflow");
    errAbort("Too many popAbortHandlers\n");
    }
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

void pushSilentWarnHandler()
/* Set warning handler to be quiet.  Do a popWarnHandler to restore. */
{
pushWarnHandler(silentVaWarn);
}

void errAbortDebugnPushPopErr()
/*  generate stack dump if there is a error in the push/pop functions */
{
debugPushPopErr = TRUE;
}

