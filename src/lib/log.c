/* log.h - write out log messages according to the
 * current verbosity level.  By default these messages
 * go to stderr, but they can end up going to a
 * file (and maybe eventually the system logging facility)
 * instead. */

#include "common.h"
#include "log.h"

static char const rcsid[] = "$Id: log.c,v 1.2 2004/02/16 23:24:52 kent Exp $";

static int logVerbosity = 1;	/* The level of log verbosity.  0 is silent. */
static FILE *logFile;	/* File to log to. */

void logVaPrintf(int verbosity, char *format, va_list args)
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

void logPrintf(int verbosity, char *format, ...)
/* Write printf formatted message to log (which by
 * default is stderr) if global verbose variable
 * is set to verbosity or higher. */
{
va_list args;
va_start(args, format);
logVaPrintf(verbosity, format, args);
va_end(args);
}

void logDot()
/* Write I'm alive dot (at verbosity level 1) */
{
logPrintf(1, ".");
}

void logToFile(char *fileName)
/* Set logging to named file rather than stderr. */
{
logFile = mustOpen(fileName, "w");
}

void logSetVerbosity(int verbosity)
/* Set verbosity level in log.  0 for no logging,
 * higher number for increasing verbosity. */
{
logVerbosity = verbosity;
}



