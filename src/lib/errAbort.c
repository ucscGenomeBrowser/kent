/* errAbort.c - our error handler.
 *
 * This maintains two stacks - a warning message printer
 * stack, and a "abort handler" stack.
 *
 * Note that the abort function always calls the warn handler first.
 * This is so that the message gets sent.
 *
 * By default the warnings will go to stderr, and
 * aborts will exit the program.  You can push a
 * function on to the appropriate stack to change
 * this behavior.  The top function on the stack
 * gets called.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

// developer: this include is for an occasionally useful means of getting stack info without
// crashing
// however, it is not supported on cygwin.  Conditionally compile this in when desired.
//#define BACKTRACE_EXISTS
#ifdef BACKTRACE_EXISTS
#include <execinfo.h>
#endif///def BACKTRACE_EXISTS
#include <pthread.h>
#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "errAbort.h"



#define maxWarnHandlers 20
#define maxAbortHandlers 12
struct perThreadAbortVars
/* per thread variables for abort and warn */
    {
    boolean debugPushPopErr;        // generate stack dump on push/pop error
    boolean errAbortInProgress;     /* Flag to indicate that an error abort is in progress.
                                      * Needed so that a warn handler can tell if it's really
                                      * being called because of a warning or an error. */
    WarnHandler warnArray[maxWarnHandlers];
    int warnIx;
    AbortHandler abortArray[maxAbortHandlers];
    int abortIx;
    };

static struct perThreadAbortVars *getThreadVars();  // forward declaration

static void defaultVaWarn(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) {
    fflush(stdout);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    }
}

static void silentVaWarn(char *format, va_list args)
/* Warning handler that just hides it.  Useful sometimes when high level code
 * expects low level code may fail (as in finding a file on the net) but doesn't
 * want user to be bothered about it. */
{
}


void vaWarn(char *format, va_list args)
/* Call top of warning stack to issue warning. */
{
struct perThreadAbortVars *ptav = getThreadVars();
ptav->warnArray[ptav->warnIx](format, args);
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
    int ix = 1;
    dyStringAppend(dy,"\nBACKTRACE (use on cmdLine):");
    if (strings[1] != NULL)
        {
        strSwapChar(strings[1],' ','\0');
        dyStringPrintf(dy,"\naddr2line -Cfise %s",strings[1]);
        strings[1] += strlen(strings[1]) + 1;
        }
    for (; ix < count && strings[ix] != NULL; ix++)
        {
        strings[ix] = skipBeyondDelimit(strings[ix],'[');
        strSwapChar(strings[ix],']','\0');
        dyStringPrintf(dy," %s",strings[ix]);
        }

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
struct perThreadAbortVars *ptav = getThreadVars();
if (ptav->warnIx >= maxWarnHandlers-1)
    {
    if (ptav->debugPushPopErr)
        dumpStack("pushWarnHandler overflow");
    errAbort("Too many pushWarnHandlers, can only handle %d\n", maxWarnHandlers-1);
    }
ptav->warnArray[++ptav->warnIx] = handler;
}

void popWarnHandler()
/* Revert to old warn handler. */
{
struct perThreadAbortVars *ptav = getThreadVars();
if (ptav->warnIx <= 0)
    {
    if (ptav->debugPushPopErr)
        dumpStack("popWarnHandler underflow");
    errAbort("Too few popWarnHandlers");
    }
--ptav->warnIx;
}

static void defaultAbort()
/* Default error handler exits program. */
{
if ((getenv("ERRASSERT") != NULL) || (getenv("ERRABORT") != NULL))
    abort();
else
    exit(-1);
}


void noWarnAbort()
/* Abort without message. */
{
struct perThreadAbortVars *ptav = getThreadVars();
ptav->abortArray[ptav->abortIx]();
exit(-1);               /* This is just to make compiler happy.
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
struct perThreadAbortVars *ptav = getThreadVars();
ptav->errAbortInProgress = TRUE;
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
struct perThreadAbortVars *ptav = getThreadVars();
if (ptav->abortIx >= maxAbortHandlers-1)
    {
    if (ptav->debugPushPopErr)
        dumpStack("pushAbortHandler overflow");
    errAbort("Too many pushAbortHandlers, can only handle %d", maxAbortHandlers-1);
    }
ptav->abortArray[++ptav->abortIx] = handler;
}

void popAbortHandler()
/* Revert to old abort handler. */
{
struct perThreadAbortVars *ptav = getThreadVars();
if (ptav->abortIx <= 0)
    {
    if (ptav->debugPushPopErr)
        dumpStack("popAbortHandler underflow");
    errAbort("Too many popAbortHandlers\n");
    }
--ptav->abortIx;
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
struct perThreadAbortVars *ptav = getThreadVars();
ptav->debugPushPopErr = TRUE;
}

boolean isErrAbortInProgress() 
/* Flag to indicate that an error abort is in progress.
 * Needed so that a warn handler can tell if it's really
 * being called because of a warning or an error. */
{
struct perThreadAbortVars *ptav = getThreadVars();
return ptav->errAbortInProgress;
}


static struct perThreadAbortVars *getThreadVars()
/* Return a pointer to the perThreadAbortVars for the current pthread. */
{
pthread_t pid = pthread_self(); //  pthread_t can be a pointer or a number, implementation-dependent.

// Test for out-of-memory condition causing re-entrancy into this function that would block
// on its own main mutex ptavMutex.  Do this by looking for its own pid.
// Some care must be exercised in testing and comparing the threads pid against one in-use.
// We need yet another mutex and a boolean to tell us when the pidInUse value may be safely compared to pid.

// Use a boolean since there is no known unused value for pthread_t variable. NULL and -1 are not portable.
static boolean pidInUseValid = FALSE;  // tells when pidInUse contains a valid pid that can be compared.
static pthread_t pidInUse; // there is no "unused" value to which we can initialize this.
static pthread_mutex_t pidInUseMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock( &pidInUseMutex );
// If this pid equals pidInUse, then this function has been re-entered due to severe out-of-memory error.
// But we only compare them when pidInUseValid is TRUE.
if (pidInUseValid && pthread_equal(pid, pidInUse)) 
    {
    // Avoid deadlock on self by exiting immediately.
    // Use pthread_equal because directly comparing two pthread_t vars is not allowed.
    // This re-entrancy only happens when it has aborted already due to out of memory
    // which should be a rare occurrence.
    char *errMsg = "errAbort re-entered due to out-of-memory condition. Exiting.\n";
    write(STDERR_FILENO, errMsg, strlen(errMsg)); 
    exit(1);   // out of memory is a serious problem, exit immediately, but allow atexit cleanup.
    }
pthread_mutex_unlock( &pidInUseMutex );

// This is the main mutex we really care about.
// It controls access to the hash where thread-specific data is stored.
static pthread_mutex_t ptavMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock( &ptavMutex );

// safely tell threads that pidInUse
// is valid and correctly set and may be compared to pid
pthread_mutex_lock( &pidInUseMutex );
pidInUse = pthread_self();  // setting it directly to pid is not allowed.
pidInUseValid = TRUE;
pthread_mutex_unlock( &pidInUseMutex );

// This means that if we crash due to out-of-memory below,
// it will be able to detect the re-entrancy and handle it above.

static struct hash *perThreadVars = NULL;
if (perThreadVars == NULL)
    perThreadVars = hashNew(0);
// convert the pid into a string for the hash key
char pidStr[64];
safef(pidStr, sizeof(pidStr), "%lld",  ptrToLL(pid));
struct hashEl *hel = hashLookup(perThreadVars, pidStr);
if (hel == NULL)
    {
    // if it is the first time, initialization the perThreadAbortVars
    struct perThreadAbortVars *ptav;
    AllocVar(ptav);
    ptav->debugPushPopErr = FALSE;
    ptav->errAbortInProgress = FALSE;
    ptav->warnIx = 0;
    ptav->warnArray[0] = defaultVaWarn;
    ptav->abortIx = 0;
    ptav->abortArray[0] = defaultAbort;
    hel = hashAdd(perThreadVars, pidStr, ptav);
    }

// safely tell other threads that pidInUse
// is no longer valid and may not be compared to pid
pthread_mutex_lock( &pidInUseMutex );
pidInUseValid = FALSE;
pthread_mutex_unlock( &pidInUseMutex );

// unlock our mutex controlling the hash of thread-specific data
pthread_mutex_unlock( &ptavMutex );
return (struct perThreadAbortVars *)(hel->val);
}

