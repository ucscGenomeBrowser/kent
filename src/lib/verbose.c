/* verbose.c - write out status messages according to the
 * current verbosity level.  These messages go to stderr. */

#include "common.h"
#include "portable.h"
#include "verbose.h"

static char const rcsid[] = "$Id: verbose.c,v 1.6 2008/10/30 09:27:23 kent Exp $";

static int logVerbosity = 1;	/* The level of log verbosity.  0 is silent. */
static FILE *logFile;	/* File to log to. */

static boolean checkedDotsEnabled = FALSE;  /* have we check for dot output
                                             * being enabled? */
static boolean dotsEnabled = FALSE;         /* is dot output enabled? */

void verboseVa(int verbosity, char *format, va_list args)
/* Log with at given verbosity vprintf formatted args. */
{
if (verbosity <= logVerbosity)
    {
    if (logFile == NULL)
        logFile = stderr;
    vfprintf(logFile, format, args);
    fflush(logFile);
    }
}

void verbose(int verbosity, char *format, ...)
/* Write printf formatted message to log (which by
 * default is stderr) if global verbose variable
 * is set to verbosity or higher. */
{
va_list args;
va_start(args, format);
verboseVa(verbosity, format, args);
va_end(args);
}

void verboseTime(int verbosity, char *label, ...)
/* Print label and how long it's been since last call.  Call with
 * a NULL label to initialize. */
{
static long lastTime = 0;
long time = clock1000();
va_list args;
va_start(args, label);
if (label != NULL)
    {
    verboseVa(verbosity, label, args);
    verbose(verbosity, ": %ld millis\n", time - lastTime);
    }
lastTime = time;
va_end(args);
}


boolean verboseDotsEnabled()
/* check if outputting of happy dots are enabled.  They will be enabled if the
 * verbosity is > 0, stderr is a tty and we don't appear to be running an
 * emacs shell. */
{
if (!checkedDotsEnabled)
    {
    if (logFile == NULL)
        logFile = stderr;
    dotsEnabled = (logVerbosity > 0) && isatty(fileno(logFile));
    if (dotsEnabled)
        {
        /* check for an possible emacs shell */
        char *emacs = getenv("emacs");
        char *term = getenv("TERM");
        if ((emacs != NULL) && (emacs[0] == 't'))
            dotsEnabled = FALSE;
        else if ((term != NULL) && sameString(term, "dumb"))
            dotsEnabled = FALSE;
        }
    checkedDotsEnabled = TRUE;
    }
return dotsEnabled;
}

void verboseDot()
/* Write I'm alive dot (at verbosity level 1) */
{
if (verboseDotsEnabled())
    verbose(1, ".");
}

void verboseSetLevel(int verbosity)
/* Set verbosity level in log.  0 for no logging,
 * higher number for increasing verbosity. */
{
logVerbosity = verbosity;
checkedDotsEnabled = FALSE; /* force rechecking of dots enabled */
}

int verboseLevel(void)
/* Get verbosity level. */
{
return logVerbosity;
}

void verboseSetLogFile(char *name)
/* Set logFile for verbose messages overrides stderr. */
{
if (sameString(name, "stdout"))
    logFile = stdout;
else if (sameString(name, "stderr"))
    logFile = stderr;
else
    logFile = mustOpen(name, "w");
}

FILE *verboseLogFile()
/* Get the verbose log file. */
{
if (logFile == NULL)
    logFile = stderr;
return logFile;
}
